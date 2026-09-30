/* Headless render of the app dialogs with the real button bar. */
#include "lvgl/lvgl.h"
#include "buttons.h"
#include "dialog.h"
#include "styles.h"
#include "keyboard.h"
#include "events.h"
#include "params/params.h"
#include "dialog_js8.h"
#include "dialog_sstv.h"
#include "dialog_wefax.h"
#include "dialog_navtex.h"
#include "dialog_ft8.h"
#include "dialog_settings.h"
#include "dialog_cluster.h"
#include "dialog_modes.h"
#include "freq_strip.h"
#include "freq_scale.h"
#include "spot_marks.h"
#include "cluster_column.h"
#include "dialog_msg_voice.h"
#include "dialog_msg_cw.h"
#include "cluster/client.h"
#include "textarea_window.h"
#include "info.h"
#include "meter.h"
#include "clock.h"
#include "tx_info.h"
#include "pubsub_ids.h"
#include "fonts/font_select.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <execinfo.h>
#include <sys/stat.h>
#include <signal.h>

#define W 800
#define H 480
static uint32_t fb[W * H];

params_t    params;
lv_group_t *keyboard_group;

void scheduler_work(void);
extern uint32_t EVENT_BAND_DOWN;
extern uint32_t EVENT_BAND_UP;
extern uint32_t EVENT_RADIO_TX, EVENT_RADIO_RX;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++) fb[y * W + x] = (px++)->full;
    lv_disp_flush_ready(drv);
}

static void shot(const char *path) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        unsigned char rgb[3] = {fb[i] >> 16, fb[i] >> 8, fb[i]};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("[shot] %s\n", path);
}

/* The real one (params.c) is not linked */
void params_str_set(params_str_t *var, const char *x) {
    strncpy(var->x, x, sizeof(var->x) - 1);
}

static void pump(int ms) {
    for (int t = 0; t < ms; t += 5) {
        lv_tick_inc(5);
        scheduler_work();
        lv_timer_handler();
        usleep(5000);
    }
}

static void app(const char *name, dialog_t *d, int pages) {
    char p[128];
    lv_obj_t *scr = lv_scr_act();
    dialog_construct(d, scr);
    pump(400);
    for (int i = 0; i < pages; i++) {
        snprintf(p, sizeof p, "%s_%d.ppm", name, i + 1);
        shot(p);
        buttons_page_t *pg = buttons_get_cur_page();
        if (!pg || !pg->items[0] || !pg->items[0]->press) break;
        pg->items[0]->press(pg->items[0]);
        pump(100);
    }
    dialog_destruct();
    pump(200);
}

extern int32_t harness_fg_freq;

/* The frequency bookmarks of an app: the Freq. button is the 2nd on page 1. */
static void bookmarks(const char *name, dialog_t *d, int32_t freq) {
    char p[64];
    harness_fg_freq = freq;
    dialog_construct(d, lv_scr_act());
    pump(300);
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[1]->press(pg->items[1]);
    pump(100);
    snprintf(p, sizeof p, "%s_freq.ppm", name);
    shot(p);
    uint32_t key = LV_KEY_RIGHT;
    lv_obj_t *f = lv_group_get_focused(keyboard_group);
    lv_event_send(f, LV_EVENT_KEY, &key);
    lv_event_send(f, LV_EVENT_KEY, &key);
    pump(100);
    snprintf(p, sizeof p, "%s_freq_2.ppm", name);
    shot(p);
    dialog_destruct();
    pump(200);
}

/* Cluster app with sample spots, VFO on 20m; then the MFK moves the
 * selection two rows down (the list stops following the VFO). */
static void cluster_demo(void) {
    static const struct { const char *call, *spotter, *comment; int khz10, age; } s[] = {
        {"VP8LP", "DL1ABC", "CW 599 up 2", 140235, 3},  {"JA1XYZ", "IK2AAA", "FT8 -12 dB", 140740, 5},
        {"EA8/G4XX", "F5ZZZ", "SSB 59", 141550, 10},    {"3B8M", "IZ1BBB", "SSB listening up 5 to 10 tnx for the new one", 141930, 1},
        {"ZL2ABC", "IU1WXY", "SSB LP 57", 141965, 0},    {"PY2XB", "EA3CCC", "SSB 55", 142050, 12},
        {"K1ABC", "G3DDD", "POTA US-1234", 142430, 14},  {"W1AW", "K2XX", "SSB", 142800, 20},
        {"VK9DX", "JA1YYY", "CW", 70120, 4},             {"4X6ZZ", "OE1AAA", "SSB", 71500, 8},
    };
    time_t now = time(NULL);
    for (unsigned i = 0; i < sizeof s / sizeof s[0]; i++) {
        time_t t = now - s[i].age * 60;
        struct tm tm;
        gmtime_r(&t, &tm);
        char line[160];
        snprintf(line, sizeof line, "DX de %s:  %d.%d  %s  %s  %02d%02dZ", s[i].spotter, s[i].khz10 / 10,
                 s[i].khz10 % 10, s[i].call, s[i].comment, tm.tm_hour, tm.tm_min);
        cluster_client_feed_line(line);
    }
    /* sh/dx reply rows at connect: DXSpider and CC Cluster layouts, one
     * 2 hours old (dropped) and one repeating a live spot (deduped) */
    static const struct { const char *fmt; int age; } sh[] = {
        {"%8.1f  %-11s %s %s %-28s<%s>", 25},
        {"%.1f %s  %s %s  %s  <%s>", 40},
        {"%8.1f  %-11s %s %s %-28s<%s>", 120},
        {"%8.1f  %-11s %s %s %-28s<%s>", 30},
    };
    static const char *sh_call[] = {"T32AZ", "A45XR", "OLD1AB", "VP8LP"};
    static const double sh_khz[] = {14080.0, 14260.5, 14100.0, 14023.6};
    for (unsigned i = 0; i < sizeof sh / sizeof sh[0]; i++) {
        time_t t = now - sh[i].age * 60;
        struct tm tm;
        gmtime_r(&t, &tm);
        char date[16], hm[8], line[160];
        strftime(date, sizeof date, "%e-%b-%Y", &tm);
        strftime(hm, sizeof hm, "%H%MZ", &tm);
        snprintf(line, sizeof line, sh[i].fmt, sh_khz[i], sh_call[i], date, hm, "RTTY sh/dx", "K2XX");
        cluster_client_feed_line(line);
    }
    {
        static cluster_spot_t snap[CLUSTER_MAX_SPOTS];
        uint32_t gen = 0;
        int n = cluster_client_snapshot(snap, CLUSTER_MAX_SPOTS, &gen);
        printf("cluster: %d spots stored (expected 12)\n", n);
    }
    FILE *conf = fopen("cluster.conf", "w"); /* CLUSTER_CONF for the harness */
    fputs("host=dxc.ve7cc.net\nport=23\nserver=VE7CC|dxc.ve7cc.net|23\n"
          "server=IK5PWJ|dxc.ik5pwj.it|8000\nserver=DX Summit|dxc.dxsummit.fi|8000\n", conf);
    fclose(conf);
    harness_fg_freq = 14195000;
    dialog_construct(dialog_cluster, lv_scr_act());
    pump(600);
    shot("cluster_1.ppm");
    uint32_t key = LV_KEY_RIGHT;
    lv_event_send(dialog_cluster->obj, LV_EVENT_KEY, &key);
    lv_event_send(dialog_cluster->obj, LV_EVENT_KEY, &key);
    pump(100);
    shot("cluster_2.ppm");
    /* Page 2, then the Server window */
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[0]->press(pg->items[0]);
    pump(100);
    shot("cluster_3.ppm");
    pg = buttons_get_cur_page();
    pg->items[1]->press(pg->items[1]);
    pump(100);
    shot("cluster_4.ppm");
    lv_event_send(dialog_cluster->obj, LV_EVENT_KEY, &key); /* IK5PWJ */
    pump(100);
    shot("cluster_4b.ppm");
    pg->items[1]->press(pg->items[1]); /* Server again closes the list */
    pump(100);
    /* Tuning: the VFO row stays in the middle, also at the band edge */
    harness_fg_freq = 14250000;
    pump(400);
    shot("cluster_5.ppm");
    harness_fg_freq = 14005000;
    pump(400);
    shot("cluster_6.ppm");
    dialog_destruct();
    pump(200);
}

/* Fake Bluetooth tools in PATH: adapter up, a device and an A2DP sink. */
static void fake_bt_tools(void) {
    static const char *tools[][2] = {
        {"hciconfig", "echo '\tUP RUNNING PSCAN'"},
        {"hcitool", "printf 'Connections:\n\t> ACL 11:22:33:44:55:66 handle 1\n'"},
        {"pactl", "printf '1\tbluez_sink.11_22_33_44_55_66.a2dp_sink\n'"},
    };
    char path[256], cwd[200];
    mkdir("fakebin", 0755);
    for (unsigned i = 0; i < sizeof tools / sizeof tools[0]; i++) {
        snprintf(path, sizeof path, "fakebin/%s", tools[i][0]);
        FILE *f = fopen(path, "w");
        fprintf(f, "#!/bin/sh\n%s\n", tools[i][1]);
        fclose(f);
        chmod(path, 0755);
    }
    snprintf(path, sizeof path, "%s/fakebin:%s", getcwd(cwd, sizeof cwd), getenv("PATH"));
    setenv("PATH", path, 1);
}

/* The main screen: top bar (info, S-meter, clock), the frequency strip and
 * the spectrum / frequency row / waterfall stack (stand-ins, the real row).
 * GPS fix then saved QTH, both themes, spectrum off, zoom x8, the TX panel,
 * then apps under the strip. */
static void topbar(void) {
    extern bool harness_gps_fix;
    lv_obj_t *scr = lv_scr_act();
    int32_t   f   = 7127000;

    /* Spectrum on: under the strip, 96 px (main_screen.c), row, waterfall */
    lv_obj_t *stack = lv_obj_create(scr);
    lv_obj_remove_style_all(stack);
    lv_obj_set_size(stack, 800, 480);
    lv_obj_t *spectrum = lv_obj_create(stack);
    lv_obj_remove_style_all(spectrum);
    lv_obj_set_pos(spectrum, 0, FREQ_STRIP_Y + FREQ_STRIP_H);
    lv_obj_set_size(spectrum, 800, 96);
    lv_obj_set_style_bg_color(spectrum, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(spectrum, LV_OPA_COVER, 0);
    freq_scale_init(stack, FREQ_STRIP_Y + FREQ_STRIP_H + 96);
    lv_obj_t *wf = lv_obj_create(stack);
    lv_obj_remove_style_all(wf);
    lv_obj_set_pos(wf, 0, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36 + SPOT_MARKS_H);
    lv_obj_set_size(wf, 800, 480);
    lv_obj_set_style_bg_color(wf, lv_color_hex(0x0a1a44), 0);
    lv_obj_set_style_bg_opa(wf, LV_OPA_COVER, 0);
    freq_scale_lines_init(stack, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36 + SPOT_MARKS_H, 480);
    freq_scale_update(f, 50000, 0xffffff);
    {   /* fake spots across 7.077-7.177 (age in min); the last ones crowd */
        static const struct { const char *call; int khz10, age; } sp[] = {
            {"IK1ABC", 70780, 1}, {"DL2XYZ", 70915, 5}, {"F5LMN", 71035, 20}, {"OK1AB", 71200, 45},
            {"EA3QQ", 71350, 8}, {"SP9XX", 71490, 25}, {"OH2BH", 71500, 0}, {"9A1CC", 71520, 3},
            {"HB9ZZ", 71540, 15}, {"LZ1ABC", 71555, 1}, {"OE3WXY", 71700, 2}, {"EA8/DL1AAA", 71760, 3},
        };
        time_t now = time(NULL);
        for (unsigned i = 0; i < sizeof sp / sizeof sp[0]; i++) {
            time_t t = now - sp[i].age * 60;
            struct tm tm;
            gmtime_r(&t, &tm);
            char line[160];
            snprintf(line, sizeof line, "DX de K1ABC:  %d.%d  %s  CW  %02d%02dZ", sp[i].khz10 / 10,
                     sp[i].khz10 % 10, sp[i].call, tm.tm_hour, tm.tm_min);
            cluster_client_feed_line(line);
        }
        static const struct { const char *call; int khz10, age; } o[] = {   /* other bands, and more 40m for the column */
            {"JA1XYZ", 140741, 1}, {"K1TTT", 142050, 8}, {"VK3ZZ", 142950, 17}, {"OH2BH", 70320, 41},
            {"EA5XX", 70410, 26}, {"DL2ABC", 70320, 4}, {"UA3AA", 71180, 12}, {"I2ABC", 71260, 33},
            {"G4XYZ", 71300, 0},
        };
        for (unsigned i = 0; i < sizeof o / sizeof o[0]; i++) {
            time_t t = now - o[i].age * 60;
            struct tm tm;
            gmtime_r(&t, &tm);
            char line[160];
            snprintf(line, sizeof line, "DX de K1ABC:  %d.%d  %s  CW  %02d%02dZ", o[i].khz10 / 10,
                     o[i].khz10 % 10, o[i].call, tm.tm_hour, tm.tm_min);
            cluster_client_feed_line(line);
        }
    }
    cluster_column_init(stack, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36 + SPOT_MARKS_H);
    spot_marks_init(stack, FREQ_STRIP_Y + FREQ_STRIP_H + 96 - 36, 36, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36);
    spot_marks_update(f, 50000);
    lv_obj_move_background(stack);   /* created first in main_screen.c */

    extern bool harness_notify;
    harness_notify = true;
    fake_bt_tools();
    FILE *fp = fopen("btcat.clients", "w");
    fprintf(fp, "1\n");
    fclose(fp);
    cluster_client_start("127.0.0.1", 1, "IU1WXY"); /* connecting: idle icon */

    harness_fg_freq = f;
    clock_update_power(12.0f, 7.8f, 80, false);
    clock_init(scr);
    info_init(scr);
    lv_msg_send(MSG_WIFI_STATE_CHANGED, NULL);
    meter_init(scr);
    lv_obj_t *tx = tx_info_init(scr);
    freq_strip_set_label("", 0);   /* refresh: the harness subscriptions do nothing */
    for (int i = 0; i < 20; i++) {
        meter_update(getenv("METER_DB") ? atof(getenv("METER_DB")) : -78.2f, 0.5f);
        pump(10);
    }
    buttons_load_page(&buttons_page_vol_1);
    pump(300);
    shot("topbar_1.ppm");          /* GPS fix: locator with the dot */
    harness_gps_fix = false;
    pump(3100);
    shot("topbar_2.ppm");          /* no fix: the saved QTH */
    styles_set_theme(THEME_LEGACY);
    lv_obj_report_style_change(NULL);
    pump(100);
    shot("topbar_3.ppm");
    styles_set_theme(THEME_SIMPLE);
    lv_obj_report_style_change(NULL);
    /* Spectrum off: the row right under the strip; zoom x8 */
    lv_obj_add_flag(spectrum, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(stack, -96);
    spot_marks_layout(FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36 + SPOT_MARKS_H, 24, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36); /* lines: top of the waterfall */
    freq_scale_update(f, 50000 / 8, 0xffffff);
    spot_marks_update(f, 50000 / 8);
    pump(1100);
    shot("topbar_4.ppm");
    freq_scale_update(f, 50000, 0xffffff);
    spot_marks_update(f, 50000);
    pump(100);
    shot("topbar_4b.ppm");         /* spectrum off, zoom x1 */
    freq_scale_update(f, 50000 / 8, 0xffffff);
    spot_marks_update(f, 50000 / 8);
    spot_marks_layout(FREQ_STRIP_Y + FREQ_STRIP_H + 96 - 36, 36, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36);
    lv_obj_clear_flag(spectrum, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(stack, 0);
    freq_scale_update(f, 50000, 0xffffff);
    spot_marks_update(f, 50000);
    harness_gps_fix = true;
    pump(3100);
    lv_event_send(tx, EVENT_RADIO_TX, NULL);
    for (int i = 0; i < 60; i++) {
        tx_info_update(5.2f, 1.6f, 7.0f);
        pump(10);
    }
    shot("topbar_5.ppm");
    lv_event_send(tx, EVENT_RADIO_RX, NULL);
    /* Apps under the strip */
    harness_fg_freq = 14230000;
    freq_strip_set_label("", 0);
    dialog_construct(dialog_sstv, scr);
    pump(300);
    shot("topbar_6.ppm");
    dialog_destruct();
    harness_fg_freq = 517000;
    freq_strip_set_label("NAVTEX - International, English", 517000);
    dialog_construct(dialog_navtex, scr);
    pump(300);
    shot("topbar_7.ppm");
    dialog_destruct();
    harness_fg_freq = 7074000;
    freq_strip_set_label("", 0);
    dialog_construct(dialog_ft8, scr);
    pump(300);
    shot("topbar_8.ppm");
    dialog_destruct();
    /* WeFax, View Big (FAX 2:3, 4th button) */
    harness_fg_freq = 7878500;
    freq_strip_set_label("", 0);
    dialog_construct(dialog_wefax, scr);
    pump(300);
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[0]->press(pg->items[0]);
    pg = buttons_get_cur_page();
    pg->items[3]->press(pg->items[3]);
    pump(300);
    shot("topbar_9.ppm");
    dialog_destruct();
    /* Cluster column: on (40m), other band, spot band off (follows the waterfall), off */
    extern int32_t harness_cluster_column;
    harness_cluster_column = 1;
    harness_fg_freq = 7127000;
    freq_strip_set_label("", 0);
    cluster_column_refresh();
    pump(1100);
    shot("cluster_column_1.ppm");
    harness_fg_freq = 14074000;
    pump(1100);
    shot("cluster_column_2.ppm");
    harness_fg_freq = 7127000;
    cluster_column_layout(FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36);
    pump(1100);
    shot("cluster_column_3.ppm");
    harness_cluster_column = 0;
    cluster_column_refresh();
    shot("cluster_column_4.ppm");
    pump(100);
}

/* Settings: every tab of both button pages. */
static void settings_demo(void) {
    char p[64];
    dialog_construct(dialog_settings, lv_scr_act());
    pump(300);
    for (int pg_n = 0; pg_n < 2; pg_n++) {
        buttons_page_t *pg = buttons_get_cur_page();
        for (int i = 1; i < BUTTONS && pg->items[i]; i++) {
            pg->items[i]->press(pg->items[i]);
            pump(300);
            snprintf(p, sizeof p, "settings_%d_%d.ppm", pg_n + 1, i);
            shot(p);
        }
        pg->items[0]->press(pg->items[0]);
        pump(100);
    }
    /* Audio scrolled to the end (the MFK wraps to the last row) */
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[3]->press(pg->items[3]);
    pump(300);
    lv_group_focus_prev(keyboard_group);
    pump(500);
    shot("settings_audio_end.ppm");
    for (int i = 0; i < 6; i++) lv_group_focus_prev(keyboard_group);
    pump(500);
    shot("settings_audio_mid.ppm");
    /* General: Callsign focused, then pressed (keyboard) */
    pg->items[1]->press(pg->items[1]);
    pump(300);
    shot("settings_station.ppm");
    lv_event_send(lv_group_get_focused(keyboard_group), LV_EVENT_CLICKED, NULL);
    pump(300);
    shot("settings_station_kbd.ppm");
    uint32_t esc = LV_KEY_ESC, enter = LV_KEY_ENTER;
    lv_event_send(textarea_window_text(), LV_EVENT_KEY, &esc);
    pump(300);
    lv_group_focus_next(keyboard_group);   /* QTH locator */
    lv_event_send(lv_group_get_focused(keyboard_group), LV_EVENT_CLICKED, NULL);
    pump(300);
    textarea_window_set("JN45AB");
    lv_event_send(textarea_window_text(), LV_EVENT_KEY, &enter);
    pump(300);
    shot("settings_station_back.ppm");
    dialog_destruct();
    pump(200);
}

/* Settings -> Wireless with fake networks/devices (stubs.c): the MFK walks
 * the rows (focus next), a press (click) opens the action menus. */
static void wireless_demo(void) {
    lv_group_t *g = keyboard_group;
    uint32_t    esc = LV_KEY_ESC;

    params.wifi_enabled.x = true;
    dialog_construct(dialog_settings, lv_scr_act());
    pump(500);
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[4]->press(pg->items[4]);
    pump(300);
    shot("wireless_1.ppm");
    lv_group_focus_next(g);
    lv_group_focus_next(g);   /* MyWiFi */
    pump(500);
    shot("wireless_2.ppm");
    lv_event_send(lv_group_get_focused(g), LV_EVENT_CLICKED, NULL);
    pump(500);
    shot("wireless_3.ppm");
    lv_event_send(lv_group_get_focused(g), LV_EVENT_KEY, &esc);
    pump(500);
    for (int i = 0; i < 7; i++) lv_group_focus_next(g);   /* BT Headphones */
    pump(500);
    shot("wireless_4.ppm");
    lv_event_send(lv_group_get_focused(g), LV_EVENT_CLICKED, NULL);
    pump(500);
    shot("wireless_5.ppm");
    lv_event_send(lv_group_get_focused(g), LV_EVENT_KEY, &esc);
    pump(500);
    lv_group_focus_prev(g);   /* Scan headphones */
    lv_event_send(lv_group_get_focused(g), LV_EVENT_CLICKED, NULL);
    pump(300);
    shot("wireless_6.ppm");
    for (int i = 0; i < 6; i++) lv_group_focus_next(g);   /* down to the audio rows */
    pump(500);
    shot("wireless_7.ppm");
    for (int i = 0; i < 14; i++) lv_group_focus_next(g);   /* down to WFVIEW SERVER */
    lv_group_focus_next(g);   /* Password, last row: scroll to the end */
    pump(300);
    for (lv_obj_t *o = lv_group_get_focused(g); o; o = lv_obj_get_parent(o)) if (lv_obj_get_scroll_bottom(o) > 0) { lv_obj_scroll_by(o, 0, -lv_obj_get_scroll_bottom(o), LV_ANIM_OFF); break; }
    pump(500);
    shot("wireless_8.ppm");
    dialog_destruct();
    pump(200);
}

/* Settings: walk the focus through every control of each tab, report the ones
 * that end up outside the visible area, shot each time the scroll moves. */
static void settings_nav(void) {
    static const char *tabs[] = { "", "general", "display", "audio", "wireless" };
    char p[64];
    params.wifi_enabled.x = true;
    dialog_construct(dialog_settings, lv_scr_act());
    pump(500);
    for (int t = 1; t <= 4; t++) {
        buttons_page_t *pg = buttons_get_cur_page();
        pg->items[t]->press(pg->items[t]);
        pump(500);
        lv_obj_t *first = lv_group_get_focused(keyboard_group);
        int last_scroll = -1, n_shot = 0, n = 0;
        do {
            lv_obj_t *f = lv_group_get_focused(keyboard_group);
            lv_area_t a;
            lv_obj_get_coords(f, &a);
            int scroll = 0;
            for (lv_obj_t *o = f; o; o = lv_obj_get_parent(o)) scroll += lv_obj_get_scroll_y(o);
            printf("[nav] %s #%d y=%d..%d x=%d..%d%s\n", tabs[t], n, a.y1, a.y2, a.x1, a.x2,
                   (a.y1 < 130 || a.y2 > 420 || a.x1 < 0 || a.x2 > 800) ? "  OUTSIDE" : "");
            if (scroll != last_scroll) {
                snprintf(p, sizeof p, "settings_b_%s_%d.ppm", tabs[t], ++n_shot);
                shot(p);
                last_scroll = scroll;
            }
            lv_group_focus_next(keyboard_group);
            pump(400);
            n++;
        } while (lv_group_get_focused(keyboard_group) != first && n < 80);
        printf("[nav] %s: %d controls\n", tabs[t], n);
    }
    dialog_destruct();
    pump(200);
}

static void crash(int sig) {
    void *bt[32];
    int n = backtrace(bt, 32);
    backtrace_symbols_fd(bt, n, 2);
    _exit(128 + sig);
}

int main(int argc, char **argv) {
    signal(SIGSEGV, crash);
    setvbuf(stdout, NULL, _IOLBF, 0);
    lv_init();
    static lv_color_t buf[W * 60];
    static lv_disp_draw_buf_t db;
    lv_disp_draw_buf_init(&db, buf, NULL, W * 60);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = W; drv.ver_res = H; drv.flush_cb = flush_cb; drv.draw_buf = &db;
    lv_disp_drv_register(&drv);

    EVENT_BAND_DOWN = lv_event_register_id();
    EVENT_BAND_UP = lv_event_register_id();
    EVENT_RADIO_TX = lv_event_register_id();
    EVENT_RADIO_RX = lv_event_register_id();
    EVENT_RADIO_TX = lv_event_register_id();
    EVENT_RADIO_RX = lv_event_register_id();
    if (getenv("FONT")) { params.font.x = atoi(getenv("FONT")); fonts_apply(params.font.x); }
    styles_init(THEME_SIMPLE);
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &background_style, 0);
    keyboard_group = lv_group_create();
    strcpy(params.callsign.x, "IU1WXY");
    strcpy(params.qth.x, "JN35");

    buttons_init(scr);
    buttons_load_page(&buttons_page_vol_1);
    freq_strip_init(scr);   /* always shown, as on the radio */
    pump(200);
    shot("00_main.ppm");
    buttons_load_page_group(buttons_group_app);
    for (int i = 0; i < 2; i++) {
        char n[32];
        snprintf(n, sizeof n, "01_app_%d.ppm", i + 1);
        pump(50);
        shot(n);
        buttons_page_t *pg = buttons_get_cur_page();
        pg->items[0]->press(pg->items[0]);
    }
    /* VOL 2:2, MFK 1:2, MFK 2:2 */
    static const int gen_pages[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) {
        char n[32];
        buttons_load_page(buttons_group_gen[gen_pages[i]]);
        pump(50);
        snprintf(n, sizeof n, "01_gen_%d.ppm", gen_pages[i] + 1);
        shot(n);
    }
    /* APP page 1 with the Modes menu open, then the MFK two steps down */
    buttons_load_page_group(buttons_group_app);
    dialog_construct(dialog_modes, scr);
    pump(100);
    shot("02_modes_1.ppm");
    uint32_t key = LV_KEY_RIGHT;
    lv_event_send(dialog_modes->obj, LV_EVENT_KEY, &key);
    lv_event_send(dialog_modes->obj, LV_EVENT_KEY, &key);
    pump(100);
    shot("02_modes_2.ppm");
    dialog_destruct();
    pump(100);
    buttons_load_page(&buttons_page_vol_1);

    const char *only = argc > 1 ? argv[1] : NULL;
    if (only && !strcmp(only, "topbar")) {
        buttons_load_page(&buttons_page_vol_1);
        topbar();
        return 0;
    }
    struct { const char *n; dialog_t **d; int pages; } apps[] = {
        {"ft8", &dialog_ft8, 3}, {"js8", &dialog_js8, 6}, {"sstv", &dialog_sstv, 3},
        {"wefax", &dialog_wefax, 3}, {"navtex", &dialog_navtex, 3},
    };
    for (unsigned i = 0; i < sizeof apps / sizeof apps[0]; i++)
        if (!only || !strcmp(only, apps[i].n)) app(apps[i].n, *apps[i].d, apps[i].pages);
    if (!only || !strcmp(only, "cluster")) cluster_demo();
    if (!only || !strcmp(only, "msg")) {
        dialog_construct(dialog_msg_voice, lv_scr_act());
        pump(300);
        shot("msg_voice_1.ppm");
        buttons_page_t *pg = buttons_get_cur_page();
        pg->items[0]->press(pg->items[0]);
        pump(200);
        shot("msg_voice_2.ppm");
        dialog_destruct();
        pump(200);
        dialog_construct(dialog_msg_cw, lv_scr_act());
        pump(300);
        shot("msg_cw_1.ppm");
        dialog_destruct();
        pump(200);
    }
    if (!only || !strcmp(only, "settings")) settings_demo();
    if (!only || !strcmp(only, "wireless")) wireless_demo();
    if (only && !strcmp(only, "settings_nav")) settings_nav();
    if (!only || !strcmp(only, "bookmarks")) {
        bookmarks("navtex", dialog_navtex, 517000);
        bookmarks("wefax", dialog_wefax, 7878500);
    }
    return 0;
}
