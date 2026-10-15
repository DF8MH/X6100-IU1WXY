/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - Olivia window
 *
 *  From SP2KMO's OpenX6100 psk31/olivia work (randal007 merge).
 */

#pragma once

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_olivia;

/* Remote commands (remote_server.cpp app_cmds) */
void dialog_olivia_remote(const char *cmd, int32_t val, const char *text);
int  dialog_olivia_remote_state(char *buf, size_t size);

#ifdef __cplusplus
}
#endif
