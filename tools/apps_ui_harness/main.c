/* Headless render of the app dialogs with the real button bar. */
#include "lvgl/lvgl.h"
#include "buttons.h"
#include "dialog.h"
#include "styles.h"
#include "theme.h"
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
#include "band_plan.h"
#include "cluster_column.h"
#include "dialog_msg_voice.h"
#include "dialog_msg_cw.h"
#include "dialog_cw_kbd.h"
#include "dialog_cw_trainer.h"
#include "dialog_wspr.h"
#include "dialog_field.h"
#include "dialog_bands.h"
#include "dialog_swrscan.h"
#include "field/flog.h"
#include "field/rbn.h"
#include "wspr/wspr.h"
#include "cw_encoder.h"
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
#include <sndfile.h>
#include <math.h>
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

/* Voice keyer: fake messages, Auto call in the pause between two calls */
static void voicekeyer(void) {
    static const char *names[] = {"CQ 20m", "CQ contest IU1WXY", "Message 3"};
    mkdir("msg_demo", 0755);
    for (int i = 0; i < 3; i++) {
        char p[64];
        SF_INFO info = {.samplerate = 8000, .channels = 1, .format = SF_FORMAT_WAV | SF_FORMAT_PCM_16};
        snprintf(p, sizeof p, "msg_demo/%s.mp3", names[i]);
        SNDFILE *f = sf_open(p, SFM_WRITE, &info);
        short b[800];
        for (int k = 0; k < 800; k++) b[k] = (short)(8000 * sin(k * 0.3));
        for (int k = 0; k < 10 * (i + 2); k++) sf_write_short(f, b, 800);
        sf_close(f);
    }
    params.voice_msg_period = 30;
    dialog_construct(dialog_msg_voice, lv_scr_act());
    pump(300);
    shot("voice_keyer_1.ppm");                 /* page 1, ready */
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[4]->press(pg->items[4]);         /* Auto call */
    pump(1500);                                /* call played, now in the pause */
    shot("voice_keyer_2.ppm");
    pg->items[4]->press(pg->items[4]);         /* stop */
    pump(400);
    pg->items[0]->press(pg->items[0]);
    pump(200);
    pg = buttons_get_cur_page();
    pg->items[1]->press(pg->items[1]);         /* Interval: MFK adjusts it */
    uint32_t key = LV_KEY_RIGHT;
    for (int i = 0; i < 15; i++) lv_event_send(dialog_msg_voice->obj, LV_EVENT_KEY, &key);
    pump(200);
    shot("voice_keyer_3.ppm");                 /* page 2, interval 45 s */
    dialog_destruct();
    pump(200);
}

/* WSPR: empty, then three beacons through the real RX chain (synth, noise,
 * baseband recorder, wsprd) shown in the table, then the TX % button. */
static void wspr_demo(void) {
    static const struct { const char *call, *grid; int dbm; float hz, snr; } tx[] = {
        {"IU1WXY", "JN35", 37, 1450.0f, -12.0f},
        {"K1ABC", "FN42", 30, 1530.0f, -22.0f},
        {"DL2XYZ", "JO62", 23, 1575.0f, -26.0f},
    };
    const int   rate = 11025, len = rate * 116;
    float      *audio = calloc(len, sizeof(float));
    float      *i = malloc(sizeof(float) * WSPR_SAMPLES), *q = malloc(sizeof(float) * WSPR_SAMPLES);
    wspr_spot_t spots[16];

    extern int32_t harness_fg_freq;
    harness_fg_freq = 14095600;
    dialog_construct(dialog_wspr, lv_scr_act());
    pump(400);
    shot("wspr_1.ppm");

    srand(7);
    for (int k = 0; k < len; k++) {     /* unit noise (Box-Muller) */
        float u1 = (rand() + 1.0f) / (RAND_MAX + 2.0f), u2 = rand() / (float)RAND_MAX;
        audio[k] = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
    }
    for (size_t t = 0; t < sizeof(tx) / sizeof(tx[0]); t++) {
        uint8_t  sym[WSPR_SYMBOLS];
        uint32_t n;
        /* SNR in 2500 Hz from unit noise over the 5512 Hz audio band */
        float    a = sqrtf(2.0f) * powf(10.0f, tx[t].snr / 20.0f) / sqrtf(rate / 2 / 2500.0f);

        wspr_encode(tx[t].call, tx[t].grid, tx[t].dbm, sym);
        int16_t *s = wspr_synth(sym, tx[t].hz, rate, &n);
        for (uint32_t k = 0; k < n && k + rate < (uint32_t)len; k++) {
            audio[k + rate] += a * s[k] / (32767.0f * 0.8f);
        }
        free(s);
    }
    wspr_bb_t *bb = wspr_bb_create(rate);
    for (int k = 0; k < len; k += 441) {
        wspr_bb_feed(bb, audio + k, len - k < 441 ? len - k : 441);
    }
    unsigned ns = wspr_bb_take(bb, i, q);
    int      n  = wspr_decode_iq(i, q, ns, 14095600, spots, 16);
    printf("wspr: %d decoded\n", n);
    for (int k = 0; k < n; k++) {
        printf("  %s %s %d dBm, %d dB, %.0f Hz\n", spots[k].call, spots[k].loc, spots[k].dbm, spots[k].snr, spots[k].freq_hz);
    }
    dialog_wspr_show_spots(spots, n);
    pump(300);
    shot("wspr_2.ppm");
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[1]->press(pg->items[1]);  /* TX 0% -> 10% */
    pg->items[3]->press(pg->items[3]);  /* Upload on */
    pump(300);
    shot("wspr_3.ppm");
    pg->items[1]->press(pg->items[1]);
    pg->items[1]->press(pg->items[1]);
    pg->items[1]->press(pg->items[1]);
    pg->items[1]->press(pg->items[1]);  /* 50% -> 0% */
    pg->items[3]->press(pg->items[3]);
    dialog_destruct();
    pump(200);
    wspr_bb_destroy(bb);
    free(audio);
    free(i);
    free(q);
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

/* The frequency bookmarks of an app: the Freq. button is button btn of page page + 1. */
static void bookmarks(const char *name, dialog_t *d, int32_t freq, int page, int btn) {
    char p[64];
    harness_fg_freq = freq;
    dialog_construct(d, lv_scr_act());
    pump(300);
    buttons_page_t *pg = buttons_get_cur_page();
    for (int i = 0; i < page; i++) {
        pg->items[0]->press(pg->items[0]);
        pg = buttons_get_cur_page();
    }
    pg->items[btn]->press(pg->items[btn]);   /* Freq. */
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

/* SSTV TX: the menu, a target callsign, Replay with picture, text and
 * colours, then the transmission (stubbed tx_player) and Halt TX. */
static void sstv_key(uint32_t key, int n) {
    for (int i = 0; i < n; i++) lv_event_send(lv_group_get_focused(keyboard_group), LV_EVENT_KEY, &key);
    pump(100);
}
static void sstv_press(int ms) {
    lv_event_send(lv_group_get_focused(keyboard_group), LV_EVENT_PRESSED, NULL);
    pump(ms);
}
static void sstv_tx_demo(void) {
    system("rm -rf sstv_demo && mkdir -p sstv_demo && cp -r '" SSTV_DEFAULTS "/tx_pics' '" SSTV_DEFAULTS "/tx_text' sstv_demo/");
    params_str_set(&params.callsign, "IU1WXY");
    harness_fg_freq = 14230000;
    dialog_construct(dialog_sstv, lv_scr_act());
    pump(300);
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[4]->press(pg->items[4]);   /* TX */
    pump(200);
    shot("sstv_tx_1.ppm");               /* the TX menu */
    sstv_key(LV_KEY_DOWN, 5);            /* SET TARGET CALLSIGN */
    sstv_press(200);
    textarea_window_set("IK1ABC");
    pump(100);
    shot("sstv_tx_2.ppm");               /* keyboard */
    uint32_t enter = LV_KEY_ENTER;
    lv_event_send(textarea_window_text(), LV_EVENT_KEY, &enter);
    pump(200);
    sstv_key(LV_KEY_UP, 3);              /* REPLAY TO IK1ABC */
    shot("sstv_tx_3.ppm");
    sstv_press(600);                     /* pictures, the first one loaded */
    sstv_key(LV_KEY_DOWN, 1);
    pg = buttons_get_cur_page();
    for (int i = 0; i < 3; i++) pg->items[3]->press(pg->items[3]);   /* TX Mode: Martin M1 */
    pump(500);
    shot("sstv_tx_4.ppm");
    sstv_press(300);                     /* text templates */
    sstv_key(LV_KEY_DOWN, 4);
    pump(200);
    shot("sstv_tx_5.ppm");
    sstv_press(300);                     /* colours */
    sstv_key(LV_KEY_DOWN, 2);
    pump(500);
    shot("sstv_tx_6.ppm");
    sstv_press(1500);                    /* transmitting */
    shot("sstv_tx_7.ppm");
    pg = buttons_get_cur_page();
    pg->items[4]->press(pg->items[4]);   /* Halt TX */
    pump(500);
    shot("sstv_tx_8.ppm");               /* back to Auto RX */
    pg->items[0]->press(pg->items[0]);
    pump(100);
    shot("sstv_tx_9.ppm");               /* page 2: Freq., browser, restart */
    pg = buttons_get_cur_page();
    pg->items[2]->press(pg->items[2]);   /* RX pictures browser */
    pump(200);
    shot("sstv_tx_10.ppm");
    pg->items[2]->press(pg->items[2]);
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
          "server=IK5PWJ|dxc.ik5pwj.it|8000\nserver=DX Summit|dxc.dxsummit.fi|8000\n"
          "alert=VP8*|20,17|CW\nalert=3B8M||\nalert_popup=1\n", conf);
    fclose(conf);
    harness_fg_freq = 14195000;
    dialog_cluster_start_client(); /* the alert popup comes from the background timer */
    pump(2100);
    shot("cluster_popup.ppm");
    bool pressed = dialog_cluster_alert_press();
    printf("cluster: popup press tunes: %d (expected 1), again: %d (expected 0)\n", pressed,
           dialog_cluster_alert_press());
    dialog_construct(dialog_cluster, lv_scr_act());
    pump(600);
    shot("cluster_1.ppm");
    {
        buttons_page_t *p1 = buttons_get_cur_page();
        p1->items[3]->press(p1->items[3]); /* Mode: CW */
        pump(300);
        shot("cluster_mode_cw.ppm");
        p1->items[3]->press(p1->items[3]);
        p1->items[3]->press(p1->items[3]);
        p1->items[3]->press(p1->items[3]); /* back to All */
        pump(300);
    }
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
    pg->items[2]->press(pg->items[2]); /* Order: newest first */
    pump(300);
    shot("cluster_time.ppm");
    pg->items[2]->press(pg->items[2]); /* back to by frequency */
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
    lv_obj_set_style_bg_color(spectrum, TC(panel), 0);
    lv_obj_set_style_bg_opa(spectrum, LV_OPA_COVER, 0);
    freq_scale_init(stack, FREQ_STRIP_Y + FREQ_STRIP_H + 96);
    lv_obj_t *wf = lv_obj_create(stack);
    lv_obj_remove_style_all(wf);
    lv_obj_set_pos(wf, 0, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36 + SPOT_MARKS_H);
    lv_obj_set_size(wf, 800, 480);
    lv_obj_set_style_bg_color(wf, (lv_color_t){.full = theme_waterfall()[16]}, 0);   /* the noise floor */
    lv_obj_set_style_bg_opa(wf, LV_OPA_COVER, 0);
    freq_scale_lines_init(stack, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36 + SPOT_MARKS_H, 480);
    freq_scale_update(f, 50000, theme_hex(TC_text));
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
    band_plan_init(stack, FREQ_STRIP_Y + FREQ_STRIP_H + 96 - BAND_PLAN_H);
    band_plan_update(f, 50000);
    spot_marks_init(stack, FREQ_STRIP_Y + FREQ_STRIP_H + 96 - 36, 36, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36);
    spot_marks_update(f, 50000);
    band_plan_update(f, 50000);
    cluster_column_init(stack, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36 + SPOT_MARKS_H);   /* over the spot calls, as main_screen.c */
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
    {   /* Waterfall Fixed: the span stays put, the VFO line is 30 kHz right of the centre */
        freq_scale_update(f - 30000, 50000, theme_hex(TC_text));
        spot_marks_update(f - 30000, 50000);
        band_plan_update(f - 30000, 50000);
        lv_obj_t *l1 = lv_obj_create(spectrum), *l2 = lv_obj_create(wf);
        lv_obj_t *ls[2] = {l1, l2};
        for (int i = 0; i < 2; i++) {
            lv_obj_remove_style_all(ls[i]);
            lv_obj_set_style_bg_color(ls[i], lv_color_hex(0xFFA000), 0);
            lv_obj_set_style_bg_opa(ls[i], LV_OPA_COVER, 0);
            lv_obj_set_size(ls[i], 2, i ? 200 : 96);
            lv_obj_set_pos(ls[i], 400 + 30000 * 800 / 100000 - 1, 0);
        }
        pump(100);
        shot("wf_mode_fixed_raw.ppm");
        lv_obj_del(l1);
        lv_obj_del(l2);
        freq_scale_update(f, 50000, theme_hex(TC_text));
        spot_marks_update(f, 50000);
        band_plan_update(f, 50000);
    }
    harness_gps_fix = false;
    pump(3100);
    shot("topbar_2.ppm");          /* no fix: the saved QTH */
    theme_init("Legacy");
    styles_apply();
    lv_obj_report_style_change(NULL);
    pump(100);
    shot("topbar_3.ppm");
    theme_init("Outdoor");
    styles_apply();
    lv_obj_report_style_change(NULL);
    pump(100);
    shot("topbar_outdoor.ppm");
    theme_init(getenv("THEME"));    /* back to the theme of the run */
    styles_apply();
    lv_obj_report_style_change(NULL);
    /* Band plan: zoom x8 on the CW/DIG edge, a band edge with beacons, out of any band */
    freq_scale_update(7040000, 50000 / 8, theme_hex(TC_text));
    band_plan_update(7040000, 50000 / 8);
    pump(100);
    shot("topbar_bp1.ppm");
    freq_scale_update(14100000, 50000, theme_hex(TC_text));
    band_plan_update(14100000, 50000);
    pump(100);
    shot("topbar_bp2.ppm");
    freq_scale_update(13500000, 50000, theme_hex(TC_text));
    band_plan_update(13500000, 50000);
    pump(100);
    shot("topbar_bp3.ppm");
    freq_scale_update(f, 50000, theme_hex(TC_text));
    band_plan_update(f, 50000);
    /* Spectrum off: the row right under the strip; zoom x8 */
    band_plan_set_y(FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36 + SPOT_MARKS_H); /* top of the waterfall */
    lv_obj_add_flag(spectrum, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(stack, -96);
    spot_marks_layout(FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36 + SPOT_MARKS_H, 24, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36); /* lines: top of the waterfall */
    freq_scale_update(f, 50000 / 8, theme_hex(TC_text));
    spot_marks_update(f, 50000 / 8);
    band_plan_update(f, 50000 / 8);
    pump(1100);
    shot("topbar_4.ppm");
    freq_scale_update(f, 50000, theme_hex(TC_text));
    spot_marks_update(f, 50000);
    band_plan_update(f, 50000);
    pump(100);
    shot("topbar_4b.ppm");         /* spectrum off, zoom x1 */
    freq_scale_update(f, 50000 / 8, theme_hex(TC_text));
    spot_marks_update(f, 50000 / 8);
    band_plan_update(f, 50000 / 8);
    spot_marks_layout(FREQ_STRIP_Y + FREQ_STRIP_H + 96 - 36, 36, FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36);
    lv_obj_clear_flag(spectrum, LV_OBJ_FLAG_HIDDEN);
    band_plan_set_y(FREQ_STRIP_Y + FREQ_STRIP_H + 96 - BAND_PLAN_H);
    lv_obj_set_y(stack, 0);
    freq_scale_update(f, 50000, theme_hex(TC_text));
    spot_marks_update(f, 50000);
    band_plan_update(f, 50000);
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
    extern int32_t harness_cluster_order, harness_cluster_all_bands;
    harness_cluster_order = 1;      /* newest first, then all bands */
    pump(1100);
    shot("cluster_column_time.ppm");
    harness_cluster_all_bands = 1;
    pump(1100);
    shot("cluster_column_time_all.ppm");
    harness_cluster_order = harness_cluster_all_bands = 0;
    pump(1100);
    cluster_column_set_active(true);
    cluster_column_move(1);
    pump(100);
    shot("cluster_mfk.ppm");
    cluster_column_set_active(false);
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
    /* As main_screen.c with the spectrum off (the only layout now): band plan
     * on top of the waterfall, spot lines under it, calls on the waterfall */
    {
        lv_coord_t y = FREQ_STRIP_Y + FREQ_STRIP_H + 96 + 36;   /* stack coords, stack at -96 */

        lv_obj_add_flag(spectrum, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(stack, -96);
        lv_obj_set_y(wf, y);
        freq_scale_lines_set_y(y);
        band_plan_set_y(y);
        spot_marks_layout(y, BAND_PLAN_H + 8, y + BAND_PLAN_H + 8);
        harness_cluster_column = 1;
        cluster_column_layout(y + BAND_PLAN_H);
        cluster_column_refresh();
        freq_scale_update(7127000, 50000, theme_hex(TC_text));
        spot_marks_update(7127000, 50000);
        band_plan_update(7127000, 50000);
        pump(1100);
        shot("main_readme.ppm");
        harness_cluster_column = 0;
        cluster_column_refresh();
        pump(1100);
        shot("main_readme_nocol.ppm");
    }
}

/* CW trainer: the decoder is fed by hand (harness_decode) with what the
 * paddle would have produced. Learn: right and wrong answers; Practice. */
extern int32_t harness_cwt_level, harness_cwt_group, harness_key_speed, harness_mode;
void harness_decode(const char *t);
void harness_decode_wpm(int w);

static void cwt_answer(const char *s) {
    for (; *s; s++) {
        char c[2] = {*s, 0};
        harness_decode(c);
        pump(50);
    }
}

/* Play, wait for the sidetone to end, answer, wait for the result */
static void cwt_round(const char *answer_or_null) {
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[1]->press(pg->items[1]);   /* Play */
    pump(3500);
    char wrong[8];
    const char *t = dialog_cw_trainer_target();
    if (answer_or_null) snprintf(wrong, sizeof wrong, "%s", answer_or_null);
    else snprintf(wrong, sizeof wrong, "%s", t);
    cwt_answer(wrong);
    pump(300);
}

/* Stats file of the demo (CWT_STATS_FILE in the harness build): a corrupt one
 * must be ignored, then a filled one is shown in the Stats view */
#define CWT_FILE "cw_trainer_demo.json"

static void cwt_write(const char *content) {
    FILE *f = fopen(CWT_FILE, "w");
    fputs(content, f);
    fclose(f);
}

static void cwt_preload(void) {
    time_t    t = time(NULL);
    struct tm tm;
    char      buf[600];
    localtime_r(&t, &tm);
    /* sample histories, newest result in bit 0 */
    snprintf(buf, sizeof buf,
             "{\"date\":%d,\"tries\":46,\"right\":38,\"sec\":1260,\"best_score\":2115,\"best_wpm\":28,\"chars\":"
             "{\"K\":[20,1048575],\"M\":[20,1048063],\"R\":[20,1031103],\"S\":[20,1048575],\"U\":[10,1019],"
             "\"A\":[20,1002255],\"P\":[6,62],\"T\":[5,1],\"L\":[20,1048447]}}",
             (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday);
    cwt_write(buf);
}

/* Wait for the sidetone of the call to end */
static void cwt_wait_sent(void) {
    pump(300);
    for (int i = 0; i < 60 && cw_encoder_state() != CW_ENCODER_IDLE; i++) pump(200);
    pump(300);
}

static void cwtrainer_demo(void) {
    cwt_write("{{ not json \x01\x02");
    harness_key_speed = 30;
    harness_cwt_level = 5;
    buttons_load_page_group(buttons_group_app);
    dialog_construct(dialog_cw_trainer, lv_scr_act());
    pump(200);
    dialog_destruct();
    pump(100);
    printf("corrupt stats file survived\n");
    cwt_preload();
    harness_key_speed = 30;
    harness_cwt_level = 5;   /* K M R S U */
    harness_cwt_group = 1;
    printf("mode before: %d\n", harness_mode);
    buttons_load_page_group(buttons_group_app);
    dialog_construct(dialog_cw_trainer, lv_scr_act());
    pump(300);
    printf("mode in the app: %d (4 = CW expected)\n", harness_mode);
    shot("cwtrainer_0.ppm");
    for (int i = 0; i < 6; i++) cwt_round(NULL);   /* six right answers for the score */
    cwt_round(NULL);
    shot("cwtrainer_1.ppm");                       /* OK */
    harness_cwt_group = 2;
    cwt_round("SS");                               /* wrong (unless it was SS) */
    shot("cwtrainer_2.ppm");                       /* WRONG */
    /* Stats view: page 3 */
    harness_cwt_level = 10;
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[0]->press(pg->items[0]);
    pump(100);
    pg = buttons_get_cur_page();
    pg->items[0]->press(pg->items[0]);
    pump(100);
    pg = buttons_get_cur_page();
    pg->items[1]->press(pg->items[1]);             /* Stats */
    pump(200);
    shot("cwtrainer_5.ppm");
    pg->items[2]->press(pg->items[2]);             /* Reset stats: first press */
    pump(200);
    shot("cwtrainer_6.ppm");                       /* "Sure?" */
    pump(3300);
    shot("cwtrainer_7.ppm");                       /* back to "Reset stats", nothing lost */
    harness_cwt_level = 25;                        /* high level: 20 tiles, the last ones */
    pg->items[1]->press(pg->items[1]);             /* Stats off */
    pg->items[1]->press(pg->items[1]);             /* Stats on */
    pump(200);
    shot("cwtrainer_8.ppm");
    pg->items[1]->press(pg->items[1]);
    harness_cwt_level = 5;
    pump(100);
    pg->items[0]->press(pg->items[0]);             /* page 1 */
    pump(100);
    pg = buttons_get_cur_page();
    /* Practice: free keying */
    pg->items[3]->press(pg->items[3]);
    pump(200);
    static const char *msg[] = {"C", "Q", " ", "C", "Q", " ", "D", "E", " ", "I", "U", "1", "W", "X", "Y", " ",
                                "I", "U", "1", "W", "X", "Y", " ", "K", " ", "T", "N", "X", " ", "G", "M", " ", "U", "R", " ",
                                "5", "9", "9", " ", "F", "B", " ", "T", "U", " ", "7", "3", " ", "K"};
    for (unsigned i = 0; i < sizeof msg / sizeof msg[0]; i++) harness_decode(msg[i]);
    harness_decode_wpm(22);
    pump(300);
    shot("cwtrainer_3.ppm");
    pg->items[0]->press(pg->items[0]);
    pump(100);
    shot("cwtrainer_4.ppm");    /* page 2 */
    pg = buttons_get_cur_page();
    pg->items[0]->press(pg->items[0]);
    pump(100);
    pg = buttons_get_cur_page();
    pg->items[0]->press(pg->items[0]);
    pump(100);
    pg = buttons_get_cur_page();            /* page 1 */

    /* Calls: the speed changes with the answers and is given back */
    pg->items[3]->press(pg->items[3]);
    pump(200);
    shot("cwtrainer_9.ppm");                /* Calls, before Start */
    pg->items[1]->press(pg->items[1]);      /* Start */
    pump(600);
    shot("cwtrainer_10.ppm");               /* Listen... */
    cwt_wait_sent();
    shot("cwtrainer_11.ppm");               /* Your turn */
    char call[16];
    snprintf(call, sizeof call, "%s", dialog_cw_trainer_target());
    printf("call: %s at %d WPM\n", call, (int)harness_key_speed);
    cwt_answer(call);
    pump(300);
    shot("cwtrainer_12.ppm");               /* right: 30 -> 31 */
    pump(1400);                             /* the next call follows by itself */
    cwt_wait_sent();
    snprintf(call, sizeof call, "%s", dialog_cw_trainer_target());
    printf("call: %s at %d WPM (31 expected)\n", call, (int)harness_key_speed);
    call[1] = call[1] == 'X' ? 'Q' : 'X';
    cwt_answer(call);
    pump(300);
    shot("cwtrainer_13.ppm");               /* wrong: 31 -> 30 */
    pg->items[1]->press(pg->items[1]);      /* Pause */
    pump(3000);
    printf("paused, speed %d\n", (int)harness_key_speed);
    pg->items[3]->press(pg->items[3]);      /* Mode: Learn, speed given back */
    pump(100);
    printf("speed after leaving Calls: %d (30 expected)\n", (int)harness_key_speed);
    pg->items[3]->press(pg->items[3]);
    pg->items[3]->press(pg->items[3]);      /* Calls again */
    pg->items[1]->press(pg->items[1]);      /* Start and close while playing */
    pump(800);
    dialog_destruct();
    printf("speed after closing in Calls: %d (30 expected)\n", (int)harness_key_speed);
    pump(100);
    dialog_construct(dialog_cw_trainer, lv_scr_act());
    pump(100);
    dialog_destruct();
    pump(200);
    printf("mode after: %d (3 expected)\n", harness_mode);
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
    /* Keys (page 2) scrolled to the end: the MFK stops on the last row */
    buttons_page_t *pg = buttons_get_cur_page();
    buttons_page_t *pg2 = pg->items[0]->next;
    pg2->items[1]->press(pg2->items[1]);
    pump(300);
    for (int i = 0; i < 60; i++) lv_group_focus_next(keyboard_group);
    pump(500);
    shot("settings_keys_end.ppm");
    /* Display scrolled to the Waterfall mode row (the second dropdown) */
    pg->items[2]->press(pg->items[2]);
    pump(300);
    for (int i = 0, n = 0; i < 40 && n < 2; i++) {
        lv_group_focus_next(keyboard_group);
        if (lv_obj_check_type(lv_group_get_focused(keyboard_group), &lv_dropdown_class)) n++;
    }
    pump(500);
    shot("wf_mode_settings.ppm");
    /* Display scrolled to the Band plan switch (MAIN SCREEN card: the 8th switch of the page) */
    pg->items[2]->press(pg->items[2]);
    pump(300);
    for (int i = 0, n = 0; i < 60 && n < 8; i++) {
        lv_group_focus_next(keyboard_group);
        if (lv_obj_check_type(lv_group_get_focused(keyboard_group), &lv_switch_class)) n++;
    }
    pump(500);
    shot("band_plan_settings.ppm");
    /* Audio scrolled to the end (the MFK stops on the last row) */
    pg->items[3]->press(pg->items[3]);
    pump(300);
    for (int i = 0; i < 60; i++) lv_group_focus_next(keyboard_group);
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
    lv_group_focus_next(g);   /* Password, last control: the page shows its end */
    pump(500);
    shot("wireless_8.ppm");
    dialog_destruct();
    pump(200);
}

/* Settings -> Cluster with some filters on, then a toggle and the file it writes */
static void cluster_filters_demo(void) {
    FILE *conf = fopen("cluster.conf", "w");
    fputs("host=dxc.ve7cc.net\nport=23\nfilter_modes=CW,SSB\nfilter_bands=40,20,15\nfilter_activity=POTA\n", conf);
    fclose(conf);
    dialog_construct(dialog_settings, lv_scr_act());
    pump(300);
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[0]->press(pg->items[0]);
    pg = buttons_get_cur_page();
    pg->items[2]->press(pg->items[2]);
    pump(300);
    shot("cluster_filters_1.ppm");
    for (int i = 0; i < 8; i++) lv_group_focus_next(keyboard_group);   /* 30 m */
    lv_event_send(lv_group_get_focused(keyboard_group), LV_EVENT_RELEASED, NULL);   /* as the MFK press: toggles */
    pump(300);
    shot("cluster_filters_2.ppm");
    char line[128];
    conf = fopen("cluster.conf", "r");
    while (fgets(line, sizeof line, conf)) printf("[conf] %s", line);
    fclose(conf);
    dialog_destruct();
    pump(200);
}

/* Settings: walk the focus through every control of each tab, report the ones
 * that end up outside the visible area, shot each time the scroll moves. */
static void settings_nav(void) {
    char p[64];
    params.wifi_enabled.x = true;
    dialog_construct(dialog_settings, lv_scr_act());
    pump(500);
    for (int pg_n = 0; pg_n < 2; pg_n++) {
      buttons_page_t *pg = buttons_get_cur_page();
      for (int t = 1; t < BUTTONS && pg->items[t]; t++) {
        pg->items[t]->press(pg->items[t]);
        pump(500);
        lv_obj_t *prev = NULL;
        int last_scroll = -1, n_shot = 0, n = 0;
        do {
            lv_obj_t *f = lv_group_get_focused(keyboard_group);
            if (!f) break;
            prev = f;
            lv_area_t a;
            lv_obj_get_coords(f, &a);
            int scroll = 0;
            for (lv_obj_t *o = f; o; o = lv_obj_get_parent(o)) scroll += lv_obj_get_scroll_y(o);
            printf("[nav] %d_%d #%d y=%d..%d x=%d..%d%s\n", pg_n + 1, t, n, a.y1, a.y2, a.x1, a.x2,
                   (a.y1 < 130 || a.y2 > 420 || a.x1 < 0 || a.x2 > 800) ? "  OUTSIDE" : "");
            if (scroll != last_scroll) {
                snprintf(p, sizeof p, "settings_b_%d_%d_%d.ppm", pg_n + 1, t, ++n_shot);
                shot(p);
                last_scroll = scroll;
            }
            lv_group_focus_next(keyboard_group);
            pump(400);
            n++;
        } while (lv_group_get_focused(keyboard_group) != prev && n < 80);   /* the MFK stops on the last control */
        lv_obj_t *pg_obj = prev;
        while (pg_obj && !lv_obj_has_flag(pg_obj, LV_OBJ_FLAG_SCROLLABLE)) pg_obj = lv_obj_get_parent(pg_obj);
        int hidden = pg_obj ? lv_obj_get_scroll_bottom(pg_obj) : 0;
        printf("[nav] %d_%d: %d controls, %d px below the last one%s\n", pg_n + 1, t, n, hidden, hidden > 1 ? "  HIDDEN" : "");
      }
      pg->items[0]->press(pg->items[0]);
      pump(100);
    }
    dialog_destruct();
    pump(200);
}

/* CW keyboard: a line typed and sent, a second one queued, then stopped */
static void cwkbd_demo(void) {
    extern int32_t harness_mode;
    int32_t        mode = harness_mode;
    const char    *line = "CQ CQ DE IU1WXY K";

    harness_mode = x6100_mode_cw;
    dialog_construct(dialog_cw_kbd, lv_scr_act());
    pump(300);
    shot("cwkbd_1.ppm");
    for (const char *c = line; *c; c++) {
        uint32_t k = (unsigned char)*c;
        lv_event_send(dialog_cw_kbd->obj, LV_EVENT_KEY, &k);
    }
    pump(200);
    shot("cwkbd_2.ppm");
    uint32_t enter = LV_KEY_ENTER, esc = LV_KEY_ESC;
    lv_event_send(dialog_cw_kbd->obj, LV_EVENT_KEY, &enter);
    for (const char *c = "tu 5nn"; *c; c++) {
        uint32_t k = (unsigned char)*c;
        lv_event_send(dialog_cw_kbd->obj, LV_EVENT_KEY, &k);
    }
    lv_event_send(dialog_cw_kbd->obj, LV_EVENT_KEY, &enter);
    pump(1500);
    shot("cwkbd_3.ppm");
    lv_event_send(dialog_cw_kbd->obj, LV_EVENT_KEY, &esc);   /* stop, or close when idle (here: the Morse table is a stub, sending is instant) */
    pump(300);
    shot("cwkbd_4.ppm");
    dialog_destruct();
    pump(200);
    harness_mode = mode;
}

static void crash(int sig) {
    void *bt[32];
    int n = backtrace(bt, 32);
    backtrace_symbols_fd(bt, n, 2);
    _exit(128 + sig);
}


/* Field Ops: home, SOTA with the nearest summits, an activation with QSOs, the log, the bands */
static void field_key(uint32_t key) {
    lv_event_send(dialog_field->obj, LV_EVENT_KEY, &key);
    pump(100);
}

static void field_press(int n) {
    buttons_page_t *pg = buttons_get_cur_page();
    pg->items[n]->press(pg->items[n]);
    pump(100);
}

static void field_demo(void) {
    system("rm -rf field_demo; mkdir -p field_demo");
    FILE *f = fopen("field_demo/refs.tsv", "w");
    fputs("S\tI/LO-318\tMonte Levanna\t45.40000\t7.20000\t3619\t10\n"
          "S\tI/LO-120\tMonte Penice\t44.79000\t9.31700\t1460\t4\n"
          "S\tI/LO-205\tMonte Lesima\t44.68300\t9.25800\t1724\t6\n"
          "S\tHB/TI-157\tMonte Generoso\t45.92800\t9.01800\t1701\t6\n"
          "S\tI/LO-001\tPizzo Coca\t46.07000\t10.00000\t3050\t10\n"
          "P\tIT-0216\tParco del Ticino\t45.40000\t8.90000\t\t\n"
          "P\tIT-0420\tParco Nord Milano\t45.53000\t9.19000\t\t\n"
          "P\tIT-0011\tParco dello Stelvio\t46.48000\t10.50000\t\t\n", f);
    fclose(f);
    dialog_construct(dialog_field, lv_scr_act());
    pump(300);
    shot("field_1_home.ppm");
    field_key(LV_KEY_ENTER);            /* SOTA */
    pump(1500);                         /* the lookup thread */
    shot("field_2_sota.ppm");
    field_key(LV_KEY_DOWN);
    field_key(LV_KEY_ENTER);            /* the second nearest */
    field_press(1);                     /* Start */
    char log[200] = "", line[300];
    f = fopen("field_demo/state", "r");
    while (f && fgets(line, sizeof line, f))
        if (!strncmp(line, "log=", 4))
            sscanf(line + 4, "%199s", log);
    if (f)
        fclose(f);
    field_station_t st = {"IU1WXY/P", "JN45", 'S', "I/LO-120"};
    const char *calls[] = {"DL8ABC", "F5XYZ", "IK1ABC", "SP9XYZ", "G4ABC"};
    for (int k = 0; k < 5; k++) {
        field_qso_t q = {time(NULL) - 600 + k * 120, "", 14285000, "SSB", "59", "57"};
        strcpy(q.call, calls[k]);
        field_log_add(log, &q, &st);
    }
    field_key(LV_KEY_ESC);
    field_key(LV_KEY_ENTER);            /* SOTA again: active, QSOs counted? (read at the log screen) */
    shot("field_3_active.ppm");
    field_key(LV_KEY_ESC);
    field_key(LV_KEY_RIGHT);
    field_key(LV_KEY_RIGHT);
    field_rbn_start("IU1WXY");
    time_t now = time(NULL);
    const char *rbn[] = {
        "DX de DL8ABC-#:  14062.0  IU1WXY  CW  7 dB  22 WPM  CQ  1234Z",
        "DX de F5XYZ-#:   14062.1  IU1WXY  CW  4 dB  22 WPM  CQ  1234Z",
        "DX de G4ABC-#:   14062.0  IU1WXY  CW  -2 dB  22 WPM  CQ  1234Z",
        "DX de EA8DEF-#:  14025.0  K1ABC   CW  12 dB  25 WPM  CQ  1234Z",
        "DX de W1ABC-#:    7025.0  K1XYZ   CW  12 dB  25 WPM  CQ  1234Z",
    };
    for (int r = 0; r < 40; r++)
        field_rbn_feed_line(rbn[r % 5], now);
    field_key(LV_KEY_ENTER);            /* BANDS */
    shot("field_5_bands.ppm");
    dialog_destruct();
    pump(100);
    /* The same numbers in the Bands app */
    dialog_construct(dialog_bands, lv_scr_act());
    pump(300);
    shot("bands_app.ppm");
    dialog_destruct();
    pump(100);
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
    theme_init(getenv("THEME"));    /* THEME=Classic, or a file in out/themes_demo; default IU1WXY */
    if (getenv("FONT")) { params.font.x = atoi(getenv("FONT")); fonts_apply(params.font.x); }
    else if (theme_shape.font >= 0) fonts_apply(theme_shape.font);
    styles_init();
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &background_style, 0);
    keyboard_group = lv_group_create();
    lv_group_set_wrap(keyboard_group, false);   /* as keyboard_init() */
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
    /* VOL 2:2, MFK 1:3, MFK 2:3, MFK 3:3 */
    static const int gen_pages[] = {1, 2, 3, 4};
    for (int i = 0; i < 4; i++) {
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
    if (only && !strcmp(only, "outdoor")) {   /* Settings in Outdoor: Display is settings_1_2 */
        theme_init("Outdoor");
        styles_apply();
        lv_obj_report_style_change(NULL);
        settings_demo();
        return 0;
    }
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
    if (!only || !strcmp(only, "sstv")) sstv_tx_demo();
    if (only && !strcmp(only, "voicekeyer")) voicekeyer();
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
    if (only && !strcmp(only, "cwtrainer")) cwtrainer_demo();
    if (!only || !strcmp(only, "wspr")) wspr_demo();
    if (!only || !strcmp(only, "settings")) settings_demo();
    if (!only || !strcmp(only, "wireless")) wireless_demo();
    if (only && !strcmp(only, "settings_nav")) settings_nav();
    if (!only || !strcmp(only, "cwkbd")) cwkbd_demo();
    if (only && !strcmp(only, "field")) field_demo();
    if (only && !strcmp(only, "swrscan")) {
        dialog_construct(dialog_swrscan, lv_scr_act());
        pump(300);
        shot("swrscan.ppm");
        dialog_destruct();
    }
    if (only && !strcmp(only, "cluster_filters")) cluster_filters_demo();
    if (!only || !strcmp(only, "bookmarks")) {
        bookmarks("navtex", dialog_navtex, 517000, 0, 1);
        bookmarks("wefax", dialog_wefax, 7878500, 0, 1);
        bookmarks("sstv", dialog_sstv, 14230000, 1, 1);
    }
    return 0;
}
