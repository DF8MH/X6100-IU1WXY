/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */
#include <cstring>

#include "panel.h"
#include "knobs.h"
#include "util.h"
#include "cfg/settings_manager.h"

extern "C" {
    #include "rtty.h"
    #include "styles.h"
    #include "theme.h"
    #include "scheduler.h"
    #include "radio.h"
    #include "params/params.h"
}

static lv_obj_t    *obj;
static lv_obj_t    *info;
static lv_anim_t    dim_anim;
static char         buf[1024] = "";
static char        *buf_write = buf;
static uint8_t      max_line_count;
static x6100_mode_t prev_mode;

static void update_visibility_cb(Subject *subj, void *user_data);
static void on_freq_change(Subject *subj, void *user_data);

static void set_opa(void * panel_obj, int32_t opa);

/* As many lines as the panel holds in its font (from upstream panel.cpp) */
static void update_line_count() {
    const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
    lv_coord_t       h = lv_obj_get_style_height(obj, 0) - lv_obj_get_style_pad_top(obj, 0)
        - lv_obj_get_style_pad_bottom(obj, 0);

    max_line_count = LV_MAX(1, h / font->line_height);
}

/* Cuts the first line when there is one too many (upstream truncate()) */
static void check_lines() {
    char        *second_line = NULL;
    char        *ptr = buf;
    uint16_t    count = 1;

    // Count lines, store start of 2nd in `second_line`
    while (*ptr) {
        if (*ptr == '\n') {
            count++;

            if (count == 2) {
                second_line = ptr + 1;
            }
        }
        ptr++;
    }
    if (ptr == buf) {
        return;
    }
    // The last empty line, the next text goes there
    if (*(ptr - 1) == '\n') {
        count--;
    }
    // Too long, cut first line
    if (count > max_line_count) {
        memmove(buf, second_line, strlen(second_line) + 1);
        buf_write = buf + strlen(buf);
    }
}

/* Settings > Display > Decoder text: Large (the stock sony_24) or Compact (sony_18, more lines).
 * Follows the setting live; ported from SP2KMO's upstream c2fbe40c, with his fix 0736fa9c: every
 * line that no longer fits goes, not one */
static void text_size_cb(Subject *subj, void *user_data) {
    size_t len;

    lv_obj_set_style_text_font(obj, cfg_sm.p_digi_text_compact.get() ? &sony_18 : &sony_24, 0);
    update_line_count();
    do {
        len = strlen(buf);
        check_lines();
    } while (strlen(buf) != len);
    lv_label_set_text_static(obj, buf);
}

static void panel_update_text_cb(const char *text) {
    lv_point_t text_size;
    char *old_write;

    if (!buf_write) {
        return;
    }
    old_write = buf_write;
    // TODO: check len of text is not exceed width
    buf_write = stpcpy(buf_write, text);
    if (buf_write - buf >= 2) {
        // new line text only
        if (strcmp(buf_write - 2, "\n\n") == 0) {
            *buf_write-- = '\0';
        } else {
            lv_txt_get_size(&text_size, buf, lv_obj_get_style_text_font(obj, 0), 0, 0, LV_COORD_MAX, 0);
            if (text_size.x > (lv_obj_get_width(obj) - 20)) {
                *old_write = '\n';
                buf_write = stpcpy(old_write + 1, text);
            }
        }
    }
    check_lines();
    lv_label_set_text_static(obj, buf);
}

static void panel_update_info_cb(const char *text) {
    lv_label_set_text(info, text);
}

lv_obj_t * panel_init(lv_obj_t *parent) {
    obj = lv_label_create(parent);

    lv_label_set_text_static(obj, buf);

    lv_obj_add_style(obj, &panel_style, 0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_anim_init(&dim_anim);
    lv_anim_set_exec_cb(&dim_anim, set_opa);
    lv_anim_set_var(&dim_anim, obj);
    lv_anim_set_time(&dim_anim, 200);

    prev_mode = (x6100_mode_t)cfg_sm.cp_cur_mode.get();

    cfg_sm.cp_cur_mode.subscribe_delayed(update_visibility_cb, NULL);
    cfg_sm.p_cw_decoder.subscribe_delayed_and_notify(update_visibility_cb, NULL);
    cfg_sm.cp_fg_freq.subscribe_delayed(on_freq_change, NULL);
    cfg_sm.p_digi_text_compact.subscribe_delayed_and_notify(text_size_cb, NULL);

    info = lv_label_create(obj);
    lv_obj_add_style(info, &panel_info_style, 0);
    lv_label_set_text(info, "");

    return obj;
}

/* A taller spectrum: the panel starts under it, shorter (upstream panel_set_height()) */
void panel_keep_below(lv_coord_t y) {
    lv_coord_t bottom = lv_obj_get_style_y(obj, 0) + lv_obj_get_style_height(obj, 0);

    if (y > lv_obj_get_style_y(obj, 0)) {
        lv_obj_set_y(obj, y);
        lv_obj_set_height(obj, bottom - y);
        text_size_cb(NULL, NULL);
    }
}

void panel_add_text(const char * text) {
    scheduler_put((void(*)(void*))panel_update_text_cb, (void*)text, strlen(text) + 1);
}

void panel_set_info(const char *text) {
    scheduler_put((void(*)(void*))panel_update_info_cb, (void*)text, strlen(text) + 1);
}

void panel_hide() {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    knobs_display(true);
}

void panel_clear() {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == false) {
        buf[0] = '\0';
        buf_write = buf;
        lv_label_set_text_static(obj, buf);
    }
}

void panel_update_visibility(bool clear) {
    x6100_mode_t    mode = (x6100_mode_t)cfg_sm.cp_cur_mode.get();
    bool            on = false;

    switch (mode) {
        case x6100_mode_cw:
        case x6100_mode_cwr:
            on = cfg_sm.p_cw_decoder.get();
            break;

        case x6100_mode_usb:
        case x6100_mode_lsb:
        case x6100_mode_usb_dig:
        case x6100_mode_lsb_dig:
            on = rtty_get_state() != RTTY_OFF;
            break;
    }

    if (on) {
        if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
            knobs_display(false);
        }
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        knobs_display(true);
    }
    if (clear) {
        panel_clear();
    }
}

static void update_visibility_cb(Subject *subj, void *user_data) {
    x6100_mode_t cur_mode = (x6100_mode_t)cfg_sm.cp_cur_mode.get();
    x6100_mode_t tmp_mode = prev_mode;
    bool clear = true;

    // Prevent clear on preserving CW / CWR mode
    if (cur_mode == x6100_mode_cwr) {
        cur_mode = x6100_mode_cw;
    }
    if (tmp_mode == x6100_mode_cwr) {
        tmp_mode = x6100_mode_cw;
    }
    if (tmp_mode == cur_mode) {
        clear = false;
    }
    prev_mode = cur_mode;
    panel_update_visibility(clear);
}

/* The parts' opacities, not the object's: an object opa below COVER makes LVGL
 * draw the whole box in a layer and blend it, every frame of this fade, which
 * runs at each VFO step (tuning stuttered in CW with the decoder on) */
static void set_opa(void * panel_obj, int32_t opa) {
    lv_obj_t *o = (lv_obj_t *)panel_obj;

    lv_obj_set_style_text_opa(o, opa, 0);
    lv_obj_set_style_bg_img_opa(o, opa, 0);
    lv_obj_set_style_border_opa(o, opa, 0);
    if (!theme_shape.images) {
        lv_obj_set_style_bg_opa(o, opa, 0);   /* a drawn box; with images there is no bg colour */
    }
}

static void revert_opa(struct _lv_anim_t * acct) {
    lv_anim_set_delay(&dim_anim, 200);
    lv_anim_set_values(&dim_anim, LV_OPA_40, LV_OPA_COVER);
    lv_anim_set_ready_cb(&dim_anim, NULL);
    lv_anim_start(&dim_anim);
}

static void on_freq_change(Subject *subj, void *user_data) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    lv_anim_del(obj, set_opa);

    lv_opa_t cur_opa = lv_obj_get_style_text_opa(obj, 0);
    lv_anim_set_ready_cb(&dim_anim, revert_opa);
    lv_anim_set_values(&dim_anim, cur_opa, LV_OPA_40);
    lv_anim_set_delay(&dim_anim, 0);
    lv_anim_start(&dim_anim);
}
