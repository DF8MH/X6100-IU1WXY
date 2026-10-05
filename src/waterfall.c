/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */
#include "waterfall.h"

#include "styles.h"
#include "theme.h"
#include "radio.h"
#include "events.h"
#include "params/params.h"
#include "cfg/cfg_api.h"
#include "band_info.h"
#include "meter.h"
#include "backlight.h"
#include "dsp.h"
#include "util.h"
#include "pubsub_ids.h"
#include "scheduler.h"
#include "lv_drivers/display/drm.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define PX_BYTES    sizeof(lv_color_t)
#define DEFAULT_MIN S4
#define DEFAULT_MAX S9_20
#define WIDTH 800

typedef struct {
    uint8_t values[WATERFALL_NFFT];
    uint32_t center_freq;
    uint32_t width;
} wf_data_row_t;

static lv_obj_t         *obj;
static lv_obj_t         *img;
static bool             ready = false;

static lv_style_t       middle_line_style;
static lv_obj_t         *middle_line;
static lv_obj_t         *filter_band;
static lv_point_t       middle_line_points[] = { {0, 0}, {0, 0} };

static lv_coord_t       height;
static int32_t          width_hz = 100000;

static float            grid_min = DEFAULT_MIN;
static float            grid_max = DEFAULT_MAX;

static lv_img_dsc_t     *frame;
static uint8_t          delay = 0;

static wf_data_row_t    *wf_rows;
static uint16_t         last_row_id;

static int32_t          radio_center_freq = 0;
static int32_t          wf_center_freq = 0;
static int32_t          mode_lo_offset = 0;
static int32_t          if_shift = 0;

static uint8_t          refresh_period = 1;
static uint8_t          refresh_counter = 0;

static uint8_t          zoom = 1;

/* Direct mode (DRM planes): the waterfall is drawn on the primary plane under
 * the GUI, which is transparent there. wfp is its portrait image: landscape
 * row y of the waterfall is portrait column y, landscape x is row 799 - x.
 * A new row shifts the image by one column and draws only that column; a new
 * centre or zoom redraws it all */
static bool             direct = false;
static uint32_t         *wfp;
static int32_t          wfp_center = 0;
static uint32_t         wfp_bw = 0;
static bool             wfp_full = true;
static int              new_rows = 0;       /* rows from the DSP thread not drawn yet */

/* Fixed mode: RF frequency at the display centre, valid while view_fixed */
static int32_t          view_center = 0;
static bool             view_fixed = false;

static void refresh_waterfall( void * arg);
static void do_scroll_timer(lv_timer_t *t);
static void update_middle_line();
static void middle_line_cb(lv_event_t * event);
static void redraw_cb(lv_event_t * e);
static void on_zoom_changed(Subject *subj, void *user_data);
static void on_view_changed(Subject *subj, void *user_data);
static void on_fg_freq_change(Subject *subj, void *user_data);
static void on_mode_lo_offset_change(Subject *subj, void *user_data);
static void on_if_shift_changed(Subject *subj, void *user_data);
static void on_filter_change(Subject *subj, void *user_data);
static void on_grid_min_change(Subject *subj, void *user_data);
static void on_grid_max_change(Subject *subj, void *user_data);


lv_obj_t * waterfall_init(lv_obj_t * parent) {
    subject_subscribe_and_notify((Subject*)cfg_fg_freq, on_fg_freq_change, NULL);
    wf_center_freq = radio_center_freq;

    obj = lv_obj_create(parent);

    lv_obj_add_style(obj, &waterfall_style, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    // Middle line style
    lv_style_init(&middle_line_style);
    lv_style_set_line_width(&middle_line_style, 1);
    /* VFO line in amber so it stands out from the grey 20 kHz grid */
    lv_style_set_line_color(&middle_line_style, TC(vfo_line));
    lv_style_set_line_opa(&middle_line_style, LV_OPA_COVER);
    lv_style_set_pad_all(&middle_line_style, 0);

    /* Filter passband: a light band of the VFO line colour, under the line */
    filter_band = lv_obj_create(obj);
    lv_obj_remove_style_all(filter_band);
    lv_obj_set_style_bg_color(filter_band, TC(vfo_line), 0);
    lv_obj_set_style_bg_opa(filter_band, LV_OPA_20, 0);
    lv_obj_set_height(filter_band, LV_PCT(100));
    lv_obj_clear_flag(filter_band, LV_OBJ_FLAG_CLICKABLE);

    middle_line = lv_line_create(obj);
    // lv_line_set_points(middle_line, middle_line_points, 2);
    lv_obj_add_style(middle_line, &middle_line_style, 0);
    lv_obj_center(middle_line);
    lv_obj_add_event_cb(obj, middle_line_cb, LV_EVENT_DRAW_POST_END, NULL);

    subject_subscribe_delayed_and_notify((Subject*)cfg_mode_zoom, on_zoom_changed, NULL);
    subject_subscribe_delayed_and_notify((Subject*)cfg_band_if_shift, on_if_shift_changed, NULL);
    subject_subscribe_delayed((Subject*)cfg_cur_filter_low, on_filter_change, NULL);
    subject_subscribe_delayed((Subject*)cfg_cur_filter_high, on_filter_change, NULL);
    subject_subscribe_delayed((Subject*)cfg_cur_mode, on_filter_change, NULL);

    subject_subscribe_delayed((Subject*)cfg_fg_freq, on_view_changed, NULL);
    subject_subscribe_delayed((Subject*)cfg_wf_mode, on_view_changed, NULL);

    subject_subscribe_and_notify((Subject*)cfg_mode_lo_offset, on_mode_lo_offset_change, NULL);
    subject_subscribe((Subject*)cfg_auto_level_enabled, on_grid_min_change, NULL);
    subject_subscribe_and_notify((Subject*)cfg_band_grid_min, on_grid_min_change, NULL);
    subject_subscribe((Subject*)cfg_auto_level_enabled, on_grid_max_change, NULL);
    subject_subscribe_and_notify((Subject*)cfg_band_grid_max, on_grid_max_change, NULL);

    return obj;
}

static void scroll_down() {
    last_row_id = (last_row_id + 1) % height;
}

void waterfall_data(float *data_buf, uint16_t size, bool tx, uint32_t base_freq, uint32_t width_hz) {
    if (delay && (base_freq == 0))
    {
        delay--;
        return;
    }
    scroll_down();

    float min, max;
    waterfall_get_levels(tx, &min, &max);
    if (base_freq == 0) {
        base_freq = radio_center_freq + mode_lo_offset;
    } else if (tx) {
        // New patched firmware
        base_freq += mode_lo_offset;
    }
    wf_rows[last_row_id].center_freq = base_freq;
    wf_rows[last_row_id].width = width_hz;

    float temp_buf[size];
    liquid_vectorf_addscalar(data_buf, size, -min, temp_buf);
    liquid_vectorf_mulscalar(temp_buf, size, 255.0f / (max - min), temp_buf);
    for (uint16_t x = 0; x < size; x++) {
        float   v = temp_buf[x];
        uint8_t id;

        if (v < 0.0f) {
            id = 0;
        } else if (v > 255.0f) {
            id = 255;
        } else {
            id = v;
        }

        wf_rows[last_row_id].values[x] = id;
    }
    __atomic_add_fetch(&new_rows, 1, __ATOMIC_RELEASE);
    scheduler_put_noargs(refresh_waterfall);
}

int32_t waterfall_view_shift(void) {
    if (!param_i_get(cfg_wf_mode)) {
        view_fixed = false;
        return 0;
    }
    int32_t rc = cparam_i_get(cfg_fg_freq) - param_i_get(cfg_band_if_shift);
    int32_t vfo = cparam_i_get(cfg_fg_freq);
    /* Keep a 20% margin: the received data (centred on rc) and the VFO must stay inside */
    int32_t limit = 50000 / param_i_get(cfg_mode_zoom) * 8 / 10;

    if (!view_fixed || abs(rc - view_center) > limit || abs(vfo - view_center) > limit) {
        view_center = rc;
        view_fixed = true;
    }
    return rc - view_center;
}

static void do_scroll_cb(lv_event_t * event) {
    do_scroll_timer(NULL);
}

/* One step of the horizontal scroll towards the VFO (direct mode: every 20 ms) */
static void do_scroll_timer(lv_timer_t *t) {
    int32_t target = radio_center_freq - waterfall_view_shift();

    if (wf_center_freq == target) {
        return;
    }
    if (params.waterfall_smooth_scroll.x) {
        wf_center_freq += (target - wf_center_freq) / 10 + 1;
    } else {
        wf_center_freq = target;
    }
    scheduler_put_noargs(refresh_waterfall);
}

void waterfall_set_height(lv_coord_t h) {
    lv_obj_set_height(obj, h);
    lv_obj_update_layout(obj);

    /* For more accurate horizontal scroll, it should be a "multiple of 500Hz" */
    /* 800 * 500Hz / 100000Hz = 4.0px */

    height = lv_obj_get_height(obj);

#if USE_DRM
    direct = drm_planes();
#endif
    if (direct) {
        /* The GUI stays transparent here: the primary plane shows through */
        wfp = calloc(WIDTH * height, sizeof(uint32_t));
        lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
        lv_obj_add_event_cb(obj, direct_hole_cb, LV_EVENT_DRAW_MAIN_END, NULL);
        lv_timer_create(do_scroll_timer, 20, NULL);
    } else {
        frame = lv_img_buf_alloc(WIDTH, height, LV_IMG_CF_TRUE_COLOR);

        img = lv_img_create(obj);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
        lv_img_set_src(img, frame);
    }

    wf_rows = calloc(height, sizeof(*wf_rows));
    for (size_t i = 0; i < height; i++) {
        wf_rows[i].center_freq = radio_center_freq;
        memset(wf_rows[i].values, 0, WATERFALL_NFFT);
    }
    last_row_id = 0;

    if (!direct) {
        lv_obj_add_event_cb(img, do_scroll_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    }

    waterfall_min_max_reset();
    band_info_init(obj);
    middle_line_points[1].y = height;
    lv_line_set_points(middle_line, middle_line_points, 2);
    if (!direct) {
        lv_obj_move_background(img);
    }
    ready = true;
}

static void middle_line_cb(lv_event_t * event) {
    if (params.waterfall_center_line.x && lv_obj_has_flag(middle_line, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(middle_line, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (!params.waterfall_center_line.x && !lv_obj_has_flag(middle_line, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(middle_line, LV_OBJ_FLAG_HIDDEN);
        return;
    }
}

void waterfall_min_max_reset() {
    if (param_i_get(cfg_auto_level_enabled)) {
        grid_min = DEFAULT_MIN;
        grid_max = DEFAULT_MAX;
    } else {
        grid_min = param_i_get(cfg_band_grid_min);
        grid_max = param_i_get(cfg_band_grid_max);
    }
}

void waterfall_update_max(float db) {
    if (param_i_get(cfg_auto_level_enabled)) {
        grid_max = db - param_f_get(cfg_auto_level_offset);
    }
}

void waterfall_get_levels(bool tx, float *min, float *max) {
    if (tx) {
        *min = DEFAULT_MIN;
        *max = DEFAULT_MAX;
    } else {
        /* Reference level: shifts the whole colour scale, + is brighter */
        *min = grid_min - param_i_get(cfg_waterfall_ref);
        *max = grid_max - param_i_get(cfg_waterfall_ref);
        if (*max < *min + 10.0f) {
            *max = *min + 10.0f;
        }
    }
}

void waterfall_update_min(float db) {
    if (param_i_get(cfg_auto_level_enabled)) {
        grid_min = db - param_f_get(cfg_auto_level_offset);
    }
}

void waterfall_refresh_reset() {
    refresh_period = 1;
}

void waterfall_refresh_period_set(uint8_t k) {
    if (k == 0) {
        return;
    }
    refresh_period = k;
}


#define LERP_INTERP_M 3
#define LERP_INTERP_FRAC (1 << LERP_INTERP_M)
static inline void lerp_row(wf_data_row_t *row_data, uint32_t dst_center_freq, uint32_t dst_width_hz, uint8_t *dst) {
    // Skip empty rows at start
    if (!row_data->width) {
        memset(dst, 0, WIDTH);
        return;
    }
    uint32_t src_start = row_data->center_freq - row_data->width / 2;
    uint32_t src_end = src_start + row_data->width;

    uint32_t dst_start = dst_center_freq - dst_width_hz / 2;
    uint32_t dst_end = dst_start + dst_width_hz;

    if ((src_start > dst_end) || (src_end < dst_start)) {
        // No overlap
        memset(dst, 0, WIDTH);
        return;
    }

    uint32_t dst_half_width_hz = dst_width_hz / 2;
    for (size_t i = 0; i < WIDTH; i++) {
        int32_t freq = dst_center_freq + (dst_width_hz * i + dst_half_width_hz) / WIDTH - dst_half_width_hz;
        // src pos is multiplied by 8
        int32_t src_pos = (freq - src_start) * (WATERFALL_NFFT << LERP_INTERP_M) / row_data->width;
        if (src_pos < 0) {
            dst[i] = 0;
            continue;
        }
        uint32_t src_pos_int = (uint32_t)src_pos >> LERP_INTERP_M;
        if (src_pos_int >= (WATERFALL_NFFT - 2)) {
            dst[i] = 0;
        } else {
            int16_t v0 = row_data->values[src_pos_int];
            int16_t v1 = row_data->values[src_pos_int + 1];
            v0 += ((v1 - v0) * (src_pos - ((uint32_t)src_pos_int << LERP_INTERP_M))) >> LERP_INTERP_M;
            dst[i] = v0;
        }
    }
}

/* A landscape row (newest first) into portrait column col of wfp */
static void direct_column(const wf_data_row_t *row, uint32_t bandwidth, int32_t col) {
    uint8_t dst[WIDTH];

    lerp_row((wf_data_row_t *) row, wf_center_freq, bandwidth, dst);
    for (size_t i = 0; i < WIDTH; i++) {
        wfp[(WIDTH - 1 - i) * height + col] = wf_palette[dst[i]] | 0xff000000;
    }
}

/* Direct mode: the waterfall area of the GUI fully transparent (alpha 0), drawn
 * after the parents and before the children (VFO line, grid, spots), so the
 * primary plane shows through whatever is behind in the GUI */
void direct_hole_cb(lv_event_t *e) {
    lv_draw_ctx_t   *ctx = lv_event_get_draw_ctx(e);
    lv_area_t       a;
    int32_t         stride = lv_area_get_width(ctx->buf_area);

    if (!_lv_area_intersect(&a, &lv_event_get_target(e)->coords, ctx->clip_area)) {
        return;
    }
    for (int32_t y = a.y1; y <= a.y2; y++) {
        lv_color_t *row = (lv_color_t *) ctx->buf + (y - ctx->buf_area->y1) * stride + (a.x1 - ctx->buf_area->x1);

        memset(row, 0, lv_area_get_width(&a) * sizeof(lv_color_t));
    }
}

static bool shown(lv_obj_t *o) {
    for (; o; o = lv_obj_get_parent(o)) {
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {
            return false;
        }
    }
    return true;
}

static void direct_redraw(int new_row) {
    uint32_t    bandwidth = params.waterfall_zoom.x ? width_hz / zoom : width_hz;
    lv_area_t   a;

    if (!shown(obj)) {
        wfp_full = true;            /* not up to date when shown again */
        return;
    }
    if (wfp_full || new_row > 1 || wfp_center != wf_center_freq || wfp_bw != bandwidth) {
        for (int32_t col = 0; col < height; col++) {
            direct_column(&wf_rows[(last_row_id - col + height) % height], bandwidth, col);
        }
        wfp_full = false;
        wfp_center = wf_center_freq;
        wfp_bw = bandwidth;
    } else if (new_row) {
        for (size_t y = 0; y < WIDTH; y++) {
            uint32_t *r = wfp + y * height;

            memmove(r + 1, r, (height - 1) * sizeof(uint32_t));
        }
        direct_column(&wf_rows[last_row_id], bandwidth, 0);
    } else {
        return;
    }
    lv_obj_get_coords(obj, &a);
#if USE_DRM
    drm_wf_present(wfp, a.y1, height);
#endif
}

static void redraw_cb(lv_event_t * e) {
    int32_t src_x_offset;
    uint16_t src_y, src_x0, dst_y, dst_x;

    uint8_t current_zoom = 1;
    uint32_t bandwidth = width_hz;
    if (params.waterfall_zoom.x) {
        current_zoom = zoom;
        bandwidth /= zoom;
    }
    lv_color_t black = lv_color_black();
    lv_color_t px_color;
    uint8_t dst[WIDTH];

    for (src_y = 0; src_y < height; src_y++) {
        wf_data_row_t row_data = wf_rows[src_y];
        dst_y = ((height - src_y + last_row_id) % height);

        lerp_row(&row_data, wf_center_freq, bandwidth, dst);
        for (size_t i = 0; i < WIDTH; i++) {
            *((lv_color_t*)frame->data + (dst_y * WIDTH + i)) = (lv_color_t)wf_palette[dst[i]];
        }
    }
}

static void refresh_waterfall( void * arg) {
    if (!ready) {
        return;
    }
    refresh_counter++;
    if (refresh_counter >= refresh_period) {
        refresh_counter = 0;
        if (direct) {
            direct_redraw(__atomic_exchange_n(&new_rows, 0, __ATOMIC_ACQUIRE));
        } else {
            redraw_cb(NULL);
            lv_obj_invalidate(img);
        }
    }
}

static void on_zoom_changed(Subject *subj, void *user_data) {
    zoom = subject_i_get((SubjectInt*)subj);
    update_middle_line();
}

static void on_view_changed(Subject *subj, void *user_data) {
    update_middle_line();
    scheduler_put_noargs(refresh_waterfall);
}

static void on_if_shift_changed(Subject *subj, void *user_data) {
    delay = 2;
    if_shift = subject_i_get((SubjectInt*)subj);
    radio_center_freq = cparam_i_get(cfg_fg_freq) - if_shift;
    update_middle_line();
}

static void on_fg_freq_change(Subject *subj, void *user_data) {
    delay = 2;
    radio_center_freq = subject_i_get((SubjectInt*)subj) - if_shift;
}

static void on_mode_lo_offset_change(Subject *subj, void *user_data) {
    mode_lo_offset = subject_i_get((SubjectInt*)subj);
}

static void update_middle_line() {
    lv_coord_t width = zoom / 2 + 3;
    lv_coord_t vfo_x = (if_shift + waterfall_view_shift()) * zoom * WIDTH / width_hz;

    lv_obj_set_pos(middle_line, vfo_x + width / 2, 0);
    lv_style_set_line_width(&middle_line_style, width);

    /* The passband, same scale as the line: from the VFO up (USB), down
     * (LSB, CWR) or both ways (AM, FM), as on the spectrum */
    int32_t low = subject_i_get((SubjectInt*)cfg_cur_filter_low);
    int32_t high = subject_i_get((SubjectInt*)cfg_cur_filter_high);
    int32_t from, to;

    switch ((x6100_mode_t) subject_i_get((SubjectInt*)cfg_cur_mode)) {
        case x6100_mode_lsb:
        case x6100_mode_lsb_dig:
        case x6100_mode_cwr:
            from = -high;
            to = -low;
            break;
        case x6100_mode_am:
        case x6100_mode_nfm:
            from = -high;
            to = high;
            break;
        default:
            from = low;
            to = high;
            break;
    }
    lv_coord_t x1 = WIDTH / 2 + vfo_x + from * zoom * WIDTH / width_hz;
    lv_coord_t x2 = WIDTH / 2 + vfo_x + to * zoom * WIDTH / width_hz;

    lv_obj_set_pos(filter_band, x1, 0);
    lv_obj_set_width(filter_band, LV_MAX(x2 - x1, 1));
}

static void on_filter_change(Subject *subj, void *user_data) {
    update_middle_line();
}

static void on_grid_min_change(Subject *subj, void *user_data) {
    if (!param_i_get(cfg_auto_level_enabled)) {
        grid_min = param_i_get(cfg_band_grid_min);
    }
}
static void on_grid_max_change(Subject *subj, void *user_data) {
    if (!param_i_get(cfg_auto_level_enabled)) {
        grid_max = param_i_get(cfg_band_grid_max);
    }
}
