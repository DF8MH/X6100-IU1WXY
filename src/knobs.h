/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2025 Adrian Grzeca SQ5FOX
 *  Copyright (c) 2025 Georgy Dyuldin R2RFE
 */

#pragma once

#include "cfg/encoder_bind_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

#include <unistd.h>


void knobs_init(lv_obj_t * parent);
void knobs_set_vol_state(bool edit);
void knobs_set_mfk_state(bool edit);
void knobs_display(bool on);
bool knobs_visible();
/* For an app that draws the knobs itself (Field Ops): what VOL (vol true) or MFK controls, its value,
 * its level 0..1 (-1: no scale) and whether turning changes the value (else it chooses the control) */
bool knobs_info(bool vol, const char **name, char *val, size_t size, float *level, bool *edit);

#ifdef __cplusplus
}

/* C++ only part */
void knobs_set_vol_param(cfg_ctrl_t control);

void knobs_set_mfk_param(cfg_ctrl_t control);
#endif
