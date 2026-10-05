/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "params/params.h"

void backlight_init();
void backlight_tick();

void backlight_set_brightness(int16_t value);
void backlight_set_buttons(buttons_light_t value);

void backlight_switch();

/* The level shown now (0-9 as the settings), -1 when the screen is off */
int16_t backlight_level();
