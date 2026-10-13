/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 */

#pragma once

#include "lvgl/lvgl.h"

/* The portrait Walkie Talkie screen (480x800); keys it does not use itself go to the main screen */
lv_obj_t * walkie_screen(lv_obj_t *main_screen_obj);

/* The walkie screen is up: the VOL knob is the volume only, the buttons' knob is the MFK */
bool walkie_on(void);
