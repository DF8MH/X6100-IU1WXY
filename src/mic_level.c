/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - the microphone gain from the ALC in voice (mic_level.h)
 */

#include "mic_level.h"

#include <stddef.h>

void mic_level_start(mic_level_t *l, float target_w) {
    *l          = (mic_level_t){0};
    l->target_w = target_w;
}

int mic_level_reading(mic_level_t *l, float alc, float pwr_w) {
    if (l->n == 0) {
        l->min_alc = l->max_alc = alc;
        l->min_w = l->max_w = pwr_w;
    }
    l->n++;
    l->sum_alc += alc;
    if (alc < l->min_alc) l->min_alc = alc;
    if (alc > l->max_alc) l->max_alc = alc;
    if (pwr_w < l->min_w) l->min_w = pwr_w;
    if (pwr_w > l->max_w) l->max_w = pwr_w;
    l->tx_n++;
    if (alc > l->tx_max_alc) l->tx_max_alc = alc;
    if (l->n < MIC_LEVEL_WINDOW) return 0;

    /* Voice moves the power and the ALC; a carrier (TUNE) does not */
    bool voice = l->max_w - l->min_w > 0.2f * l->target_w || l->max_alc - l->min_alc > 1.0f;
    float a = l->sum_alc / l->n;
    l->n = 0;
    l->sum_alc = 0.0f;
    if (voice) l->tx_voice = true;
    if (voice && a > MIC_LEVEL_ALC_HIGH && l->down < MIC_LEVEL_DOWN_MAX) {
        l->down += MIC_LEVEL_DOWN;
        return -MIC_LEVEL_DOWN;
    }
    return 0;
}

int mic_level_end(mic_level_t *l) {
    /* At least 4 s of talking and the ALC never reached 1: up, so it works on the peaks */
    if (l->tx_voice && l->tx_n >= 2 * MIC_LEVEL_WINDOW && l->tx_max_alc < MIC_LEVEL_ALC_PEAK)
        return MIC_LEVEL_UP;
    return 0;
}

#ifndef MIC_LEVEL_DECISIONS_ONLY   /* tests/test_mic_level.cpp: the arithmetic alone */
#include "cfg/cfg_api.h"
#include "radio.h"
#include "scheduler.h"
#include "util.h"

/* The gain of the microphone in use (both when Auto: the radio picks the hand mic when plugged,
 * the other one just follows). ponytail: the radio does not say which mic is plugged */
static void gain_step(void *arg) {
    int d   = *(int *)arg;
    int mic = radio_get_mic();      /* the walkie screen may use another than cfg_mic */

    if (mic != 0) param_i_set(cfg_hmic, LV_CLAMP(MIC_LEVEL_MIN_GAIN, param_i_get(cfg_hmic) + d, 50));
    if (mic != 1) param_i_set(cfg_imic, LV_CLAMP(MIC_LEVEL_MIN_GAIN, param_i_get(cfg_imic) + d, 50));
}

static mic_level_t level;
static uint64_t    last_ms;

void mic_level_update(float alc, float pwr_w) {
    uint64_t now = get_time();
    int      d   = 0;

    if (radio_get_modem()) return;   /* a recording or an app playing, not the mic */
    if (now - last_ms > 1500) {     /* a new transmission: settle what the last one said */
        d = level.target_w > 0.0f ? mic_level_end(&level) : 0;
        mic_level_start(&level, param_f_get(cfg_pwr));
    }
    last_ms = now;
    if (!d) d = mic_level_reading(&level, alc, pwr_w);
    if (d) scheduler_put(gain_step, &d, sizeof(d));
}
#endif
