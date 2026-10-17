/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - voice from the phone (voice_tx.c): when the radio may stay keyed.
 *  Keyed only while packets with the PTT flag keep coming: a released PTT, VOICE_WATCH_SILENCE_MS
 *  without packets or VOICE_WATCH_LIMIT_MS on air unkeys. After silence, the limit or a stop from
 *  outside (SWR, a refusal) it keys again only after the phone has released its PTT.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VOICE_WATCH_SILENCE_MS 500
#define VOICE_WATCH_LIMIT_MS   (180 * 1000)

typedef enum {
    VW_NONE,
    VW_KEY,
    VW_UNKEY_RELEASE,   /* the phone let go */
    VW_UNKEY_SILENCE,   /* no packets: connection lost, app in the background */
    VW_UNKEY_LIMIT,     /* too long on air */
} vw_action_t;

typedef struct {
    bool     keyed;
    bool     blocked;   /* until a packet without PTT */
    uint64_t last_ms, start_ms;
} voice_watch_t;

/* A packet from the owner of the session */
vw_action_t voice_watch_packet(voice_watch_t *w, bool ptt, uint64_t now_ms);

/* Often (every 20-50 ms) */
vw_action_t voice_watch_tick(voice_watch_t *w, uint64_t now_ms);

/* Unkeyed from outside, or keying refused: wait for a release */
void voice_watch_stop(voice_watch_t *w);

/* Linear interpolation from in_rate to out_rate, carried over between blocks (pos and prev
 * start at 0). Returns the samples written, at most n * out_rate / in_rate + 1. */
size_t voice_resample(const int16_t *in, size_t n, int16_t *out, double step, double *pos, int16_t *prev);

#ifdef __cplusplus
}
#endif
