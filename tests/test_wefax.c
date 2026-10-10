/*
 * WeFax AUTO start/stop detection: faxes made by src/wefax/encoder.c at the
 * decoder's rate go through a channel (frequency shift, white noise) into
 * src/wefax/decoder.c, which is re-armed on STOP as dialog_wefax.c does.
 *
 * Exit status 0 when every case passes.
 */

#include "wefax/encoder.h"

#include <math.h>
#include <png.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIC_H    100
#define PIC_PATH "/tmp/test_wefax.png"
#define HILBERT  127
#define PI       3.14159265358979323846

bool     wefax_decoder_auto_fax_confirmed(const wefax_decoder_t *decoder);
uint32_t wefax_decoder_auto_start_generation(const wefax_decoder_t *decoder);

static wefax_decoder_t *dec;
static unsigned rows;           /* rows kept (fax confirmed), as the dialog */
static unsigned stops;          /* STOPs seen, decoder re-armed after each */
static uint32_t generation;     /* START generation seen before the last re-arm */

/* Channel */
static double shift_hz, noise_rms, mix_phase;
static float  hil[HILBERT], hist[HILBERT];
static unsigned hpos;

/* Gradient on the left, a checkerboard on the right, black bars every 60 lines */
static int write_pic(void)
{
    FILE       *fp = fopen(PIC_PATH, "wb");
    png_structp p  = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   in = png_create_info_struct(p);
    uint8_t     row[WEFAX_WIDTH];

    if (!fp)
        return -1;
    png_init_io(p, fp);
    png_set_IHDR(p, in, WEFAX_WIDTH, PIC_H, 8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(p, in);
    for (unsigned y = 0; y < PIC_H; y++) {
        for (unsigned x = 0; x < WEFAX_WIDTH; x++)
            row[x] = x < WEFAX_WIDTH / 2 ? (y % 60 < 6 ? 0 : x * 255 / (WEFAX_WIDTH / 2))
                                         : ((y / 25 + x / 100) & 1) * 255;
        png_write_row(p, row);
    }
    png_write_end(p, NULL);
    png_destroy_write_struct(&p, &in);
    fclose(fp);
    return 0;
}

static void row_cb(const uint8_t *row, unsigned int width, unsigned int n, void *ctx)
{
    (void)row; (void)width; (void)n; (void)ctx;
    if (wefax_decoder_auto_fax_confirmed(dec))
        rows++;
}

static double gauss(void)
{
    double u = (rand() + 1.0) / (RAND_MAX + 2.0), v = rand() / (RAND_MAX + 1.0);

    return sqrt(-2.0 * log(u)) * cos(2.0 * PI * v);
}

/* Shift up by shift_hz (Hilbert FIR, single sideband), add noise, decode */
static void rx(const int16_t *pcm, size_t n)
{
    float buf[1024];

    for (size_t i = 0; i < n; i++) {
        float x = pcm[i] / 32768.0f, q = 0, d;

        hist[hpos] = x;
        for (unsigned k = 0; k < HILBERT; k++)
            q += hil[k] * hist[(hpos + HILBERT - k) % HILBERT];
        d = hist[(hpos + HILBERT - HILBERT / 2) % HILBERT];
        hpos = (hpos + 1) % HILBERT;

        buf[i] = shift_hz ? d * cos(mix_phase) - q * sin(mix_phase) : x;
        buf[i] += noise_rms * gauss();
        mix_phase = fmod(mix_phase + 2.0 * PI * shift_hz / WEFAX_SAMPLE_RATE, 2.0 * PI);
    }
    wefax_decoder_process(dec, buf, (unsigned)n);
    if (wefax_decoder_auto_stop_detected(dec)) {
        stops++;
        generation = wefax_decoder_auto_start_generation(dec);
        wefax_decoder_set_mode(dec, WEFAX_MODE_AUTO);   /* dialog: save, re-arm */
    }
}

/* A fax, cut after max_s seconds when max_s > 0 */
static void send_fax(double max_s)
{
    int              rc;
    int16_t          pcm[1024];
    size_t           n, left = max_s > 0 ? (size_t)(max_s * WEFAX_SAMPLE_RATE) : (size_t)-1;
    wefax_encoder_t *enc = wefax_encoder_open(PIC_PATH, WEFAX_SAMPLE_RATE, &rc);

    if (!enc) {
        printf("encoder open failed %d\n", rc);
        exit(1);
    }
    while (left && (n = wefax_encoder_fill(pcm, left < 1024 ? left : 1024, enc))) {
        rx(pcm, n);
        left -= n;
    }
    wefax_encoder_close(enc);
}

static void send_silence(double s)
{
    int16_t pcm[1024] = {0};

    for (size_t left = (size_t)(s * WEFAX_SAMPLE_RATE), n; left; left -= n) {
        n = left < 1024 ? left : 1024;
        rx(pcm, n);
    }
}

static void begin(double shift, double noise)
{
    shift_hz  = shift;
    noise_rms = noise;
    mix_phase = 0;
    hpos      = 0;
    memset(hist, 0, sizeof(hist));
    rows = stops = generation = 0;
    wefax_decoder_set_mode(dec, WEFAX_MODE_AUTO);
}

static int check(const char *name, bool ok)
{
    printf("%-44s %s (gen %u, stops %u, rows %u, now gen %u confirmed %d)\n", name, ok ? "PASS" : "FAIL",
           generation, stops, rows, wefax_decoder_auto_start_generation(dec), wefax_decoder_auto_fax_confirmed(dec));
    return !ok;
}

int main(void)
{
    int fails = 0;

    for (int k = 0; k < HILBERT; k++) {
        int n = k - HILBERT / 2;

        hil[k] = n & 1 ? (float)(2.0 / (PI * n) * (0.54 - 0.46 * cos(2.0 * PI * k / (HILBERT - 1)))) : 0;
    }
    if (write_pic())
        return 1;
    srand(1);
    dec = wefax_decoder_create();
    wefax_decoder_set_row_callback(dec, row_cb, NULL);

    /* Signal rms 0.53: noise 0.02 is ~28 dB SNR, 0.5 is ~0.5 dB in 5.5 kHz (~4 dB in 2.5 kHz) */
    begin(0, 0.02);
    send_silence(3);
    send_fax(0);
    send_silence(3);
    fails += check("clean: start and stop", stops == 1 && generation == 1 && rows >= PIC_H);

    begin(0, 0.5);
    send_silence(3);
    send_fax(0);
    send_silence(3);
    fails += check("low SNR: stop", stops == 1 && generation == 1);

    begin(150, 0.02);
    send_silence(3);
    send_fax(0);
    send_silence(3);
    fails += check("mistuned +150 Hz: stop", stops == 1 && generation == 1);

    begin(-150, 0.02);
    send_silence(3);
    send_fax(0);
    send_silence(3);
    fails += check("mistuned -150 Hz: stop", stops == 1 && generation == 1);

    /* A second fax starts in the middle of the first picture (STOP missed) */
    begin(0, 0.02);
    send_silence(3);
    send_fax(5 + 30 + 25);
    bool first_kept = wefax_decoder_auto_fax_confirmed(dec) && wefax_decoder_auto_start_generation(dec) == 1;
    send_fax(0);
    send_silence(3);
    fails += check("restart: second fax replaces the first", first_kept && stops == 1 && generation == 2);

    /* After STOP: rest of the STOP tone, 10 s black, noise: nothing must start */
    begin(0, 0.02);
    send_fax(0);
    send_silence(20);
    fails += check("no ghost fax after STOP", stops == 1 &&
                   wefax_decoder_auto_start_generation(dec) == 0 && !wefax_decoder_auto_fax_confirmed(dec));

    wefax_decoder_destroy(dec);
    printf(fails ? "FAIL\n" : "PASS\n");
    return fails != 0;
}
