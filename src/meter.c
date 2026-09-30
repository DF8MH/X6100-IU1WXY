/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "meter.h"
#include "styles.h"
#include "events.h"
#include "params/params.h"
#include "cfg/cfg_api.h"
#include "spectrum.h"
#include "util.h"
#include "scheduler.h"

#define NUM_ITEMS   7
#define METER_PEAK_HOLD 1500
#define METER_PEAK_SPEED 20

static int16_t          min_db = S1;
static int16_t          max_db = S9_40;

static float            meter_db = S1;
static float            meter_db_raw = S1;
static float            noise_level = S_MIN;

static float            meter_peak = S1;
static int64_t          meter_peak_time;
static int64_t          now;

static bool             pre=false;
static bool             att=false;

static lv_obj_t         *obj;
static lv_obj_t         *db_val_label;

typedef struct {
    char    *label;
    int16_t db;
} s_item_t;

static s_item_t s_items[NUM_ITEMS] = {
    { .label = "S1",    .db = S1 },
    { .label = "3",     .db = S3 },
    { .label = "5",     .db = S5 },
    { .label = "7",     .db = S7 },
    { .label = "9",     .db = S9 },
    { .label = "+20",   .db = S9_20 },
    { .label = "+40",   .db = S9_40 }
};

static void on_bool_value_change(Subject *subj, void *user_data) {
    *(bool*)user_data = subject_i_get((SubjectInt*)subj);
}

static void update_db_label() {
    lv_label_set_text_fmt(db_val_label, "%.1f", meter_db_raw);
}

static void meter_draw_cb(lv_event_t * e) {
    lv_obj_t            *obj = lv_event_get_target(e);
    lv_draw_ctx_t       *draw_ctx = lv_event_get_draw_ctx(e);
    lv_draw_rect_dsc_t  rect_dsc;
    lv_draw_label_dsc_t label_dsc;
    lv_area_t           area;

    /* R1CBU layout fitted to the box: bars full width, dB value below */
    lv_coord_t y1 = obj->coords.y1 + 11;

    /* Two thin bars per S unit up to S9 (3 dB), then one per 5 dB */
    static const int16_t bar_db[] = { S1, S1 + 3, S1 + 6, S1 + 9, S1 + 12, S1 + 15, S1 + 18, S1 + 21, S1 + 24,
                                      S1 + 27, S1 + 30, S1 + 33, S1 + 36, S1 + 39, S1 + 42, S1 + 45, S9,
                                      S9 + 5, S9 + 10, S9 + 15, S9_20, S9 + 25, S9 + 30, S9 + 35, S9_40 };
    const uint8_t slices_total = sizeof(bar_db) / sizeof(bar_db[0]);

    /* First bar centred 16 px in, last one 22 px before the edge (room for "+40") */
    uint8_t     slice_w = (lv_obj_get_width(obj) - 16 - 22) / (slices_total - 1);
    uint8_t     slice_spacing = slice_w * 45 / 100;   /* thin bars, wide gaps */
    lv_coord_t  x1 = obj->coords.x1 + 16 - 30;

    /* Rects */

    lv_draw_rect_dsc_init(&rect_dsc);

    rect_dsc.bg_opa = LV_OPA_80;

    area.y1 = y1 - 5;
    area.y2 = y1 + 32;

    uint8_t peak_i = 0;

    for (uint8_t i = 0; i < slices_total; i++) {
        int16_t db = bar_db[i];

        if (meter_peak >= db - 1.5f)
            peak_i = i;
        if (meter_db < db - 1.5f)
            continue;
        if (db <= noise_level) {
            rect_dsc.bg_color = lv_color_hex(0x777777);
        } else if (db <= -73) {
            rect_dsc.bg_color = lv_color_hex(0xAAAAAA);
        } else if (db <= -53) {
            rect_dsc.bg_color = lv_color_hex(0xAAAA00);
        } else {
            rect_dsc.bg_color = lv_color_hex(0xAA0000);
        }
        area.x1 = x1 + 30 + i * slice_w - slice_w / 2 + slice_spacing / 2;
        area.x2 = area.x1 + slice_w - slice_spacing;

        lv_draw_rect(draw_ctx, &rect_dsc, &area);
    }

    /* Peak */
    if (peak_i > 0 && meter_db < bar_db[peak_i] - 1.5f) {
        area.x1 = x1 + 30 - slice_w / 2 + slice_w * peak_i + slice_spacing / 2;
        area.x2 = area.x1 + slice_w - slice_spacing;
        rect_dsc.bg_opa = LV_OPA_50;
        rect_dsc.bg_color = lv_color_hex(0xAAAAAA);
        lv_draw_rect(draw_ctx, &rect_dsc, &area);
    }

    /* Labels */

    lv_draw_label_dsc_init(&label_dsc);

    label_dsc.color = lv_color_white();
    label_dsc.font = &sony_22;

    area.x1 = x1;
    area.x2 = x1 + 20;
    area.y1 = y1 + 5;
    area.y2 = area.y1 + 18;

    lv_point_t label_size;

    for (uint8_t i = 0; i < NUM_ITEMS; i++) {
        char    *label = s_items[i].label;
        int16_t db = s_items[i].db;

        lv_txt_get_size(&label_size, label, label_dsc.font, 0, 0, LV_COORD_MAX, 0);

        uint8_t bar = 0;
        while (bar_db[bar] != db)
            bar++;
        area.x1 = x1 + 30 + slice_w * bar - label_size.x / 2;
        area.x2 = area.x1 + label_size.x;

        lv_draw_label(draw_ctx, &label_dsc, &area, label, NULL);
    }
}

static void tx_cb(lv_event_t * e) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void rx_cb(lv_event_t * e) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}


lv_obj_t * meter_init(lv_obj_t * parent) {
    obj = lv_obj_create(parent);

    lv_obj_add_style(obj, &meter_style, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    lv_obj_add_event_cb(obj, tx_cb, EVENT_RADIO_TX, NULL);
    lv_obj_add_event_cb(obj, rx_cb, EVENT_RADIO_RX, NULL);
    lv_obj_add_event_cb(obj, meter_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);

    subject_subscribe_delayed_and_notify((Subject*)cfg_cur_pre, on_bool_value_change, &pre);
    subject_subscribe_delayed_and_notify((Subject*)cfg_cur_att, on_bool_value_change, &att);

    db_val_label = lv_label_create(obj);
    lv_obj_set_style_text_font(db_val_label, &sony_20, 0);
    lv_obj_align(db_val_label, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_text_color(db_val_label, lv_color_white(), 0);
    lv_label_set_text(db_val_label, "");
    return obj;
}

void meter_set_noise(float val) {
    noise_level = val;
    if (att) {
        noise_level+= 14.0f;
    }
    if (pre){
        noise_level -= 14.0f;
    }
}

void meter_update(float db, float beta) {
    if (att) {
        db += 15.0f;
    }
    if (pre){
        db -= 19.0f;
    }
    if (db < min_db) {
        db = min_db;
    } else if (db > max_db) {
        db = max_db;
    }
    meter_db_raw = db;
    now = get_time();
    if (db > meter_peak) {
        meter_peak = db;
        meter_peak_time = now;
    } else if (now - meter_peak_time > METER_PEAK_HOLD) {
        meter_peak -= (now - meter_peak_time - METER_PEAK_HOLD) * METER_PEAK_SPEED / 1000;
    }
    meter_db = meter_db * beta + db * (1.0f - beta);
    scheduler_put_noargs(update_db_label);
    event_send(obj, LV_EVENT_REFRESH, NULL);
}

int16_t meter_get_raw_db() {
    return meter_db_raw;
}
