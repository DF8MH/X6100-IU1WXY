/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "dialog.h"

#include <stdio.h>
#include <string.h>

#include "styles.h"
#include "theme.h"
#include "main_screen.h"
#include "radio.h"
#include "freq_strip.h"
#include "cfg/cfg_api.h"
#include "keyboard.h"
#include "events.h"
#include "waterfall.h"
#include "knobs.h"

static lv_obj_t     *obj;
static dialog_t     *current_dialog = NULL;
static int32_t      restore_mode = -1;     /* the mode before an app changed it, back at its close */
static int32_t      forced_mode;

static bool         restore_mem;            /* dialog_restore_radio_on_close() */
static char         mem_name[8];            /* dialog_app_memory(): the open app's, "" none */

#define APP_FREQS "/mnt/app_freqs"

bool app_mem_load(const char *name, int32_t *freq, int32_t *mode) {
    FILE *f = fopen(APP_FREQS, "r");
    char  n[16];
    int   fr, md;
    bool  ok = false;

    if (!f) return false;
    while (!ok && fscanf(f, "%15s %d %d", n, &fr, &md) == 3) {
        ok = !strcmp(n, name);
    }
    fclose(f);
    if (ok) {
        *freq = fr;
        *mode = md;
    }
    return ok;
}

void app_mem_save(const char *name) {
    char  lines[16][48], n[16];
    int   cnt = 0, fr, md;
    FILE *f = fopen(APP_FREQS, "r");

    if (f) {
        while (cnt < 16 && fscanf(f, "%15s %d %d", n, &fr, &md) == 3) {
            if (strcmp(n, name)) snprintf(lines[cnt++], sizeof(lines[0]), "%s %d %d", n, fr, md);
        }
        fclose(f);
    }
    if (!(f = fopen(APP_FREQS ".tmp", "w"))) return;
    for (int i = 0; i < cnt; i++) fprintf(f, "%s\n", lines[i]);
    fprintf(f, "%s %d %d\n", name, (int)cparam_i_get(cfg_fg_freq), (int)cparam_i_get(cfg_cur_mode));
    fclose(f);
    rename(APP_FREQS ".tmp", APP_FREQS);
}

void dialog_app_memory(const char *name) {
    snprintf(mem_name, sizeof(mem_name), "%s", name);
}

void dialog_restore_mode_on_close(int32_t mode, int32_t forced) {
    restore_mode = mode;
    forced_mode  = forced;
}

void dialog_restore_radio_on_close(void) {
    if (current_dialog && current_dialog->run && !current_dialog->restore_radio) {
        mem_save(MEM_BACKUP_ID);
        restore_mem = true;
    }
}

void dialog_construct(dialog_t *dialog, lv_obj_t *parent) {
    if (dialog && !dialog->run) {
        knobs_display(false);
        waterfall_refresh_period_set(2);
        main_screen_keys_enable(false);
        if (dialog->restore_radio) {
            mem_save(MEM_BACKUP_ID);
        }
        int32_t freq, mode;
        if (mem_name[0] && app_mem_load(mem_name, &freq, &mode)) {
            main_screen_set_freq(freq);
            cparam_i_set(cfg_cur_mode, mode);
        }
        dialog->prev_page = buttons_get_cur_page();
        buttons_unload_page();
        if (dialog->btn_page) {
            buttons_load_page(dialog->btn_page);
        }
        dialog->construct_cb(parent);

        dialog->run = true;
    }

    current_dialog = dialog;
}

void dialog_destruct() {
    freq_strip_set_app(NULL);
    if (current_dialog && current_dialog->run) {
        knobs_display(true);
        waterfall_refresh_reset();
        current_dialog->run = false;

        if (current_dialog->destruct_cb) {
            current_dialog->destruct_cb();
        }
        if (mem_name[0]) {
            app_mem_save(mem_name);     /* before the radio goes back to before the app */
            mem_name[0] = 0;
            radio_set_digi_cap(false);
        }

        if (current_dialog->obj) {
            lv_obj_del(current_dialog->obj);
        }
        if (current_dialog->restore_radio || restore_mem) {
            mem_load(MEM_BACKUP_ID);
            if (restore_mem)
                restore_mode = -1;  /* the mode is back already */
            restore_mem = false;
        }
        buttons_unload_page();
        if (current_dialog->prev_page) {
            buttons_load_page(current_dialog->prev_page);
        }
        main_screen_keys_enable(true);
        current_dialog = NULL;
    }
    if (restore_mode >= 0) {
        /* Unless the mode was changed again meanwhile */
        if (cparam_i_get(cfg_cur_mode) == forced_mode)
            cparam_i_set(cfg_cur_mode, restore_mode);
        restore_mode = -1;
    }
}

void dialog_send(lv_event_code_t event_code, void *param) {
    if (dialog_is_run()) {
        event_send(current_dialog->obj, event_code, param);
    }
}

bool dialog_is_run() {
    return (current_dialog != NULL) && current_dialog->run;
}

bool dialog_type_is_run(dialog_t *dialog) {
    return (current_dialog == dialog) && current_dialog->run;
}

lv_obj_t * dialog_init(lv_obj_t *parent) {
    obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, &dialog_style, 0);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    return obj;
}

void dialog_item(dialog_t *dialog, lv_obj_t *obj) {
    lv_obj_add_style(obj, &dialog_item_style, LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &dialog_item_focus_style, LV_STATE_FOCUSED);
    lv_obj_add_style(obj, &dialog_item_edited_style, LV_STATE_EDITED);

    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_text_color(obj, TC(text), LV_PART_CURSOR);
    lv_obj_set_style_text_color(obj, TC(focus_text), LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(obj, 128, LV_PART_CURSOR | LV_STATE_EDITED);

    lv_obj_set_style_border_width(obj, 2, LV_STATE_FOCUS_KEY | LV_PART_INDICATOR);
    lv_obj_set_style_border_color(obj, TC(focus), LV_STATE_FOCUS_KEY | LV_PART_INDICATOR);

    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_group_add_obj(keyboard_group, obj);

    if (dialog->key_cb) {
        lv_obj_add_event_cb(obj, dialog->key_cb, LV_EVENT_KEY, NULL);
    }
}

bool dialog_need_audio() {
    return dialog_is_run() && current_dialog->audio_cb;
}

void dialog_audio_samples(unsigned int n, float *samples) {
    if (dialog_need_audio()) {
        current_dialog->audio_cb(n, samples);
    }
}

void dialog_rotary(int32_t diff) {
    if (dialog_is_run() && current_dialog->rotary_cb) {
        current_dialog->rotary_cb(diff);
    }
}
