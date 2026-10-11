/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 */

#pragma once

#include "dialog.h"

extern dialog_t *dialog_snake;

/* The other way in: 3 s on 14.141.414 or 7.070.700 with no app open starts the game */
void dialog_snake_watch(void);
