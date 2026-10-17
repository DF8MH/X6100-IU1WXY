/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - WeFax app
 */

#pragma once

#include "app_window.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_wefax;

/* The app is open (the waterfall shows its tuning marks) */
bool dialog_wefax_is_active(void);

/* Web remote (X6100 Link): an app command (LVGL thread) and the "ax" state object (any thread) */
void dialog_wefax_remote(const char *cmd, int32_t val, const char *text);   // wefax_mode, wefax_save, wefax_clear, wefax_tx, wefax_stop, app_base
int  dialog_wefax_remote_state(char *buf, size_t size);   // writes the ax JSON object incl "base"

#ifdef __cplusplus
}
#endif
