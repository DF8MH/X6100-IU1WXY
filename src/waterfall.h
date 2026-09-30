/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>

#include <unistd.h>
#include <stdint.h>

#include "lvgl/lvgl.h"

lv_obj_t * waterfall_init(lv_obj_t * parent);
void waterfall_data(float *data_buf, uint16_t size, bool tx, uint32_t base_freq, uint32_t width_hz);
void waterfall_set_height(lv_coord_t h);
void waterfall_min_max_reset();

void waterfall_update_max(float db);
void waterfall_update_min(float db);

/* Colour scale limits (dB) used for the waterfall rows */
void waterfall_get_levels(bool tx, float *min, float *max);
/* Fixed mode (cfg_wf_mode): the displayed span stays put and the VFO moves across it.
 * Returns how far (Hz) the centre of the received data is from the display centre;
 * always 0 in Centered mode. Re-centres the view when the VFO nears an edge. */
int32_t waterfall_view_shift(void);

void waterfall_refresh_reset();
void waterfall_refresh_period_set(uint8_t k);
