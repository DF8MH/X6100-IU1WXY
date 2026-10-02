/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "params/params.h"

#include <unistd.h>
#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// COLORS

#define COLOR_LIGHT_GREEN (0x80FF80)
#define COLOR_LIGHT_YELLOW (0xFFFF30)
#define COLOR_LIGHT_RED (0xFF8080)

// Symbols

#define SYMBOL_PLUG "\xEF\x87\xA6"  // F1E6
#define SYMBOL_PLUG_CHARGE "\xEE\x95\x9B"  // E55B
#define SYMBOL_NORTH_WEST_ARROW "\xE2\x86\x96"
#define SYMBOL_SOUTH_WEST_ARROW "\xE2\x86\x99"

// Top bar: info, S-meter and clock panels

#define TOP_BAR_HEIGHT  76
#define TOP_BAR_MARGIN  2
#define CLOCK_W         168
#define TOP_BAR_BOX_H   (TOP_BAR_HEIGHT - TOP_BAR_MARGIN * 2)
#define TOP_BAR_METER_X (TOP_BAR_MARGIN + 360 + 3)
#define TOP_BAR_METER_W (800 - TOP_BAR_METER_X - 3 - CLOCK_W - TOP_BAR_MARGIN)

/* Frequency strip under the top bar, always shown */
#define FREQ_STRIP_Y    TOP_BAR_HEIGHT
#define FREQ_STRIP_H    50

/* Bottom buttons (buttons.h BTN_HEIGHT) */
/* Set at boot by fonts_apply(): taller with DejaVu (two-line labels) */
#define BUTTONS_H_MIN   54
#define BUTTONS_H_MAX   62
extern int buttons_h;
#define BUTTONS_H       buttons_h

/* App dialogs: from under the frequency strip down to the buttons */
#define DIALOG_Y        (FREQ_STRIP_Y + FREQ_STRIP_H + TOP_BAR_MARGIN)
#define DIALOG_H        (480 - BUTTONS_H - TOP_BAR_MARGIN - DIALOG_Y)
#define DIALOG_H_MAX    (480 - BUTTONS_H_MIN - TOP_BAR_MARGIN - DIALOG_Y)   /* for static buffers */

// Pallete

extern const uint32_t *wf_palette;

extern lv_color_t   bg_color;

extern lv_style_t   background_style;
extern lv_style_t   spectrum_style;
extern lv_style_t   freq_style;
extern lv_style_t   waterfall_style;

extern lv_style_t   btn_style;
extern lv_style_t   btn_active_style;
extern lv_style_t   btn_disabled_style;
extern lv_style_t   btn_mark_style;
extern lv_style_t   btn_mark_assigned_style;
extern lv_style_t   btn_mark_active_style;     /* the control the encoder is on now */

extern lv_style_t   msg_style;
extern lv_style_t   msg_tiny_style;
extern lv_style_t   clock_style;
extern lv_style_t   knobs_style;
extern lv_style_t   info_style;
extern lv_style_t   info_row_style;
extern lv_style_t   info_item_style;
extern lv_style_t   meter_style;
extern lv_style_t   tx_info_style;

extern lv_style_t   panel_top_style;
extern lv_style_t   panel_mid_style;
extern lv_style_t   panel_bottom_style;
extern lv_style_t   panel_style;
extern lv_style_t   panel_info_style;

extern lv_style_t   dialog_style;
extern lv_style_t   freq_strip_style;
extern lv_style_t   dialog_item_style;
extern lv_style_t   dialog_item_focus_style;
extern lv_style_t   dialog_item_edited_style;
extern lv_style_t   dialog_dropdown_list_style;

extern lv_style_t   cw_tune_style;
extern lv_style_t   card_style;     /* Settings cards */

extern lv_font_t    sony_14;
extern lv_font_t    sony_16;
extern lv_font_t    sony_18;
extern lv_font_t    sony_20;
extern lv_font_t    sony_22;
extern lv_font_t    sony_24;
extern lv_font_t    sony_26;
extern lv_font_t    sony_28;
extern lv_font_t    sony_30;
extern lv_font_t    sony_32;
extern lv_font_t    sony_34;
extern lv_font_t    sony_36;
extern lv_font_t    sony_38;
extern lv_font_t    sony_40;
extern lv_font_t    sony_42;
extern lv_font_t    sony_44;
extern lv_font_t    sony_60;

/* The shared styles from the theme loaded with theme_init() */
void styles_init(void);

/* Again after another theme_init() (the UI harness switches themes) */
void styles_apply(void);

#ifdef __cplusplus
}
#endif
