/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Frequency strip: band on the left, VFO frequency in the middle, then the
 *  locator and the mode on the right. Always shown; the app dialogs start
 *  below it.
 */

#include "freq_strip.h"

#include "cfg/cfg_api.h"
#include "cluster/spots.h"
#include "info.h"
#include "params/params.h"
#include "station.h"
#include "styles.h"
#include "util.h"

#include <stdio.h>

#define QTH_POLL_MS 3000

static lv_obj_t *strip, *band_label, *freq_label, *mode_label, *qth_dot, *qth_label;
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

/* The GPS locator (green dot) while there is a fix, else the saved QTH */
static void qth_update_cb(lv_timer_t *t) {
    (void)t;
    const char *qth = station_qth();

    lv_label_set_text(qth_label, qth);
    if (qth != params.qth.x) {
        lv_obj_clear_flag(qth_dot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(qth_dot, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *make_label(lv_obj_t *parent, lv_align_t align, lv_coord_t x, const lv_font_t *font,
                            uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
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

    band_label = make_label(strip, LV_ALIGN_LEFT_MID, 14, &sony_24, 0x90f0d0);
    freq_label = make_label(strip, LV_ALIGN_CENTER, 0, &sony_36, 0xffffff);

    /* Right: [dot] locator, mode */
    lv_obj_t *right = lv_obj_create(strip);
    lv_obj_remove_style_all(right);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 6, 0);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, -14, 0);

    qth_dot = lv_obj_create(right);
    lv_obj_remove_style_all(qth_dot);
    lv_obj_set_size(qth_dot, 10, 10);
    lv_obj_set_style_radius(qth_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(qth_dot, lv_color_hex(COLOR_LIGHT_GREEN), 0);
    lv_obj_set_style_bg_opa(qth_dot, LV_OPA_COVER, 0);
    qth_label = make_label(right, LV_ALIGN_DEFAULT, 0, &sony_24, 0xffffff);
    mode_label = make_label(right, LV_ALIGN_DEFAULT, 0, &sony_24, 0xff6666);
    lv_obj_set_style_pad_left(mode_label, 16, 0);

    subject_subscribe_delayed((Subject *)cfg_fg_freq, update_cb, NULL);
    subject_subscribe_delayed((Subject *)cfg_bg_freq, update_cb, NULL);
    subject_subscribe_delayed((Subject *)cfg_band_split, update_cb, NULL);
    subject_subscribe_delayed((Subject *)cfg_cur_mode, update_cb, NULL);
    update_cb(NULL, NULL);
    qth_update_cb(NULL);
    lv_timer_create(qth_update_cb, QTH_POLL_MS, NULL);
    return strip;
}

void freq_strip_set_label(const char *text, int32_t freq_hz) {
    snprintf(note, sizeof(note), "%s", text);
    note_freq = freq_hz;
    if (strip) update_cb(NULL, NULL);
}
