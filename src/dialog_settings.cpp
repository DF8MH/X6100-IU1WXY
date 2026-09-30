/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "dialog_settings.h"

#include "dsp.h"
#include "wifi.h"
#include "btaudio.h"
#include "cfg/settings_manager.h"
#include <vector>
#include <thread>
#include <string>
#include <cstdio>
#include <functional>

extern "C" {

    #include "dialog.h"
    #include "styles.h"
    #include "params/params.h"
    #include "backlight.h"
    #include "radio.h"
    #include "events.h"
    #include "keyboard.h"
    #include "clock.h"
    #include "audio.h"
    #include "meter.h"
    #include "main.h"
    #include "msg.h"
    #include "textarea_window.h"
    #include "station.h"

    #include "lvgl/lvgl.h"
    #include <sys/time.h>
    #include <time.h>
    #include <fcntl.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <linux/rtc.h>
    #include <errno.h>

}

#define SMALL_PAD   5

#define SMALL_1     57
#define SMALL_2     (SMALL_1 * 2 + SMALL_PAD * 1)
#define SMALL_3     (SMALL_1 * 3 + SMALL_PAD * 2)
#define SMALL_4     (SMALL_1 * 4 + SMALL_PAD * 3)
#define SMALL_5     (SMALL_1 * 5 + SMALL_PAD * 4)
#define SMALL_6     (SMALL_1 * 6 + SMALL_PAD * 5)

#define SMALL_WIDTH 57

static void make_general_page();
static void make_display_page();
static void make_audio_page();
static void make_wireless_page();
static void make_info_page();

static void load_general_page(button_data_t *btn_data);
static void load_display_page(button_data_t *btn_data);
static void load_audio_page(button_data_t *btn_data);
static void load_wireless_page(button_data_t *btn_data);
static void load_info_page(button_data_t *btn_data);

static void construct_cb(lv_obj_t *parent);
static void destruct_cb();
static void key_cb(lv_event_t * e);

static lv_obj_t     *grid;
static lv_obj_t     *page;      /* scrolling column of cards */
static lv_obj_t     *band;      /* row of two half cards, or NULL */
static lv_coord_t   col_dsc[] = { LV_GRID_FR(1), SMALL_1, SMALL_1, SMALL_1, SMALL_1, SMALL_1, SMALL_1, LV_GRID_TEMPLATE_LAST };
static lv_coord_t   col_station[] = { LV_GRID_FR(1), 150, LV_GRID_TEMPLATE_LAST };
static lv_coord_t   col_datetime[] = { LV_GRID_FR(1), 78, 78, 100, LV_GRID_TEMPLATE_LAST };
static lv_coord_t   row_pool[8][64];    /* row descriptors: one per card of the page */
static lv_coord_t   *row_dsc = row_pool[0];
static int          card_n;

static bool         datetime_changed;
static time_t       now;
struct tm           ts;

static lv_obj_t     *day;
static lv_obj_t     *month;
static lv_obj_t     *year;
static lv_obj_t     *hour;
static lv_obj_t     *min;
static lv_obj_t     *sec;

static std::vector<Subscription> observers;

static button_data_t btn_p1 = {
    .type  = BTN_TEXT,
    .label = "(SETTINGS\n1:2)",
    .press = button_next_page_cb,
};

static button_data_t btn_p2 = {
    .type  = BTN_TEXT,
    .label = "(SETTINGS\n2:2)",
    .press = button_next_page_cb,
};

static button_data_t btn_general = {
    .type  = BTN_TEXT,
    .label = "General",
    .press = load_general_page,
};

static button_data_t btn_display = {
    .type  = BTN_TEXT,
    .label = "Display",
    .press = load_display_page,
};

static button_data_t btn_audio = {
    .type  = BTN_TEXT,
    .label = "Audio",
    .press = load_audio_page,
};

static button_data_t btn_wireless = {
    .type  = BTN_TEXT,
    .label = "Wireless",
    .press = load_wireless_page,
};

static button_data_t btn_info = {
    .type  = BTN_TEXT,
    .label = "Info",
    .press = load_info_page,
};

buttons_page_t btn_page = {
    {
     &btn_p1,
     &btn_general,
     &btn_display,
     &btn_audio,
     &btn_wireless,
     }
};

static buttons_page_t btn_page_2 = {
    {
     &btn_p2,
     &btn_info,
     }
};

static dialog_t     dialog = {
    .construct_cb = construct_cb,
    .destruct_cb = destruct_cb,
    .audio_cb = NULL,
    .btn_page = &btn_page,
    .key_cb = key_cb,
    .run = false,
};

dialog_t            *dialog_settings = &dialog;

static void mark_tab(button_data_t *btn_data) {
    for (buttons_page_t *page : {&btn_page, &btn_page_2}) {
        for (auto &&btn : page->items) {
            buttons_mark(btn, false);
        }
    }
    buttons_mark(btn_data, true);
}

static void load_general_page(button_data_t *btn_data) {
    make_general_page();
    mark_tab(btn_data);
}

static void load_display_page(button_data_t *btn_data) {
    make_display_page();
    mark_tab(btn_data);
}

static void load_audio_page(button_data_t *btn_data) {
    make_audio_page();
    mark_tab(btn_data);
}

static void load_wireless_page(button_data_t *btn_data) {
    make_wireless_page();
    mark_tab(btn_data);
}

static void load_info_page(button_data_t *btn_data) {
    make_info_page();
    mark_tab(btn_data);
}

/* Shared update */

static void bool_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    params_bool_t   *var = (params_bool_t*)lv_event_get_user_data(e);

    params_bool_set(var, lv_obj_has_state(obj, LV_STATE_CHECKED));
}

static void bool_update_subj_cb(lv_event_t *e) {
    lv_obj_t *obj  = lv_event_get_target(e);
    ParamInt *param = (ParamInt *)lv_event_get_user_data(e);

    param->set(lv_obj_has_state(obj, LV_STATE_CHECKED));
}

static void uint8_spinbox_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    params_uint8_t  *var = (params_uint8_t*)lv_event_get_user_data(e);

    params_uint8_set(var, lv_spinbox_get_value(obj));
    void(*update_cb)(void) = (void(*)(void))lv_obj_get_user_data(obj);
    if (update_cb) {
        update_cb();
    }
}

static void uint8_dropdown_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    params_uint8_t  *var = (params_uint8_t*)lv_event_get_user_data(e);

    params_uint8_set(var, lv_dropdown_get_selected(obj));
}

static void theme_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    params_uint8_t  *var = (params_uint8_t*)lv_event_get_user_data(e);

    params_uint8_set(var, lv_dropdown_get_selected(obj));
    styles_set_theme((themes_t)var->x);
}

/* Shared create */
static lv_obj_t * create_switch(lv_obj_t *parent){
    lv_obj_t *obj = lv_switch_create(parent);
    dialog_item(&dialog, obj);
    lv_obj_center(obj);
    return obj;
}

static lv_obj_t * switch_bool(lv_obj_t *parent, params_bool_t *var) {
    lv_obj_t *obj = create_switch(parent);
    lv_obj_add_event_cb(obj, bool_update_cb, LV_EVENT_VALUE_CHANGED, var);
    if (var->x) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    }
    return obj;
}

static lv_obj_t * switch_bool(lv_obj_t *parent, ParamInt &param) {
    lv_obj_t *obj = create_switch(parent);
    lv_obj_add_event_cb(obj, bool_update_subj_cb, LV_EVENT_VALUE_CHANGED, &param);

    if (param.get()) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    }
    return obj;
}

static lv_obj_t * spinbox_uint8(lv_obj_t *parent, params_uint8_t *var, void(*update_cb)(void)=nullptr) {
    lv_obj_t *obj = lv_spinbox_create(parent);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, var->x);
    lv_spinbox_set_range(obj, var->min, var->max);

    if (update_cb) {
        lv_obj_set_user_data(obj, (void*)update_cb);
    }

    lv_obj_add_event_cb(obj, uint8_spinbox_update_cb, LV_EVENT_VALUE_CHANGED, var);

    return obj;
}

static lv_obj_t * dropdown_uint8(lv_obj_t *parent, params_uint8_t *var, const char *options, lv_event_cb_t cb=uint8_dropdown_update_cb) {
    lv_obj_t *obj = lv_dropdown_create(parent);

    dialog_item(&dialog, obj);

    lv_obj_add_event_cb(obj, cb, LV_EVENT_VALUE_CHANGED, var);

    lv_obj_t *list = lv_dropdown_get_list(obj);
    lv_obj_add_style(list, &dialog_dropdown_list_style, 0);

    lv_dropdown_set_options(obj, options);
    lv_dropdown_set_symbol(obj, NULL);

    lv_dropdown_set_selected(obj, var->x);

    return obj;
}

template <typename T>
lv_obj_t *slider_with_text(lv_obj_t *cell, T val, T min, T max, T step, size_t width, const char *fmt,
                           lv_event_cb_t event_cb, void *cb_user_data = nullptr) {
    lv_obj_t *obj;
    obj = lv_slider_create(cell);

    dialog_item(&dialog, obj);

    lv_slider_set_mode(obj, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_range(obj, min / step, max / step);
    lv_slider_set_value(obj, val / step, LV_ANIM_OFF);
    lv_obj_set_width(obj, width);

    /*Create a label below the slider*/
    lv_obj_t *slider_label = lv_label_create(cell);
    lv_obj_set_user_data(slider_label, (void *)fmt);
    lv_label_set_text_fmt(slider_label, fmt, val);
    lv_obj_align(slider_label, LV_ALIGN_RIGHT_MID, 12, 0);
    lv_obj_set_style_text_color(slider_label, lv_color_white(), 0);

    lv_obj_set_user_data(obj, slider_label);

    lv_obj_add_event_cb(obj, event_cb, LV_EVENT_VALUE_CHANGED, cb_user_data);
    return obj;
}

/**
 * Update bg opacity for some UI elements to preview changes
 */
static void change_bg_opa_cb(lv_event_t *e) {
    lv_obj_t       *obj  = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    lv_anim_t bg_fade_anim;
    lv_anim_init(&bg_fade_anim);
    lv_anim_set_exec_cb(&bg_fade_anim,
                        [](void *var, int32_t value) { lv_obj_set_style_bg_img_opa((lv_obj_t *)var, value, 0); });
    lv_anim_set_var(&bg_fade_anim, dialog.obj);
    lv_anim_set_time(&bg_fade_anim, 500);
    lv_opa_t cur_opa = lv_obj_get_style_bg_img_opa(dialog.obj, 0);

    if (((lv_obj_check_type(obj, &lv_slider_class) || lv_obj_check_type(obj, &lv_spinbox_class)) &&
         lv_obj_has_state(obj, LV_STATE_EDITED)) ||
        (lv_obj_check_type(obj, &lv_switch_class) && (code == LV_EVENT_FOCUSED))) {
        if (cur_opa != LV_OPA_60) {
            lv_anim_set_values(&bg_fade_anim, cur_opa, LV_OPA_60);
            lv_anim_start(&bg_fade_anim);
        }
    } else {
        if (cur_opa != LV_OPA_COVER) {
            lv_anim_set_values(&bg_fade_anim, cur_opa, LV_OPA_COVER);
            lv_anim_start(&bg_fade_anim);
        }
    }
}

/* Datetime */

static void datetime_update_cb(lv_event_t * e) {
    ts.tm_mday = lv_spinbox_get_value(day);
    ts.tm_mon = lv_spinbox_get_value(month) - 1;
    ts.tm_year = lv_spinbox_get_value(year) - 1900;
    ts.tm_hour = lv_spinbox_get_value(hour);
    ts.tm_min = lv_spinbox_get_value(min);
    ts.tm_sec = lv_spinbox_get_value(sec);

    datetime_changed = true;

    /* Set system */

    struct timespec tp;

    tp.tv_sec = mktime(&ts);
    tp.tv_nsec = 0;

    int res = clock_settime(CLOCK_REALTIME, &tp);
    if (res != 0)
    {
        LV_LOG_ERROR("Can't set system time: %s\n", strerror(errno));
        return;
    }

}

static void datetime_set_rtc_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    if (!lv_obj_has_state(obj, LV_STATE_EDITED) && datetime_changed)
    {
        /* Set RTC */

        int rtc = open("/dev/rtc1", O_WRONLY);

        if (rtc > 0) {
            int res = ioctl(rtc, RTC_SET_TIME, &ts);
            if (res != 0)
            {
                LV_LOG_ERROR("Can't set RTC time: %s\n", strerror(errno));
                return;
            }
            close(rtc);
        } else {
            LV_LOG_ERROR("Can't open /dev/rtc1: %s\n", strerror(errno));
        }
        datetime_changed = false;
    }

}

static uint8_t make_date(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    /* Label */

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Date");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* Day */

    obj = lv_spinbox_create(grid);
    day = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_mday);
    lv_spinbox_set_range(obj, 1, 31);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, 78, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 1, LV_GRID_ALIGN_CENTER, row, 1);   col++;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    /* Month */

    obj = lv_spinbox_create(grid);
    month = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_mon + 1);
    lv_spinbox_set_range(obj, 1, 12);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, 78, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 1, LV_GRID_ALIGN_CENTER, row, 1);   col++;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    /* Year */

    obj = lv_spinbox_create(grid);
    year = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_year + 1900);
    lv_spinbox_set_range(obj, 2020, 2038);
    lv_spinbox_set_digit_format(obj, 4, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, 100, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 1, LV_GRID_ALIGN_CENTER, row, 1);   col++;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    return row + 1;
}

static uint8_t make_time(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    /* Label */

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Time");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* Hour */

    obj = lv_spinbox_create(grid);
    hour = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_hour);
    lv_spinbox_set_range(obj, 0, 23);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, 78, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 1, LV_GRID_ALIGN_CENTER, row, 1);   col++;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    /* Min */

    obj = lv_spinbox_create(grid);
    min = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_min);
    lv_spinbox_set_range(obj, 0, 59);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, 78, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 1, LV_GRID_ALIGN_CENTER, row, 1);   col++;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    /* Sec */

    obj = lv_spinbox_create(grid);
    sec = obj;

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, ts.tm_sec);
    lv_spinbox_set_range(obj, 0, 59);
    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, 78, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 1, LV_GRID_ALIGN_CENTER, row, 1);   col++;
    lv_obj_add_event_cb(obj, datetime_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, datetime_set_rtc_cb, LV_EVENT_FOCUSED, NULL);

    return row + 1;
}

/* Backlight */

static void backlight_timeout_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    params_lock();
    params.brightness_timeout = lv_spinbox_get_value(obj);
    params_unlock(&params.dirty.brightness_timeout);

    backlight_tick();
}

static void backlight_brightness_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    params_lock();
    params.brightness_normal = lv_slider_get_value(obj);
    params_unlock(&params.dirty.brightness_normal);

    params_lock();
    params.brightness_idle = lv_slider_get_left_value(obj);
    params_unlock(&params.dirty.brightness_idle);

    backlight_set_brightness(params.brightness_normal);
}

static void backlight_buttons_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    backlight_set_buttons((buttons_light_t)lv_dropdown_get_selected(obj));
}

static uint8_t make_backlight(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    /* Label */

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Timeout, Brightness");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* Timeout */

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, params.brightness_timeout);
    lv_spinbox_set_range(obj, 5, 120);
    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 2, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, backlight_timeout_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Brightness */

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_4, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 4, LV_GRID_ALIGN_CENTER, row, 1);   col += 4;
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = lv_slider_create(obj);

    dialog_item(&dialog, obj);

    lv_slider_set_mode(obj, LV_SLIDER_MODE_RANGE);
    lv_slider_set_value(obj, params.brightness_normal, LV_ANIM_OFF);
    lv_slider_set_left_value(obj, params.brightness_idle, LV_ANIM_OFF);
    lv_slider_set_range(obj, -1, 9);
    lv_obj_set_width(obj, SMALL_4 - 30);
    lv_obj_center(obj);

    lv_obj_add_event_cb(obj, backlight_brightness_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    row++;
    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Buttons brightness");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_dropdown_create(grid);

    dialog_item(&dialog, obj);

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    lv_obj_t *list = lv_dropdown_get_list(obj);
    lv_obj_add_style(list, &dialog_dropdown_list_style, 0);

    lv_dropdown_set_options(obj, " Always Off \n Always On \n Temporarily On ");
    lv_dropdown_set_symbol(obj, NULL);
    lv_dropdown_set_selected(obj, params.brightness_buttons);
    lv_obj_add_event_cb(obj, backlight_buttons_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return row + 1;
}

/* Line-in, Line-out */

static void line_in_out_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    void (*fn)(uint8_t) = (void (*)(uint8_t))lv_event_get_user_data(e);
    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    int32_t val = lv_slider_get_value(obj);
    fn(val);
    lv_label_set_text_fmt(slider_label, fmt, val);
}

static uint8_t make_line_gain(uint8_t row) {
    lv_obj_t    *obj;
    lv_obj_t    *cell;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "Line-in, Line-out");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, (int)params.line_in, 0, 36, 1, SMALL_3 - 30 - 60, "%d", line_in_out_update_cb, (void*)radio_set_line_in);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, (int)params.line_out, 0, 36, 1, SMALL_3 - 30 - 60, "%d", line_in_out_update_cb, (void*)radio_set_line_out);

    return row + 1;
}

/* Mag Freq, Info, ALC */

static uint8_t make_mag(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Mag Freq, Info, ALC");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* Freq */

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.mag_freq);

    lv_obj_set_width(obj, SMALL_2 - 30);

    /* Info */

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.mag_info);

    lv_obj_set_width(obj, SMALL_2 - 30);

    /* ALC */

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 3, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.mag_alc);

    lv_obj_set_width(obj, SMALL_2 - 30);

    return row + 1;
}

/* Long press actions */

typedef struct {
    const char                *label;
    press_action_t      action;
} action_items_t;

static action_items_t long_action_items[] = {
    { .label = " None ", .action = ACTION_NONE },
    { .label = " Screenshot ", .action = ACTION_SCREENSHOT },
    { .label = " Recorder on/off ", .action = ACTION_RECORDER },
    { .label = " Mute ", .action = ACTION_MUTE },
    { .label = " APP RTTY ", .action = ACTION_APP_RTTY },
    { .label = " APP FT8 ", .action = ACTION_APP_FT8 },
    { .label = " APP JS8 ", .action = ACTION_APP_JS8 },
    { .label = " APP SSTV ", .action = ACTION_APP_SSTV },
    { .label = " APP Cluster ", .action = ACTION_APP_CLUSTER },
    { .label = " APP WeFax ", .action = ACTION_APP_WEFAX },
    { .label = " APP NavTex ", .action = ACTION_APP_NAVTEX },
    { .label = " APP SWR Scan ", .action = ACTION_APP_SWRSCAN },
    { .label = " APP GPS ", .action = ACTION_APP_GPS },
    { .label = " APP Settings", .action = ACTION_APP_SETTINGS },
    { .label = " APP Recorder", .action = ACTION_APP_RECORDER },
    { .label = " QTH Grid", .action = ACTION_APP_QTH },
    { .label = NULL, .action = ACTION_NONE }
};

static void long_action_update_cb(lv_event_t * e) {
    lv_obj_t    *obj = lv_event_get_target(e);
    uint32_t    *i = (uint32_t*)lv_event_get_user_data(e);
    uint8_t     val = long_action_items[lv_dropdown_get_selected(obj)].action;

    params_lock();

    switch (*i) {
        case 0:
            params.long_gen = val;
            params_unlock(&params.dirty.long_gen);
            break;

        case 1:
            params.long_app = val;
            params_unlock(&params.dirty.long_app);
            break;

        case 2:
            params.long_key = val;
            params_unlock(&params.dirty.long_key);
            break;

        case 3:
            params.long_msg = val;
            params_unlock(&params.dirty.long_msg);
            break;

        case 4:
            params.long_dfn = val;
            params_unlock(&params.dirty.long_dfn);
            break;

        case 5:
            params.long_dfl = val;
            params_unlock(&params.dirty.long_dfl);
            break;
    }
}

static uint8_t make_long_action(uint8_t row) {
    const char        *labels[] = {
        "GEN long press",
        "APP long press",
        "KEY long press",
        "MSG long press",
        "DFN long press",
        "DFL long press",
    };
    lv_obj_t    *obj;

    for (uint8_t i = 0; i < 6; i++) {

        obj = lv_label_create(grid);

        lv_label_set_text(obj, labels[i]);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

        obj = lv_dropdown_create(grid);

        dialog_item(&dialog, obj);

        lv_obj_set_size(obj, SMALL_6, 56);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, row, 1);
        lv_obj_center(obj);

        lv_obj_t *list = lv_dropdown_get_list(obj);
        lv_obj_add_style(list, &dialog_dropdown_list_style, 0);

        lv_dropdown_set_symbol(obj, NULL);

        uint8_t x;

        switch (i) {
            case 0: x = params.long_gen;    break;
            case 1: x = params.long_app;    break;
            case 2: x = params.long_key;    break;
            case 3: x = params.long_msg;    break;
            case 4: x = params.long_dfn;    break;
            case 5: x = params.long_dfl;    break;

            default:
                x = ACTION_NONE;
                break;
        }

        lv_dropdown_clear_options(obj);

        uint8_t n = 0;

        while (long_action_items[n].label) {
            lv_dropdown_add_option(obj, long_action_items[n].label, LV_DROPDOWN_POS_LAST);

            if (long_action_items[n].action == x) {
                lv_dropdown_set_selected(obj, n);
            }

            n++;
        }

        uint32_t *param = (uint32_t*)malloc(sizeof(uint32_t));
        *param = i;

        lv_obj_add_event_cb(obj, long_action_update_cb, LV_EVENT_VALUE_CHANGED, param);

        row++;
    }

    return row;
}

/* HMic F1, F2 actions */

static action_items_t hmic_action_items[] = {
    { .label = " None ", .action = ACTION_NONE },
    { .label = " Recorder on/off ", .action = ACTION_RECORDER },
    { .label = " Mute ", .action = ACTION_MUTE },
    { .label = " Step up ", .action = ACTION_STEP_UP },
    { .label = " Step down ", .action = ACTION_STEP_DOWN },
    { .label = " NR toggle ", .action = ACTION_NR_TOGGLE },
    { .label = " NB toggle ", .action = ACTION_NB_TOGGLE },
    { .label = NULL, .action = ACTION_NONE }
};

static void hmic_action_update_cb(lv_event_t * e) {
    lv_obj_t    *obj = lv_event_get_target(e);
    uint8_t     *i = (uint8_t*)lv_event_get_user_data(e);
    uint8_t     val = hmic_action_items[lv_dropdown_get_selected(obj)].action;

    params_lock();

    switch (*i) {
        case 0:
            params.press_f1 = val;
            params_unlock(&params.dirty.press_f1);
            break;

        case 1:
            params.press_f2 = val;
            params_unlock(&params.dirty.press_f2);
            break;

        case 2:
            params.long_f1 = val;
            params_unlock(&params.dirty.long_f1);
            break;

        case 3:
            params.long_f2 = val;
            params_unlock(&params.dirty.long_f2);
            break;
    }
}

static uint8_t make_hmic_action(uint8_t row) {
    struct {const char *label; uint8_t param;} items[] = {
        {"HMic F1 press", params.press_f1},
        {"HMic F2 press", params.press_f2},
        {"HMic F1 long press", params.long_f1},
        {"HMic F2 long press", params.long_f2}
    };
    size_t items_len = sizeof(items) / sizeof(items[0]);
    lv_obj_t    *obj;

    for (uint8_t i = 0; i < items_len; i++) {

        obj = lv_label_create(grid);

        lv_label_set_text(obj, items[i].label);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

        obj = lv_dropdown_create(grid);

        dialog_item(&dialog, obj);

        lv_obj_set_size(obj, SMALL_6, 56);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, row, 1);
        lv_obj_center(obj);

        lv_obj_t *list = lv_dropdown_get_list(obj);
        lv_obj_add_style(list, &dialog_dropdown_list_style, 0);

        lv_dropdown_set_symbol(obj, NULL);

        lv_dropdown_clear_options(obj);

        uint8_t n = 0;

        while (hmic_action_items[n].label) {
            lv_dropdown_add_option(obj, hmic_action_items[n].label, LV_DROPDOWN_POS_LAST);

            if (hmic_action_items[n].action == items[i].param) {
                lv_dropdown_set_selected(obj, n);
            }

            n++;
        }

        uint8_t *param = (uint8_t*)malloc(sizeof(uint8_t));
        *param = i;

        lv_obj_add_event_cb(obj, hmic_action_update_cb, LV_EVENT_VALUE_CHANGED, param);

        row++;
    }

    return row;
}

/* Play,Rec gain */

static void play_gain_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    float val = audio_set_play_vol(lv_slider_get_value(obj));

    params_float_set(&params.play_gain_db_f, val);

    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    radio_set_line_in(val);
    lv_label_set_text_fmt(slider_label, fmt, val);
}

static void rec_gain_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    float val = audio_set_rec_vol(lv_slider_get_value(obj));

    params_float_set(&params.rec_gain_db_f, val);
    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    radio_set_line_in(val);
    lv_label_set_text_fmt(slider_label, fmt, val);
}

static uint8_t make_audio_gain(uint8_t row) {
    lv_obj_t    *obj;
    lv_obj_t    *cell;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "Play,Rec gain");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, params.play_gain_db_f.x, -10.0f, 10.0f, 1.0f, SMALL_3 - 30 - 65, "%.1f", play_gain_update_cb);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, params.rec_gain_db_f.x, -10.0f, 10.0f, 1.0f, SMALL_3 - 30 - 65, "%.1f", rec_gain_update_cb);

    return row + 1;
}

/* Transverter */

static void transverter_update_cb(lv_event_t * e) {
    lv_obj_t        *obj = lv_event_get_target(e);
    ParamInt        *param = (ParamInt*)lv_event_get_user_data(e);

    param->set(lv_spinbox_get_value(obj) * 1000000L);
}

static uint8_t make_transverter(uint8_t row, ParamInt &from, ParamInt &to, ParamInt &shift) {
    lv_obj_t        *obj;
    uint8_t         col = 0;

    /* Label */

    obj = lv_label_create(grid);

    lv_label_set_text_fmt(obj, "Transverter %i", from.context_id() + 1);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* From */

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, from.get() / 1000000L);
    lv_spinbox_set_range(obj, 70, 500);
    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 2, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, transverter_update_cb, LV_EVENT_VALUE_CHANGED, &from);

    /* To */

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, to.get() / 1000000L);
    lv_spinbox_set_range(obj, 70, 500);
    lv_spinbox_set_digit_format(obj, 3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 2, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, transverter_update_cb, LV_EVENT_VALUE_CHANGED, &to);

    /* Shift */

    obj = lv_spinbox_create(grid);

    dialog_item(&dialog, obj);

    lv_spinbox_set_value(obj, shift.get() / 1000000L);
    lv_spinbox_set_range(obj, 42, 500);
    lv_spinbox_set_digit_format(obj,3, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 2, LV_GRID_ALIGN_CENTER, row, 1);   col += 2;
    lv_obj_add_event_cb(obj, transverter_update_cb, LV_EVENT_VALUE_CHANGED, &shift);

    return row + 1;
}

/* Spectrum and waterfall auto */

#define AUTO_LEVEL_STEP 0.2f

/* Spectrum above the waterfall on/off; the waterfall takes its place */

static uint8_t make_spectrum_show(uint8_t row) {
    lv_obj_t *obj;

    obj = lv_label_create(grid);
    lv_label_set_text(obj, "Spectrum (restart)");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);
    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);
    obj = switch_bool(obj, cfg_sm.p_spectrum_show);
    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static uint8_t make_cluster_spots_wf(uint8_t row) {
    lv_obj_t *obj;

    obj = lv_label_create(grid);
    lv_label_set_text(obj, "Cluster spots (restart)");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);
    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);
    obj = switch_bool(obj, cfg_sm.p_cluster_spots_wf);
    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static void auto_level_offset_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    float val = lv_slider_get_value(obj) * AUTO_LEVEL_STEP;

    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    lv_label_set_text_fmt(slider_label, fmt, val);
    cfg_sm.p_auto_level_offset.set(val);
}

static uint8_t make_auto_offset(uint8_t row) {
    lv_obj_t *obj;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Spectrum auto, offset");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    /* On/off */
    obj = lv_obj_create(grid);
    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);
    obj = switch_bool(obj, cfg_sm.p_auto_level_enabled);
    lv_obj_set_width(obj, SMALL_3 - 30);

    /* Offset */
    obj = lv_obj_create(grid);
    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);
    obj = slider_with_text(obj, cfg_sm.p_auto_level_offset.get(), -15.0f, 15.0f, AUTO_LEVEL_STEP,
                           SMALL_3 - 120, "%0.1f", auto_level_offset_update_cb);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_DEFOCUSED, NULL);

    return row + 1;
}

/* Spectrum  min, max (when no auto)*/

static void grid_min_max_update_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    int       val = lv_slider_get_value(obj);

    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char     *fmt          = (char *)lv_obj_get_user_data(slider_label);
    lv_label_set_text_fmt(slider_label, fmt, val);
    auto param = static_cast<ParamInt*>(lv_event_get_user_data(e));
    param->set(val);
}

uint8_t make_spectrum_min_max(uint8_t row) {
    lv_obj_t *obj;
    lv_obj_t *cell;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "Spectrum man min, max");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    obj = slider_with_text(cell, cfg_sm.p_band_grid_min.get(), S_MIN, S7, 1, SMALL_3 - 120, "%d",
                           grid_min_max_update_cb, (void*)&cfg_sm.p_band_grid_min);

    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_DEFOCUSED, NULL);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    obj = slider_with_text(cell, cfg_sm.p_band_grid_max.get(), S8, S9_40, 1, SMALL_3 - 120, "%d",
                           grid_min_max_update_cb, (void*)&cfg_sm.p_band_grid_max);

    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_DEFOCUSED, NULL);

    return row + 1;
}

/* Spectrum fill, peaks */

uint8_t make_spectrum_fill_peak(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Spectrum fill, peaks");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.spectrum_filled);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_set_width(obj, SMALL_3 - 30);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.spectrum_peak);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

/* Spectrum beta, peak hold, peak speed */

uint8_t make_spectrum_beta_peak_hold_speed(uint8_t row) {
    lv_obj_t *obj;
    uint8_t   col = 1;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Spec. beta, hold, speed");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = spinbox_uint8(grid, &params.spectrum_beta, []() { dsp_set_spectrum_beta(params.spectrum_beta.x * 0.01f); });
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 2, LV_GRID_ALIGN_CENTER, row, 1);
    col += 2;

    obj = spinbox_uint8(grid, &params.spectrum_peak_hold);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_DEFOCUSED, NULL);


    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 2, LV_GRID_ALIGN_CENTER, row, 1);
    col += 2;

    obj = spinbox_uint8(grid, &params.spectrum_peak_speed);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_spinbox_set_digit_format(obj, 2, 0);
    lv_spinbox_set_digit_step_direction(obj, LV_DIR_LEFT);
    lv_obj_set_size(obj, SMALL_2, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 2, LV_GRID_ALIGN_CENTER, row, 1);
    col += 2;

    return row + 1;
}

/* Waterfall center line and zoom */

static uint8_t make_waterfall_line_zoom(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Waterfall line, zoom");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.waterfall_center_line);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(obj, change_bg_opa_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_set_width(obj, SMALL_3 - 30);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.waterfall_zoom);

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static uint8_t make_waterfall_smooth_scroll(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Waterfall smooth scroll");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, &params.waterfall_smooth_scroll);

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static uint8_t make_knob_info(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Knob info");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, cfg_sm.p_knob_info);

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static void sp_mode_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);

    if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
        radio_change_spmode(-1);
    } else {
        radio_change_spmode(1);
    }
}

static uint8_t make_sp_mode(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Speaker mode");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = lv_switch_create(obj);

    dialog_item(&dialog, obj);

    lv_obj_center(obj);
    lv_obj_add_event_cb(obj, sp_mode_update_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (!params.spmode.x) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    }

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

/* Compressor threshold, makeup */
#define COMP_TH_MAKEUP_STEP 0.5f

static void comp_update_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    float val = lv_slider_get_value(obj) * COMP_TH_MAKEUP_STEP;

    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    lv_label_set_text_fmt(slider_label, fmt, val);

    ParamFloat *param = (ParamFloat *)lv_event_get_user_data(e);
    param->set(val);
}


static uint8_t make_comp_th_makeup(uint8_t row) {
    lv_obj_t    *obj;
    lv_obj_t    *cell;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "Comp threshold, makeup");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, cfg_sm.p_comp_threshold_offset.get(),
        -15.0f, 15.0f, COMP_TH_MAKEUP_STEP,
        SMALL_3 - 120, "%0.1f", comp_update_cb,
        (void *)&cfg_sm.p_comp_threshold_offset);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, cfg_sm.p_comp_makeup_offset.get(),
        -15.0f, 15.0f, COMP_TH_MAKEUP_STEP,
        SMALL_3 - 120, "%0.1f", comp_update_cb,
        (void *)&cfg_sm.p_comp_makeup_offset);

    return row + 1;
}

/* TX offset */
#define TX_OFFSET_SCALE 50

static void tx_offset_update_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(obj);

    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    lv_label_set_text_fmt(slider_label, fmt, val);

    ParamInt *param = (ParamInt *)lv_event_get_user_data(e);
    param->set(val * TX_OFFSET_SCALE);
}

static void on_tx_offset_change(Subject *subj, void *user_data) {
    lv_obj_t *slider = (lv_obj_t *)user_data;
    auto *subj_t = static_cast<SubjectInt *>(subj);
    lv_slider_set_value(slider, subj_t->get() / TX_OFFSET_SCALE, LV_ANIM_OFF);
    lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);
}

static uint8_t make_tx_offset(uint8_t row) {
    lv_obj_t    *obj;
    lv_obj_t    *cell;
    lv_obj_t    *slider;
    Observer    *observer;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "TX IQ offsets");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider = slider_with_text(cell, cfg_sm.p_band_tx_i_offset.get() / TX_OFFSET_SCALE,
        -10000 / TX_OFFSET_SCALE, 10000 / TX_OFFSET_SCALE, 1,
        SMALL_3 - 110, "%d", tx_offset_update_cb,
        (void *)&cfg_sm.p_band_tx_i_offset);

    observer = cfg_sm.p_band_tx_i_offset.subscribe(on_tx_offset_change, slider);
    observers.emplace_back(observer);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_3, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider = slider_with_text(cell, cfg_sm.p_band_tx_q_offset.get() / TX_OFFSET_SCALE,
        -10000 / TX_OFFSET_SCALE, 10000 / TX_OFFSET_SCALE, 1,
        SMALL_3 - 110, "%d", tx_offset_update_cb,
        (void *)&cfg_sm.p_band_tx_q_offset);

    observer = cfg_sm.p_band_tx_q_offset.subscribe(on_tx_offset_change, slider);
    observers.emplace_back(observer);

    return row + 1;
}

/* TX codec gain */
#define OUTPUT_GAIN_STEP 0.2f

static void codec_gain_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    float val = (float)lv_slider_get_value(obj) * OUTPUT_GAIN_STEP;

    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    lv_label_set_text_fmt(slider_label, fmt, val);
    cfg_sm.p_output_gain.set(val);
}

static uint8_t make_codec_gain(uint8_t row) {
    lv_obj_t    *obj;
    lv_obj_t    *cell;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "TX codec gain");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_6, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, cfg_sm.p_output_gain.get(),
        -25.0f, 25.0f, OUTPUT_GAIN_STEP,
        SMALL_6 - 120, "%0.1f", codec_gain_update_cb);

    return row + 1;
}

/* TX codec DAC gain */

static void band_out_gain_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    float val = (float)lv_slider_get_value(obj) * OUTPUT_GAIN_STEP;

    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    lv_label_set_text_fmt(slider_label, fmt, val);
    cfg_sm.p_band_dac_offset.set(val);
}

static void on_dac_gain_change(Subject *subj, void *user_data) {
    lv_obj_t *slider = (lv_obj_t*)user_data;
    lv_slider_set_value(slider, cfg_sm.p_band_dac_offset.get() / OUTPUT_GAIN_STEP, LV_ANIM_OFF);
    lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);
}

static uint8_t band_out_gain_correction(uint8_t row) {
    lv_obj_t    *obj;
    lv_obj_t    *cell;
    lv_obj_t    *slider;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "Band output gain corr");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_6, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider = slider_with_text(cell, cfg_sm.p_band_dac_offset.get(),
        -6.0f, 6.0f, OUTPUT_GAIN_STEP,
        SMALL_6 - 120, "%0.1f", band_out_gain_update_cb);

    Observer *observer = cfg_sm.p_band_dac_offset.subscribe(on_dac_gain_change, slider);
    observers.emplace_back(observer);

    return row + 1;
}


uint8_t make_fm_emphasis(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "FM pre/de-emphasis");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = switch_bool(obj, cfg_sm.p_fm_emphasis);

    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

/* TX filter */

#define TX_FILTER_STEP 10

static void tx_filter_low_high_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    uint32_t val = lv_slider_get_value(obj) * TX_FILTER_STEP;

    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    lv_label_set_text_fmt(slider_label, fmt, val);
    ParamInt *param = (ParamInt *)lv_event_get_user_data(e);
    param->set(val);
}

uint8_t make_tx_filter(uint8_t row) {
    lv_obj_t    *obj;
    lv_obj_t    *cell;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "TX filter low");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_6, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, cfg_sm.p_tx_filter_low.get(),
        50, 400, TX_FILTER_STEP,
        SMALL_6 - 150, "%d", tx_filter_low_high_update_cb, (void*)&cfg_sm.p_tx_filter_low);
    row++;

    cell = lv_label_create(grid);

    lv_label_set_text(cell, "TX filter high");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_6, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    slider_with_text(cell, cfg_sm.p_tx_filter_high.get(),
        2000, 4000, TX_FILTER_STEP,
        SMALL_6 - 150, "%d", tx_filter_low_high_update_cb, (void*)&cfg_sm.p_tx_filter_high);
    row++;

    return row;
}

/* CESSB */

#define CESSB_POWER_UP_STEP 0.1f

static void cessb_power_up_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    float val = lv_slider_get_value(obj) * CESSB_POWER_UP_STEP;

    lv_obj_t *slider_label = (lv_obj_t *)lv_obj_get_user_data(obj);
    char *fmt = (char *)lv_obj_get_user_data(slider_label);
    lv_label_set_text_fmt(slider_label, fmt, val);
    cfg_sm.p_cessb_power_up.set(val);
}

uint8_t make_cessb(uint8_t row) {
    lv_obj_t    *obj;
    lv_obj_t    *cell;


    /* Label */
    cell = lv_label_create(grid);

    lv_label_set_text(cell, "CESSB on, level");
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);


    /* on */
    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_2, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 1, 2, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    obj = switch_bool(cell, cfg_sm.p_cessb_on);
    lv_obj_set_width(obj, SMALL_2 - 30);


    /* level */
    cell = lv_obj_create(grid);

    lv_obj_set_size(cell, SMALL_4, 56);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_START, 3, 4, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(cell);

    obj = slider_with_text(cell, cfg_sm.p_cessb_power_up.get(), 0.0f, 5.0f, CESSB_POWER_UP_STEP,
                           SMALL_3 - 40, "%0.1f", cessb_power_up_update_cb);

    row++;

    return row;
}

/* Charger */

/* Bluetooth CAT (btcat) */

#define BTCAT_CTL "/usr/sbin/btcat-ctl"

static lv_obj_t *bt_visible_sw = NULL;

static bool btcat_read(const char *key, bool def) {
    const char *files[] = { "/etc/default/btcat", "/mnt/btcat.conf" };
    bool val = def;
    size_t klen = strlen(key);

    for (auto f : files) {
        FILE *fp = fopen(f, "r");
        if (!fp) continue;
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                const char *v = line + klen + 1;
                if (*v == '"') v++;
                val = strncmp(v, "yes", 3) == 0;
            }
        }
        fclose(fp);
    }
    return val;
}

static void btcat_run(const char *what, bool on) {
    std::string cmd = std::string(BTCAT_CTL " ") + what + (on ? " on" : " off") + " >/dev/null 2>&1";
    std::thread([cmd]() { system(cmd.c_str()); }).detach();
}

static void bt_power_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    bool on = lv_obj_has_state(obj, LV_STATE_CHECKED);

    btcat_run("bt", on);
    if (bt_visible_sw) {
        if (on) lv_obj_clear_state(bt_visible_sw, LV_STATE_DISABLED);
        else    lv_obj_add_state(bt_visible_sw, LV_STATE_DISABLED);
    }
}

static void bt_visible_update_cb(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target(e);
    btcat_run("discoverable", lv_obj_has_state(obj, LV_STATE_CHECKED));
}

static void bt_visible_delete_cb(lv_event_t * e) {
    bt_visible_sw = NULL;
}

static lv_obj_t * bt_switch_cell(uint8_t row, uint8_t col, bool state, lv_event_cb_t cb) {
    lv_obj_t *obj = lv_obj_create(grid);

    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);

    obj = create_switch(obj);
    lv_obj_set_width(obj, SMALL_3 - 30);
    if (state) lv_obj_add_state(obj, LV_STATE_CHECKED);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return obj;
}

static uint8_t make_bluetooth(uint8_t row) {
    lv_obj_t *obj;
    bool      power = btcat_read("BTCAT_BT", true);

    obj = lv_label_create(grid);
    lv_label_set_text(obj, "Bluetooth");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);
    bt_switch_cell(row, 1, power, bt_power_update_cb);
    row++;

    obj = lv_label_create(grid);
    lv_label_set_text(obj, "Discoverable (CAT)");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);
    bt_visible_sw = bt_switch_cell(row, 1, btcat_read("BTCAT_DISCOVERABLE", true), bt_visible_update_cb);
    lv_obj_add_event_cb(bt_visible_sw, bt_visible_delete_cb, LV_EVENT_DELETE, NULL);
    if (!power) lv_obj_add_state(bt_visible_sw, LV_STATE_DISABLED);

    return row + 1;
}

uint8_t make_charger(uint8_t row) {
    lv_obj_t *obj;
    uint8_t   col = 0;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Charger");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = dropdown_uint8(grid, &params.charger, " Off \n On \n Shadow", [](lv_event_t *e) {
        uint8_dropdown_update_cb(e);
        radio_set_charger(params.charger.x == RADIO_CHARGER_ON);
    });

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    return row + 1;
}

static uint8_t make_freq_accel(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Freq acceleration");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = dropdown_uint8(grid, &params.freq_accel, " None \n Lite \n Strong");

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    return row + 1;
}

static uint8_t make_theme(uint8_t row) {
    lv_obj_t    *obj;
    uint8_t     col = 0;

    obj = lv_label_create(grid);

    lv_label_set_text(obj, "Theme");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

    obj = dropdown_uint8(grid, &params.theme, " Simple \n Legacy", theme_update_cb);

    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_center(obj);

    return row + 1;
}

static uint8_t make_delimiter(uint8_t row) {
    row_dsc[row] = 10;

    return row + 1;
}

/* Wireless: WiFi and Bluetooth. The MFK walks the rows; a press on a
 * network or device opens its action menu. */

#define WL_ROW_H    40
#define WIFI_ROWS   8
#define BT_ROWS     8

typedef struct {
    lv_obj_t *obj;
    lv_obj_t *name;
    lv_obj_t *pill_1;
    lv_obj_t *pill_2;
    lv_obj_t *note;
    lv_obj_t *bars[4];
    lv_obj_t *lock;
} wl_row_t;

typedef struct {
    std::string           label;
    std::string           note;
    std::function<void()> fn;
} wl_item_t;

typedef struct {
    std::string mac;
    std::string name;
    bool        connected;
    bool        hp;
    bool        autoconn;
    bool        is_new;
} bt_dev_t;

static int                    wl_gen;       /* results for a deleted page are dropped */
static lv_timer_t            *wl_timer;
static int                    wl_ticks;
static bool                   wl_kbd;       /* password keyboard open */
static wifi_ap_info_t         wl_kbd_ap;

static lv_obj_t              *wl_menu;      /* action menu */
static lv_obj_t              *wl_menu_back; /* row focused again when it closes */
static std::vector<wl_item_t> wl_items;
static std::vector<lv_obj_t *> wl_item_rows;
static int                    wl_sel;

static struct {
    lv_obj_t      *status_pill;
    lv_obj_t      *status_ssid;
    lv_obj_t      *ip;
    lv_obj_t      *scan_btn;
    lv_obj_t      *scan_note;
    wl_row_t       rows[WIFI_ROWS];
    wifi_ap_info_t aps[WIFI_ROWS];
    bool           scanning;
    time_t         last_scan;
} wf;

static struct {
    lv_obj_t             *scan_btn;
    lv_obj_t             *scan_note;
    lv_obj_t             *rx;
    lv_obj_t             *vol;
    lv_obj_t             *mute;
    wl_row_t              rows[BT_ROWS];
    std::vector<bt_dev_t> devs;
    std::vector<bt_dev_t> found;    /* last "Scan headphones" */
    bool                  listing;
    bool                  scanning;
    bool                  busy;
    int                   vol_pending = -1;
    bool                  vol_busy;
} bt;

static lv_obj_t * wl_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, lv_coord_t x) {
    lv_obj_t *obj = lv_label_create(parent);

    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_align(obj, LV_ALIGN_LEFT_MID, x, 0);
    lv_label_set_text(obj, "");
    return obj;
}

static lv_obj_t * wl_pill_create(lv_obj_t *parent, lv_coord_t x) {
    lv_obj_t *obj = wl_label(parent, &sony_18, 0xffffff, x);

    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 10, 0);
    lv_obj_set_style_pad_hor(obj, 8, 0);
    lv_obj_set_style_pad_ver(obj, 2, 0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    return obj;
}

static void wl_pill(lv_obj_t *obj, const char *text, uint32_t bg, uint32_t fg) {
    if (!text) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(obj, text);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(fg), 0);
}

#define PILL_OK     "connected", 0x1f4a24, 0x88ff88
#define PILL_HP     "headphones", 0x4b3c14, 0xffd24a
#define PILL_CAT    "CAT", 0x3a2f5a, 0xccbbff
#define PILL_NEW    "new", 0x2f3d4a, 0xccddee

/* Cards: a rounded panel with a header (icon, title, divider) and its own
 * grid. `grid` is the card being filled, the option rows go from row 1. */

#define CARD_HEAD_H     36

static lv_coord_t *card_cols;

static void band_begin() {
    band = lv_obj_create(page);
    lv_obj_remove_style_all(band);
    lv_obj_set_size(band, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(band, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(band, 8, 0);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
}

static void band_end() {
    band = NULL;
}

static uint8_t card_begin(const char *icon, const char *title, lv_coord_t *cols = col_dsc) {
    lv_obj_t *obj;

    grid = lv_obj_create(band ? band : page);
    lv_obj_remove_style_all(grid);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    if (band) {
        lv_obj_set_size(grid, 10, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(grid, 1);
    } else {
        lv_obj_set_size(grid, lv_pct(100), LV_SIZE_CONTENT);
    }
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(grid, lv_color_white(), 0);
    lv_obj_set_style_bg_color(grid, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_10, 0);
    lv_obj_set_style_border_width(grid, 1, 0);
    lv_obj_set_style_border_color(grid, lv_color_hex(0x506070), 0);
    lv_obj_set_style_radius(grid, 10, 0);
    lv_obj_set_style_pad_hor(grid, 12, 0);
    lv_obj_set_style_pad_ver(grid, 6, 0);
    lv_obj_set_style_pad_column(grid, SMALL_PAD, 0);
    lv_obj_set_style_pad_row(grid, 5, 0);

    card_cols = cols;
    row_dsc = row_pool[card_n++];
    std::fill_n(row_dsc, 64, 55);
    row_dsc[0] = CARD_HEAD_H;

    int ncols = 0;
    while (cols[ncols] != LV_GRID_TEMPLATE_LAST) ncols++;

    obj = lv_obj_create(grid);
    lv_obj_remove_style_all(obj);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(obj, 10, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x506070), 0);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 0, ncols, LV_GRID_ALIGN_STRETCH, 0, 1);

    lv_obj_t *ico = lv_label_create(obj);
    lv_obj_set_style_text_font(ico, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(ico, lv_color_hex(0xffd24a), 0);
    lv_label_set_text(ico, icon);

    lv_obj_t *lbl = lv_label_create(obj);
    lv_obj_set_style_text_font(lbl, &sony_20, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x90f0d0), 0);
    lv_obj_set_style_text_letter_space(lbl, 2, 0);
    lv_label_set_text(lbl, title);
    return 1;
}

/* Rows go up to `row` (exclusive): indent the labels, put a lone control at
 * the right edge, draw a divider under each row but the last. */
static void card_end(uint8_t row) {
    uint8_t  ctl[64] = {};
    int      ncols = 0;
    uint32_t n = lv_obj_get_child_cnt(grid);

    while (card_cols[ncols] != LV_GRID_TEMPLATE_LAST) ncols++;
    row_dsc[row] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_grid_dsc_array(grid, card_cols, row_dsc);

    for (uint32_t i = 1; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(grid, i);

        if (lv_obj_get_style_grid_cell_column_pos(c, 0) >= 1) {
            ctl[lv_obj_get_style_grid_cell_row_pos(c, 0)]++;
        }
    }
    for (uint32_t i = 1; i < n; i++) {
        lv_obj_t *c    = lv_obj_get_child(grid, i);
        int       pos  = lv_obj_get_style_grid_cell_column_pos(c, 0);
        int       span = lv_obj_get_style_grid_cell_column_span(c, 0);
        int       r    = lv_obj_get_style_grid_cell_row_pos(c, 0);

        if (pos == 0 && lv_obj_check_type(c, &lv_label_class)) {
            lv_obj_set_style_pad_left(c, 14, 0);
        } else if (pos >= 1 && ctl[r] == 1 && pos + span < ncols) {
            lv_coord_t w = lv_obj_get_style_width(c, 0);

            if (LV_COORD_IS_PX(w) && w > span * (SMALL_1 + SMALL_PAD) - SMALL_PAD) {
                span = ncols - 1;   /* wider than its cells */
            }
            lv_obj_set_grid_cell(c, (lv_grid_align_t)lv_obj_get_style_grid_cell_x_align(c, 0), ncols - span, span,
                                 (lv_grid_align_t)lv_obj_get_style_grid_cell_y_align(c, 0), r, 1);
        }
    }
    for (uint8_t r = 1; r + 1 < row; r++) {
        if (row_dsc[r] == 10 || row_dsc[r] == LV_GRID_CONTENT || row_dsc[r + 1] == 10) {
            continue;
        }
        lv_obj_t *obj = lv_obj_create(grid);

        lv_obj_remove_style_all(obj);
        lv_obj_set_height(obj, 1);
        lv_obj_set_style_bg_color(obj, lv_color_hex(0x506070), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_50, 0);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 0, ncols, LV_GRID_ALIGN_END, r, 1);
    }
}

static void wl_row_label(uint8_t row, const char *text) {
    lv_obj_t *obj = lv_label_create(grid);

    lv_label_set_text(obj, text);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);
}

static lv_obj_t * wl_button(uint8_t row, const char *text, lv_event_cb_t cb) {
    lv_obj_t *obj = lv_btn_create(grid);

    lv_obj_remove_style_all(obj);
    dialog_item(&dialog, obj);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x8a8a8a), 0);
    lv_obj_set_size(obj, SMALL_3, 48);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(obj);
    lv_obj_set_style_text_font(label, &sony_26, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return obj;
}

/* Note to the right of a 3-column control */
static lv_obj_t * wl_note(uint8_t row) {
    lv_obj_t *obj = wl_label(grid, &sony_18, 0x99aabb, 0);

    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    return obj;
}

/* A list (all the columns) of hidden rows; the refresh fills and shows them */
static lv_obj_t * wl_list(uint8_t row) {
    lv_obj_t *obj = lv_obj_create(grid);

    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(obj, 2, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 0, 7, LV_GRID_ALIGN_START, row, 1);
    row_dsc[row] = LV_GRID_CONTENT;
    return obj;
}

static void wl_row_create(wl_row_t *r, lv_obj_t *list, lv_event_cb_t cb, int i) {
    lv_obj_t *obj = lv_obj_create(list);

    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, lv_pct(100), WL_ROW_H);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);   /* a MFK press is a click */
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_radius(obj, 4, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x5a4818), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(obj, lv_color_white(), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(obj, 2, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(obj, key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, obj);

    r->obj = obj;
    r->name = wl_label(obj, &sony_24, 0xffffff, 10);
    lv_obj_set_width(r->name, 270);
    lv_label_set_long_mode(r->name, LV_LABEL_LONG_DOT);
}

/* Hidden rows can't have the focus: move it before hiding the focused one */
static void wl_row_show(wl_row_t *r, bool show, lv_obj_t *fallback) {
    if (!show && lv_group_get_focused(keyboard_group) == r->obj) {
        lv_group_focus_obj(fallback);
    }
    if (show) {
        lv_obj_clear_flag(r->obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(r->obj, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Action menu */

static void wl_menu_show_sel() {
    for (size_t i = 0; i < wl_item_rows.size(); i++) {
        bool on = (int)i == wl_sel;

        lv_obj_set_style_bg_opa(wl_item_rows[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(wl_item_rows[i], on ? 2 : 0, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(wl_item_rows[i], 0), on ? lv_color_hex(0xffd24a) : lv_color_white(), 0);
    }
}

static void wl_menu_close() {
    if (!wl_menu) {
        return;
    }
    lv_obj_del(wl_menu);
    wl_menu = NULL;
    wl_item_rows.clear();
    if (wl_menu_back) {
        lv_group_focus_obj(wl_menu_back);
    }
    lv_group_set_editing(keyboard_group, false);
}

/* Run after the event: closing deletes the menu that got it */
static void wl_menu_pick_async(void *arg) {
    if (!wl_menu) {
        return;
    }
    int sel = (int)(intptr_t)arg;
    auto fn = sel >= 0 ? wl_items[sel].fn : nullptr;

    wl_menu_close();
    if (fn) {
        fn();
    }
}

static void wl_menu_key_cb(lv_event_t *e) {
    switch (*((uint32_t *)lv_event_get_param(e))) {
        case LV_KEY_LEFT:
        case LV_KEY_UP:
            if (wl_sel > 0) wl_sel--;
            wl_menu_show_sel();
            break;

        case LV_KEY_RIGHT:
        case LV_KEY_DOWN:
            if (wl_sel < (int)wl_items.size() - 1) wl_sel++;
            wl_menu_show_sel();
            break;

        case LV_KEY_ENTER:
            lv_async_call(wl_menu_pick_async, (void *)(intptr_t)wl_sel);
            break;

        case LV_KEY_ESC:
            lv_async_call(wl_menu_pick_async, (void *)(intptr_t)-1);
            break;

        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            break;

        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            break;
    }
}

static void wl_menu_click_cb(lv_event_t *e) {
    lv_obj_t   *target = lv_event_get_target(e);
    lv_indev_t *indev = lv_indev_get_act();

    /* On the menu itself only the MFK press counts, not a touch */
    if (target == wl_menu && !(indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER)) {
        return;
    }

    for (size_t i = 0; i < wl_item_rows.size(); i++) {
        if (wl_item_rows[i] == target) {
            wl_sel = i;
        }
    }
    lv_async_call(wl_menu_pick_async, (void *)(intptr_t)wl_sel);
}

static void wl_menu_open(const std::string &title, std::vector<wl_item_t> items) {
    const lv_coord_t w = 380, head_h = 36, item_h = 40, hint_h = 30;

    wl_menu_close();
    wl_items = std::move(items);
    wl_menu_back = lv_group_get_focused(keyboard_group);

    wl_menu = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(wl_menu);
    lv_obj_set_size(wl_menu, w, head_h + wl_items.size() * item_h + hint_h + 8);
    lv_obj_center(wl_menu);
    lv_obj_set_style_bg_color(wl_menu, lv_color_hex(0x0b1622), 0);
    lv_obj_set_style_bg_opa(wl_menu, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(wl_menu, lv_color_hex(0x99aaaa), 0);
    lv_obj_set_style_border_width(wl_menu, 2, 0);
    lv_obj_set_style_radius(wl_menu, 10, 0);
    lv_obj_clear_flag(wl_menu, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *obj = wl_label(wl_menu, &sony_18, 0x90f0d0, 16);
    lv_obj_set_width(obj, w - 32);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_label_set_text(obj, title.c_str());
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 16, 8);

    for (size_t i = 0; i < wl_items.size(); i++) {
        lv_obj_t *row = lv_obj_create(wl_menu);

        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, w - 4, item_h);
        lv_obj_set_pos(row, 0, head_h + i * item_h);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x5a4818), 0);
        lv_obj_set_style_border_color(row, lv_color_white(), 0);
        lv_obj_set_style_border_side(row, (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM), 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, wl_menu_click_cb, LV_EVENT_CLICKED, NULL);

        obj = wl_label(row, &sony_26, 0xffffff, 16);
        lv_label_set_text(obj, wl_items[i].label.c_str());

        obj = wl_label(row, &sony_18, 0x99aabb, 0);
        lv_label_set_text(obj, wl_items[i].note.c_str());
        lv_obj_align(obj, LV_ALIGN_RIGHT_MID, -16, 0);

        wl_item_rows.push_back(row);
    }

    obj = wl_label(wl_menu, &sony_16, 0x88aa99, 16);
    lv_label_set_text(obj, "MFK: choose - press: confirm - Wireless: close");
    lv_obj_align(obj, LV_ALIGN_BOTTOM_LEFT, 16, -8);

    wl_sel = 0;
    wl_menu_show_sel();

    lv_group_add_obj(keyboard_group, wl_menu);
    lv_obj_add_event_cb(wl_menu, wl_menu_key_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(wl_menu, wl_menu_click_cb, LV_EVENT_CLICKED, NULL);
    lv_group_focus_obj(wl_menu);
    lv_group_set_editing(keyboard_group, true);
}

/* WiFi */

static int wifi_compare_aps(const void *a, const void *b) {
    const wifi_ap_info_t *ap_a = (const wifi_ap_info_t *)a;
    const wifi_ap_info_t *ap_b = (const wifi_ap_info_t *)b;
    int score_a = ap_a->is_connected * 2 + ap_a->known;
    int score_b = ap_b->is_connected * 2 + ap_b->known;

    if (score_a != score_b) {
        return score_b - score_a;
    }
    return ap_b->strength - ap_a->strength;
}

static void wifi_update() {
    static const char *status_names[] = { "off", "starting", "disconnected", "connecting", "connected" };
    wifi_status_t status = wifi_get_status();
    bool          on = status != WIFI_OFF && status != WIFI_STARTING;
    const char   *ssid = "";
    int           n = 0;

    if (status == WIFI_CONNECTED) {
        wl_pill(wf.status_pill, PILL_OK);
    } else {
        wl_pill(wf.status_pill, status_names[status], 0x2f3d4a, 0xccddee);
    }

    if (on) {
        wifi_ap_arr_t aps = wifi_get_available_access_points();

        if (aps.count) {
            qsort(aps.ap_arr, aps.count, sizeof(wifi_ap_info_t), wifi_compare_aps);
        }
        for (; n < aps.count && n < WIFI_ROWS; n++) {
            wf.aps[n] = aps.ap_arr[n];
        }
        wifi_aps_info_delete(aps);
    }

    for (int i = 0; i < WIFI_ROWS; i++) {
        wl_row_t       *r = &wf.rows[i];
        wifi_ap_info_t *ap = &wf.aps[i];

        wl_row_show(r, i < n, wf.scan_btn);
        if (i >= n) {
            continue;
        }
        lv_label_set_text(r->name, ap->ssid);
        for (int b = 0; b < 4; b++) {
            bool lit = b < (ap->strength + 24) / 25;

            lv_obj_set_style_bg_color(r->bars[b], lv_color_hex(lit ? 0x88ff88 : 0x555555), 0);
        }
        if (ap->password_validator) {
            lv_obj_clear_flag(r->lock, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(r->lock, LV_OBJ_FLAG_HIDDEN);
        }
        if (ap->is_connected) {
            wl_pill(r->pill_1, PILL_OK);
            ssid = ap->ssid;
        } else {
            wl_pill(r->pill_1, NULL, 0, 0);
        }
        lv_label_set_text(r->note, ap->known ? "saved" : "");
    }
    lv_label_set_text(wf.status_ssid, ssid);

    char  ip[16] = "", gw[16] = "";
    char *ip_p = ip, *gw_p = gw;

    if (status == WIFI_CONNECTED && wifi_get_ipaddr(&ip_p, &gw_p)) {
        lv_label_set_text_fmt(wf.ip, "%s  /  %s", ip, gw);
    } else {
        lv_label_set_text(wf.ip, "-");
    }

    bool scanning = on && wifi_scanning();

    if (wf.scanning && !scanning) {
        wf.last_scan = time(NULL);
    }
    wf.scanning = scanning;
    if (scanning) {
        lv_label_set_text(wf.scan_note, "scanning...");
    } else if (on && wf.last_scan) {
        lv_label_set_text_fmt(wf.scan_note, "last scan %i s ago", (int)(time(NULL) - wf.last_scan));
    } else {
        lv_label_set_text(wf.scan_note, "");
    }
}

static void wifi_power_cb(lv_event_t *e) {
    if (lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED)) {
        wifi_power_on();
        msg_update_text_fmt("Turning on");
    } else {
        wifi_power_off();
    }
    wifi_update();
}

static void wifi_scan_cb(lv_event_t *e) {
    wifi_status_t status = wifi_get_status();

    if (status == WIFI_OFF || status == WIFI_STARTING || wifi_scanning()) {
        return;
    }
    wifi_start_scan();
}

static bool wl_kbd_close() {
    wl_kbd = false;
    textarea_window_close();
    make_wireless_page();
    return true;
}

static bool wl_kbd_ok() {
    const char *password = textarea_window_get();

    if (wl_kbd_ap.password_validator && !wl_kbd_ap.password_validator(password)) {
        msg_update_text_fmt("Incorrect password");
        return false;
    }
    if (wl_kbd_ap.known) {
        wifi_update_connection(wl_kbd_ap.ssid, password);
    } else {
        wifi_add_connection(wl_kbd_ap.ssid, password);
    }
    return wl_kbd_close();
}

/* The keyboard gets all the keys: the page is built again when it closes */
static void wl_kbd_open(const wifi_ap_info_t &ap) {
    wl_kbd_ap = ap;
    wl_kbd = true;
    lv_group_remove_all_objs(keyboard_group);
    textarea_window_open(wl_kbd_ok, wl_kbd_close);
    lv_textarea_set_placeholder_text(textarea_window_text(), " Password");
}

static void wifi_row_cb(lv_event_t *e) {
    wifi_ap_info_t         ap = wf.aps[(intptr_t)lv_event_get_user_data(e)];
    std::vector<wl_item_t> items;

    if (ap.is_connected) {
        items.push_back({"Disconnect", "", [] { wifi_disconnect(); }});
    } else {
        items.push_back({"Connect", "", [ap] {
            if (ap.known) {
                wifi_connect(ap.ssid);
            } else if (!ap.password_validator) {
                wifi_add_connection(ap.ssid, NULL);
            } else {
                wl_kbd_open(ap);
            }
        }});
    }
    if (ap.known && ap.password_validator) {
        items.push_back({"Change password", "", [ap] { wl_kbd_open(ap); }});
    }
    if (ap.known) {
        items.push_back({"Delete", "", [ap] {
            wifi_delete_connection(ap.ssid);
            msg_update_text_fmt("Connection deleted");
        }});
    }
    wl_menu_open(ap.ssid, items);
}

static uint8_t make_wifi(uint8_t row) {
    lv_obj_t *obj;

    wl_row_label(row, "WiFi/BT power");
    bt_switch_cell(row, 1, params.wifi_enabled.x, wifi_power_cb);
    row = make_delimiter(row + 1);

    wl_row_label(row, "Status");
    obj = lv_obj_create(grid);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(obj, 12, 0);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, row, 1);
    wf.status_pill = wl_pill_create(obj, 0);
    wf.status_ssid = wl_label(obj, &sony_26, 0xffffff, 0);
    row++;

    obj = wl_label(grid, &sony_22, 0x99aabb, 0);
    lv_label_set_text(obj, "IP / Gateway");
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);
    wf.ip = wl_label(grid, &sony_22, 0x99aabb, 0);
    lv_obj_set_grid_cell(wf.ip, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, row, 1);
    row_dsc[row] = 36;
    row = make_delimiter(row + 1);

    wl_row_label(row, "Networks");
    wf.scan_btn = wl_button(row, "Scan", wifi_scan_cb);
    wf.scan_note = wl_note(row);
    row++;

    lv_obj_t *list = wl_list(row);

    for (int i = 0; i < WIFI_ROWS; i++) {
        wl_row_t *r = &wf.rows[i];

        wl_row_create(r, list, wifi_row_cb, i);
        for (int b = 0; b < 4; b++) {
            obj = lv_obj_create(r->obj);
            lv_obj_remove_style_all(obj);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
            lv_obj_set_size(obj, 8, 6 + b * 5);
            lv_obj_align(obj, LV_ALIGN_BOTTOM_LEFT, 300 + b * 11, -9);
            r->bars[b] = obj;
        }
        /* padlock: shackle and body */
        r->lock = lv_obj_create(r->obj);
        lv_obj_remove_style_all(r->lock);
        lv_obj_set_size(r->lock, 14, 20);
        lv_obj_align(r->lock, LV_ALIGN_LEFT_MID, 380, 0);
        obj = lv_obj_create(r->lock);
        lv_obj_remove_style_all(obj);
        lv_obj_set_size(obj, 10, 12);
        lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_radius(obj, 5, 0);
        lv_obj_set_style_border_width(obj, 2, 0);
        lv_obj_set_style_border_color(obj, lv_color_hex(0xe0a030), 0);
        obj = lv_obj_create(r->lock);
        lv_obj_remove_style_all(obj);
        lv_obj_set_size(obj, 14, 11);
        lv_obj_align(obj, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_radius(obj, 2, 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(obj, lv_color_hex(0xe0a030), 0);

        r->pill_1 = wl_pill_create(r->obj, 440);
        r->note = wl_label(r->obj, &sony_20, 0x99aabb, 620);
    }
    return row + 1;
}

/* Bluetooth devices and headphones (btaudio-ctl) */

static std::vector<std::string> split(const std::string &s, char sep) {
    std::vector<std::string> out;
    size_t                   start = 0, pos;

    while ((pos = s.find(sep, start)) != std::string::npos) {
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    out.push_back(s.substr(start));
    return out;
}

static void bt_set_enabled(lv_obj_t *obj, bool on) {
    if (on) {
        lv_obj_clear_state(obj, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
}

static void bt_update_view() {
    size_t n = bt.devs.size();

    for (size_t i = 0; i < BT_ROWS; i++) {
        wl_row_t *r = &bt.rows[i];

        wl_row_show(r, i < n, bt.scan_btn);
        if (i >= n) {
            continue;
        }
        bt_dev_t *d = &bt.devs[i];

        lv_label_set_text(r->name, d->name.empty() ? d->mac.c_str() : d->name.c_str());
        if (d->is_new) {
            wl_pill(r->pill_1, PILL_NEW);
        } else if (d->hp) {
            wl_pill(r->pill_1, PILL_HP);
        } else {
            wl_pill(r->pill_1, PILL_CAT);
        }
        wl_pill(r->pill_2, d->connected ? "connected" : NULL, 0x1f4a24, 0x88ff88);
        if (d->is_new) {
            lv_label_set_text(r->note, "press to pair");
        } else if (!d->connected) {
            lv_label_set_text(r->note, "not connected");
        } else {
            lv_label_set_text(r->note, "");
        }
        lv_obj_align(r->note, LV_ALIGN_LEFT_MID, d->connected ? 580 : 430, 0);
    }
}

static void bt_refresh();

static void bt_list_done(const char *out, int status, void *user) {
    if ((intptr_t)user != wl_gen) {
        return;
    }
    bt.listing = false;
    bt.devs.clear();

    bool hp = false;

    for (auto &line : split(out, '\n')) {
        auto f = split(line, '\t');

        if (f[0] == "DEV" && f.size() >= 6) {
            bt.devs.push_back({f[1], f[5], f[2] == "1", f[3] == "hp", f[4] == "1", false});
        } else if (f[0] == "HP" && f.size() >= 3) {
            hp = f[1] == "1";
            int vol = atoi(f[2].c_str());

            if (vol >= 0 && bt.vol_pending < 0 && !bt.vol_busy && !lv_obj_has_state(bt.vol, LV_STATE_EDITED)) {
                lv_slider_set_value(bt.vol, vol, LV_ANIM_OFF);
                lv_label_set_text_fmt((lv_obj_t *)lv_obj_get_user_data(bt.vol), "%i %%", vol);
            }
        } else if (f[0] == "RX" && f.size() >= 2) {
            if (f[1] == "yes") {
                lv_obj_add_state(bt.rx, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(bt.rx, LV_STATE_CHECKED);
            }
        }
    }
    for (auto &d : bt.found) {
        bool paired = false;

        for (auto &p : bt.devs) {
            paired |= p.mac == d.mac;
        }
        if (!paired) {
            bt.devs.push_back(d);
        }
    }
    bt_set_enabled(bt.rx, hp);
    bt_set_enabled(bt.vol, hp);
    bt_set_enabled(bt.mute, hp);
    bt_update_view();
}

static void bt_refresh() {
    if (bt.listing || bt.scanning || bt.busy) {
        return;
    }
    bt.listing = true;
    btaudio_run("list", bt_list_done, (void *)(intptr_t)wl_gen);
}

static void bt_action_done(const char *out, int status, void *user) {
    if ((intptr_t)user != wl_gen) {
        return;
    }
    bt.busy = false;
    lv_label_set_text(bt.scan_note, "");
    if (status != 0 && *out) {
        msg_update_text_fmt("%s", split(out, '\n')[0].c_str());
    }
    bt_refresh();
}

static void bt_action(const std::string &args, const std::string &note) {
    if (bt.busy || bt.scanning) {
        return;
    }
    bt.busy = true;
    lv_label_set_text(bt.scan_note, note.c_str());
    btaudio_run(args.c_str(), bt_action_done, (void *)(intptr_t)wl_gen);
}

static void bt_scan_done(const char *out, int status, void *user) {
    if ((intptr_t)user != wl_gen) {
        return;
    }
    bt.scanning = false;
    bt.found.clear();
    for (auto &line : split(out, '\n')) {
        auto f = split(line, '\t');

        if (f[0] == "NEW" && f.size() >= 3) {
            bt.found.push_back({f[1], f[2], false, true, false, true});
        }
    }
    lv_label_set_text(bt.scan_note, bt.found.empty() ? "no headphones found" : "");
    bt_refresh();
}

static void bt_scan_cb(lv_event_t *e) {
    if (bt.busy || bt.scanning) {
        return;
    }
    bt.scanning = true;
    lv_label_set_text(bt.scan_note, "scanning 15 s, put the headphones in pairing");
    btaudio_run("scan", bt_scan_done, (void *)(intptr_t)wl_gen);
}

static void bt_row_cb(lv_event_t *e) {
    size_t i = (intptr_t)lv_event_get_user_data(e);

    if (i >= bt.devs.size()) {
        return;
    }
    bt_dev_t    d = bt.devs[i];
    std::string name = d.name.empty() ? d.mac : d.name;

    if (d.is_new) {
        bt_action("pair " + d.mac, "pairing " + name + "...");
        return;
    }

    std::vector<wl_item_t> items;

    if (d.connected) {
        items.push_back({"Disconnect", "", [d, name] { bt_action("disconnect " + d.mac, "disconnecting..."); }});
    } else if (d.hp) {
        items.push_back({"Connect", "", [d, name] { bt_action("connect " + d.mac, "connecting " + name + "..."); }});
    }
    if (d.hp) {
        items.push_back({"Connect at startup", d.autoconn ? "on" : "off",
                         [d] { bt_action("autoconnect " + d.mac + (d.autoconn ? " off" : " on"), ""); }});
    }
    items.push_back({"Remove", "", [d] { bt_action("remove " + d.mac, "removing..."); }});
    wl_menu_open(name + " - " + d.mac, items);
}

static void bt_rx_cb(lv_event_t *e) {
    bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);

    btaudio_run(on ? "rx on" : "rx off", NULL, NULL);
}

static void bt_vol_send();

static void bt_vol_done(const char *out, int status, void *user) {
    bt.vol_busy = false;
    bt_vol_send();
}

/* One volume command at a time, the last value wins */
static void bt_vol_send() {
    if (bt.vol_busy || bt.vol_pending < 0) {
        return;
    }
    std::string args = "volume " + std::to_string(bt.vol_pending);

    bt.vol_pending = -1;
    bt.vol_busy = true;
    btaudio_run(args.c_str(), bt_vol_done, NULL);
}

static void bt_vol_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    int       vol = lv_slider_get_value(obj);

    lv_label_set_text_fmt((lv_obj_t *)lv_obj_get_user_data(obj), "%i %%", vol);
    bt.vol_pending = vol;
    bt_vol_send();
}

static void bt_mute_cb(lv_event_t *e) {
    btaudio_mute_set(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}

static uint8_t make_bt_devices(uint8_t row) {
    lv_obj_t *obj;

    wl_row_label(row, "Devices");
    bt.scan_btn = wl_button(row, "Scan headphones", bt_scan_cb);
    lv_obj_set_width(bt.scan_btn, SMALL_4);
    lv_obj_set_grid_cell(bt.scan_btn, LV_GRID_ALIGN_START, 1, 4, LV_GRID_ALIGN_CENTER, row, 1);
    bt.scan_note = wl_note(row);
    lv_obj_set_grid_cell(bt.scan_note, LV_GRID_ALIGN_START, 5, 2, LV_GRID_ALIGN_CENTER, row, 1);
    lv_label_set_long_mode(bt.scan_note, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(bt.scan_note, SMALL_2);
    row++;

    lv_obj_t *list = wl_list(row);

    for (int i = 0; i < BT_ROWS; i++) {
        wl_row_t *r = &bt.rows[i];

        wl_row_create(r, list, bt_row_cb, i);
        r->pill_1 = wl_pill_create(r->obj, 300);
        r->pill_2 = wl_pill_create(r->obj, 430);
        r->note = wl_label(r->obj, &sony_20, 0x99aabb, 430);
    }
    return row + 1;
}

static uint8_t make_headphones(uint8_t row) {
    lv_obj_t *obj;

    wl_row_label(row, "RX to headphones");
    bt.rx = bt_switch_cell(row, 1, false, bt_rx_cb);
    row++;

    wl_row_label(row, "Headphone volume");
    obj = lv_obj_create(grid);
    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    bt.vol = slider_with_text(obj, 50, 0, 100, 1, SMALL_4, "%i %%", bt_vol_cb);
    lv_obj_align(bt.vol, LV_ALIGN_LEFT_MID, 10, 0);
    row++;

    wl_row_label(row, "Mute speaker");
    bt.mute = bt_switch_cell(row, 1, btaudio_mute_get(), bt_mute_cb);
    lv_label_set_text(wl_note(row), "AF 0 while connected");
    row++;

    bt_set_enabled(bt.rx, false);
    bt_set_enabled(bt.vol, false);
    bt_set_enabled(bt.mute, false);
    return row;
}

static void wl_timer_cb(lv_timer_t *t) {
    wifi_update();
    if (++wl_ticks % 5 == 0) {
        bt_refresh();
    }
}

static void wl_delete_cb(lv_event_t *e) {
    wl_gen++;
    lv_timer_del(wl_timer);
    wl_timer = NULL;
    wl_menu_back = NULL;
    wl_menu_close();
    if (wl_kbd) {
        wl_kbd = false;
        textarea_window_close();
    }
}

static void grid_create() {
    card_n = 0;
    band = NULL;
    page = lv_obj_create(dialog.obj);
    lv_obj_set_size(page, 780, DIALOG_H - 8);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_text_color(page, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(page, 6, 0);
    lv_obj_set_style_pad_row(page, 8, 0);
    lv_obj_set_style_pad_bottom(page, 40, 0);   /* room under the last row */

    lv_obj_center(page);
}

static void grid_delete() {
    if (page) {
        lv_group_set_editing(keyboard_group, false);
        // lv_group_remove_all_objs(keyboard_group);
        lv_obj_del(page);
        page = NULL;
        grid = NULL;
    }
}

/* Station: callsign and QTH locator, edited with the on-screen keyboard */

#define ST_CALLSIGN 1
#define ST_QTH      2

static int st_kbd;  /* keyboard open for ST_CALLSIGN / ST_QTH */

/* The keyboard gets all the keys: the page is built again when it closes */
static bool st_kbd_close() {
    st_kbd = 0;
    textarea_window_close();
    make_general_page();
    return true;
}

static bool st_kbd_ok() {
    const char *val = textarea_window_get();
    bool        ok  = st_kbd == ST_CALLSIGN ? station_callsign_save(val) : station_qth_save(val);

    return ok && st_kbd_close();
}

static void st_kbd_open_cb(lv_event_t *e) {
    st_kbd = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    lv_group_remove_all_objs(keyboard_group);
    textarea_window_open(st_kbd_ok, st_kbd_close);
    if (st_kbd == ST_CALLSIGN) {
        station_callsign_edit(textarea_window_text());
    } else {
        station_qth_edit(textarea_window_text());
    }
}

static void st_delete_cb(lv_event_t *e) {
    if (st_kbd) {
        st_kbd = 0;
        textarea_window_close();
    }
}

static uint8_t make_station_row(uint8_t row, const char *label, const char *val, int which) {
    wl_row_label(row, label);

    lv_obj_t *obj = wl_button(row, val[0] ? val : "-", st_kbd_open_cb);
    lv_obj_set_size(obj, 150, 48);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_user_data(obj, (void *)(intptr_t)which);
    return row + 1;
}

static uint8_t make_station(uint8_t row) {
    row = make_station_row(row, "Callsign", params.callsign.x, ST_CALLSIGN);
    row = make_station_row(row, "QTH locator", params.qth.x, ST_QTH);
    lv_obj_add_event_cb(grid, st_delete_cb, LV_EVENT_DELETE, NULL);
    return row;
}

/* Audio: microphone, VOX, AGC */

typedef struct {
    ParamInt   *param;
    int32_t     min;
    int32_t     max;
    int32_t     step;
    const char *fmt;
} param_slider_t;

static param_slider_t hmic_slider      = {&cfg_sm.p_hmic, 0, 50, 1, "%i"};
static param_slider_t imic_slider      = {&cfg_sm.p_imic, 0, 50, 1, "%i"};
static param_slider_t vox_gain_slider  = {&cfg_sm.p_vox_gain, 0, 100, 1, "%i"};
static param_slider_t vox_ag_slider    = {&cfg_sm.p_vox_ag, 0, 100, 1, "%i"};
static param_slider_t vox_delay_slider = {&cfg_sm.p_vox_delay, 100, 2000, 50, "%i ms"};
static param_slider_t agc_knee_slider  = {&cfg_sm.p_agc_knee, -100, 0, 1, "%i dB"};
static param_slider_t agc_slope_slider = {&cfg_sm.p_agc_slope, 0, 10, 1, "%i dB"};

static void param_slider_cb(lv_event_t *e) {
    lv_obj_t       *obj = lv_event_get_target(e);
    param_slider_t *p   = (param_slider_t *)lv_event_get_user_data(e);
    int32_t         val = lv_slider_get_value(obj) * p->step;

    lv_label_set_text_fmt((lv_obj_t *)lv_obj_get_user_data(obj), p->fmt, val);
    p->param->set(val);
}

static uint8_t make_param_slider(uint8_t row, const char *label, param_slider_t *p) {
    lv_obj_t *obj;

    wl_row_label(row, label);

    obj = lv_obj_create(grid);
    lv_obj_set_size(obj, SMALL_6, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 1, 6, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);
    slider_with_text(obj, p->param->get(), p->min, p->max, p->step, SMALL_6 - 120, p->fmt, param_slider_cb, p);

    return row + 1;
}

static uint8_t make_param_switch(uint8_t row, const char *label, ParamInt &param) {
    lv_obj_t *obj;

    wl_row_label(row, label);

    obj = lv_obj_create(grid);
    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(obj);
    obj = switch_bool(obj, param);
    lv_obj_set_width(obj, SMALL_3 - 30);

    return row + 1;
}

static void mic_update_cb(lv_event_t *e) {
    cfg_sm.p_mic.set(lv_dropdown_get_selected(lv_event_get_target(e)));
}

static uint8_t make_mic(uint8_t row) {
    lv_obj_t *obj;

    wl_row_label(row, "Microphone");

    obj = lv_dropdown_create(grid);
    dialog_item(&dialog, obj);
    lv_obj_add_style(lv_dropdown_get_list(obj), &dialog_dropdown_list_style, 0);
    /* Same order as x6100_mic_builtin, x6100_mic_handle, x6100_mic_auto */
    lv_dropdown_set_options(obj, " Built-in \n Hand \n Auto ");
    lv_dropdown_set_symbol(obj, NULL);
    lv_dropdown_set_selected(obj, cfg_sm.p_mic.get());
    lv_obj_add_event_cb(obj, mic_update_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_size(obj, SMALL_3, 56);
    lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_START, 4, 3, LV_GRID_ALIGN_CENTER, row, 1);

    return row + 1;
}

static void make_general_page() {
    grid_delete();
    grid_create();

    uint8_t row;

    x6100_base_ver_t base_ver = x6100_control_get_base_ver();

    now = time(NULL);
    struct tm *t = localtime(&now);

    memcpy(&ts, t, sizeof(ts));

    band_begin();
    row = card_begin(LV_SYMBOL_HOME, "STATION", col_station);
    row = make_station(row);
    card_end(row);

    row = card_begin(LV_SYMBOL_BELL, "DATE / TIME", col_datetime);
    row = make_date(row);
    row = make_time(row);
    card_end(row);
    band_end();

    row = card_begin(LV_SYMBOL_LIST, "CONTROLS");
    row = make_long_action(row);
    row = make_hmic_action(row);
    row = make_freq_accel(row);
    row = make_charger(row);
    card_end(row);

    /* TX calibration */
    row = card_begin(LV_SYMBOL_SETTINGS, "TX CALIBRATION");
    row = make_tx_offset(row);
    if (base_ver.rev >= 8) {
        row = band_out_gain_correction(row);
    }
    card_end(row);

    row = card_begin(LV_SYMBOL_SHUFFLE, "TRANSVERTERS");
    row = make_transverter(row, cfg_sm.p_transverter_0_from, cfg_sm.p_transverter_0_to, cfg_sm.p_transverter_0_shift);
    row = make_transverter(row, cfg_sm.p_transverter_1_from, cfg_sm.p_transverter_1_to, cfg_sm.p_transverter_1_shift);
    card_end(row);
}

static void make_display_page() {
    grid_delete();
    grid_create();
    uint8_t row;

    row = card_begin(LV_SYMBOL_EYE_OPEN, "BACKLIGHT");
    row = make_backlight(row);
    card_end(row);

    row = card_begin(LV_SYMBOL_IMAGE, "SPECTRUM");
    row = make_spectrum_show(row);
    row = make_cluster_spots_wf(row);
    row = make_auto_offset(row);
    row = make_spectrum_min_max(row);
    row = make_spectrum_fill_peak(row);
    row = make_spectrum_beta_peak_hold_speed(row);
    card_end(row);

    row = card_begin(LV_SYMBOL_DOWNLOAD, "WATERFALL");
    row = make_waterfall_line_zoom(row);
    row = make_waterfall_smooth_scroll(row);
    card_end(row);

    row = card_begin(LV_SYMBOL_SETTINGS, "INTERFACE");
    row = make_mag(row);
    row = make_knob_info(row);
    row = make_theme(row);
    card_end(row);
}

static void make_audio_page() {
    grid_delete();
    grid_create();
    uint8_t row;

    x6100_base_ver_t base_ver = x6100_control_get_base_ver();

    /* RX / speaker */
    row = card_begin(LV_SYMBOL_VOLUME_MAX, "RX AUDIO");
    row = make_sp_mode(row);
    row = make_line_gain(row);
    row = make_audio_gain(row);
    card_end(row);

    /* TX audio */
    if (base_ver.rev >= 3) {
        row = card_begin(LV_SYMBOL_UPLOAD, "TX AUDIO");
        row = make_codec_gain(row);
        row = make_comp_th_makeup(row);
        if (base_ver.rev >= 9) {
            row = make_tx_filter(row);
        }
        if (base_ver.rev >= 12) {
            row = make_cessb(row);
        }
        if (base_ver.rev >= 8) {
            row = make_fm_emphasis(row);
        }
        card_end(row);
    }

    row = card_begin(LV_SYMBOL_AUDIO, "MICROPHONE");
    row = make_mic(row);
    row = make_param_slider(row, "Hand mic gain", &hmic_slider);
    row = make_param_slider(row, "Built-in mic gain", &imic_slider);
    card_end(row);

    row = card_begin(LV_SYMBOL_PLAY, "VOX");
    row = make_param_switch(row, "VOX", cfg_sm.p_vox_en);
    row = make_param_slider(row, "VOX gain", &vox_gain_slider);
    row = make_param_slider(row, "Anti-VOX", &vox_ag_slider);
    row = make_param_slider(row, "VOX delay", &vox_delay_slider);
    card_end(row);

    row = card_begin(LV_SYMBOL_LOOP, "AGC");
    row = make_param_switch(row, "AGC hang", cfg_sm.p_agc_hang);
    row = make_param_slider(row, "AGC knee", &agc_knee_slider);
    row = make_param_slider(row, "AGC slope", &agc_slope_slider);
    card_end(row);
}

static void make_wireless_page() {
    grid_delete();
    grid_create();
    uint8_t row;

    bt.listing = false;
    bt.scanning = false;
    bt.busy = false;
    bt.devs.clear();

    row = card_begin(LV_SYMBOL_WIFI, "WIFI");
    row = make_wifi(row);
    card_end(row);

    row = card_begin(LV_SYMBOL_BLUETOOTH, "BLUETOOTH");
    row = make_bluetooth(row);
    row = make_bt_devices(row);
    card_end(row);

    row = card_begin(LV_SYMBOL_AUDIO, "HEADPHONES");
    row = make_headphones(row);
    card_end(row);

    lv_obj_add_event_cb(grid, wl_delete_cb, LV_EVENT_DELETE, NULL);
    wl_ticks = 0;
    wl_timer = lv_timer_create(wl_timer_cb, 1000, NULL);
    wifi_update();
    bt_update_view();
    bt_refresh();
}

static void make_info_page() {
    grid_delete();

    lv_obj_t    *label;

    page = lv_obj_create(dialog.obj);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(page, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(page, 30, 0);
    lv_obj_set_style_pad_ver(page, 40, 0);

    label = lv_label_create(page);
    lv_obj_set_style_text_line_space(label, 20, LV_PART_MAIN);
    lv_label_set_text_fmt(label, "App version: %s\nBASE version: %s", VERSION, x6100_control_get_fw_version_str());
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);
    btn_p1.next = &btn_page_2;
    btn_p2.next = &btn_page;
    make_general_page();
    mark_tab(&btn_general);
}

static void destruct_cb() {
    grid_delete();
    observers.clear();

}

static void key_cb(lv_event_t * e) {
    uint32_t    key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case HKEY_FINP:
             lv_group_set_editing(keyboard_group, !lv_group_get_editing((const lv_group_t*) keyboard_group));
             break;

        case LV_KEY_ESC:
            dialog_destruct();
            break;

        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            break;

        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            break;
    }
}
