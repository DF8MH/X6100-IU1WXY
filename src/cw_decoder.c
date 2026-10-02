/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include <stdio.h>
#include "cw_decoder.h"
#include "panel.h"
#include "remote_server.h"
#include <string.h>

cw_characters_t cw_characters[] = {
    { .morse = ".-",        .character = "A" },
    { .morse = "-...",      .character = "B" },
    { .morse = "-.-.",      .character = "C" },
    { .morse = "-..",       .character = "D" },
    { .morse = ".",         .character = "E" },
    { .morse = "..-.",      .character = "F" },
    { .morse = "--.",       .character = "G" },
    { .morse = "....",      .character = "H" },
    { .morse = "..",        .character = "I" },
    { .morse = ".---",      .character = "J" },
    { .morse = "-.-",       .character = "K" },
    { .morse = ".-..",      .character = "L" },
    { .morse = "--",        .character = "M" },
    { .morse = "-.",        .character = "N" },
    { .morse = "---",       .character = "O" },
    { .morse = ".--.",      .character = "P" },
    { .morse = "--.-",      .character = "Q" },
    { .morse = ".-.",       .character = "R" },
    { .morse = "...",       .character = "S" },
    { .morse = "-",         .character = "T" },
    { .morse = "..-",       .character = "U" },
    { .morse = "...-",      .character = "V" },
    { .morse = ".--",       .character = "W" },
    { .morse = "-..-",      .character = "X" },
    { .morse = "-.--",      .character = "Y" },
    { .morse = "--..",      .character = "Z" },

    { .morse = ".----",     .character = "1" },
    { .morse = "..---",     .character = "2" },
    { .morse = "...--",     .character = "3" },
    { .morse = "....-",     .character = "4" },
    { .morse = ".....",     .character = "5" },
    { .morse = "-....",     .character = "6" },
    { .morse = "--...",     .character = "7" },
    { .morse = "---..",     .character = "8" },
    { .morse = "----.",     .character = "9" },
    { .morse = "-----",     .character = "0" },

    { .morse = "-.-.--",    .character = "!" },
    { .morse = "..--.",     .character = "!" },
    { .morse = ".-..-.",    .character = "\"" },
    { .morse = "...-..-",   .character = "$" },
    { .morse = ".----.",    .character = "'" },
    { .morse = "--..--",    .character = "," },
    { .morse = "-....-",    .character = "-" },
    { .morse = ".-.-.-",    .character = "." },
    { .morse = "-..-.",     .character = "/" },
    { .morse = "---...",    .character = ":" },
    { .morse = "-.-.-.",    .character = ";" },
    { .morse = "-...-",     .character = "=" },
    { .morse = "..--..",    .character = "?" },
    { .morse = ".--.-.",    .character = "@" },
    { .morse = "..--.-",    .character = "_" },

    { .morse = ".-.-.",     .character = "<AR>" },
    { .morse = ".-...",     .character = "<AS>" },
    { .morse = "-...-.-",   .character = "<BK>" },
    { .morse = "-.-..-..",  .character = "<CL>" },
    { .morse = "-.-.-",     .character = "<CT>" },
    { .morse = "-.--.",     .character = "<KN>" },
    { .morse = "...-.-",    .character = "<SK>" },
    { .morse = "...-.",     .character = "<SN>" },
    { .morse = "...---...", .character = "<SOS>" },
    { .morse = "-.-.--.-",  .character = "<CQ>" },

    { .morse = "......",    .character = "<ERR>" },
    { .morse = ".......",   .character = "<ERR>" },
    { .morse = "........",  .character = "<ERR>" },
    { .morse = NULL }
};

static void (*sink_text)(const char *);
static void (*sink_wpm)(int);

void cw_decoder_set_sink(void (*text)(const char *), void (*wpm)(int)) {
    sink_text = text;
    sink_wpm  = wpm;
}

void cw_decoder_text(const char *text) {
    if (sink_text) {
        sink_text(text);
    } else {
        panel_add_text(text);
        remote_text("cw_text", text, strlen(text));
    }
}

void cw_decoder_wpm(int wpm) {
    if (sink_wpm) {
        sink_wpm(wpm);
        return;
    }
    char buf[8];
    snprintf(buf, 8, "WPM: %d", wpm);
    panel_set_info(buf);
}
