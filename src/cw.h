/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "helpers.h"
#include "dsp.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "audio.h"

#include <stdint.h>
#include <stdbool.h>
#include <liquid/liquid.h>

void cw_init();

void cw_put_audio_samples(unsigned int n, float *samples);

float cw_get_tone_freq(void);

/* Decoded speed (WPM), the last one measured */
int cw_get_wpm(void);

/* CW trainer: decode always (the decoder listens to the sidetone) */
void cw_trainer_set(bool on);

#ifdef __cplusplus
}
#endif
