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

#define NUM_PWR_ITEMS 6
#define NUM_VSWR_ITEMS 5
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

static lv_obj_t     *obj;
static lv_obj_t     *pwr_label;
static lv_obj_t     *vswr_label;
static lv_grad_dsc_t grad;


typedef struct {
    char *label;
    float val;
} item_t;

static item_t pwr_items[NUM_PWR_ITEMS] = {
    {.label = "PWR", .val = 0.0f },
    {.label = "2",   .val = 2.0f },
    {.label = "4",   .val = 4.0f },
    {.label = "6",   .val = 6.0f },
    {.label = "8",   .val = 8.0f },
    {.label = "10",  .val = 10.0f}
};

static item_t vswr_items[NUM_VSWR_ITEMS] = {
    {.label = "SWR", .val = 1.0f},
    {.label = "2",   .val = 2.0f},
    {.label = "3",   .val = 3.0f},
    {.label = "4",   .val = 4.0f},
    {.label = ">5",  .val = 5.0f}
};

static void on_cur_mode_change(Subject *subj, void *user_data);

/* One row of the TX meter: scale on top, bars below it (like the S-meter). */
static void draw_row(lv_draw_ctx_t *draw_ctx, lv_coord_t x1, lv_coord_t y1, const item_t *items, uint8_t num_items,
                     float min, float max, float step, float val, bool swr) {
    lv_draw_rect_dsc_t  rect_dsc;
    lv_draw_label_dsc_t label_dsc;
    lv_area_t           area;
    lv_point_t          label_size;
    uint8_t             slice_w = 5;
    uint8_t             slices_total = (max - min) / step + 0.5f;

    uint32_t count = (LV_MAX(val, min) - min + step) / step;
    count = LV_MIN(count, slices_total);

    lv_draw_rect_dsc_init(&rect_dsc);

    area.y1 = y1 + 22;
    area.y2 = y1 + 36;

    for (uint16_t i = 0; i < slices_total; i++) {
        float v = min + i * step;

        rect_dsc.bg_opa = LV_OPA_80;
        if (i >= count) {
            rect_dsc.bg_color = lv_color_hex(0x000000);
            rect_dsc.bg_opa = LV_OPA_40;
        } else if (!swr || v <= 2.0f) {
            rect_dsc.bg_color = lv_color_hex(0xAAAAAA);
        } else if (v <= 3.0f) {
            rect_dsc.bg_color = lv_color_hex(0xAAAA00);
        } else {
            rect_dsc.bg_color = lv_color_hex(0xAA0000);
        }
        area.x1 = x1 + i * slice_w;
        area.x2 = area.x1 + slice_w - 2;

        lv_draw_rect(draw_ctx, &rect_dsc, &area);
    }

    lv_draw_label_dsc_init(&label_dsc);

    label_dsc.color = lv_color_white();
    label_dsc.font  = &sony_16;

    area.y1 = y1 + 3;
    area.y2 = area.y1 + 18;

    for (uint8_t i = 0; i < num_items; i++) {
        lv_txt_get_size(&label_size, items[i].label, label_dsc.font, 0, 0, LV_COORD_MAX, 0);

        area.x1 = x1 + slice_w * (uint8_t)((items[i].val - min) / step + 0.5f) - label_size.x / 2;
        area.x1 = LV_MAX(area.x1, x1 - 4);
        area.x2 = area.x1 + label_size.x;

        lv_draw_label(draw_ctx, &label_dsc, &area, items[i].label, NULL);
    }
}

static void tx_info_draw_cb(lv_event_t *e) {
    lv_obj_t      *obj      = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);

    lv_coord_t x1 = obj->coords.x1 + 16;
    lv_coord_t y1 = obj->coords.y1;

    draw_row(draw_ctx, x1, y1, pwr_items, NUM_PWR_ITEMS, min_pwr, max_pwr, 0.25f, pwr, false);
    draw_row(draw_ctx, x1, y1 + TOP_BAR_BOX_H, vswr_items, NUM_VSWR_ITEMS, min_swr, max_swr, 0.1f, vswr, true);
}

static void tx_cb(lv_event_t *e) {
    pwr  = 0.0f;
    vswr = 0.0f;
    alc  = 0.0f;

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(obj);
}

static void rx_cb(lv_event_t *e) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}


static void update_tx_info(void *arg) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    lv_label_set_text_fmt(vswr_label, "%.2f", vswr);
    lv_label_set_text_fmt(pwr_label, "%.2fW\nALC %.1f", pwr, alc);

    lv_obj_invalidate(obj);
}



/*
static void update_tx_info(void *arg) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    lv_label_set_text_fmt(alc_label, "ALC: %1.1f", alc);
    lv_label_set_text_fmt(vswr_label, "%.2f", vswr);
    lv_label_set_text_fmt(pwr_label, "%.2f", pwr);
    
     * PWR / SWR bars are drawn in obj's DRAW_MAIN_END handler. The labels
     * only invalidate their own (small) bounding boxes, which does not cover
     * the bar area. Explicitly invalidate the container so the bars repaint
     * on every update -- previously we relied on the main_screen spectrum
     * widget (which fully overlaps tx_info) being invalidated periodically
     * by spectrum_data(), but FT8 dialog turns the spectrum/waterfall DSP
     * off, removing that side effect. *

    lv_obj_invalidate(obj);
    if (params.mag_alc.x) {
        lv_obj_add_flag(alc_label, LV_OBJ_FLAG_HIDDEN);
        msg_tiny_set_text_fmt("ALC: %.1f", alc);
    }
    if (dialog_is_run() || !params.mag_alc.x) {
        lv_obj_clear_flag(alc_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(vswr_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(alc_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(vswr_label, LV_OBJ_FLAG_HIDDEN);
    }
}
*/




lv_obj_t *tx_info_init(lv_obj_t *parent) {
    obj = lv_obj_create(parent);

    lv_obj_add_style(obj, &tx_info_style, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(obj, tx_cb, EVENT_RADIO_TX, NULL);
    lv_obj_add_event_cb(obj, rx_cb, EVENT_RADIO_RX, NULL);
    lv_obj_add_event_cb(obj, tx_info_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);

    grad.dir         = LV_GRAD_DIR_VER;
    grad.stops_count = 4;

    grad.stops[0].color = lv_color_lighten(bg_color, 200);
    grad.stops[1].color = bg_color;
    grad.stops[2].color = bg_color;
    grad.stops[3].color = lv_color_darken(bg_color, 200);

    grad.stops[0].frac = 0;
    grad.stops[1].frac = 128 - 10;
    grad.stops[2].frac = 128 + 10;
    grad.stops[3].frac = 255;

    // pwr and alc indicator
    pwr_label = lv_label_create(obj);
    lv_obj_set_style_text_font(pwr_label, &sony_16, 0);
    lv_obj_set_style_text_align(pwr_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(pwr_label, LV_ALIGN_TOP_RIGHT, -8, 3);
    lv_obj_set_style_text_color(pwr_label, lv_color_white(), 0);
    lv_label_set_text(pwr_label, "");

    // swr indicator
    vswr_label = lv_label_create(obj);
    lv_obj_set_style_text_font(vswr_label, &sony_20, 0);
    lv_obj_align(vswr_label, LV_ALIGN_BOTTOM_RIGHT, -8, -10);
    lv_obj_set_style_text_color(vswr_label, lv_color_white(), 0);
    lv_label_set_text(vswr_label, "");

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
