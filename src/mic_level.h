/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - the microphone gain from the ALC in voice
 *
 *  As tx_level.c does for the digital modes: on baseband r13 the ALC holds the power at the
 *  setting and reads 8-10 when the drive is far too hot. While talking, over two seconds at a
 *  time, an ALC that stays high (not just on the peaks) takes the microphone gain down 2 steps;
 *  a whole transmission with the ALC at zero and the power well short of the setting takes it
 *  up 1. A steady carrier (no voice: the power and ALC do not move) is left alone.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float   target_w;   /* the power setting */
    int     n;          /* readings in the window */
    float   sum_alc, min_alc, max_alc, min_w, max_w;
    int     down;       /* steps taken down this transmission */
    int     tx_n;       /* readings this transmission */
    float   tx_max_alc, tx_max_w;
    bool    tx_voice;   /* the power moved at some point: someone talked */
} mic_level_t;

#define MIC_LEVEL_WINDOW    5       /* readings per decision, about 2 s */
#define MIC_LEVEL_ALC_HIGH  3.0f    /* mean ALC over the window above this: too hot */
#define MIC_LEVEL_DOWN      2       /* gain steps down per decision */
#define MIC_LEVEL_DOWN_MAX  6       /* at most this many down in one transmission */
#define MIC_LEVEL_MIN_GAIN  4

void mic_level_start(mic_level_t *l, float target_w);

/* One reading (ALC 0-10, power in W): the gain change to make now (0, -2) */
int mic_level_reading(mic_level_t *l, float alc, float pwr_w);

/* The transmission is over: the gain change to make (0, +1) */
int mic_level_end(mic_level_t *l);

/* Called by tx_info.c with every TX reading in a voice mode */
void mic_level_update(float alc, float pwr_w);

#ifdef __cplusplus
}
#endif
