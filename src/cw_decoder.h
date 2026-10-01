/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <stdbool.h>

typedef struct {
    char    *morse;
    char    *character;
} cw_characters_t;

extern cw_characters_t cw_characters[];

/* Redirects the decoded text and the WPM estimate (called from the audio
 * thread) instead of the panel; NULL, NULL restores the panel. */
void cw_decoder_set_sink(void (*text)(const char *), void (*wpm)(int));

/* Decoder output (audio thread): to the sink if set, else to the panel */
void cw_decoder_text(const char *text);
void cw_decoder_wpm(int wpm);
