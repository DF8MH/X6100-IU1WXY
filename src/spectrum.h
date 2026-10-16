/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <unistd.h>
#include <stdint.h>

#include "lvgl/lvgl.h"

lv_obj_t * spectrum_init(lv_obj_t * parent);
void spectrum_data(float *data_buf, uint16_t size, bool tx, uint32_t base_freq, uint8_t fft_dec);
void spectrum_min_max_reset();

void spectrum_update_max(float db);
void spectrum_update_min(float db);
void spectrum_clear();

/* The walkie screen's bandscope: the same trace and markers on its own object (LV_EVENT_DRAW_MAIN_END) */
void spectrum_draw_cb(lv_event_t *e);
/* The span shown, Hz; *vfo_at: where the VFO line is, 0..1 from the left */
int32_t spectrum_span(float *vfo_at);
// void spectrum_update_filters();
// void spectrum_update_factor();
