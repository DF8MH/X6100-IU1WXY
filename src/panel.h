/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <stdint.h>

#include "lvgl/lvgl.h"

lv_obj_t * panel_init(lv_obj_t *parent);

void panel_hide();
void panel_clear();
void panel_update_visibility(bool clear);
void panel_add_text(const char * text);
void panel_set_info(const char * text);
void panel_keep_below(lv_coord_t y);

/* cw_calls from X6100 Link: "IU1WXY,IK1ABC", calls it found on QRZ. With Parse on they count as
 * confirmed, the panel colours them and tells the remote clients (cw_calls event). LVGL thread */
void panel_remote(const char *cmd, int32_t val, const char *text);

/* The last callsign the decoder wrote (CW / RTTY panel), "" when none */
const char *panel_last_call(void);

#ifdef __cplusplus
}
#endif
