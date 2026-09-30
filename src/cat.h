/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cat_init();

/* Process one CI-V frame (FE FE .. FD) from a network client, returns response length (0 = invalid) */
size_t cat_process_frame(const uint8_t *req, size_t len, uint8_t *resp, size_t resp_max);

/* Unsolicited CI-V frames (transceive) are also sent to this sink */
void cat_set_net_sink(void (*sink)(const uint8_t *data, size_t len));

/* Waterfall PSD (dB) from the DSP, sent to network clients as CI-V scope data when enabled */
void cat_scope_data(const float *psd_db, size_t n, uint32_t center_hz, uint32_t width_hz, bool tx);

#ifdef __cplusplus
}
#endif
