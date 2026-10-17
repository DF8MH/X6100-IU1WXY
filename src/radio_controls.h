/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2026 Giovanni Ricciardi IU1WXY
 */

#pragma once

#include "button_bar.h"

#ifdef __cplusplus

/* Turn what a knob controls by diff steps (0: leave it, the message only); msg (made with
 * room, e.g. std::string(64, '\0')) gets "Name: value" */
void controls_encoder_update(cfg_ctrl_t ctrl, int32_t diff, std::string &msg);

extern "C" {
#endif

/* The control after current (dir > 0), before it (dir < 0) or current itself if it is
 * bound (dir 0) among those bound to the encoder (ENCODER_BIND_VOL or _MFK) */
cfg_ctrl_t controls_encoder_get_next(encoder_binds_t encoder, cfg_ctrl_t current, int16_t dir);

/* Button actions */
void controls_toggle_key_train(button_data_t *data);
void controls_toggle_key_iambic_mode(button_data_t *data);
void controls_toggle_cw_decoder(button_data_t *data);
void controls_toggle_cw_tuner(button_data_t *data);
void controls_toggle_cw_peak(button_data_t *data);

void controls_toggle_dnf(button_data_t *data);
void controls_toggle_dnf_auto(button_data_t *data);
void controls_toggle_nb(button_data_t *data);
void controls_toggle_nr(button_data_t *data);

void controls_toggle_vox(button_data_t *data);
void controls_toggle_wf_mode(button_data_t *data);
void controls_span_wider(button_data_t *data);
void controls_span_narrower(button_data_t *data);

void controls_cw_zap(button_data_t *data);

#ifdef __cplusplus
}
#endif
