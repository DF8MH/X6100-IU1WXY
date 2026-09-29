/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Frequency strip: band on the left, VFO frequency in the middle, mode on
 *  the right. The app dialogs start below it.
 */

#include "freq_strip.h"

#include "cfg/cfg_api.h"
#include "cluster/spots.h"
#include "info.h"
#include "styles.h"
#include "util.h"

#include <stdio.h>

static lv_obj_t *strip, *band_label, *freq_label, *mode_label;
static char      note[64];
static int32_t   note_freq;

static void update_cb(Subject *subj, void *user_data) {
    (void)subj;
    (void)user_data;
    uint16_t mhz, khz, hz;
    int32_t  f = cparam_i_get(cfg_fg_freq);

    split_freq(f, &mhz, &khz, &hz);
    if (param_i_get(cfg_band_split)) {
        uint16_t mhz2, khz2, hz2;
        split_freq(cparam_i_get(cfg_bg_freq), &mhz2, &khz2, &hz2);
        lv_label_set_text_fmt(freq_label, "%i.%03i.%03i / %i.%03i.%03i", mhz, khz, hz, mhz2, khz2, hz2);
    } else {
        lv_label_set_text_fmt(freq_label, "%i.%03i.%03i", mhz, khz, hz);
    }
    lv_label_set_text(band_label, note[0] && f == note_freq ? note : cluster_band_name(cluster_band_of(f)));
    lv_label_set_text(mode_label, info_params_mode_label_get());
}

static lv_obj_t *make_label(lv_align_t align, lv_coord_t x, const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(strip);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_align(l, align, x, 0);
    return l;
}

lv_obj_t *freq_strip_init(lv_obj_t *parent) {
    strip = lv_obj_create(parent);
    lv_obj_remove_style_all(strip);
    lv_obj_add_style(strip, &freq_strip_style, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(strip, LV_OBJ_FLAG_HIDDEN);

    band_label = make_label(LV_ALIGN_LEFT_MID, 14, &sony_20, 0x90f0d0);
    freq_label = make_label(LV_ALIGN_CENTER, 0, &sony_26, 0xffffff);
    mode_label = make_label(LV_ALIGN_RIGHT_MID, -14, &sony_20, 0xff6666);

    subject_subscribe_delayed((Subject *)cfg_fg_freq, update_cb, NULL);
    subject_subscribe_delayed((Subject *)cfg_bg_freq, update_cb, NULL);
    subject_subscribe_delayed((Subject *)cfg_band_split, update_cb, NULL);
    subject_subscribe_delayed((Subject *)cfg_cur_mode, update_cb, NULL);
    update_cb(NULL, NULL);
    return strip;
}

void freq_strip_set_label(const char *text, int32_t freq_hz) {
    snprintf(note, sizeof(note), "%s", text);
    note_freq = freq_hz;
    if (strip) update_cb(NULL, NULL);
}

void freq_strip_show(bool show) {
    if (!strip) return;
    if (show) {
        update_cb(NULL, NULL);
        lv_obj_clear_flag(strip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(strip);
    } else {
        lv_obj_add_flag(strip, LV_OBJ_FLAG_HIDDEN);
    }
}
