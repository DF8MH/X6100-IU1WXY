/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include "main.h"
#include "main_screen.h"
#include "screen_rec.h"
#include "styles.h"
#include "fonts/font_select.h"
#include "radio.h"
#include "dsp.h"
#include "util.h"
#include "keyboard.h"
#include "spectrum.h"
#include "waterfall.h"
#include "keypad.h"
#include "params/params.h"
#include "cfg/cfg_api.h"
#include "audio.h"
#include "cw.h"
#include "panel.h"
#include "cat.h"
#include "wfview_server.h"
#include "remote_server.h"
#include "clock.h"
#include "rtty.h"
#include "backlight.h"
#include "events.h"
#include "gps.h"
#include "mfk.h"
#include "vol.h"
#include "qso_log.h"
#include "scheduler.h"
#include "wifi.h"
#include "btaudio.h"
#include "usb_devices.h"
#include "dialog_cluster.h"

#define DISP_BUF_SIZE (800 * 480 * 4)

rotary_t                    *vol;
encoder_t                   *mfk;

static lv_color_t           buf1[DISP_BUF_SIZE];
static lv_color_t           buf2[DISP_BUF_SIZE];
static lv_disp_draw_buf_t   disp_buf;
static lv_disp_drv_t        disp_drv;

void * tick_thread (void *args);

/* Display settings that would break the screen are put right at every start
 * (an old or hand-edited params.db, a backup restored from another version):
 * the spectrum is disabled in this firmware, the theme is IU1WXY, the font
 * must exist and the legacy spectrum values must stay in their ranges. */
static void fix_uint8(params_uint8_t *p) {
    if (p->max > p->min && (p->x < p->min || p->x > p->max)) {
        uint8_t v = p->x < p->min ? p->min : p->max;

        fprintf(stderr, "Settings: %s %u out of range, set to %u\n", p->name, p->x, v);
        params_uint8_set(p, v);
    }
}

static void display_settings_check(void) {
    if (param_i_get(cfg_spectrum_show)) {
        fprintf(stderr, "Settings: spectrum_show on, set off (the spectrum is disabled)\n");
        param_i_set(cfg_spectrum_show, 0);
    }
    /* Only the IU1WXY theme for now (Legacy and Outdoor are out of the menu) */
    if (params.theme.x != THEME_SIMPLE) {
        fprintf(stderr, "Settings: theme %u, set to IU1WXY\n", params.theme.x);
        params_uint8_set(&params.theme, THEME_SIMPLE);
    }
    if (params.font.x > FONT_DEJAVU) {
        fprintf(stderr, "Settings: font %u unknown, set to the default\n", params.font.x);
        params_uint8_set(&params.font, FONT_SONY);
    }
    fix_uint8(&params.spectrum_beta);
    fix_uint8(&params.spectrum_peak_hold);
    fix_uint8(&params.spectrum_peak_speed);
}

int main(void) {
    clock_tz_init();
    lv_init();
    // lv_png_init();

    fbdev_init();
    audio_init();
    event_init();
    usb_devices_monitor_init();

    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, DISP_BUF_SIZE);
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf   = &disp_buf;
    disp_drv.flush_cb   = screen_rec_flush;
    // disp_drv.hor_res    = 480;
    // disp_drv.ver_res    = 800;
    disp_drv.hor_res    = 800;
    disp_drv.ver_res    = 480;
    // disp_drv.sw_rotate  = 1;
    // disp_drv.rotated    = LV_DISP_ROT_90;

    lv_disp_drv_register(&disp_drv);

    lv_disp_set_bg_color(lv_disp_get_default(), lv_color_black());
    lv_disp_set_bg_opa(lv_disp_get_default(), LV_OPA_COVER);

    keyboard_init();

    keypad_init("/dev/input/event0");
    keypad_init("/dev/input/event4");

    rotary_init("/dev/input/event1");

    vol = rotary_init("/dev/input/event2");
    mfk = encoder_init("/dev/input/event3");

    vol->left[VOL_STATE_EDIT] = KEY_VOL_LEFT_EDIT;
    vol->right[VOL_STATE_EDIT] = KEY_VOL_RIGHT_EDIT;

    vol->left[VOL_STATE_SELECT] = KEY_VOL_LEFT_SELECT;
    vol->right[VOL_STATE_SELECT] = KEY_VOL_RIGHT_SELECT;
    vol->state = VOL_STATE_EDIT;

    params_init();
    display_settings_check();
    audio_set_play_vol(params.play_gain_db_f.x);
    audio_set_rec_vol(params.rec_gain_db_f.x);
    mfk_init();
    vol_init();
    fonts_apply(params.font.x);
    styles_init(params.theme.x);

    radio_init();
    audio_mixer_setup(x6100_control_get_base_ver());
    dsp_init();
    lv_obj_t *main_obj = main_screen();
    radio_start();

    cw_init();
    rtty_init();
    radio_set_rx_tx_notify_fn(&main_screen_notify_rx_tx);
    radio_set_low_power_cb(&main_screen_notify_low_power);
    wifi_power_setup();
    btaudio_init();
    backlight_init();
    cat_init();
    wfview_server_init();
    remote_server_init();
    gps_init();
    if (!qso_log_init()) {
        LV_LOG_ERROR("Can't init QSO log");
    }
    qso_log_import_adif("/mnt/incoming_log.adi");
    dialog_cluster_start_client();

    pthread_t thread;
    pthread_create(&thread, NULL, tick_thread, NULL);
    pthread_detach(thread);

#if 0
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_0, 0);
    lv_scr_load_anim(main_obj, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
#else
    lv_scr_load(main_obj);
#endif

    int64_t next_loop_time, sleep_time, loop_start_time;
    while (1) {
        loop_start_time = get_time();
        // observer_delayed_notify_all();
        event_obj_check();
        scheduler_work();
        next_loop_time = lv_timer_handler() + loop_start_time;
        sleep_time = next_loop_time - get_time();
        if (sleep_time > 0) {
            usleep(sleep_time * 1000);
        } else {
            /* Cap loop rate when LVGL reports no delay to reduce CPU. */
            usleep(5000);
        }
    }
    return 0;
}

void * tick_thread (void *args)
{
      while(1) {
        usleep(5 * 1000);
        lv_tick_inc(5);
    }
}
