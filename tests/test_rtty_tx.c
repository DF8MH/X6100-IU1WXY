/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Host check of the RTTY TX of src/rtty.c: rtty_remote("rtty_send") makes the AFSK, the fake
 *  tx_player_play() below keeps it, and the receiver of rtty.c decodes it back, in USB and LSB,
 *  normal and reverse. From the repo root (liquid from tools/js8_ui_harness/fetch_deps.sh):
 *
 *    D=tools/apps_ui_harness/deps
 *    gcc -std=gnu11 -I. -Isrc -isystem $D/include -isystem $D/src -DLV_DRAW_NEON=0 tests/test_rtty_tx.c \
 *        src/rtty.c src/cJSON.c $D/lib/libliquid.a -lfftw3f -lm -lpthread -o /tmp/test_rtty_tx && /tmp/test_rtty_tx
 */

#include "rtty.h"

#include "audio.h"
#include "cJSON.h"
#include "cfg/cfg_api.h"
#include "dsp.h"
#include "params/params.h"
#include "tx_player.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* What rtty.c needs from the firmware */

params_t          params = {.rtty_center = 800, .rtty_shift = 450, .rtty_rate = 5000, .rtty_bits = 5, .rtty_snr = 3.0f};
ComputedParamInt *cfg_cur_mode;

static int32_t     mode;
static observer_cb mode_cb;
static char        rx[1024];
static int16_t    *played;
static uint32_t    played_n;
static volatile int result = 0;

int32_t cparam_i_get(const ComputedParamInt *p) { (void)p; return mode; }
Observer *subject_subscribe_and_notify(Subject *subj, observer_cb fn, void *user_data) {
    mode_cb = fn;
    fn(subj, user_data);
    return NULL;
}
void params_lock() {}
void params_unlock(bool *dirty) { (void)dirty; }
int32_t align_int(int32_t x, uint16_t step) { return x / step * step; }
int32_t limit(int32_t x, int32_t min, int32_t max) { return x < min ? min : x > max ? max : x; }
void panel_add_text(const char *text) { strncat(rx, text, sizeof(rx) - strlen(rx) - 1); }
void msg_update_text_fmt(const char *fmt, ...) { printf("msg: %s\n", fmt); }
void remote_text(const char *type, const char *s, size_t n) { (void)type; (void)s; (void)n; }
void remote_event(cJSON *o) {
    if (!strcmp(cJSON_GetObjectItem(o, "t")->valuestring, "rtty_txdone"))
        result = cJSON_GetObjectItem(o, "result")->valueint;
    cJSON_Delete(o);
}
float tx_player_base_gain_offset(void) { return 0.0f; }
bool tx_player_play(int16_t *samples, uint32_t n, int32_t tx_offset_hz, float gain, tx_abort_fn_t abort_check, void *ctx) {
    (void)gain;
    assert(tx_offset_hz == TX_PLAYER_AUDIO_HZ);
    assert(rtty_get_state() == RTTY_TX);
    played   = malloc(n * sizeof(int16_t));
    played_n = n;
    memcpy(played, samples, n * sizeof(int16_t));
    for (uint32_t i = 0; i < n; i += 2048)
        assert(!abort_check(ctx));
    return true;
}

static void check(int32_t m, bool rev, const char *text, const char *want) {
    char state[128];

    mode                = m;
    params.rtty_reverse = rev;
    mode_cb(NULL, NULL);
    rx[0]  = 0;
    result = 0;
    rtty_remote("rtty_send", 0, text);
    for (int i = 0; i < 1000 && !result; i++)
        usleep(10000);
    assert(result == 1 && rtty_get_state() == RTTY_RX);
    rtty_remote_state(state, sizeof(state));

    /* The capture rate of the receiver: every AUDIO_DECIM-th sample (the tones are far below its Nyquist) */
    for (uint32_t i = 0; i + AUDIO_DECIM * 256 <= played_n; i += AUDIO_DECIM * 256) {
        float buf[256];
        for (int k = 0; k < 256; k++)
            buf[k] = played[i + k * AUDIO_DECIM] / 32768.0f;
        rtty_put_audio_samples(256, buf);
    }
    free(played);
    printf("mode %i rev %i: %.1f s, \"%s\" %s\n", (int)m, rev, played_n / (float)AUDIO_PLAY_RATE, rx, state);
    assert(!strcmp(rx, want));
}

int main() {
    rtty_init();
    rtty_set_state(RTTY_RX);
    rtty_remote("rtty_rate", 4545, "");
    rtty_remote("rtty_shift", 170, "");
    rtty_remote("rtty_center", 1000, "");
    assert(params.rtty_rate == 4545 && params.rtty_shift == 170 && params.rtty_center == 1000);

    const char *text = "cq cq de iu1wxy 599 73/? -k";
    const char *want = "CQ CQ DE IU1WXY 599 73/? -K";
    check(x6100_mode_usb, false, text, want);
    check(x6100_mode_usb, true, text, want);
    check(x6100_mode_lsb, false, text, want);
    check(x6100_mode_lsb_dig, true, text, want);
    check(x6100_mode_usb_dig, false, "A~B", "AB");   /* no Baudot for ~ */

    rtty_remote("rtty_rate", 7500, "");
    rtty_remote("rtty_shift", 850, "");
    check(x6100_mode_usb, false, text, want);
    printf("all OK\n");
    return 0;
}
