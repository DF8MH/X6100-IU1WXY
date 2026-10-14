/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Walkie Talkie: a portrait screen (480x800, the knobs on top) in place of
 *  the main one, which is still built and does the radio work unseen. On and
 *  off live: built the first time, then kept.
 */

#include "walkie.h"

#include "styles.h"
#include "theme.h"
#include "events.h"
#include "rotary.h"
#include "vol.h"
#include "radio.h"
#include "util.h"
#include "info.h"
#include "meter.h"
#include "tx_info.h"
#include "clock.h"
#include "dialog_channels.h"
#include "buttons.h"
#include "main.h"
#include "main_screen.h"
#include "params/params.h"
#include "cfg/cfg_api.h"
#include "cfg/settings_manager_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W           480
#define SIDE_W      104             /* the bottom buttons, in a column on the left edge */
#define SIDE_KEY_H  160             /* the F keys are 160 px apart, as the landscape buttons */
#define X0          (SIDE_W + 8)    /* the rest of the screen */
#define CW          (W - X0 - 8)
#define XC          ((X0 + CW / 2) - W / 2)     /* centre offset for LV_ALIGN_TOP_MID */
#define STRIP_H     200
#define METER_H     170
#define BAR_H       30
#define ROW_STEP    110             /* VOL, SQL, PWR: a name and value over a bar */

static lv_obj_t *main_scr;              /* the main screen */
static lv_obj_t *main_obj;              /* the same while the walkie screen is up */
static lv_timer_t *timer;
static lv_obj_t *scr, *clock_label, *bat_label, *chan_label, *freq_label, *mode_label;
static lv_obj_t *meter_box, *tx_label, *rxtx, *rxtx_label;
static lv_obj_t *vol_bar, *vol_val, *sql_bar, *sql_val, *pwr_bar, *pwr_val;
static lv_style_t scr_style, strip_style, meter_box_style;     /* boxes of the theme */
static bool     tx;
static float    tx_pwr, tx_swr, tx_alc;
static uint8_t  tx_msg_id;

static void set_text(lv_obj_t *label, const char *text) {
    if (strcmp(lv_label_get_text(label), text))
        lv_label_set_text(label, text);
}

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color) {
    lv_obj_t *l = lv_label_create(parent);

    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, "");
    return l;
}

/* sony_60 is the biggest font: the frequency is zoomed from its centre to fill the strip */
static void freq_set(const char *text) {
    lv_point_t s;

    if (!strcmp(lv_label_get_text(freq_label), text))
        return;
    lv_label_set_text(freq_label, text);
    lv_txt_get_size(&s, text, &sony_60, 0, 0, LV_COORD_MAX, 0);
    lv_obj_set_style_transform_pivot_x(freq_label, s.x / 2, 0);
    lv_obj_set_style_transform_pivot_y(freq_label, s.y / 2, 0);
    lv_obj_set_style_transform_zoom(freq_label, LV_MIN(LV_IMG_ZOOM_NONE * 3 / 2, LV_IMG_ZOOM_NONE * (CW - 24) / s.x), 0);
}

/* A level row: the name on the left, the value on the right, a bar under them */
static lv_obj_t *level_row(const char *name, lv_coord_t y, int32_t max, lv_obj_t **value) {
    lv_obj_t *l = label(scr, &sony_28, TC(text_head));
    lv_obj_t *b = lv_bar_create(scr);

    lv_label_set_text(l, name);
    lv_obj_set_pos(l, X0 + 2, y);
    *value = label(scr, &sony_32, TC(text));
    lv_obj_set_width(*value, 180);
    lv_obj_set_style_text_align(*value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(*value, X0 + CW - 182, y - 2);
    lv_obj_set_size(b, CW, BAR_H);
    lv_obj_set_pos(b, X0, y + 44);
    lv_bar_set_range(b, 0, max);
    lv_obj_set_style_radius(b, theme_shape.radius, LV_PART_MAIN);
    lv_obj_set_style_radius(b, theme_shape.radius, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(b, LV_MAX(theme_shape.border, 1), LV_PART_MAIN);
    lv_obj_set_style_border_color(b, TC(bar_border), LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, TC(chip), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, TC(text_value), LV_PART_INDICATOR);
    return b;
}

/* One TX bar (0-10) with its name on the left */
static void tx_bar(lv_obj_t *obj, lv_draw_ctx_t *draw_ctx, lv_coord_t y, const char *name, float v, lv_color_t on) {
    lv_draw_rect_dsc_t  dsc;
    lv_draw_label_dsc_t ld;
    lv_area_t           a;

    lv_draw_label_dsc_init(&ld);
    ld.font  = &sony_26;
    ld.color = TC(text_value);
    a.x1 = obj->coords.x1 + 16;
    a.x2 = a.x1 + 70;
    a.y1 = obj->coords.y1 + y - 2;
    a.y2 = a.y1 + 30;
    lv_draw_label(draw_ctx, &ld, &a, name, NULL);

    lv_draw_rect_dsc_init(&dsc);
    a.x1 = obj->coords.x1 + 90;
    a.x2 = obj->coords.x2 - 16;
    a.y1 = obj->coords.y1 + y;
    a.y2 = a.y1 + 24;
    dsc.bg_color = TC(chip);
    lv_draw_rect(draw_ctx, &dsc, &a);
    a.x2 = a.x1 + (lv_area_get_width(&a) - 1) * LV_CLAMP(0.0f, v, 10.0f) / 10.0f;
    dsc.bg_color = on;
    lv_draw_rect(draw_ctx, &dsc, &a);
}

/* TX: power (0-10 W) and ALC bars instead of the S-meter needle */
static void meter_draw_cb(lv_event_t *e) {
    lv_obj_t      *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);

    if (!tx) {
        meter_needle_draw(obj, draw_ctx);
        return;
    }
    tx_bar(obj, draw_ctx, 24, "PWR", tx_pwr, TC(tx));
    tx_bar(obj, draw_ctx, 66, "ALC", tx_alc, TC(alc));
}

static void mem_step(int dir) {
    if (dialog_channels_recall_relative(dir))
        return;
    param_i_set(cfg_walkie_mem, 0);     /* no memories: stay on the VFO */
}

static void rotary_cb(lv_event_t *e) {
    rotary_data_t *data = lv_event_get_param(e);

    if (!param_i_get(cfg_walkie_mem)) {
        lv_event_send(main_obj, EVENT_ROTARY, data);    /* the main screen tunes and frees it */
        return;
    }
    for (int i = 0; i < LV_ABS(data->diff) && i < 5; i++)
        mem_step(data->diff > 0 ? 1 : -1);
    free(data);
}

/* The WLK pages: the front keys of the main screen (data->ctrl), short and held, and the
 * walkie's own MEM/VFO and exit */
static void front_key(keypad_key_t key, keypad_state_t state) {
    event_keypad_t ev = { key, state };

    lv_event_send(main_obj, EVENT_KEYPAD, &ev);
}

static void key_press_cb(button_data_t *d) {
    front_key(d->ctrl, KEYPAD_RELEASE);
}

static void key_hold_cb(button_data_t *d) {
    front_key(d->ctrl, KEYPAD_LONG);
}

static void mem_toggle_cb(button_data_t *d) {
    param_i_set(cfg_walkie_mem, !param_i_get(cfg_walkie_mem));
}

static void exit_cb(button_data_t *d) {
    param_i_set(cfg_walkie, 0);         /* on_walkie_change() goes back */
}

#define KEY_BTN(text, key, held) \
    { .type = BTN_TEXT, .label = text, .press = key_press_cb, .hold = held, .ctrl = key }

static buttons_page_t wlk_1, wlk_2, wlk_3, wlk_4;

static button_data_t wlk_p1 = { .type = BTN_TEXT, .label = "(WLK 1:4)", .press = button_next_page_cb,
                                .hold = button_prev_page_cb, .next = &wlk_2, .prev = &wlk_4 };
static button_data_t wlk_p2 = { .type = BTN_TEXT, .label = "(WLK 2:4)", .press = button_next_page_cb,
                                .hold = button_prev_page_cb, .next = &wlk_3, .prev = &wlk_1 };
static button_data_t wlk_p3 = { .type = BTN_TEXT, .label = "(WLK 3:4)", .press = button_next_page_cb,
                                .hold = button_prev_page_cb, .next = &wlk_4, .prev = &wlk_2 };
static button_data_t wlk_p4 = { .type = BTN_TEXT, .label = "(WLK 4:4)", .press = button_next_page_cb,
                                .hold = button_prev_page_cb, .next = &wlk_1, .prev = &wlk_3 };
static button_data_t wlk_mem = { .type = BTN_TEXT, .label = "MEM\nVFO", .press = mem_toggle_cb };
static button_data_t wlk_band_up = KEY_BTN("Band +", KEYPAD_BAND_UP, NULL);
static button_data_t wlk_band_down = KEY_BTN("Band -", KEYPAD_BAND_DOWN, NULL);
static button_data_t wlk_am = KEY_BTN("AM\nFM", KEYPAD_MODE_AM, NULL);
static button_data_t wlk_ssb = KEY_BTN("SSB", KEYPAD_MODE_SSB, key_hold_cb);    /* held: DIG */
static button_data_t wlk_atu = KEY_BTN("ATU", KEYPAD_ATU, key_hold_cb);         /* held: tune, it transmits */
static button_data_t wlk_agc = KEY_BTN("AGC", KEYPAD_AGC, key_hold_cb);         /* held: split */
static button_data_t wlk_pre = KEY_BTN("PRE", KEYPAD_PRE, key_hold_cb);         /* held: ATT */
static button_data_t wlk_ab = KEY_BTN("A/B", KEYPAD_AB, key_hold_cb);           /* held: copy */
static button_data_t wlk_step = KEY_BTN("Step", KEYPAD_FST, key_hold_cb);       /* held: back */
static button_data_t wlk_lock = KEY_BTN("Lock", KEYPAD_LOCK, NULL);             /* held would reset */
static button_data_t wlk_exit = { .type = BTN_TEXT, .label = "Exit\nhold", .hold = exit_cb };


static void levels_cb(button_data_t *d) {
    buttons_load_page(&buttons_page_vol_1);
}

static button_data_t wlk_levels = { .type = BTN_TEXT, .label = "Vol Sql\nRF Pwr", .press = levels_cb };

static buttons_page_t wlk_1 = {{&wlk_p1, &wlk_mem, &wlk_band_up, &wlk_band_down, &btn_tx_pwr}};
static buttons_page_t wlk_2 = {{&wlk_p2, &wlk_am, &wlk_ssb, &wlk_atu, &wlk_pre}};
static buttons_page_t wlk_3 = {{&wlk_p3, &wlk_agc, &wlk_ab, &wlk_step, &wlk_lock}};
static buttons_page_t wlk_4 = {{&wlk_p4, &wlk_levels, &wlk_exit}};

static void keypad_cb(lv_event_t *e) {
    event_keypad_t *k = lv_event_get_param(e);

    switch (k->key) {
        case KEYPAD_APP:
            if (k->state == KEYPAD_RELEASE)
                buttons_load_page(&wlk_1);
            break;

        /* The page keys as on the main screen; held they may start an app, unseen here */
        case KEYPAD_GEN:
        case KEYPAD_KEY:
        case KEYPAD_DFN:
        case KEYPAD_DFL:
        case KEYPAD_VM:
            if (k->state == KEYPAD_RELEASE)
                lv_event_send(main_obj, EVENT_KEYPAD, k);
            break;

        case KEYPAD_PTT:            /* the PTT key on the radio's side, with the builtin mic */
        case KEYPAD_F1:
        case KEYPAD_F2:
        case KEYPAD_F3:
        case KEYPAD_F4:
        case KEYPAD_F5:
        case KEYPAD_POWER:
        case KEYPAD_LOCK:
        case KEYPAD_BAND_UP:
        case KEYPAD_BAND_DOWN:
        case KEYPAD_MODE_AM:
        case KEYPAD_MODE_CW:
        case KEYPAD_MODE_SSB:
        case KEYPAD_FST:
        case KEYPAD_PRE:
        case KEYPAD_AB:
        case KEYPAD_ATU:
        case KEYPAD_AGC:
            lv_event_send(main_obj, EVENT_KEYPAD, k);
            break;

        default:
            break;
    }
}

static void update_cb(lv_timer_t *t) {
    char      buf[64];
    time_t    now = time(NULL);
    struct tm tm;
    bool      on_air = radio_get_state() == RADIO_TX;
    uint16_t  mhz, khz, hz;

    localtime_r(&now, &tm);
    strftime(buf, sizeof(buf), "%H:%M", &tm);
    set_text(clock_label, buf);
    snprintf(buf, sizeof(buf), "%.1f V", clock_battery_volts());
    set_text(bat_label, buf);

    if (param_i_get(cfg_walkie_mem)) {
        int16_t i = dialog_channels_last_recalled_index();

        if (i < 0)
            snprintf(buf, sizeof(buf), "MEM --");
        else
            snprintf(buf, sizeof(buf), "MEM %02i - %s", i + 1, dialog_channels_last_recalled_name());
    } else {
        snprintf(buf, sizeof(buf), "VFO");
    }
    set_text(chan_label, buf);

    split_freq(cparam_i_get(cfg_fg_freq), &mhz, &khz, &hz);
    snprintf(buf, sizeof(buf), "%i.%03i.%03i", mhz, khz, hz);
    freq_set(buf);

    /* No repeater offset or tone in this firmware: split is the TX on the other VFO */
    if (param_i_get(cfg_band_split)) {
        split_freq(cparam_i_get(cfg_bg_freq), &mhz, &khz, &hz);
        snprintf(buf, sizeof(buf), "%s - SPLIT %i.%03i.%03i", info_params_mode_label_get(), mhz, khz, hz);
    } else {
        snprintf(buf, sizeof(buf), "%s", info_params_mode_label_get());
    }
    set_text(mode_label, buf);

    lv_bar_set_value(vol_bar, param_i_get(cfg_volume), LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%i", (int)param_i_get(cfg_volume));
    set_text(vol_val, buf);
    lv_bar_set_value(sql_bar, param_i_get(cfg_squelch), LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%i", (int)param_i_get(cfg_squelch));
    set_text(sql_val, buf);
    lv_bar_set_value(pwr_bar, (int32_t)(param_f_get(cfg_pwr) * 10.0f + 0.5f), LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%.1f W", param_f_get(cfg_pwr));
    set_text(pwr_val, buf);

    if (on_air != tx) {
        tx = on_air;
        lv_obj_set_style_border_width(scr, tx ? 4 : 0, 0);
        lv_obj_set_style_bg_color(rxtx, tx ? TC(tx_bg) : TC(ok_bg), 0);
        lv_obj_set_style_border_color(rxtx, tx ? TC(tx) : TC(ok), 0);
        lv_obj_set_style_text_color(rxtx_label, tx ? TC(text) : TC(ok), 0);
        lv_label_set_text(rxtx_label, tx ? "TX" : "RX");
        if (!tx)
            lv_label_set_text(tx_label, "");
    }
    if (tx && tx_info_refresh(&tx_msg_id, &tx_alc, &tx_pwr, &tx_swr)) {
        /* SWR from 2 a warning in yellow, from 3 in red (the bar colours of the TX panel) */
        if (tx_swr >= 2.0f)
            snprintf(buf, sizeof(buf), "SWR %.1f HIGH", tx_swr);
        else
            snprintf(buf, sizeof(buf), "%.1f W - SWR %.1f", tx_pwr, tx_swr);
        set_text(tx_label, buf);
        lv_obj_set_style_text_color(tx_label, tx_swr >= 3.0f ? TC(bar_high) : tx_swr >= 2.0f ? TC(bar_mid) : TC(text), 0);
    }
    lv_obj_invalidate(meter_box);       /* the needle moves with meter_update */
}

bool walkie_on(void) {
    return main_obj != NULL;
}

static void build(void) {
    lv_obj_t *o;

    scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    styles_theme_box(&scr_style, TC_bg, TC_bg, TC_bg, 0, TI_bg, W, 800);
    lv_obj_add_style(scr, &scr_style, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);         /* opaque: nothing of the waterfall plane */
    lv_obj_set_style_border_color(scr, TC(tx), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_add_event_cb(scr, rotary_cb, EVENT_ROTARY, NULL);
    lv_obj_add_event_cb(scr, keypad_cb, EVENT_KEYPAD, NULL);

    /* Header: the time and the battery over a rule */
    clock_label = label(scr, &sony_28, TC(text_soft));
    lv_obj_set_pos(clock_label, X0 + 2, 8);
    bat_label = label(scr, &sony_28, TC(text_soft));
    lv_obj_align(bat_label, LV_ALIGN_TOP_RIGHT, -10, 8);
    o = lv_obj_create(scr);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, CW, 1);
    lv_obj_set_pos(o, X0, 50);
    lv_obj_set_style_bg_color(o, TC(strip_border), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);

    /* The channel panel on the theme's strip: VFO or memory and RX/TX, the frequency, the mode */
    o = lv_obj_create(scr);
    lv_obj_remove_style_all(o);
    styles_theme_box(&strip_style, TC_strip, TC_strip_grad, TC_strip_border, theme_shape.radius,
                     TI_strip, CW, STRIP_H);
    lv_obj_add_style(o, &strip_style, 0);
    lv_obj_set_size(o, CW, STRIP_H);
    lv_obj_set_pos(o, X0, 60);
    chan_label = label(o, &sony_28, TC(text_head));
    lv_obj_set_style_max_width(chan_label, CW - 120, 0);     /* a long memory name ends in dots */
    lv_label_set_long_mode(chan_label, LV_LABEL_LONG_DOT);
    lv_obj_set_height(chan_label, lv_font_get_line_height(&sony_28));
    lv_obj_set_pos(chan_label, 14, 12);

    rxtx = lv_obj_create(o);
    lv_obj_remove_style_all(rxtx);
    lv_obj_set_size(rxtx, 72, 40);
    lv_obj_set_style_radius(rxtx, LV_MIN(theme_shape.radius, 6), 0);
    lv_obj_set_style_border_width(rxtx, 2, 0);
    lv_obj_set_style_border_color(rxtx, TC(ok), 0);
    lv_obj_set_style_bg_opa(rxtx, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(rxtx, TC(ok_bg), 0);
    lv_obj_align(rxtx, LV_ALIGN_TOP_RIGHT, -10, 8);
    rxtx_label = label(rxtx, &sony_28, TC(ok));
    lv_label_set_text(rxtx_label, "RX");
    lv_obj_center(rxtx_label);

    freq_label = label(o, &sony_60, TC(text));
    lv_obj_align(freq_label, LV_ALIGN_TOP_MID, 0, 74);
    mode_label = label(o, &sony_32, TC(text_soft));
    lv_obj_align(mode_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    meter_box = lv_obj_create(scr);
    lv_obj_remove_style_all(meter_box);
    styles_theme_box(&meter_box_style, TC_bar, TC_bar_grad, TC_bar_border, theme_shape.radius,
                     TI_bar, CW, METER_H);
    lv_obj_add_style(meter_box, &meter_box_style, 0);
    lv_obj_set_size(meter_box, CW, METER_H);
    lv_obj_set_pos(meter_box, X0, 270);
    lv_obj_set_style_clip_corner(meter_box, true, 0);
    lv_obj_add_event_cb(meter_box, meter_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    tx_label = label(meter_box, &sony_32, TC(text));
    lv_obj_align(tx_label, LV_ALIGN_BOTTOM_MID, 0, -24);

    vol_bar = level_row("VOL", 458, 55, &vol_val);
    sql_bar = level_row("SQL", 458 + ROW_STEP, 100, &sql_val);
    pwr_bar = level_row("PWR", 458 + 2 * ROW_STEP, 100, &pwr_val);

    timer = lv_timer_create(update_cb, 100, NULL);
}

static void enter(void) {
    if (!main_display_portrait(true)) {
        return;                                     /* fbdev: no portrait */
    }
    main_screen_close_app();                        /* no app or Settings left open under it */
    main_obj = main_scr;
    vol_set_ctrl(CTRL_VOL);                         /* the VOL knob: the volume, whatever it was on */
    radio_set_mic_override(x6100_mic_builtin);     /* the PTT key and VOX: the builtin mic */
    if (!scr) {
        build();
    }
    /* The F1-F5 keys are under the screen in landscape: in portrait they run down the left
     * edge, F1 on top, so the main screen's buttons go there and the knobs work as there */
    buttons_vertical(scr, SIDE_W, SIDE_KEY_H);
    buttons_load_page(&wlk_1);
    lv_timer_resume(timer);
    lv_scr_load(scr);
}

static void leave(void) {
    main_obj = NULL;                                /* walkie_on() false from here */
    lv_timer_pause(timer);
    radio_set_mic_override(-1);                     /* cfg_mic again, never changed */
    buttons_horizontal(main_scr);
    buttons_load_page(&buttons_page_vol_1);         /* as at start */
    main_display_portrait(false);
    lv_scr_load(main_scr);
}

static void on_walkie_change(Subject *subj, void *user_data) {
    bool on = param_i_get(cfg_walkie);

    if (on && !walkie_on()) {
        enter();
    } else if (!on && walkie_on()) {
        leave();
    }
}

void walkie_init(lv_obj_t *main_screen_obj) {
    main_scr = main_screen_obj;
    subject_subscribe_delayed_and_notify((Subject *)cfg_walkie, on_walkie_change, NULL);
}
