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
    for (int i = 0; i < 4; i++) {
        char n[32];
        snprintf(n, sizeof n, "01_app_%d.ppm", i + 1);
        pump(50);
        shot(n);
        buttons_page_t *pg = buttons_get_cur_page();
        pg->items[0]->press(pg->items[0]);
    }
    buttons_load_page(&buttons_page_vol_1);

    const char *only = argc > 1 ? argv[1] : NULL;
    struct { const char *n; dialog_t **d; int pages; } apps[] = {
        {"ft8", &dialog_ft8, 3}, {"js8", &dialog_js8, 6}, {"sstv", &dialog_sstv, 3},
        {"wefax", &dialog_wefax, 3}, {"navtex", &dialog_navtex, 3},
    };
    for (unsigned i = 0; i < sizeof apps / sizeof apps[0]; i++)
        if (!only || !strcmp(only, apps[i].n)) app(apps[i].n, *apps[i].d, apps[i].pages);
    return 0;
}
