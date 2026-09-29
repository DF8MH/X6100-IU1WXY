/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "info.h"

#include "cfg/cfg_api.h"
#include "styles.h"
#include "params/params.h"
#include "pubsub_ids.h"
#include "wifi.h"
#include "gps.h"
#include "scheduler.h"
#include "cluster/client.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

/* Written by btcat: the number of connected Bluetooth CAT clients. */
#ifndef BTCAT_CLIENTS_FILE
#define BTCAT_CLIENTS_FILE "/tmp/btcat.clients"
#endif

#define STATUS_POLL_S   3

#define COLOR_ICON_OFF  0x151515
#define COLOR_ICON_IDLE 0x909090
#define COLOR_ICON_BT   0x55CCFF

typedef enum {
    INFO_VFO = 0,
    INFO_MODE,
    INFO_AGC,
    INFO_PRE,
    INFO_ATT,
    INFO_ATU,
    INFO_WIFI,
    INFO_BT,
    INFO_HEADPHONES,
    INFO_CAT,
    INFO_GPS,
    INFO_CLUSTER,
    INFO_NUM
} info_items_t;

typedef enum {
    STATUS_OFF = 0,
    STATUS_IDLE,
    STATUS_ON
} status_t;

typedef struct {
    status_t bt;
    status_t headphones;
    status_t cat;
    status_t gps;
    status_t cluster;
} info_status_t;

static lv_obj_t     *obj;
static lv_obj_t     *items[INFO_NUM];

static SubjectInt *mode_lock;

static void wifi_state_change_cb(void *s, lv_msg_t *m);
static void *status_thread(void *arg);

static void vfo_label_update(Subject *subj, void * user_data);
static void mode_label_update(Subject *subj, void * user_data);
static void atu_label_update(Subject *subj, void * user_data);
static void agc_label_update(Subject *subj, void * user_data);
static void att_label_update(Subject *subj, void * user_data);
static void pre_label_update(Subject *subj, void * user_data);

lv_obj_t * info_init(lv_obj_t * parent) {

    mode_lock = subject_i_create(false);

    obj = lv_obj_create(parent);

    lv_obj_add_style(obj, &info_style, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Two rows on the same 6 column grid: radio state, then status icons */
    lv_coord_t col_w = lv_obj_get_style_width(obj, 0) - 2 * lv_obj_get_style_border_width(obj, 0)
        - lv_obj_get_style_pad_left(obj, 0) - lv_obj_get_style_pad_right(obj, 0);
    col_w /= 6;

    lv_obj_t *row;

    for (uint8_t i = 0; i < INFO_NUM; i++) {
        if (i % 6 == 0) {
            row = lv_obj_create(obj);
            lv_obj_add_style(row, &info_row_style, 0);
            lv_obj_set_size(row, col_w * 6 - 4, i ? 20 : 25);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        }
        lv_obj_t *item = lv_label_create(row);
        lv_obj_set_width(item, col_w - 4);
        lv_obj_set_style_text_align(item, LV_TEXT_ALIGN_CENTER, 0);
        if (i < INFO_WIFI) {
            lv_obj_add_style(item, &info_item_style, 0);
            lv_obj_set_style_text_color(item, lv_color_white(), 0);
        } else {
            lv_obj_set_style_text_font(item, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(item, lv_color_hex(COLOR_ICON_OFF), 0);
        }
        items[i] = item;
    }

    lv_label_set_text(items[INFO_PRE], "PRE");
    lv_label_set_text(items[INFO_ATT], "ATT");
    lv_label_set_text(items[INFO_WIFI], LV_SYMBOL_WIFI);
    lv_label_set_text(items[INFO_BT], LV_SYMBOL_BLUETOOTH);
    lv_label_set_text(items[INFO_HEADPHONES], LV_SYMBOL_AUDIO);
    lv_label_set_text(items[INFO_CAT], LV_SYMBOL_KEYBOARD);
    lv_label_set_text(items[INFO_GPS], LV_SYMBOL_GPS);
    lv_label_set_text(items[INFO_CLUSTER], LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(items[INFO_WIFI], lv_color_hex(COLOR_ICON_IDLE), 0);

    subject_subscribe_delayed((Subject*)cfg_band_current_vfo, vfo_label_update, NULL);
    subject_subscribe_delayed_and_notify((Subject*)cfg_band_split, vfo_label_update, NULL);

    subject_subscribe_delayed((Subject*)cfg_cur_mode, mode_label_update, NULL);
    subject_subscribe_delayed_and_notify((Subject*)mode_lock, mode_label_update, NULL);

    subject_subscribe_delayed((Subject*)cfg_ant_id, atu_label_update, NULL);
    subject_subscribe_delayed((Subject*)cfg_fg_freq, atu_label_update, NULL);
    cfg_atu_loaded_subscribe_delayed(atu_label_update, NULL);
    subject_subscribe_delayed_and_notify((Subject*)cfg_atu_enabled, atu_label_update, NULL);

    subject_subscribe_delayed_and_notify((Subject*)cfg_cur_agc, agc_label_update, NULL);

    subject_subscribe_delayed_and_notify((Subject*)cfg_cur_att, att_label_update, NULL);
    subject_subscribe_delayed_and_notify((Subject*)cfg_cur_pre, pre_label_update, NULL);

    lv_msg_subscribe(MSG_WIFI_STATE_CHANGED, wifi_state_change_cb, NULL);

    pthread_t thread;
    pthread_create(&thread, NULL, status_thread, NULL);
    pthread_detach(thread);

    return obj;
}

const char* info_params_mode_label_get() {
    x6100_mode_t    mode = cparam_i_get(cfg_cur_mode);
    char            *str;

    switch (mode) {
        case x6100_mode_lsb:
            str = "LSB";
            break;

        case x6100_mode_lsb_dig:
            str = "LSB-D";
            break;

        case x6100_mode_usb:
            str = "USB";
            break;

        case x6100_mode_usb_dig:
            str = "USB-D";
            break;

        case x6100_mode_cw:
            str = "CW";
            break;

        case x6100_mode_cwr:
            str = "CW-R";
            break;

        case x6100_mode_am:
            str = "AM";
            break;

        case x6100_mode_nfm:
            str = "NFM";
            break;

        default:
            str = "?";
            break;
    }

    return str;
}

const char* info_params_agc() {
    x6100_agc_t     agc = cparam_i_get(cfg_cur_agc);
    char            *str;

    switch (agc) {
        case x6100_agc_off:
            str = "OFF";
            break;

        case x6100_agc_slow:
            str = "SLOW";
            break;

        case x6100_agc_fast:
            str = "FAST";
            break;

        case x6100_agc_auto:
            str = "AUTO";
            break;

        default:
            str = "?";
            break;

    }

    return str;
}

const char* info_params_vfo_label_get() {
    x6100_vfo_t cur_vfo = param_i_get(cfg_band_current_vfo);
    char            *str;

    if (param_i_get(cfg_band_split)) {
        str = cur_vfo == X6100_VFO_A ? "SPL-A" : "SPL-B";
    } else {
        str = cur_vfo == X6100_VFO_A ? "VFO-A" : "VFO-B";
    }

    return str;
}

void info_lock_mode(bool lock) {
    subject_i_set(mode_lock, lock);
}

static void wifi_state_change_cb(void *s, lv_msg_t *m) {
    lv_color_t color;
    switch (wifi_get_status()) {
    case WIFI_CONNECTED:
        color = lv_color_white();
        break;
    case WIFI_OFF:
        color = lv_color_black();
        break;
    default:
        color = lv_color_hex(COLOR_ICON_IDLE);
        break;
    }
    lv_obj_set_style_text_color(items[INFO_WIFI], color, 0);
}

static void set_icon(info_items_t item, status_t st, uint32_t on_color) {
    uint32_t color = st == STATUS_ON ? on_color : st == STATUS_IDLE ? COLOR_ICON_IDLE : COLOR_ICON_OFF;
    lv_obj_set_style_text_color(items[item], lv_color_hex(color), 0);
}

static void status_update(void *arg) {
    info_status_t *st = arg;

    set_icon(INFO_BT, st->bt, COLOR_ICON_BT);
    set_icon(INFO_HEADPHONES, st->headphones, COLOR_ICON_BT);
    set_icon(INFO_CAT, st->cat, 0xFFFFFF);
    set_icon(INFO_GPS, st->gps, COLOR_LIGHT_GREEN);
    set_icon(INFO_CLUSTER, st->cluster, COLOR_LIGHT_GREEN);
}

/* Bluetooth: adapter up, any device connected, an A2DP headset among them.
 * The tools are skipped while the adapter is down. */
static void poll_bluetooth(info_status_t *st) {
    int up = 0, con = 0, a2dp = 0;
    FILE *f = popen("up=$(hciconfig hci0 2>/dev/null | grep -c 'UP RUNNING'); echo $up; "
                    "[ \"$up\" = 1 ] && { hcitool con 2>/dev/null | grep -c ACL; "
                    "pactl list sinks short 2>/dev/null | grep -c bluez_sink; }", "r");

    if (f) {
        if (fscanf(f, "%d %d %d", &up, &con, &a2dp) < 3) {
            con = a2dp = 0;
        }
        pclose(f);
    }
    st->bt = !up ? STATUS_OFF : con ? STATUS_ON : STATUS_IDLE;
    st->headphones = up && a2dp ? STATUS_ON : STATUS_OFF;

    /* The file exists while btcat runs */
    int clients = 0;
    f = fopen(BTCAT_CLIENTS_FILE, "r");
    if (f) {
        if (fscanf(f, "%d", &clients) != 1) {
            clients = 0;
        }
        fclose(f);
        st->cat = clients > 0 ? STATUS_ON : up ? STATUS_IDLE : STATUS_OFF;
    } else {
        st->cat = STATUS_OFF;
    }
}

static void *status_thread(void *arg) {
    info_status_t st;
    double lat, lon;
    int age;

    while (true) {
        poll_bluetooth(&st);

        if (gps_last_fix(&lat, &lon, &age) && age < 10) {
            st.gps = STATUS_ON;
        } else {
            st.gps = gps_status() == GPS_STATUS_WORKING ? STATUS_IDLE : STATUS_OFF;
        }

        switch (cluster_client_state(NULL, 0)) {
            case CLUSTER_CONNECTED:
                st.cluster = STATUS_ON;
                break;
            case CLUSTER_STOPPED:
                st.cluster = STATUS_OFF;
                break;
            default:
                st.cluster = STATUS_IDLE;
                break;
        }

        scheduler_put(status_update, &st, sizeof(st));
        sleep(STATUS_POLL_S);
    }
    return NULL;
}


static void vfo_label_update(Subject *subj, void * user_data) {
    lv_label_set_text(items[INFO_VFO], info_params_vfo_label_get());
}

static void mode_label_update(Subject *subj, void *user_data) {
    lv_label_set_text(items[INFO_MODE], info_params_mode_label_get());
    x6100_mode_t mode = cparam_i_get(cfg_cur_mode);
    if ((mode == x6100_mode_lsb_dig) || (mode == x6100_mode_usb_dig)) {
        lv_obj_set_style_text_color(items[INFO_MODE], lv_color_hex(COLOR_LIGHT_RED), 0);
    } else if (subject_i_get(mode_lock)) {
        lv_obj_set_style_text_color(items[INFO_MODE], lv_color_hex(0xAAAAAA), 0);
    } else {
        lv_obj_set_style_text_color(items[INFO_MODE], lv_color_white(), 0);
    }
}

static void atu_label_update(Subject *subj, void * user_data) {
    int32_t ant = param_i_get(cfg_ant_id);
    lv_label_set_text_fmt(items[INFO_ATU], "ATU%i", ant);
    int32_t freq = cparam_i_get(cfg_fg_freq);

    if (!param_i_get(cfg_atu_enabled)) {
        lv_obj_set_style_text_color(items[INFO_ATU], lv_color_white(), 0);
        lv_obj_set_style_bg_color(items[INFO_ATU], lv_color_black(), 0);
        lv_obj_set_style_bg_opa(items[INFO_ATU], LV_OPA_0, 0);
    } else {
        if (cfg_transverter_shift_for(freq)) {
            lv_obj_set_style_text_color(items[INFO_ATU], lv_color_hex(0xAAAAAA), 0);
            lv_obj_set_style_bg_opa(items[INFO_ATU], LV_OPA_20, 0);
        } else {
            lv_obj_set_style_text_color(items[INFO_ATU], cfg_atu_is_loaded() ? lv_color_black() : lv_color_hex(0xFF0000), 0);
            lv_obj_set_style_bg_opa(items[INFO_ATU], LV_OPA_50, 0);
        }
        lv_obj_set_style_bg_color(items[INFO_ATU], lv_color_white(), 0);
    }
}

static void agc_label_update(Subject *subj, void * user_data) {
    lv_label_set_text(items[INFO_AGC], info_params_agc());
}

static void att_label_update(Subject *subj, void * user_data) {
    if (cparam_i_get(cfg_cur_att)) {
        lv_obj_set_style_text_color(items[INFO_ATT], lv_color_black(), 0);
        lv_obj_set_style_bg_color(items[INFO_ATT], lv_color_white(), 0);
        lv_obj_set_style_bg_opa(items[INFO_ATT], LV_OPA_50, 0);
    } else {
        lv_obj_set_style_text_color(items[INFO_ATT], lv_color_white(), 0);
        lv_obj_set_style_bg_color(items[INFO_ATT], lv_color_black(), 0);
        lv_obj_set_style_bg_opa(items[INFO_ATT], LV_OPA_0, 0);
    }
}

static void pre_label_update(Subject *subj, void * user_data) {
    if (cparam_i_get(cfg_cur_pre)) {
        lv_obj_set_style_text_color(items[INFO_PRE], lv_color_black(), 0);
        lv_obj_set_style_bg_color(items[INFO_PRE], lv_color_white(), 0);
        lv_obj_set_style_bg_opa(items[INFO_PRE], LV_OPA_50, 0);
    } else {
        lv_obj_set_style_text_color(items[INFO_PRE], lv_color_white(), 0);
        lv_obj_set_style_bg_color(items[INFO_PRE], lv_color_black(), 0);
        lv_obj_set_style_bg_opa(items[INFO_PRE], LV_OPA_0, 0);
    }
}
