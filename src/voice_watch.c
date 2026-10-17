/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - voice from the phone: the keying rules (voice_watch.h)
 */

#include "voice_watch.h"

vw_action_t voice_watch_tick(voice_watch_t *w, uint64_t now_ms) {
    if (!w->keyed)
        return VW_NONE;
    if (now_ms - w->last_ms > VOICE_WATCH_SILENCE_MS) {
        voice_watch_stop(w);
        return VW_UNKEY_SILENCE;
    }
    if (now_ms - w->start_ms >= VOICE_WATCH_LIMIT_MS) {
        voice_watch_stop(w);
        return VW_UNKEY_LIMIT;
    }
    return VW_NONE;
}

vw_action_t voice_watch_packet(voice_watch_t *w, bool ptt, uint64_t now_ms) {
    vw_action_t a = voice_watch_tick(w, now_ms);

    w->last_ms = now_ms;
    if (a != VW_NONE)
        return a;
    if (!ptt) {
        w->blocked = false;
        if (!w->keyed)
            return VW_NONE;
        w->keyed = false;
        return VW_UNKEY_RELEASE;
    }
    if (w->keyed || w->blocked)
        return VW_NONE;
    w->keyed    = true;
    w->start_ms = now_ms;
    return VW_KEY;
}

void voice_watch_stop(voice_watch_t *w) {
    w->keyed   = false;
    w->blocked = true;
}

size_t voice_resample(const int16_t *in, size_t n, int16_t *out, double step, double *pos, int16_t *prev) {
    size_t k = 0;

    if (!n)
        return 0;
    /* pos -1 is prev, the last sample of the block before */
    double p = *pos - 1.0;
    while (p < (double)n - 1.0) {
        long   i = p < 0.0 ? -1 : (long)p;
        double a = i < 0 ? *prev : in[i];
        out[k++] = (int16_t)(a + (in[i + 1] - a) * (p - i));
        p += step;
    }
    *pos  = p - ((double)n - 1.0);
    *prev = in[n - 1];
    return k;
}
