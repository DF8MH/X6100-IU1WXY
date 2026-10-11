/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "meter.h"
#include "styles.h"
#include "theme.h"
#include "events.h"
#include "params/params.h"
#include "cfg/cfg_api.h"
#include "spectrum.h"
#include "util.h"
#include "scheduler.h"

#include <math.h>
#include <stdio.h>

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
    if (!theme_shape.vision) {
        lv_label_set_text_fmt(db_val_label, "%.1f", meter_db_raw);
    } else if (meter_db_raw < S9 + 1) {   /* 6 dB a unit */
        lv_label_set_text_fmt(db_val_label, "S%d", (int)LV_CLAMP(1, 1 + (meter_db_raw - S1 + 3) / 6, 9));
    } else {
        lv_label_set_text_fmt(db_val_label, "S9+%d", (int)(meter_db_raw - S9 + 0.5f));
    }
}

#define VISION_VALUE_W 150   /* VISION: room for "S9+40" right of the bars */

/* VISION keeps the bars: its meter is a wide, low row with the value beside it */
static bool needle_on(void) {
    return param_i_get(cfg_smeter_needle) && !theme_shape.vision;
}

/* The dB value: under the bars, or under the dial in the needle's dim colour */
static void db_label_place(void) {
    if (theme_shape.vision) {
        lv_obj_align(db_val_label, LV_ALIGN_RIGHT_MID, -12, 0);
    } else if (needle_on()) {
        lv_obj_set_style_text_font(db_val_label, &sony_18, 0);
        lv_obj_set_style_text_color(db_val_label, TC(text_dim), 0);
        lv_obj_align(db_val_label, LV_ALIGN_BOTTOM_MID, 0, -1);
    } else {
        lv_obj_set_style_text_font(db_val_label, &sony_20, 0);
        lv_obj_set_style_text_color(db_val_label, TC(text), 0);
        lv_obj_align(db_val_label, LV_ALIGN_BOTTOM_MID, 0, -2);
    }
}

static void on_needle_change(Subject *subj, void *user_data) {
    db_label_place();
    lv_obj_invalidate(obj);
}

/* Bar i of n at db */
static lv_color_t bar_color(int16_t db, uint8_t i, uint8_t n) {
    if (theme_shape.meter_gradient) {
        return lv_color_mix(TC(meter_high), TC(meter_weak), i * 255 / (n - 1));
    }
    if (db >= S9) {
        /* Green at S9 fading to red at +40 */
        uint8_t mix = (db - S9) * 255 / (S9_40 - S9);
        return lv_color_mix(TC(meter_high), TC(meter_low), mix);
    }
    return db <= noise_level ? TC(text_mute) : TC(meter_bar);
}

/* Needle S-meter (Settings, Display): a dial whose pivot is under the box, so only the top of
 * the arc shows. S1..S9 take 70% of the arc in meter_bar, +20 and +40 the rest in meter_high
 * (thicker, the red zone of a real meter), the needle in tx: every colour from the theme. */
#define NEEDLE_A0       (-32)          /* degrees from vertical, left end */
#define NEEDLE_A1       32
#define NEEDLE_S9_U     0.7f

static float needle_u(float db) {
    float u = db <= S9 ? (db - S1) / (float)(S9 - S1) * NEEDLE_S9_U
                       : NEEDLE_S9_U + (db - S9) / (float)(S9_40 - S9) * (1.0f - NEEDLE_S9_U);
    return LV_CLAMP(0.0f, u, 1.0f);
}

static lv_point_t needle_pt(lv_point_t c, float a_deg, lv_coord_t r) {
    float      a = a_deg * (float)M_PI / 180.0f;
    lv_point_t p = {c.x + (lv_coord_t)lroundf(r * sinf(a)), c.y - (lv_coord_t)lroundf(r * cosf(a))};
    return p;
}

static void needle_draw(lv_obj_t *obj, lv_draw_ctx_t *draw_ctx) {
    lv_coord_t          w = lv_obj_get_width(obj);
    lv_coord_t          r = w * 76 / 100;                  /* wide and flat: 198 px on the 260 px box */
    lv_point_t          c = {obj->coords.x1 + w / 2, obj->coords.y1 + 24 + r};   /* the arc top 24 px down */
    lv_draw_arc_dsc_t   arc_dsc;
    lv_draw_line_dsc_t  line_dsc;
    lv_draw_label_dsc_t label_dsc;
    float               span = NEEDLE_A1 - NEEDLE_A0;

    /* LVGL arcs: 0 degrees at 3 o'clock, clockwise; vertical up is 270 */
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = TC(meter_bar);
    arc_dsc.width = 2;
    lv_draw_arc(draw_ctx, &arc_dsc, &c, r, 270 + NEEDLE_A0, 270 + NEEDLE_A0 + (int)(span * NEEDLE_S9_U));
    arc_dsc.color = TC(meter_high);
    arc_dsc.width = 5;
    lv_draw_arc(draw_ctx, &arc_dsc, &c, r + 2, 270 + NEEDLE_A0 + (int)(span * NEEDLE_S9_U), 270 + NEEDLE_A1);

    lv_draw_line_dsc_init(&line_dsc);
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = &sony_20;

    for (int s = 1; s <= 9; s++) {      /* a tick a S unit, long ones and numbers on the odd */
        float      a   = NEEDLE_A0 + span * needle_u(S1 + (s - 1) * 6);
        bool       big = s % 2;
        lv_point_t p1  = needle_pt(c, a, r), p2 = needle_pt(c, a, r - (big ? 9 : 5));

        line_dsc.color = TC(meter_bar);
        line_dsc.width = big ? 2 : 1;
        lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);
        if (big) {
            char       txt[4];
            lv_point_t sz, l = needle_pt(c, a, r + 13);
            lv_area_t  area;

            snprintf(txt, sizeof(txt), s == 1 ? "S1" : "%d", s);
            lv_txt_get_size(&sz, txt, label_dsc.font, 0, 0, LV_COORD_MAX, 0);
            area.x1 = l.x - sz.x / 2;
            area.y1 = l.y - sz.y / 2;
            area.x2 = area.x1 + sz.x;
            area.y2 = area.y1 + sz.y;
            label_dsc.color = TC(text);
            lv_draw_label(draw_ctx, &label_dsc, &area, txt, NULL);
        }
    }
    for (int d = 10; d <= 40; d += 10) {
        float      a   = NEEDLE_A0 + span * needle_u(S9 + d);
        bool       big = d % 20 == 0;
        lv_point_t p1  = needle_pt(c, a, r + 2), p2 = needle_pt(c, a, r - (big ? 10 : 6));

        line_dsc.color = TC(meter_high);
        line_dsc.width = big ? 2 : 1;
        lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);
        if (big) {
            char       txt[4];
            lv_point_t sz, l = needle_pt(c, a, r + 14);
            lv_area_t  area;

            snprintf(txt, sizeof(txt), "+%d", d);
            lv_txt_get_size(&sz, txt, label_dsc.font, 0, 0, LV_COORD_MAX, 0);
            area.x1 = l.x - sz.x / 2;
            area.y1 = l.y - sz.y / 2;
            area.x2 = area.x1 + sz.x;
            area.y2 = area.y1 + sz.y;
            label_dsc.color = TC(meter_over);
            lv_draw_label(draw_ctx, &label_dsc, &area, txt, NULL);
        }
    }

    /* The needle, from the pivot to just past the arc */
    lv_point_t tip = needle_pt(c, NEEDLE_A0 + span * needle_u(meter_db), r + 2);

    line_dsc.color       = TC(tx);
    line_dsc.width       = 2;
    line_dsc.round_end   = 1;
    lv_draw_line(draw_ctx, &line_dsc, &c, &tip);
}

static void meter_draw_cb(lv_event_t * e) {
    lv_obj_t            *obj = lv_event_get_target(e);
    lv_draw_ctx_t       *draw_ctx = lv_event_get_draw_ctx(e);
    lv_draw_rect_dsc_t  rect_dsc;
    lv_draw_label_dsc_t label_dsc;
    lv_area_t           area;

    if (needle_on()) {
        needle_draw(obj, draw_ctx);
        return;
    }

    /* R1CBU layout fitted to the box: bars full width, dB value below */
    lv_coord_t y1 = obj->coords.y1 + 11;

    /* Two thin bars per S unit up to S9 (3 dB), then one per 5 dB */
    static const int16_t bar_db[] = { S1, S1 + 3, S1 + 6, S1 + 9, S1 + 12, S1 + 15, S1 + 18, S1 + 21, S1 + 24,
                                      S1 + 27, S1 + 30, S1 + 33, S1 + 36, S1 + 39, S1 + 42, S1 + 45, S9,
                                      S9 + 5, S9 + 10, S9 + 15, S9_20, S9 + 25, S9 + 30, S9 + 35, S9_40 };
    const uint8_t slices_total = sizeof(bar_db) / sizeof(bar_db[0]);

    /* First bar centred 16 px in, last one 22 px before the edge (room for "+40") */
    lv_coord_t  bars_w = lv_obj_get_width(obj) - (theme_shape.vision ? VISION_VALUE_W : 0);
    uint8_t     slice_w = (bars_w - 16 - 22) / (slices_total - 1);
    uint8_t     slice_spacing = slice_w * 45 / 100;   /* thin bars, wide gaps */
    lv_coord_t  x1 = obj->coords.x1 + 16 - 30;

    /* Rects */

    lv_draw_rect_dsc_init(&rect_dsc);

    rect_dsc.bg_opa = LV_OPA_80;

    /* VISION: the bars on top, the scale under them (white on white could not be read) */
    area.y1 = theme_shape.vision ? obj->coords.y1 + 4 : y1 - 5;
    area.y2 = theme_shape.vision ? obj->coords.y1 + 20 : y1 + 32;

    uint8_t peak_i = 0;

    for (uint8_t i = 0; i < slices_total; i++) {
        int16_t db = bar_db[i];

        if (meter_peak >= db - 1.5f)
            peak_i = i;
        if (meter_db < db - 1.5f)
            continue;
        rect_dsc.bg_color = bar_color(db, i, slices_total);
        area.x1 = x1 + 30 + i * slice_w - slice_w / 2 + slice_spacing / 2;
        area.x2 = area.x1 + slice_w - slice_spacing;

        lv_draw_rect(draw_ctx, &rect_dsc, &area);
    }

    /* Peak */
    if (peak_i > 0 && meter_db < bar_db[peak_i] - 1.5f) {
        area.x1 = x1 + 30 - slice_w / 2 + slice_w * peak_i + slice_spacing / 2;
        area.x2 = area.x1 + slice_w - slice_spacing;
        rect_dsc.bg_opa = LV_OPA_COVER;    /* bright: gradient colour from S9, white below */
        rect_dsc.bg_color = bar_db[peak_i] >= S9 || theme_shape.meter_gradient ? bar_color(bar_db[peak_i], peak_i, slices_total)
                                                                             : TC(text);
        lv_draw_rect(draw_ctx, &rect_dsc, &area);
    }

    /* Labels */

    lv_draw_label_dsc_init(&label_dsc);

    label_dsc.color = TC(text);
    label_dsc.font = &sony_22;

    area.x1 = x1;
    area.x2 = x1 + 20;
    area.y1 = theme_shape.vision ? obj->coords.y1 + 21 : y1 + 5;
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
        label_dsc.color = db > S9 ? TC(meter_over) : TC(text);

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
    lv_obj_set_style_text_font(db_val_label, theme_shape.vision ? &sony_40 : &sony_20, 0);
    lv_obj_set_style_text_color(db_val_label, TC(text), 0);
    db_label_place();
    lv_label_set_text(db_val_label, "");
    subject_subscribe_delayed((Subject*)cfg_smeter_needle, on_needle_change, NULL);
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
    /* A NaN (empty PSD while the span changes) would stick in the smoothing forever */
    if (isnan(db)) {
        return;
    }
    if (!isfinite(meter_db) || !isfinite(meter_peak)) {
        meter_db = meter_peak = min_db;
    }
    if (att) {
        db += 15.0f;
    }
    if (pre){
        db -= 19.0f;
    }
    /* Calibration (Settings, S-meter cal: this radio read ~22 dB above an IC-7300 on the same
     * antenna) and RF gain, which lowers the reading 0.60 dB a step (3 broadcast carriers,
     * 100 to 0): the reading is the same at any RF gain, as PRE and ATT above */
    db += param_i_get(cfg_smeter_cal) + 0.60f * (100 - param_i_get(cfg_rfgain));
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
