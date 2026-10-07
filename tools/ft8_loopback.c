/*
 * FT8 / FT4 TX -> RX loopback on the host with the app's own worker (src/ft8/worker.c): each message
 * made by ftx_worker_generate_tx_samples() at the play rate, as the TX does, several stations summed
 * with noise, brought down to the decoder's rate as the RX does (44100 / 4), fed block by block and
 * decoded at the end of the slot. Checks every message comes back with its frequency.
 *
 *   cd ~/X6100-IU1WXY
 *   D=tools/apps_ui_harness/deps; S=/tmp/ft8_stub; mkdir -p $S/lvgl
 *   echo '#include <stdio.h>
 *   #define LV_LOG_ERROR(...) (fprintf(stderr, __VA_ARGS__), fputc(10, stderr))
 *   #define LV_LOG_USER(...) LV_LOG_ERROR(__VA_ARGS__)
 *   #define LV_LOG_INFO(...) ((void)0)' > $S/lvgl/lvgl.h
 *   gcc -O2 -std=gnu11 -I$S -Isrc -isystem $D/include -isystem $D/src/ft8_lib -o /tmp/ft8_loopback \
 *       tools/ft8_loopback.c src/ft8/worker.c src/ft8/gfsk.c $D/src/ft8_lib/ft8/*.c $D/lib/libliquid.a \
 *       -lfftw3f -lm && /tmp/ft8_loopback
 *
 * Exit status 0 when every message decodes in both protocols.
 */

#include "ft8/worker.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAY_RATE 44100
#define DECIM     4
#define RX_RATE   (PLAY_RATE / DECIM)

static const struct {
    const char *text;
    int         freq;
    float       snr_db; /* in 2500 Hz, as WSJT-X */
} stations[] = {
    {"CQ IU1WXY JN35", 600, -5},
    {"IU1WXY DL1ABC JO62", 1100, -10},
    {"DL1ABC IU1WXY -12", 1500, -14},
    {"EA3XYZ IU1WXY R-08", 2000, -16},
    {"IU1WXY K1ABC RR73", 2400, -18},
};
#define N_ST (int)(sizeof(stations) / sizeof(stations[0]))

static bool  found[N_ST];
static float freq_tol; /* half a tone: FT8 6.25 Hz, FT4 20.8 Hz apart */
static float snr_shift; /* added to every station (env SNR_SHIFT): to find the decoder's limit */
static int  n_found;

static void msg_cb(const char *text, int snr, float freq_hz, float time_sec, void *user) {
    (void)user;
    for (int i = 0; i < N_ST; i++)
        if (!found[i] && !strcmp(text, stations[i].text) && fabsf(freq_hz - stations[i].freq) <= freq_tol) {
            found[i] = true;
            n_found++;
            printf("  %-20s %5.0f Hz  %+3d dB  dt %.2f\n", text, freq_hz, snr, time_sec);
            return;
        }
    printf("  unexpected: %s %.0f Hz\n", text, freq_hz);
}

static double gauss(void) {
    double u = (rand() + 1.0) / (RAND_MAX + 2.0), v = (rand() + 1.0) / (RAND_MAX + 2.0);
    return sqrt(-2 * log(u)) * cos(2 * M_PI * v);
}

static int run(ftx_protocol_t proto, const char *name, float slot_s) {
    long   n_play = (long)(slot_s * PLAY_RATE);
    float *mix    = calloc(n_play, sizeof(float));
    long   lead   = (long)(0.5 * PLAY_RATE); /* the signal starts 0.5 s into the slot, as on the air */

    ftx_worker_init(RX_RATE, proto);
    freq_tol = proto == FTX_PROTOCOL_FT8 ? 6.25f : 20.83f;
    for (int i = 0; i < N_ST; i++) {
        int16_t *s = NULL;
        uint32_t n = 0;
        if (!ftx_worker_generate_tx_samples(stations[i].text, stations[i].freq, PLAY_RATE, &s, &n)) {
            printf("%s: can't encode \"%s\"\n", name, stations[i].text);
            return 1;
        }
        /* amplitude for the SNR: noise of unit power per sample has 2500 / (PLAY_RATE / 2) of it in 2500 Hz */
        double pw = 0;
        for (uint32_t k = 0; k < n; k++) pw += (double)s[k] * s[k];
        double noise_2500 = 2500.0 / (PLAY_RATE / 2.0);
        double g          = sqrt(noise_2500 * pow(10, (stations[i].snr_db + snr_shift) / 10) / (pw / n));
        for (uint32_t k = 0; k < n && lead + k < (uint32_t)n_play; k++) mix[lead + k] += (float)(g * s[k]);
        free(s);
    }
    for (long k = 0; k < n_play; k++) mix[k] += (float)gauss();

    /* to RX_RATE: windowed-sinc low pass at 4 kHz, every 4th sample; then the level the RX sees */
    enum { TAPS = 63 };
    float h[TAPS], hs = 0;
    for (int k = 0; k < TAPS; k++) {
        double m = k - (TAPS - 1) / 2.0, fc = 4000.0 / PLAY_RATE;
        h[k] = (float)((m == 0 ? 2 * fc : sin(2 * M_PI * fc * m) / (M_PI * m)) * (0.54 - 0.46 * cos(2 * M_PI * k / (TAPS - 1))));
        hs += h[k];
    }
    long   n_rx = n_play / DECIM;
    float *rx   = malloc(n_rx * sizeof(float));
    for (long j = 0; j < n_rx; j++) {
        double acc = 0;
        for (int k = 0; k < TAPS; k++) {
            long i = j * DECIM - k;
            if (i >= 0) acc += h[k] / hs * mix[i];
        }
        rx[j] = (float)(acc * 0.05);
    }
    free(mix);

    memset(found, 0, sizeof(found));
    n_found = 0;
    printf("%s:\n", name);
    int bs = ftx_worker_get_block_size();
    /* as audio_worker.c: a decode after every block (candidates first, then early passes), the last full */
    for (long j = 0; j + bs <= n_rx && !ftx_worker_is_full(); j += bs) {
        ftx_worker_put_rx_samples(rx + j, bs);
        ftx_worker_decode(msg_cb, ftx_worker_is_full(), NULL);
    }
    ftx_worker_decode(msg_cb, true, NULL);
    free(rx);
    ftx_worker_free();
    for (int i = 0; i < N_ST; i++)
        if (!found[i]) printf("  MISSED %s (%d Hz, %+.0f dB)\n", stations[i].text, stations[i].freq, stations[i].snr_db);
    printf("  %d of %d decoded\n", n_found, N_ST);
    return n_found != N_ST;
}

int main(void) {
    srand(3);
    if (getenv("SNR_SHIFT")) snr_shift = (float)atof(getenv("SNR_SHIFT"));
    int fails = run(FTX_PROTOCOL_FT8, "FT8", 15.0f);
    snr_shift += 4; /* ft8_lib's FT4 holds to about -14 dB (WSJT-X -17.5): the same stations 4 dB up */
    fails += run(FTX_PROTOCOL_FT4, "FT4", 7.5f);
    printf(fails ? "FAILED\n" : "all OK\n");
    return fails != 0;
}
