/*
 * SSTV receive: VIS detector (sstv/vis.c), line-sync tracker and autodetect
 * (sstv/linesync.c), progressive display (sstv/preview.c) and the decoders,
 * on pictures made by our encoder (src/sstv/encoder.c) in the 7 modes we
 * send, through a channel: frequency shift, clock error (line period too
 * long, in ppm), white noise.
 * SNR is signal rms against white noise over the whole 5.5-kHz band
 * (+3.4 dB for 2.5 kHz).
 *
 * Exit status 0 when every case passes.
 */

#include "sstv/decoder.h"
#include "sstv/encoder.h"
#include "sstv/linesync.h"
#include "sstv/preview.h"
#include "sstv/vis.h"

#include <math.h>
#include <png.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR      SSTV_SAMPLE_RATE
#define PIC     "/tmp/test_sstv.png"
#define HILBERT 127
#define PI      3.14159265358979323846
#define VIS_LEN ((long)(0.910 * SR))

typedef int (*enc_fn)(const char *, unsigned, int16_t **, size_t *);

static const struct {
    const char *name;
    enc_fn      enc;
    sstv_mode_t mode;
} modes[] = {
    {"Robot 8 BW", sstv_encode_robot8_png_pcm, SSTV_MODE_ROBOT8_BW},
    {"Robot 36", sstv_encode_robot36_png_pcm, SSTV_MODE_ROBOT36},
    {"Robot 72", sstv_encode_robot72_png_pcm, SSTV_MODE_ROBOT72},
    {"Martin M1", sstv_encode_martin_m1_png_pcm, SSTV_MODE_MARTIN_M1},
    {"Martin M2", sstv_encode_martin_m2_png_pcm, SSTV_MODE_MARTIN_M2},
    {"Scottie S1", sstv_encode_scottie_s1_png_pcm, SSTV_MODE_SCOTTIE_S1},
    {"Scottie S2", sstv_encode_scottie_s2_png_pcm, SSTV_MODE_SCOTTIE_S2},
};
#define NMODES (int)(sizeof(modes) / sizeof(modes[0]))

static float *clean[NMODES];     /* encoder output, VIS first */
static long   clean_n[NMODES];
static double clean_rms[NMODES];
static int    fails;

static void write_picture(void)
{
    static uint8_t rows[256][320 * 3];
    png_bytep      rp[256];

    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t *p = &rows[y][x * 3];
            if (y < 128) {                                  /* colour bars on top, gradients below */
                p[0] = (x / 40) & 1 ? 255 : 0;
                p[1] = (x / 40) & 2 ? 255 : 0;
                p[2] = (x / 40) & 4 ? 255 : 0;
            } else {
                p[0] = (uint8_t)(x * 255 / 319);
                p[1] = (uint8_t)((y - 128) * 255 / 127);
                p[2] = (uint8_t)(255 - x * 255 / 319);
            }
        }
        rp[y] = rows[y];
    }
    FILE       *f   = fopen(PIC, "wb");
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   inf = png_create_info_struct(png);
    png_init_io(png, f);
    png_set_IHDR(png, inf, 320, 256, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, inf);
    png_write_image(png, rp);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &inf);
    fclose(f);
}

static double gauss(void)
{
    double u = (rand() + 1.0) / (RAND_MAX + 2.0), v = rand() / (RAND_MAX + 1.0);
    return sqrt(-2.0 * log(u)) * cos(2.0 * PI * v);
}

/* lead s of noise, the signal of mode m from sample skip on, tail s of noise.
 * shift: Hz (Hilbert FIR, single sideband); ppm: transmitter clock error. */
static float *channel(int m, long skip, double lead, double tail, double shift, double ppm, double snr_db, long *n)
{
    static float hil[HILBERT];
    long         a = (long)(lead * SR), sig = (long)((clean_n[m] - skip) * (1.0 + ppm * 1e-6));
    double       nz = clean_rms[m] / pow(10.0, snr_db / 20.0);

    if (!hil[0] && !hil[1])
        for (int k = 0; k < HILBERT; k++) {
            int d  = k - HILBERT / 2;
            hil[k] = d & 1 ? (float)(2.0 / (PI * d) * (0.54 - 0.46 * cos(2.0 * PI * k / (HILBERT - 1)))) : 0;
        }
    *n       = a + sig + (long)(tail * SR);
    float *s = calloc((size_t)*n, sizeof(float)), *x = calloc((size_t)*n, sizeof(float));
    for (long i = 0; i < sig; i++) {               /* resampled: line period ppm too long */
        double t = skip + i / (1.0 + ppm * 1e-6);
        long   k = (long)t;
        s[a + i] = k + 1 < clean_n[m] ? (float)(clean[m][k] + (clean[m][k + 1] - clean[m][k]) * (t - k)) : 0;
    }
    for (long i = 0; i < *n; i++) {
        double v = s[i];
        if (shift && i >= HILBERT) {
            double q = 0;
            for (int k = 0; k < HILBERT; k++) q += hil[k] * s[i - k];
            double ph = 2.0 * PI * shift * i / SR;
            v         = s[i - HILBERT / 2] * cos(ph) - q * sin(ph);
        }
        x[i] = (float)(v + nz * gauss());
    }
    free(s);
    return x;
}

/* Auto RX: 1-s windows every 50 ms.  Returns the detected mode or -1. */
static int vis_scan(const float *x, long n, sstv_vis_result_t *r, long *win)
{
    sstv_info_t info;
    for (long w = rand() % (SR / 20); w + SR <= n; w += SR / 20)
        if (sstv_vis_detect(x + w, SR, &info, r)) {
            if (win) *win = w;
            return info.mode;
        } else if (r->unsupported_vis >= 0)
            return -2;
    return -1;
}

static void check(const char *name, bool ok, const char *fmt, ...)
{
    va_list ap;
    printf("%-44s %s  ", name, ok ? "PASS" : "FAIL");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fails += !ok;
}

/* Luma correlation of rows [0, rows) of got against picture rows [off, off + rows) */
static double corr(const uint8_t *got, int w, int h, int rows, int off)
{
    static uint8_t want[SSTV_MAX_W * SSTV_MAX_H * 3];
    double         sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
    long           n  = 0;
    sstv_load_png_scaled(PIC, want, w, h);
    for (int y = 0; y < rows && y + off < h; y++)
        for (int x = 0; x < w; x++, n++) {
            const uint8_t *p = got + ((size_t)y * w + x) * 3, *q = want + ((size_t)(y + off) * w + x) * 3;
            double         a = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
            double         b = 0.299 * q[0] + 0.587 * q[1] + 0.114 * q[2];
            sa += a, sb += b, saa += a * a, sbb += b * b, sab += a * b;
        }
    if (!n) return 0;
    double cov = sab / n - sa / n * sb / n, va = saa / n - sa / n * sa / n, vb = sbb / n - sb / n * sb / n;
    return cov / sqrt(va * vb + 1e-9);
}

/* A VIS frame alone (leader, break, start bit, 7 bits, parity, stop bit) */
static float *vis_only(int code, long *n)
{
    static const struct { double hz, ms; } head[] = {{1900, 300}, {1200, 10}, {1900, 300}, {1200, 30}};
    double tones[14][2];
    int    k = 0, ones = 0;
    for (int i = 0; i < 4; i++) tones[k][0] = head[i].hz, tones[k++][1] = head[i].ms;
    for (int b = 0; b < 7; b++) {
        ones += (code >> b) & 1;
        tones[k][0] = (code >> b) & 1 ? 1100 : 1300, tones[k++][1] = 30;
    }
    tones[k][0] = ones & 1 ? 1100 : 1300, tones[k++][1] = 30;
    tones[k][0] = 1200, tones[k++][1] = 30;
    *n       = 2 * SR;
    float *x = calloc((size_t)*n, sizeof(float));
    double ph = 0;
    long   i = SR / 4;
    for (int t = 0; t < k; t++)
        for (long e = i + (long)(tones[t][1] * SR / 1000); i < e; i++, ph += 2 * PI * tones[t][0] / SR)
            x[i] = (float)(0.5 * sin(ph) + 0.01 * gauss());
    return x;
}

static void test_vis(void)
{
    sstv_vis_result_t r;
    long              n;
    int               ok = 0, wrong = 0, tot = 0;

    /* Measured with 20-30 trials per mode: the Goertzel detector this
     * replaces (live_find_vis) found 82% at -13 dB (91% at -12), never
     * Robot 8, and 4% wrong modes; the new one 91% at -13 dB, no wrong mode. */
    for (int m = 0; m < NMODES; m++)
        for (int t = 0; t < 8; t++, tot++) {
            float *x   = channel(m, 0, 1.0, 0.0, 0, 0, -13.0, &n);
            int    got = vis_scan(x, n < 4 * SR ? n : 4 * SR, &r, NULL);
            ok += got == (int)modes[m].mode;
            wrong += got >= 0 && got != (int)modes[m].mode;
            free(x);
        }
    check("VIS at -13 dB, all modes", ok >= tot * 85 / 100 && wrong <= 1, "%d%% found, %d wrong", 100 * ok / tot, wrong);

    for (int s = -50; s <= 50; s += 100) {
        double worst = 0;
        ok = tot = 0;
        for (int m = 0; m < NMODES; m++, tot++) {
            float *x = channel(m, 0, 1.0, 0.0, s, 0, 10.0, &n);
            ok += vis_scan(x, 4 * SR, &r, NULL) == (int)modes[m].mode;
            if (fabs(r.offset_hz - s) > worst) worst = fabs(r.offset_hz - s);
            free(x);
            x = channel(m, 0, 1.0, 0.0, s, 0, -10.0, &n);
            ok += vis_scan(x, 4 * SR, &r, NULL) == (int)modes[m].mode;
            free(x);
        }
        char name[64];
        snprintf(name, sizeof(name), "VIS mistuned %+d Hz, +10 and -10 dB", s);
        check(name, ok == 2 * tot && worst < 1.0, "offset error %.2f Hz, %d missed", worst, 2 * tot - ok);
    }

    /* 2 minutes of noise: no VIS */
    float *x   = channel(0, clean_n[0], 120.0, 0.0, 0, 0, 0.0, &n);
    int    got = 0;
    sstv_info_t info;
    for (long w = 0; w + SR <= n; w += SR / 20) got += sstv_vis_detect(x + w, SR, &info, &r) || r.unsupported_vis >= 0;
    free(x);
    check("VIS in 120 s of noise", got == 0, "%d false headers", got);

    x   = vis_only(95, &n);         /* PD 120: a known mode we do not decode */
    got = vis_scan(x, n, &r, NULL);
    check("VIS 95 (PD 120) unsupported", got == -2 && r.unsupported_vis == 95,
          "unsupported_vis %d", r.unsupported_vis);
    free(x);
}

/* Line syncs without VIS: the stream starts 1.5 s into the picture, after
 * 3 s of noise, and ends with 25 s of noise.  ppm: transmitter clock error. */
static void test_linesync(int m, double snr, double ppm)
{
    long           n, skip = VIS_LEN + (long)(1.5 * SR);
    float         *x   = channel(m, skip, 3.0, 25.0, 0, ppm, snr, &n);
    long           end = n - 25 * SR, lock = -1, lost = -1;
    sstv_linesync_t ls;
    char           name[64];

    sstv_linesync_init(&ls, SSTV_MODE_UNKNOWN);
    for (long k = 0; k < n && lost < 0; k += 1024) {
        sstv_linesync_process(&ls, x + k, (size_t)(n - k < 1024 ? n - k : 1024));
        if (lock < 0 && ls.st.state != SSTV_LS_SEARCH) lock = k;
        if (ls.st.state == SSTV_LS_LOST) lost = k;
    }
    snprintf(name, sizeof(name), "autodetect %s %+.0f dB %+.0f ppm", modes[m].name, snr, ppm);
    check(name, ls.st.mode == modes[m].mode && (!ppm || fabs(ls.st.clock_ppm - ppm) < 150),
          "locked after %.1f s, clock %+.0f ppm", (lock - 3.0 * SR) / SR, ls.st.clock_ppm);
    /* The end also needs 10 s without video: in-band energy share >= 0.65,
     * which white noise over 5.5 kHz hides below about +3 dB */
    if (snr >= 6.0) {
        snprintf(name, sizeof(name), "end of picture %s %+.0f dB", modes[m].name, snr);
        check(name, lost > end && lost < end + 13 * SR, "LOST %.1f s after the end", (double)(lost - end) / SR);
    }

    long        ev;
    sstv_mode_t c = sstv_linesync_classify(x, n, SSTV_MODE_UNKNOWN, &ev);
    snprintf(name, sizeof(name), "classify %s", modes[m].name);
    check(name, c == modes[m].mode, "%ld line intervals", ev);
    free(x);
}

/* Manual RX with no VIS, Start pressed 20 s before the picture: the mode
 * from the line syncs, then the whole picture decoded. */
static void test_manual(int m)
{
    static uint8_t rgb[SSTV_MAX_W * SSTV_MAX_H * 3];
    sstv_info_t    info;
    long           n;
    float         *x = channel(m, VIS_LEN, 20.0, 8.0, 0, 0, 10.0, &n);
    sstv_mode_t    c = sstv_linesync_classify(x, n, SSTV_MODE_UNKNOWN, NULL);
    int            rows = c == modes[m].mode ? sstv_decode_cont_samples(x, n, c, rgb, &info) : -1;
    double         r    = rows > 0 ? corr(rgb, info.width, info.height, rows, 0) : 0;
    char           name[64];

    snprintf(name, sizeof(name), "Manual late start %s", modes[m].name);
    check(name, rows >= info.height * 95 / 100 && r > 0.9, "%d rows, luma correlation %.3f", rows, r);
    free(x);
}

/* Progressive display as the dialog runs it: audio in 1024-sample blocks into
 * the capture and the tracker, rows drawn as soon as their audio is there.
 * vis: row 0 from the VIS (Auto RX), else from the tracker (Manual RX). */
static void test_preview(int m, bool vis)
{
    static uint8_t img[SSTV_MAX_W * SSTV_MAX_H * 3];
    long           n, win = 0;
    float         *x = channel(m, vis ? 0 : VIS_LEN, 1.0, 2.0, 0, 0, 10.0, &n);
    sstv_vis_result_t r;
    sstv_linesync_t   ls;
    sstv_preview_t    pv;
    int               drawn = 0, last = -1;

    double row0 = -1;
    if (vis && vis_scan(x, n, &r, &win) == (int)modes[m].mode)
        row0 = win + (r.start_s + SSTV_VIS_LEN_S + sstv_row0_anchor_after_vis(modes[m].mode)) * SR;
    sstv_linesync_init(&ls, modes[m].mode);
    sstv_preview_init(&pv, modes[m].mode, row0);
    memset(img, 0, sizeof(img));
    for (long k = 0; k < n; k += 1024) {
        sstv_linesync_process(&ls, x + k, (size_t)(n - k < 1024 ? n - k : 1024));
        long a, b, written = k + 1024 < n ? k + 1024 : n;
        while (sstv_preview_need(&pv, written, &ls.st, &a, &b)) {
            int row = sstv_preview_render(&pv, x + a, img + (size_t)pv.next_row * pv.width * 3);
            drawn += row >= 0;
            last = row;
        }
    }
    /* Manual: row 0 is the first sync of the lock, a few lines in */
    double best = 0;
    int    off  = 0;
    for (int o = 0; o <= (vis ? 0 : 6); o++) {
        double c = corr(img, pv.width, pv.height, last + 1 - o, o);
        if (c > best) best = c, off = o;
    }
    char name[64];
    snprintf(name, sizeof(name), "preview %s %s", vis ? "Auto" : "Manual", modes[m].name);
    check(name, drawn >= pv.height * 9 / 10 - off && best > 0.85, "%d rows, luma correlation %.3f", drawn, best);
    free(x);
}

int main(void)
{
    write_picture();
    srand(7);
    for (int m = 0; m < NMODES; m++) {
        int16_t *pcm = NULL;
        size_t   n   = 0;
        double   pw  = 0;
        if (modes[m].enc(PIC, SR, &pcm, &n) != 0 || !n) {
            printf("%s: encode failed\n", modes[m].name);
            return 1;
        }
        clean[m]   = malloc(n * sizeof(float));
        clean_n[m] = (long)n;
        for (size_t i = 0; i < n; i++) clean[m][i] = pcm[i] / 32768.0f, pw += (double)clean[m][i] * clean[m][i];
        clean_rms[m] = sqrt(pw / n);
        free(pcm);
    }

    test_vis();
    for (int m = 0; m < NMODES; m++) test_linesync(m, 0.0, 0), test_linesync(m, 6.0, 0);
    test_linesync(3, 10.0, 300);       /* the clock error is measured at a better SNR */
    test_linesync(1, 10.0, -300);
    for (int m = 0; m < NMODES; m++) test_manual(m);
    for (int m = 0; m < NMODES; m++) test_preview(m, true), test_preview(m, false);

    printf(fails ? "%d FAILED\n" : "all OK\n", fails);
    return fails != 0;
}
