/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  Xiegu X6100 LVGL GUI - JTTY window
 */

#pragma once

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_jtty;

void dialog_jtty_remote(const char *cmd, int32_t val, const char *text);
int  dialog_jtty_remote_state(char *buf, size_t size);

#ifdef __cplusplus
}
#endif
