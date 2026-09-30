/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "cluster_column.h"

#include "cluster/client.h"
#include "cfg/cfg_api.h"
#include "styles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COL_W   200
#define TITLE_H 26
#define ROW_H   24
#define POOL    16

typedef struct {
    lv_obj_t *freq, *call, *age;
} row_t;

static lv_obj_t      *box;
static row_t          rows[POOL];
static cluster_spot_t spots[CLUSTER_MAX_SPOTS];
static int            spots_n;
static uint32_t       spots_gen;
static uint16_t       last_min = 0xFFFF;
static int32_t        last_vfo = -1;
static int32_t        vfo;

static uint32_t age_color(uint16_t age) {
    if (age < 2)  return 0x7CFF7C;
    if (age < 10) return 0xE8E070;
    if (age < 30) return 0xC08A4A;
    return 0x777777;
}

static int by_freq(const void *a, const void *b) {
    return ((const cluster_spot_t *)a)->freq_hz - ((const cluster_spot_t *)b)->freq_hz;
}

static void show_row(row_t *r, bool show) {
    void (*f)(lv_obj_t *, lv_obj_flag_t) = show ? lv_obj_clear_flag : lv_obj_add_flag;

    f(r->freq, LV_OBJ_FLAG_HIDDEN);
    f(r->call, LV_OBJ_FLAG_HIDDEN);
    f(r->age, LV_OBJ_FLAG_HIDDEN);
}

/* The band's spots by frequency with the VFO marker row among them, kept
 * in the middle of the box: tuning scrolls the names past it. */
static void redraw(void) {
    static cluster_spot_t band[CLUSTER_MAX_SPOTS];
    int       n = 0, m = 0;
    int       cb = cluster_band_of(vfo);
    time_t    t  = time(NULL);
    struct tm tm;
    uint16_t  now;

    if (!box) {
        return;
    }
    if (!param_i_get(cfg_cluster_column)) {
        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(box, LV_OBJ_FLAG_HIDDEN);
    gmtime_r(&t, &tm);
    now = tm.tm_hour * 60 + tm.tm_min;

    if (cb >= 0) {
        for (int i = 0; i < spots_n; i++) {
            if (cluster_band_of(spots[i].freq_hz) == cb) {
                band[n++] = spots[i];
            }
        }
        qsort(band, n, sizeof(band[0]), by_freq);
    }
    while (m < n && band[m].freq_hz < vfo) {
        m++;    /* the marker goes before band[m] */
    }
    int fit = (lv_obj_get_height(box) - TITLE_H - 8) / ROW_H;

    if (fit > POOL) {
        fit = POOL;
    }
    for (int k = 0; k < fit; k++) {
        int   idx = m - fit / 2 + k;     /* < m: spots below the marker, m: the marker, > m: spots (idx - 1) */
        row_t *r  = &rows[k];

        if (idx < 0 || idx > n) {
            show_row(r, false);
        } else if (idx == m) {
            char buf[32];

            snprintf(buf, sizeof buf, "%d.%03d.%02d", vfo / 1000000, (vfo / 1000) % 1000, (vfo % 1000) / 10);
            lv_label_set_text(r->freq, buf);
            lv_obj_set_width(r->freq, COL_W - 12);
            lv_obj_set_style_text_align(r->freq, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(r->freq, lv_color_hex(0x7CC4FF), 0);
            lv_obj_set_style_bg_opa(r->freq, LV_OPA_COVER, 0);
            show_row(r, true);
            lv_obj_add_flag(r->call, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(r->age, LV_OBJ_FLAG_HIDDEN);
        } else {
            const cluster_spot_t *s = &band[idx < m ? idx : idx - 1];
            uint16_t age = cluster_age_min(s->utc_min, now);
            uint32_t c   = age_color(age);
            char     buf[16];

            snprintf(buf, sizeof buf, "%d", (int)(s->freq_hz / 1000));
            lv_label_set_text(r->freq, buf);
            lv_obj_set_width(r->freq, 56);
            lv_obj_set_style_text_align(r->freq, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_style_text_color(r->freq, lv_color_hex(0xDDE6F0), 0);
            lv_obj_set_style_bg_opa(r->freq, LV_OPA_TRANSP, 0);
            lv_label_set_text(r->call, s->dx);
            snprintf(buf, sizeof buf, "%um", age);
            lv_label_set_text(r->age, buf);
            lv_obj_set_style_text_color(r->call, lv_color_hex(c), 0);
            lv_obj_set_style_text_color(r->age, lv_color_hex(c), 0);
            show_row(r, true);
        }
    }
    for (int k = fit; k < POOL; k++) {
        show_row(&rows[k], false);
    }
}

/* 1 Hz, redraws only when the spots, the minute or the VFO changed */
static void timer_cb(lv_timer_t *t) {
    int      n   = cluster_client_snapshot(spots, CLUSTER_MAX_SPOTS, &spots_gen);
    uint16_t min = (time(NULL) / 60) % 1440;

    vfo = cparam_i_get(cfg_fg_freq);
    if (n >= 0) {
        spots_n = n;
    }
    if (n >= 0 || min != last_min || vfo != last_vfo) {
        last_min = min;
        last_vfo = vfo;
        redraw();
    }
}

void cluster_column_layout(lv_coord_t y) {
    if (box) {
        lv_obj_set_y(box, y);
        lv_obj_set_height(box, 480 - BUTTONS_H - y);
        redraw();
    }
}

void cluster_column_refresh(void) {
    vfo      = cparam_i_get(cfg_fg_freq);
    last_vfo = vfo;
    redraw();
}

static lv_obj_t *cell(lv_obj_t *p, lv_coord_t x, lv_coord_t w, lv_coord_t y, lv_text_align_t al, uint32_t c) {
    lv_obj_t *l = lv_label_create(p);

    lv_obj_set_style_text_font(l, &sony_18, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(c), 0);
    lv_obj_set_style_text_align(l, al, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    lv_obj_set_style_bg_color(l, lv_color_hex(0x0A1F3A), 0);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

void cluster_column_init(lv_obj_t *parent, lv_coord_t y) {
    box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, 800 - COL_W, y);
    lv_obj_set_size(box, COL_W, 480 - BUTTONS_H - y);
    lv_obj_set_style_radius(box, 10, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x080E1E), 0);
    lv_obj_set_style_bg_opa(box, 200, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x3A4A6A), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = cell(box, 0, COL_W - 2, 2, LV_TEXT_ALIGN_CENTER, 0x60E0D0);
    lv_label_set_text(title, "DX Cluster");
    lv_obj_clear_flag(title, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < POOL; i++) {
        lv_coord_t ry = TITLE_H + i * ROW_H;

        rows[i].freq = cell(box, 5, 56, ry, LV_TEXT_ALIGN_LEFT, 0xDDE6F0);
        rows[i].call = cell(box, 66, 86, ry, LV_TEXT_ALIGN_LEFT, 0xFFFFFF);
        rows[i].age  = cell(box, 152, 42, ry, LV_TEXT_ALIGN_RIGHT, 0xFFFFFF);
    }
    lv_timer_create(timer_cb, 1000, NULL);
    timer_cb(NULL);
}
