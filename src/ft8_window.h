// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 *  Xiegu X6100 LVGL GUI - the FT8 / FT4 / FT2 window
 *
 *  Copyright (c) 2026 Giovanni Ricciardi IU1WXY
 */

#pragma once

#include "app_window.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opened from the Modes menu */
extern dialog_t *dialog_ft8;

/* Web remote (X6100 Link): an app command (LVGL thread) and the "ax" state object (any thread) */
void dialog_ft8_remote(const char *cmd, int32_t val, const char *text);
int  dialog_ft8_remote_state(char *buf, size_t size);

#ifdef __cplusplus
}
#endif
