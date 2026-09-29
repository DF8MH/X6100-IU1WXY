/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "clock.h"

#include "styles.h"
#include "backlight.h"
#include "msg.h"

#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/stat.h>

static lv_obj_t         *obj;
static lv_obj_t         *time_label;
static lv_obj_t         *sec_label;
static lv_obj_t         *power_labels[4];  /* battery symbol, volts, ext symbol, volts or % */
static pthread_mutex_t  power_mux;

static lv_timer_t       *timer_time_sync;
static time_t           last_time_sync=0;

static float            v_ext;
static float            v_bat;
static uint8_t          cap_bat;
static bool             charging;

static void check_time_sync_cb(lv_timer_t * t);
static time_t get_last_sync_time();

/* Time on top (seconds smaller), voltages below, both always shown. */
static void show_time() {
    time_t      now;
    struct tm   *t;

    now = time(NULL);
    t = localtime(&now);

    lv_label_set_text_fmt(time_label, "%02i:%02i", t->tm_hour, t->tm_min);
    lv_label_set_text_fmt(sec_label, ":%02i", t->tm_sec);

    pthread_mutex_lock(&power_mux);
    const char * bat_sym;
    const char * ext_sym;
    switch ((uint8_t)(v_bat * 10)) {
        case 0 ... 63:
            bat_sym = LV_SYMBOL_BATTERY_EMPTY;
            break;
        case 64 ... 69:
            bat_sym = LV_SYMBOL_BATTERY_1;
            break;
        case 70 ... 75:
            bat_sym = LV_SYMBOL_BATTERY_2;
            break;
        case 76 ... 81:
            bat_sym = LV_SYMBOL_BATTERY_3;
            break;
        default:
            bat_sym = LV_SYMBOL_BATTERY_FULL;
            break;
    }
    if (charging) {
        ext_sym = LV_SYMBOL_CHARGE;
    } else {
        ext_sym = LV_SYMBOL_POWER;
    }

    lv_label_set_text(power_labels[0], bat_sym);
    lv_label_set_text_fmt(power_labels[1], "%.1fv", v_bat);
    if (v_ext < 3.0f) {
        lv_label_set_text(power_labels[2], "");
        lv_label_set_text_fmt(power_labels[3], "%i%%", cap_bat);
    } else {
        lv_label_set_text(power_labels[2], ext_sym);
        lv_label_set_text_fmt(power_labels[3], "%.1fv", v_ext);
    }

    pthread_mutex_unlock(&power_mux);
}

lv_obj_t * clock_init(lv_obj_t * parent) {
    pthread_mutex_init(&power_mux, NULL);

    obj = lv_obj_create(parent);

    lv_obj_add_style(obj, &clock_style, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *row = lv_obj_create(obj);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);

    time_label = lv_label_create(row);
    lv_obj_set_style_text_font(time_label, &sony_32, 0);

    sec_label = lv_label_create(row);
    lv_obj_set_style_text_font(sec_label, &sony_20, 0);
    lv_obj_set_style_pad_bottom(sec_label, 2, 0);

    /* Voltages: symbols in Montserrat, numbers in Sony */
    row = lv_obj_create(obj);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 4, 0);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -3);

    for (int i = 0; i < 4; i++) {
        power_labels[i] = lv_label_create(row);
        lv_obj_set_style_text_font(power_labels[i], i % 2 ? &sony_16 : &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(power_labels[i], lv_color_hex(0xBBBBBB), 0);
    }
    lv_obj_set_style_pad_left(power_labels[2], 8, 0);

    show_time();
    lv_timer_create(show_time, 500, NULL);

    last_time_sync = get_last_sync_time();
    timer_time_sync = lv_timer_create(check_time_sync_cb, 200, NULL);
    return obj;
}

void clock_update_power(float ext, float bat, uint8_t cap, bool charge_flag) {
    pthread_mutex_lock(&power_mux);
    v_ext = ext;
    v_bat = bat;
    cap_bat = cap;
    charging = charge_flag;
    pthread_mutex_unlock(&power_mux);
}

void clock_set_view(clock_view_t x) {
    params_lock();
    params.clock_view = x;
    params_unlock(&params.dirty.clock_view);
}

void clock_set_time_timeout(uint8_t sec) {
    params_lock();
    params.clock_time_timeout = sec;
    params_unlock(&params.dirty.clock_time_timeout);
}

void clock_set_power_timeout(uint8_t sec) {
    params_lock();
    params.clock_power_timeout = sec;
    params_unlock(&params.dirty.clock_power_timeout);
}

void clock_set_tx_timeout(uint8_t sec) {
    params_lock();
    params.clock_tx_timeout = sec;
    params_unlock(&params.dirty.clock_tx_timeout);
}

static void check_time_sync_cb(lv_timer_t * t) {
    time_t new_time = get_last_sync_time();
    if (last_time_sync != new_time) {
        last_time_sync = new_time;
        msg_schedule_text_fmt("Time is synchronized with NTP");
    }
}

static time_t get_last_sync_time() {
    struct stat attr;
    if (stat("/var/run/time-sync", &attr) == 0) {
        return attr.st_atime;
    }
    return 0;
}
