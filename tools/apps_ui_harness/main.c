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
#include "dialog_msg_voice.h"
#include "dialog_msg_cw.h"
#include "cluster/client.h"
#include "textarea_window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define W 800
#define H 480
static uint32_t fb[W * H];

params_t    params;
lv_group_t *keyboard_group;

void scheduler_work(void);
extern uint32_t EVENT_BAND_DOWN;
extern uint32_t EVENT_BAND_UP;

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
        {"EA8/G4XX", "F5ZZZ", "SSB 59", 141550, 10},    {"3B8M", "IZ1BBB", "SSB listening up 5", 141930, 1},
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
    /* Tuning: the VFO row stays in the middle, also at the band edge */
    textarea_window_close();
    harness_fg_freq = 14250000;
    pump(400);
    shot("cluster_5.ppm");
    harness_fg_freq = 14005000;
    pump(400);
    shot("cluster_6.ppm");
    dialog_destruct();
    pump(200);
}

int main(int argc, char **argv) {
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
    styles_init(THEME_SIMPLE);
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &background_style, 0);
    keyboard_group = lv_group_create();
    strcpy(params.callsign.x, "IU1WXY");
    strcpy(params.qth.x, "JN35");

    buttons_init(scr);
    buttons_load_page(&buttons_page_vol_1);
    pump(200);
    shot("00_main.ppm");
    buttons_load_page_group(buttons_group_app);
    for (int i = 0; i < 3; i++) {
        char n[32];
        snprintf(n, sizeof n, "01_app_%d.ppm", i + 1);
        pump(50);
        shot(n);
        buttons_page_t *pg = buttons_get_cur_page();
        pg->items[0]->press(pg->items[0]);
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
    if (!only || !strcmp(only, "bookmarks")) {
        bookmarks("navtex", dialog_navtex, 517000);
        bookmarks("wefax", dialog_wefax, 7878500);
    }
    return 0;
}
