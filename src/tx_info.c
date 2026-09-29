/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "tx_info.h"

#include <stdio.h>

#include "dialog.h"
#include "events.h"
#include "msg_tiny.h"
#include "params/params.h"
#include "cfg/cfg_api.h"
#include "scheduler.h"
#include "styles.h"
#include "util.h"
#include "info.h"

#define UPDATE_UI_MS 40

static const float min_pwr = 0.0f;
static const float max_pwr = 10.0f;

static const float min_swr = 1.0f;
static const float max_swr = 5.0f;

static float pwr  = 0.0f;
static float vswr = 0.0f;
static float alc;

static uint8_t msg_id;

static uint64_t prev_ui_update = 0;

static x6100_mode_t cur_mode;

static lv_obj_t     *obj;        /* "TX" in the S-meter box */
static lv_obj_t     *panel;      /* big TX panel under the top bars */
static lv_obj_t     *head_label;
static lv_obj_t     *time_label;
static lv_obj_t     *pwr_label;
static lv_obj_t     *vswr_label;
static lv_obj_t     *alc_label;
static uint32_t      tx_start;

#define PANEL_W   500
#define PANEL_H   176
#define ROW_Y     52
#define ROW_STEP  42
#define BARS_X    64
#define BARS_W    290
#define SLICES    25

static void on_cur_mode_change(Subject *subj, void *user_data);

/* One bar row: SLICES slices, lit up to val; SWR turns yellow then red. */
static void draw_bar(lv_draw_ctx_t *draw_ctx, lv_coord_t x1, lv_coord_t y1, float min, float max, float val,
                     bool swr, uint32_t on_color) {
    lv_draw_rect_dsc_t rect_dsc;
    lv_area_t          area;
    lv_coord_t         slice_w = BARS_W / SLICES;
    int                lit = (LV_MAX(val, min) - min) / (max - min) * SLICES + 0.5f;

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_COVER;
    area.y1 = y1;
    area.y2 = y1 + 15;

    for (int i = 0; i < SLICES; i++) {
        float v = min + (max - min) * i / SLICES;

        if (i >= lit) {
            rect_dsc.bg_color = lv_color_hex(0x2a2f36);
        } else if (swr && v >= 3.0f) {
            rect_dsc.bg_color = lv_color_hex(0xcc3333);
        } else if (swr && v >= 2.0f) {
            rect_dsc.bg_color = lv_color_hex(0xcccc33);
        } else {
            rect_dsc.bg_color = lv_color_hex(on_color);
        }
        area.x1 = x1 + i * slice_w;
        area.x2 = area.x1 + slice_w - 3;
        lv_draw_rect(draw_ctx, &rect_dsc, &area);
    }
}

static void panel_draw_cb(lv_event_t *e) {
    lv_obj_t      *o        = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_coord_t     x1       = o->coords.x1 + BARS_X;
    lv_coord_t     y1       = o->coords.y1 + ROW_Y + 4;

    draw_bar(draw_ctx, x1, y1, min_pwr, max_pwr, pwr, false, 0xaaaaaa);
    draw_bar(draw_ctx, x1, y1 + ROW_STEP, min_swr, max_swr, vswr, true, 0xaaaaaa);
    draw_bar(draw_ctx, x1, y1 + 2 * ROW_STEP, 0.0f, 10.0f, alc, false, 0x66aabb);

    /* PWR scale under its bar */
    lv_draw_label_dsc_t label_dsc;
    lv_area_t           area;
    char                text[4];

    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_color_hex(0x88aa99);
    label_dsc.font  = &sony_14;
    for (int w = 0; w <= 10; w += 2) {
        snprintf(text, sizeof(text), "%i", w);
        area.x1 = x1 + BARS_W * w / 10 - 6;
        area.x2 = area.x1 + 20;
        area.y1 = y1 + 17;
        area.y2 = area.y1 + 16;
        lv_draw_label(draw_ctx, &label_dsc, &area, text, NULL);
    }
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, "");
    return l;
}

static void update_head(void) {
    uint32_t  s    = lv_tick_elaps(tx_start) / 1000;
    int32_t   f    = cparam_i_get(cfg_fg_freq);
    uint16_t  mhz, khz, hz;

    split_freq(f, &mhz, &khz, &hz);
    lv_label_set_text_fmt(head_label, "%s - %i.%03i.%03i", info_params_mode_label_get(), mhz, khz, hz);
    lv_label_set_text_fmt(time_label, "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

static void tx_cb(lv_event_t *e) {
    pwr  = 0.0f;
    vswr = 0.0f;
    alc  = 0.0f;
    tx_start = lv_tick_get();

    /* Under the top bar, and under the frequency strip when an app is open */
    lv_obj_set_y(panel, (dialog_is_run() ? DIALOG_Y : TOP_BAR_HEIGHT) + 8);
    update_head();

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(obj);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel);
}

static void rx_cb(lv_event_t *e) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

static void update_tx_info(void *arg) {
    if (lv_obj_has_flag(panel, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    lv_label_set_text_fmt(pwr_label, "%.1f W", pwr);
    lv_label_set_text_fmt(vswr_label, "%.1f", vswr);
    lv_label_set_text_fmt(alc_label, "%.1f", alc);
    update_head();
    lv_obj_invalidate(panel);
}

lv_obj_t *tx_info_init(lv_obj_t *parent) {
    /* "TX" in place of the S-meter */
    obj = lv_obj_create(parent);
    lv_obj_add_style(obj, &tx_info_style, 0);
    /* Solid: LVGL caches gradients by size, the S-meter's would be reused */
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x7a1818), 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xff6666), 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *tx = make_label(obj, &sony_24, 0xffffff);
    lv_label_set_text(tx, "TX");
    lv_obj_center(tx);

    /* The big panel */
    panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, PANEL_W, PANEL_H);
    lv_obj_set_x(panel, (800 - PANEL_W) / 2);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0b1622), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(panel, panel_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);

    lv_obj_t *badge = make_label(panel, &sony_18, 0xffffff);
    lv_label_set_text(badge, "TX");
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xb01e1e), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 5, 0);
    lv_obj_set_style_pad_hor(badge, 10, 0);
    lv_obj_set_style_pad_ver(badge, 2, 0);
    lv_obj_set_pos(badge, 16, 10);

    head_label = make_label(panel, &sony_18, 0x90f0d0);
    lv_obj_align(head_label, LV_ALIGN_TOP_MID, 0, 12);
    time_label = make_label(panel, &sony_18, 0xffffff);
    lv_obj_align(time_label, LV_ALIGN_TOP_RIGHT, -16, 12);

    lv_obj_t *line = lv_obj_create(panel);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, PANEL_W - 36, 1);
    lv_obj_set_pos(line, 16, 42);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x334455), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);

    static const char *names[] = {"PWR", "SWR", "ALC"};
    lv_obj_t          *values[3];
    for (int i = 0; i < 3; i++) {
        lv_obj_t *n = make_label(panel, &sony_20, 0xccddee);
        lv_label_set_text(n, names[i]);
        lv_obj_set_pos(n, 16, ROW_Y + i * ROW_STEP);
        values[i] = make_label(panel, &sony_26, 0xffffff);
        lv_obj_set_width(values[i], 110);
        lv_obj_set_style_text_align(values[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(values[i], PANEL_W - 16 - 4 - 110, ROW_Y - 3 + i * ROW_STEP);
    }
    pwr_label  = values[0];
    vswr_label = values[1];
    alc_label  = values[2];

    lv_obj_add_event_cb(obj, tx_cb, EVENT_RADIO_TX, NULL);
    lv_obj_add_event_cb(obj, rx_cb, EVENT_RADIO_RX, NULL);

    subject_subscribe((Subject*)cfg_cur_mode, on_cur_mode_change, NULL);

    return obj;
}

void tx_info_update(float p, float s, float a) {
    // Use EMA for smoothing values
    const float beta = 0.9f;

    a = 10.f - a;
    s = LV_MIN(max_swr, s);

    switch (cur_mode) {
        case x6100_mode_lsb_dig:
        case x6100_mode_usb_dig:
            pwr  = p;
            alc  = a;
            vswr = s;
            break;
        default:
            lpf(&pwr, p, beta, 0.0f);
            lpf(&alc, a, beta, 0.0f);
            lpf(&vswr, s, beta, 0.0f);
    }
    msg_id++;
    scheduler_put_noargs(update_tx_info);
}

bool tx_info_refresh(uint8_t *prev_msg_id, float *alc_p, float *pwr_p, float *vswr_p) {
    if (*prev_msg_id == msg_id) {
        return false;
    }
    if (alc_p)
        *alc_p = alc;
    if (pwr_p)
        *pwr_p = pwr;
    if (vswr_p)
        *vswr_p = vswr;
    *prev_msg_id = msg_id;
    return true;
}


static void on_cur_mode_change(Subject *subj, void *user_data) {
    cur_mode = cparam_i_get(cfg_cur_mode);
}
