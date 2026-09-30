/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Web remote: local control socket for the webserver (/remote page)
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void remote_server_init();

/* Waterfall PSD (dB) from the DSP, sent to the subscribed web clients (~10 fps) */
void remote_scope_data(const float *psd_db, size_t n, uint32_t center_hz, uint32_t width_hz, bool tx);

#ifdef __cplusplus
}
#endif
