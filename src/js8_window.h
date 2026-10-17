/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 app
 */

#pragma once

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_js8; /* the app, opened from APP */

/* Web remote (X6100 Link): an app command (LVGL thread) and the "ax" state object (any thread) */
void dialog_js8_remote(const char *cmd, int32_t val, const char *text);
int  dialog_js8_remote_state(char *buf, size_t size);

/* The call of the station the list selection is on, false if none (the UI
 * test harness reads it). */
bool dialog_js8_selected_call(char *call, unsigned len); /* len: size of call */

#ifdef __cplusplus
}
#endif
