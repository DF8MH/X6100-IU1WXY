/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - Field Ops
 *
 *  Screens: home (SOTA, POTA, Bands, Log tiles), SOTA or POTA (the reference, the
 *  nearest ones, the activation), Bands (activity and who hears you, from RBN) and the
 *  activation log. The dialog covers the whole screen above the buttons; the main knob
 *  still tunes. The network jobs (reference list update, POTA spot, QRZ upload) run
 *  x6100-field in a thread; its one-line answer comes back through the scheduler.
 */

#include "dialog_field.h"

#include "buttons.h"
#include "cfg/cfg_api.h"
#include "cw_encoder.h"
#include "dialog.h"
#include "events.h"
#include "field/flog.h"
#include "field/rbn.h"
#include "field/refs.h"
#include "gps.h"
#include "keyboard.h"
#include "main_screen.h"
#include "msg.h"
#include "params/params.h"
#include "qth/qth.h"
#include "radio.h"
#include "scheduler.h"
#include "station.h"
#include "styles.h"
#include "textarea_window.h"
#include "theme.h"
#include "cluster/client.h"
#include "cluster/spots.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef FIELD_DIR   /* overridden by the UI harness */
#define FIELD_DIR   "/mnt/field"
#endif
#define REFS_PATH   FIELD_DIR "/refs.tsv"
#define STATE_PATH  FIELD_DIR "/state"
#define LOGS_DIR    FIELD_DIR "/logs"
#define HELPER      "x6100-field"

#define W           800
#define HEAD_H      44
#define MAX_NEAR    30
#define MAX_QSO     200
#define MAX_ROWS    MAX_QSO
#define GPS_MAX_AGE 600     /* s */
#define PHONE_MAX_AGE 1800
#define HEARD_S     600     /* the skimmers of the last 10 minutes */

typedef enum { SCR_HOME, SCR_PROG, SCR_BANDS, SCR_LOG } screen_t;

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void key_cb(lv_event_t *e);
static void show(screen_t s);

/* Kept on the SD card */
static struct {
    screen_t    screen;
    int         home_sel;
    char        prog;       /* 'S' SOTA, 'P' POTA */
    bool        have_ref;
    field_ref_t ref;
    bool        active;
    time_t      start;
    char        log[160];
} st = {.prog = 'S'};
static bool st_loaded;

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .key_cb       = key_cb,
};

dialog_t *dialog_field = &dialog;

static lv_obj_t   *title, *head_right, *body, *list, *status;
static lv_coord_t  body_h;
static lv_obj_t   *info_ref, *info_name, *info_lines, *info_state;
static lv_timer_t *timer;
static int         sel, rows_n;
static bool        kbd_open;

static field_ref_t near[MAX_NEAR];
static int         near_n = -2;     /* -2: not looked up yet, -1: no list */
static double      near_lat = 999, near_lon;
static char        near_prog;

static field_qso_t qsos[MAX_QSO];
static int         qsos_n;

static atomic_bool busy;            /* an x6100-field job runs */
static bool        end_armed;
static time_t      end_armed_at;

/* State file: key=value lines */

static void state_save(void) {
    char  tmp[] = STATE_PATH ".tmp";
    FILE *f;

    mkdir(FIELD_DIR, 0755);
    f = fopen(tmp, "w");
    if (!f)
        return;
    fprintf(f, "screen=%d\nhome=%d\nprog=%c\nactive=%d\nstart=%lld\nlog=%s\n", st.screen, st.home_sel, st.prog,
            st.active, (long long)st.start, st.log);
    if (st.have_ref)
        fprintf(f, "ref=%c\t%s\t%s\t%.5f\t%.5f\t%d\t%d\n", st.ref.type, st.ref.ref, st.ref.name, st.ref.lat,
                st.ref.lon, st.ref.alt_m, st.ref.points);
    fflush(f);
    fsync(fileno(f));
    if (fclose(f) == 0)
        rename(tmp, STATE_PATH);
}

static void state_load(void) {
    FILE *f = fopen(STATE_PATH, "r");
    char  line[300];

    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        char *v = strchr(line, '=');

        if (!v)
            continue;
        *v++ = 0;
        v[strcspn(v, "\r\n")] = 0;
        if (!strcmp(line, "screen"))
            st.screen = (screen_t)atoi(v);
        else if (!strcmp(line, "home"))
            st.home_sel = atoi(v);
        else if (!strcmp(line, "prog"))
            st.prog = *v == 'P' ? 'P' : 'S';
        else if (!strcmp(line, "active"))
            st.active = atoi(v);
        else if (!strcmp(line, "start"))
            st.start = (time_t)atoll(v);
        else if (!strcmp(line, "log"))
            snprintf(st.log, sizeof(st.log), "%s", v);
        else if (!strcmp(line, "ref"))
            st.have_ref = field_ref_parse(v, &st.ref);
    }
    fclose(f);
    if (st.screen > SCR_LOG)
        st.screen = SCR_HOME;
}

/* Helpers */

static const char *my_call(void) {
    static char call[16];
    const char *c = params.callsign.x;
    size_t      n = 0;

    /* Only what a callsign has: it goes on a command line */
    for (; c && *c && n < sizeof(call) - 1; c++)
        if (isalnum((unsigned char)*c) || *c == '/')
            call[n++] = toupper((unsigned char)*c);
    call[n] = 0;
    return call;
}

static bool ref_ok(const char *r) {
    if (!r[0])
        return false;
    for (; *r; r++)
        if (!isalnum((unsigned char)*r) && *r != '/' && *r != '-')
            return false;
    return true;
}

static const char *adif_mode(void) {
    switch (cparam_i_get(cfg_cur_mode)) {
        case x6100_mode_cw:
        case x6100_mode_cwr:
            return "CW";
        case x6100_mode_am:
            return "AM";
        case x6100_mode_nfm:
            return "FM";
        case x6100_mode_lsb_dig:
        case x6100_mode_usb_dig:
            return "FT8";
        default:
            return "SSB";
    }
}

static bool is_cw(void) {
    int m = cparam_i_get(cfg_cur_mode);
    return m == x6100_mode_cw || m == x6100_mode_cwr;
}

/* The best position: radio GPS, the phone, the QTH locator. src: "GPS", "PHONE", "QTH" */
static bool position(double *lat, double *lon, const char **src, int *age) {
    *age = 0;
    if (gps_last_fix(lat, lon, age) && *age < GPS_MAX_AGE) {
        *src = "GPS";
        return true;
    }
    if (gps_phone_fix(lat, lon, age) && *age < PHONE_MAX_AGE) {
        *src = "PHONE";
        return true;
    }
    const char *qth = station_qth();
    if (qth && strlen(qth) >= 4) {
        qth_str_to_pos(qth, lat, lon);
        *src = "QTH";
        *age = 0;
        return true;
    }
    *src = "";
    return false;
}

static void deg_text(char *buf, size_t size, double lat, double lon) {
    /* Decimal degrees: the fonts have no degree sign */
    snprintf(buf, size, "%.4f %c   %.4f %c", fabs(lat), lat >= 0 ? 'N' : 'S', fabs(lon), lon >= 0 ? 'E' : 'W');
}

static const char *prog_name(void) {
    return st.prog == 'S' ? "SOTA" : "POTA";
}

/* x6100-field jobs */

typedef void (*job_done_t)(const char *line);

typedef struct {
    job_done_t done;
    char       line[160];
} job_result_t;

typedef struct {
    job_done_t done;
    char       cmd[400];
} job_t;

static void job_result_cb(void *arg) {
    job_result_t *r = arg;
    r->done(r->line);
}

static void *job_thread(void *arg) {
    job_t       *j = arg;
    job_result_t r = {.done = j->done, .line = "error no answer"};
    FILE        *p = popen(j->cmd, "r");

    if (p) {
        if (!fgets(r.line, sizeof(r.line), p))
            snprintf(r.line, sizeof(r.line), "error no answer");
        pclose(p);
    }
    r.line[strcspn(r.line, "\r\n")] = 0;
    free(j);
    atomic_store(&busy, false);
    scheduler_put(job_result_cb, &r, sizeof(r));
    return NULL;
}

static bool job_start(job_done_t done, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static bool job_start(job_done_t done, const char *fmt, ...) {
    pthread_t t;
    va_list   ap;
    job_t    *j;
    bool      idle = false;

    if (!atomic_compare_exchange_strong(&busy, &idle, true)) {
        msg_update_text_fmt("Field: wait, still busy");
        return false;
    }
    j       = calloc(1, sizeof(*j));
    j->done = done;
    va_start(ap, fmt);
    vsnprintf(j->cmd, sizeof(j->cmd), fmt, ap);
    va_end(ap);
    strncat(j->cmd, " 2>/dev/null", sizeof(j->cmd) - strlen(j->cmd) - 1);
    if (pthread_create(&t, NULL, job_thread, j) != 0) {
        free(j);
        atomic_store(&busy, false);
        return false;
    }
    pthread_detach(t);
    return true;
}

/* The nearest references, looked up in a thread (the list has ~260000 lines) */

typedef struct {
    char        prog;
    double      lat, lon;
    int         n;
    field_ref_t refs[MAX_NEAR];
} near_result_t;

static void refresh(void);

static void near_result_cb(void *arg) {
    near_result_t *r = arg;

    near_n    = r->n;
    near_prog = r->prog;
    near_lat  = r->lat;
    near_lon  = r->lon;
    memcpy(near, r->refs, sizeof(near));
    /* The nearest one is the reference while none is chosen */
    if (!st.active && near_n > 0 && (!st.have_ref || st.ref.type != r->prog)) {
        st.ref      = near[0];
        st.have_ref = true;
        state_save();
    }
    if (dialog.run && st.screen == SCR_PROG) {
        sel = 0;
        show(SCR_PROG);
    }
}

static void *near_thread(void *arg) {
    near_result_t *r = arg;

    r->n = field_refs_near(REFS_PATH, r->prog, r->lat, r->lon, r->refs, MAX_NEAR);
    scheduler_put(near_result_cb, r, sizeof(*r));
    free(r);
    return NULL;
}

static void near_lookup(bool force) {
    double      lat, lon;
    const char *src;
    int         age;
    pthread_t   t;

    if (!position(&lat, &lon, &src, &age))
        return;
    /* Again when the program changed or we moved 300 m */
    if (!force && near_prog == st.prog && near_n != -2 && fabs(lat - near_lat) < 0.003 && fabs(lon - near_lon) < 0.004)
        return;
    near_result_t *r = calloc(1, sizeof(*r));
    r->prog = st.prog;
    r->lat  = lat;
    r->lon  = lon;
    near_prog = st.prog;
    near_lat  = lat;
    near_lon  = lon;
    if (pthread_create(&t, NULL, near_thread, r) == 0)
        pthread_detach(t);
    else
        free(r);
}

/* Widgets */

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, lv_coord_t x, lv_coord_t y,
                       lv_coord_t w) {
    lv_obj_t *l = lv_label_create(parent);

    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_recolor(l, true);
    lv_obj_set_pos(l, x, y);
    if (w > 0) {
        lv_obj_set_width(l, w);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    }
    lv_label_set_text(l, "");
    return l;
}

static lv_obj_t *panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t *p = lv_obj_create(parent);

    lv_obj_remove_style_all(p);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, TC(panel), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(p, TC(panel_border), 0);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_radius(p, 6, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

/* A list of rows, one selected; the columns of a row are separated by tabs */
static const lv_coord_t *cols;
static int               cols_n;

static void list_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                        const lv_coord_t *widths, int n) {
    list = panel(parent, x, y, w, h);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 4, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    cols   = widths;
    cols_n = n;
    rows_n = 0;
}

static lv_obj_t *row_create(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y) {
    lv_obj_t *row = lv_obj_create(parent);
    char      buf[200];
    char     *cell = buf;

    if (parent != list)
        lv_obj_set_pos(row, x, y);

    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, parent == list ? lv_pct(100) : W - 24, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_ver(row, 3, 0);
    lv_obj_set_style_pad_hor(row, 6, 0);
    lv_obj_set_style_radius(row, 4, 0);
    lv_obj_set_style_text_font(row, &sony_22, 0);
    lv_obj_set_style_text_color(row, TC(text), 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    snprintf(buf, sizeof(buf), "%s", text);
    for (int c = 0; c < cols_n && cell; c++) {
        char     *next = strchr(cell, '\t');
        lv_obj_t *l    = lv_label_create(row);

        if (next)
            *next++ = 0;
        lv_label_set_recolor(l, true);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, cols[c]);
        lv_label_set_text(l, cell);
        cell = next;
    }
    return row;
}

static void list_add(const char *text) {
    row_create(list, text, 0, 0);
    rows_n++;
}

static void list_select(int k) {
    if (!list || rows_n == 0)
        return;
    sel = k < 0 ? 0 : k >= rows_n ? rows_n - 1 : k;
    for (int i = 0; i < rows_n; i++) {
        lv_obj_t *row = lv_obj_get_child(list, i);
        bool      on  = i == sel;

        lv_obj_set_style_bg_color(row, TC(sel_bg), 0);
        lv_obj_set_style_bg_opa(row, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(row, on ? TC(sel_text) : TC(text), 0);
    }
    lv_obj_scroll_to_view(lv_obj_get_child(list, sel), LV_ANIM_OFF);
}

/* Buttons */

static void page_cb(button_data_t *b);
static void open_cb(button_data_t *b);
static void boot_cb(button_data_t *b);
static void exit_cb(button_data_t *b);
static void back_cb(button_data_t *b);
static void activate_cb(button_data_t *b);
static void spot_cb(button_data_t *b);
static void log_cb(button_data_t *b);
static void ref_cb(button_data_t *b);
static void update_cb(button_data_t *b);
static void beacon_cb(button_data_t *b);
static void tune_cb(button_data_t *b);
static void qrz_cb(button_data_t *b);
static const char *boot_label(void);
static const char *activate_label(void);

static button_data_t b_home   = {.type = BTN_TEXT, .label = "(FIELD)"};
static button_data_t b_open   = {.type = BTN_TEXT, .label = "Select", .press = open_cb};
static button_data_t b_boot   = {.type = BTN_TEXT_FN, .label_fn = boot_label, .press = boot_cb};
static button_data_t b_exit   = {.type = BTN_TEXT, .label = "Radio", .press = exit_cb};
static button_data_t b_back   = {.type = BTN_TEXT, .label = "Back", .press = back_cb};
static button_data_t b_prog1  = {.type = BTN_TEXT, .label = "(1:2)", .press = page_cb};
static button_data_t b_prog2  = {.type = BTN_TEXT, .label = "(2:2)", .press = page_cb};
static button_data_t b_act    = {.type = BTN_TEXT_FN, .label_fn = activate_label, .press = activate_cb};
static button_data_t b_spot   = {.type = BTN_TEXT, .label = "Spot", .press = spot_cb};
static button_data_t b_log    = {.type = BTN_TEXT, .label = "Log\nQSO", .press = log_cb};
static button_data_t b_ref    = {.type = BTN_TEXT, .label = "Ref...", .press = ref_cb};
static button_data_t b_update = {.type = BTN_TEXT, .label = "Update\nlist", .press = update_cb};
static button_data_t b_bands  = {.type = BTN_TEXT, .label = "(BANDS)"};
static button_data_t b_beacon = {.type = BTN_TEXT, .label = "Test\nbeacon", .press = beacon_cb};
static button_data_t b_tune   = {.type = BTN_TEXT, .label = "Use\nband", .press = tune_cb};
static button_data_t b_logpg  = {.type = BTN_TEXT, .label = "(LOG)"};
static button_data_t b_qrz    = {.type = BTN_TEXT, .label = "Upload\nQRZ", .press = qrz_cb};

static buttons_page_t page_home  = {.items = {&b_home, &b_open, &b_boot, NULL, &b_exit}};
static buttons_page_t page_prog1 = {.items = {&b_prog1, &b_act, &b_spot, &b_log, &b_back}};
static buttons_page_t page_prog2 = {.items = {&b_prog2, &b_ref, &b_update, NULL, &b_back}};
static buttons_page_t page_bands = {.items = {&b_bands, &b_beacon, &b_tune, NULL, &b_back}};
static buttons_page_t page_log   = {.items = {&b_logpg, &b_log, &b_qrz, NULL, &b_back}};

static const char *boot_label(void) {
    return param_i_get(cfg_field_boot) ? "Start here\nOn" : "Start here\nOff";
}

static const char *activate_label(void) {
    return !st.active ? "Start" : end_armed ? "End?" : "End";
}

static void page_cb(button_data_t *b) {
    buttons_unload_page();
    buttons_load_page(b == &b_prog1 ? &page_prog2 : &page_prog1);
}

static void boot_cb(button_data_t *b) {
    param_i_set(cfg_field_boot, !param_i_get(cfg_field_boot));
    buttons_refresh(&b_boot);
}

static void exit_cb(button_data_t *b) {
    main_screen_close_app();
}

static void back_cb(button_data_t *b) {
    show(SCR_HOME);
}

/* Screens */

static const char *home_names[] = {"SOTA", "POTA", "BANDS", "LOG"};

static void home_open(int k) {
    st.home_sel = k;
    if (k <= 1) {
        st.prog = k == 0 ? 'S' : 'P';
        /* The activation keeps its program */
        if (st.active && st.have_ref)
            st.prog = st.ref.type;
        show(SCR_PROG);
    } else {
        show(k == 2 ? SCR_BANDS : SCR_LOG);
    }
}

static void open_cb(button_data_t *b) {
    home_open(sel);
}

static void build_home(void) {
    static const char *subs[] = {"Summits\nOn The Air", "Parks\nOn The Air", "Activity\nRBN, beacon", "Activation\nlog"};
    lv_color_t          colors[] = {TC(warn), TC(ok), TC(info), TC(text_head)};
    lv_coord_t          tw       = 180, gap = 10, x0 = (W - 4 * tw - 3 * gap) / 2;

    lv_label_set_text(title, "FIELD OPERATIONS");
    list   = NULL;
    rows_n = 4;
    for (int k = 0; k < 4; k++) {
        lv_obj_t *t = panel(body, x0 + k * (tw + gap), 24, tw, body_h - 80);
        lv_obj_t *n = label(t, &sony_40, colors[k], 0, 80, tw);
        lv_obj_t *s = label(t, &sony_22, TC(text_dim), 0, 150, tw);

        lv_label_set_text(n, home_names[k]);
        lv_label_set_text(s, subs[k]);
        lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_border_color(t, colors[k], 0);
    }
    sel = st.home_sel;
    buttons_load_page(&page_home);
}

static void home_select(int k) {
    sel = (k + 4) % 4;
    for (int i = 0; i < 4; i++) {
        lv_obj_t *t = lv_obj_get_child(body, i);
        lv_obj_set_style_border_width(t, i == sel ? 4 : 1, 0);
        lv_obj_set_style_bg_color(t, i == sel ? TC(focus_bg) : TC(panel), 0);
    }
}

static void prog_info_update(void) {
    char        buf[200], deg[48], grid[16] = "";
    double      lat, lon;
    const char *src;
    int         age;

    if (!info_ref)
        return;
    bool have_pos = position(&lat, &lon, &src, &age);
    if (st.have_ref && st.ref.type == st.prog) {
        lv_label_set_text(info_ref, st.ref.ref);
        lv_label_set_text(info_name, st.ref.name);
    } else {
        lv_label_set_text(info_ref, "No reference");
        lv_label_set_text(info_name, near_n == -1 ? "No list: Update list (page 2)"
                                     : !have_pos  ? "No position: GPS, phone or QTH locator"
                                                  : "Looking...");
    }
    if (have_pos) {
        qth_pos_to_str(lat, lon, grid);
        grid[6] = 0;
        deg_text(deg, sizeof(deg), lat, lon);
    }
    char alt[40] = "";
    if (st.have_ref && st.ref.type == 'S' && st.prog == 'S')
        snprintf(alt, sizeof(alt), "ALT %d m   %d pt\n", st.ref.alt_m, st.ref.points);
    snprintf(buf, sizeof(buf), "%s%s\n%s\n#%06x Position: %s%s#", alt, have_pos ? grid : "", have_pos ? deg : "No position",
             theme_hex(TC_text_dim), have_pos ? src : "none", have_pos && age > 60 ? " (old)" : "");
    lv_label_set_text(info_lines, buf);

    if (st.active) {
        int s = (int)(time(NULL) - st.start);
        lv_label_set_text_fmt(info_state, "#%06x ACTIVE#   %d:%02d   %d QSO", theme_hex(TC_ok), s / 3600,
                              s / 60 % 60, qsos_n);
    } else {
        lv_label_set_text_fmt(info_state, "#%06x NOT ACTIVE#", theme_hex(TC_err));
    }
}

static void build_prog(void) {
    lv_coord_t h = body_h;
    char       row[120];

    lv_label_set_text_fmt(title, "%s MODE", prog_name());
    if (st.active && st.log[0])
        qsos_n = field_log_read(st.log, qsos, MAX_QSO);
    lv_obj_t *p = panel(body, 8, 6, 380, h - 44);
    info_ref    = label(p, &sony_36, TC(text_head), 14, 8, 352);
    info_name   = label(p, &sony_24, TC(text), 14, 56, 352);
    info_lines  = label(p, &sony_22, TC(text), 14, 100, 352);
    lv_label_set_long_mode(info_lines, LV_LABEL_LONG_WRAP);
    info_state  = label(p, &sony_24, TC(text), 14, h - 44 - 46, 352);

    static const lv_coord_t widths[] = {130, 90, 150};
    list_create(body, 396, 6, W - 404, h - 44, widths, 3);
    if (near_n > 0 && near_prog == st.prog) {
        for (int k = 0; k < near_n; k++) {
            snprintf(row, sizeof(row), "%s\t#%06x %.*f km#\t%s", near[k].ref, theme_hex(TC_text_dim), near[k].km < 10 ? 1 : 0, near[k].km,
                     near[k].name);
            list_add(row);
            if (st.have_ref && !strcmp(near[k].ref, st.ref.ref))
                sel = k;
        }
        list_select(sel);
    }
    prog_info_update();
    lv_label_set_text(status, near_n == -1 ? "Reference list missing: Update list (page 2), WiFi needed"
                                           : "MFK: choose the reference, press to use it");
    buttons_load_page(&page_prog1);
    near_lookup(false);
}

/* Bands: from the cluster spots of the last 30 minutes and RBN */
static int band_scores(int spots[FIELD_RBN_BANDS], int rbn[FIELD_RBN_BANDS], int heard[FIELD_RBN_BANDS],
                       int best_snr[FIELD_RBN_BANDS]) {
    static cluster_spot_t snap[CLUSTER_MAX_SPOTS];
    static int            snap_n;
    static uint32_t       gen;
    field_rbn_hit_t       hits[32];
    int                   n, best = -1, best_score = 0;

    memset(spots, 0, sizeof(int) * FIELD_RBN_BANDS);
    memset(heard, 0, sizeof(int) * FIELD_RBN_BANDS);
    for (int b = 0; b < FIELD_RBN_BANDS; b++)
        best_snr[b] = -99;
    n = cluster_client_snapshot(snap, CLUSTER_MAX_SPOTS, &gen);
    if (n >= 0)
        snap_n = n;
    struct tm tm;
    time_t    now = time(NULL);
    gmtime_r(&now, &tm);
    for (int k = 0; k < snap_n; k++) {
        int b = cluster_band_of(snap[k].freq_hz);
        if (b >= 0 && b < FIELD_RBN_BANDS && cluster_age_min(snap[k].utc_min, tm.tm_hour * 60 + tm.tm_min) <= 30)
            spots[b]++;
    }
    field_rbn_activity(rbn);
    n = field_rbn_heard(HEARD_S, hits, 32);
    for (int k = 0; k < n; k++) {
        int b = cluster_band_of(hits[k].freq_hz);
        if (b >= 0 && b < FIELD_RBN_BANDS) {
            heard[b]++;
            if (hits[k].snr > best_snr[b])
                best_snr[b] = hits[k].snr;
        }
    }
    /* Who hears you counts most, then the activity */
    for (int b = 0; b < FIELD_RBN_BANDS; b++) {
        int score = heard[b] * 1000 + spots[b] * 10 + rbn[b];
        if (score > best_score) {
            best_score = score;
            best       = b;
        }
    }
    return best;
}

static void bands_fill(void) {
    int  spots[FIELD_RBN_BANDS], rbn[FIELD_RBN_BANDS], heard[FIELD_RBN_BANDS], snr[FIELD_RBN_BANDS];
    int  best = band_scores(spots, rbn, heard, snr);
    int  max  = 1;
    char row[160], bar[32];

    for (int b = 0; b < FIELD_RBN_BANDS; b++)
        if (spots[b] + rbn[b] > max)
            max = spots[b] + rbn[b];
    lv_obj_clean(list);
    rows_n = 0;
    for (int b = 0; b < FIELD_RBN_BANDS; b++) {
        int len = (spots[b] + rbn[b]) * 10 / max;
        int p   = 0;

        for (int i = 0; i < 10; i++)
            p += snprintf(bar + p, sizeof(bar) - p, "%s", i < len ? "|" : ".");
        char heard_txt[40] = "";
        if (heard[b])
            snprintf(heard_txt, sizeof(heard_txt), "#%06x %d, best %+d dB#", theme_hex(TC_ok), heard[b], snr[b]);
        snprintf(row, sizeof(row), "%s\t%s\t%d\t%d\t%s\t%s", cluster_band_name(b), bar, spots[b], rbn[b], heard_txt,
                 b == best ? "BEST" : "");
        list_add(row);
    }
    list_select(sel);

    field_rbn_hit_t hits[6];
    int             n = field_rbn_heard(HEARD_S, hits, 6), p = 0;
    char            line[200];

    p = snprintf(line, sizeof(line), "RBN %s.  ", field_rbn_state());
    if (!n)
        p += snprintf(line + p, sizeof(line) - p, "Nobody heard you in the last 10 min (Test beacon: CW)");
    for (int k = 0; k < n && p < (int)sizeof(line); k++)
        p += snprintf(line + p, sizeof(line) - p, "%s %+d dB  ", hits[k].skimmer, hits[k].snr);
    lv_label_set_text(status, line);
}

static void build_bands(void) {
    static const lv_coord_t widths[] = {80, 150, 90, 90, 220, 90};
    lv_label_set_text(title, "BAND OVERVIEW");
    list_create(body, 8, 40, W - 16, body_h - 78, widths, 6);
    lv_obj_t *head = row_create(body, "BAND\tACTIVITY\tSPOTS\tRBN\tHEARD YOU", 12, 8);
    lv_obj_set_style_text_color(head, TC(text_dim), 0);
    sel = cluster_band_of(cparam_i_get(cfg_fg_freq));
    bands_fill();
    buttons_load_page(&page_bands);
}

static void build_log(void) {
    char      row[120];
    struct tm tm;

    lv_label_set_text_fmt(title, "LOGBOOK  %s", st.have_ref ? st.ref.ref : "");
    static const lv_coord_t widths[] = {55, 90, 190, 80, 80, 150};
    list_create(body, 8, 6, W - 16, body_h - 44, widths, 6);
    qsos_n = st.log[0] ? field_log_read(st.log, qsos, MAX_QSO) : 0;
    for (int k = 0; k < qsos_n; k++) {
        gmtime_r(&qsos[k].t, &tm);
        snprintf(row, sizeof(row), "%d\t%02d:%02d\t%s\t%s\t%s\t%s / %s", k + 1, tm.tm_hour, tm.tm_min,
                 qsos[k].call, field_band(qsos[k].freq_hz), qsos[k].mode, qsos[k].rst_s, qsos[k].rst_r);
        list_add(row);
    }
    list_select(qsos_n - 1);
    lv_label_set_text_fmt(status, "%d QSO%s   %s", qsos_n, qsos_n == 1 ? "" : "s",
                          st.log[0] ? strrchr(st.log, '/') + 1 : "No activation yet");
    buttons_load_page(&page_log);
}

static void show(screen_t s) {
    if (st.screen != s)
        sel = 0;
    st.screen = s;
    state_save();
    end_armed = false;
    info_ref  = NULL;
    list      = NULL;
    lv_obj_clean(body);
    buttons_unload_page();
    switch (s) {
        case SCR_HOME:
            build_home();
            home_select(sel);
            break;
        case SCR_PROG:
            build_prog();
            break;
        case SCR_BANDS:
            build_bands();
            break;
        case SCR_LOG:
            build_log();
            break;
    }
}

static void refresh(void) {
    show(st.screen);
}

/* Actions */

static void activate_cb(button_data_t *b) {
    if (!st.active) {
        if (!st.have_ref || st.ref.type != st.prog) {
            msg_update_text_fmt("Choose the %s reference first", prog_name());
            return;
        }
        if (!my_call()[0]) {
            msg_update_text_fmt("Set your callsign in Settings");
            return;
        }
        char      name[40], *p;
        struct tm tm;

        st.active = true;
        st.start  = time(NULL);
        gmtime_r(&st.start, &tm);
        snprintf(name, sizeof(name), "%s", st.ref.ref);
        for (p = name; *p; p++)
            if (*p == '/')
                *p = '_';
        mkdir(FIELD_DIR, 0755);
        mkdir(LOGS_DIR, 0755);
        snprintf(st.log, sizeof(st.log), "%s/%04d%02d%02d-%02d%02d_%s.adi", LOGS_DIR, tm.tm_year + 1900, tm.tm_mon + 1,
                 tm.tm_mday, tm.tm_hour, tm.tm_min, name);
        qsos_n = 0;
        state_save();
        msg_update_text_fmt("Activation started: %s", st.ref.ref);
        refresh();
        return;
    }
    if (!end_armed || time(NULL) - end_armed_at > 4) {
        end_armed    = true;
        end_armed_at = time(NULL);
        buttons_refresh(&b_act);
        msg_update_text_fmt("Press End again to end the activation");
        return;
    }
    int s     = (int)(time(NULL) - st.start);
    st.active = false;
    state_save();
    msg_update_text_fmt("Activation complete: %s, %d QSO%s, %d min", st.ref.ref, qsos_n, qsos_n == 1 ? "" : "s", s / 60);
    show(SCR_LOG);
}

static void spot_done(const char *line) {
    msg_update_text_fmt("POTA spot: %s", line);
}

static void spot_cb(button_data_t *b) {
    if (!st.have_ref || st.ref.type != st.prog || !ref_ok(st.ref.ref)) {
        msg_update_text_fmt("Choose the %s reference first", prog_name());
        return;
    }
    if (st.prog == 'S') {
        /* SOTAwatch spots need an API client approved by SOTA: not yet */
        msg_update_text_fmt("SOTAwatch: not available yet. Post an alert, then CQ in CW: RBNHole spots you");
        return;
    }
    if (!my_call()[0]) {
        msg_update_text_fmt("Set your callsign in Settings");
        return;
    }
    if (job_start(spot_done, HELPER " pota-spot --call %s --freq-khz %.1f --ref %s --mode %s", my_call(),
                  cparam_i_get(cfg_fg_freq) / 1000.0, st.ref.ref, adif_mode()))
        msg_update_text_fmt("POTA spot: %s %.1f %s...", st.ref.ref, cparam_i_get(cfg_fg_freq) / 1000.0, adif_mode());
}

/* Log QSO: "CALL [rst received [rst sent]]" on the keyboard */

static void kbd_end(void) {
    textarea_window_close();
    kbd_open = false;
    lv_group_add_obj(keyboard_group, dialog.obj);
    lv_group_focus_obj(dialog.obj);
    lv_group_set_editing(keyboard_group, true);
}

static bool log_ok(void) {
    const char     *v = textarea_window_get();
    field_qso_t     q = {.t = time(NULL), .freq_hz = cparam_i_get(cfg_fg_freq)};
    field_station_t sta;
    const char     *rst = is_cw() ? "599" : "59";
    char            call[32] = "", r1[8] = "", r2[8] = "";

    if (v && sscanf(v, "%31s %7s %7s", call, r1, r2) >= 1 && ref_ok(call)) {
        for (char *p = call; *p; p++)
            *p = toupper((unsigned char)*p);
        snprintf(q.call, sizeof(q.call), "%s", call);
        snprintf(q.mode, sizeof(q.mode), "%s", adif_mode());
        snprintf(q.rst_r, sizeof(q.rst_r), "%s", r1[0] ? r1 : rst);
        snprintf(q.rst_s, sizeof(q.rst_s), "%s", r2[0] ? r2 : rst);
        snprintf(sta.call, sizeof(sta.call), "%s", my_call());
        snprintf(sta.grid, sizeof(sta.grid), "%s", station_qth() ? station_qth() : "");
        sta.prog = st.ref.type;
        snprintf(sta.ref, sizeof(sta.ref), "%s", st.ref.ref);
        if (field_log_add(st.log, &q, &sta)) {
            qsos_n = field_log_read(st.log, qsos, MAX_QSO);
            msg_update_text_fmt("QSO %d: %s %s %s", qsos_n, q.call, q.rst_s, q.rst_r);
        } else {
            msg_update_text_fmt("Log: can't write %s", st.log);
        }
    } else if (v && *v) {
        msg_update_text_fmt("Log: \"%s\" is not a callsign", v);
    }
    kbd_end();
    refresh();
    return true;
}

static bool kbd_cancel(void) {
    kbd_end();
    return true;
}

static void log_cb(button_data_t *b) {
    if (!st.active) {
        msg_update_text_fmt("Start the activation first");
        return;
    }
    kbd_open = true;
    lv_group_remove_obj(dialog.obj);
    textarea_window_open_w_label(log_ok, kbd_cancel, "Call [rst rcvd [sent]]: ");
}

static bool ref_ok_cb(void) {
    const char *v = textarea_window_get();
    field_ref_t r;
    char        ref[16];

    snprintf(ref, sizeof(ref), "%s", v ? v : "");
    for (char *p = ref; *p; p++)
        *p = toupper((unsigned char)*p);
    if (ref[0] && field_refs_find(REFS_PATH, ref, &r)) {
        st.ref      = r;
        st.have_ref = true;
        st.prog     = r.type;
        state_save();
    } else if (ref[0]) {
        msg_update_text_fmt("%s: not in the reference list", ref);
    }
    kbd_end();
    refresh();
    return true;
}

static void ref_cb(button_data_t *b) {
    if (st.active) {
        msg_update_text_fmt("End the activation to change the reference");
        return;
    }
    kbd_open = true;
    lv_group_remove_obj(dialog.obj);
    textarea_window_open_w_label(ref_ok_cb, kbd_cancel, "Reference: ");
}

static void update_done(const char *line) {
    if (!strncmp(line, "ok", 2)) {
        int p = 0, s = 0;
        sscanf(line, "ok %d %d", &p, &s);
        msg_update_text_fmt("Reference list: %d parks, %d summits", p, s);
        near_lookup(true);
    } else {
        msg_update_text_fmt("Reference list: %s", line);
    }
}

static void update_cb(button_data_t *b) {
    if (job_start(update_done, HELPER " update --call %s", my_call()[0] ? my_call() : "unknown"))
        msg_update_text_fmt("Downloading the parks and summits (about 35 MB)...");
}

static void beacon_cb(button_data_t *b) {
    char text[64];

    if (!my_call()[0]) {
        msg_update_text_fmt("Set your callsign in Settings");
        return;
    }
    if (!is_cw()) {
        msg_update_text_fmt("Test beacon: switch to CW first");
        return;
    }
    snprintf(text, sizeof(text), "TEST DE %s %s %s", my_call(), my_call(), my_call());
    cw_encoder_send(text, false);
    msg_update_text_fmt("Test beacon on the air: watch who hears you");
}

/* Typical activation frequencies: CW and phone */
static const int32_t band_freq[FIELD_RBN_BANDS][2] = {
    {1810000, 1850000},   {3560000, 3760000},   {5355000, 5360000},   {7030000, 7118000},
    {10118000, 10118000}, {14060000, 14285000}, {18086000, 18130000}, {21060000, 21285000},
    {24906000, 24950000}, {28060000, 28400000}, {50060000, 50150000},
};

static void tune_cb(button_data_t *b) {
    if (sel < 0 || sel >= FIELD_RBN_BANDS)
        return;
    if (main_screen_freq_locked()) {
        msg_update_text_fmt("The frequency is locked");
        return;
    }
    int32_t f = band_freq[sel][is_cw() ? 0 : 1];
    main_screen_set_freq(f);
    if (!is_cw() && !main_screen_mode_locked())
        cparam_i_set(cfg_cur_mode, f < 10000000 ? x6100_mode_lsb : x6100_mode_usb);
    msg_update_text_fmt("%s: %.3f MHz", cluster_band_name(sel), f / 1e6);
}

static void qrz_done(const char *line) {
    int sent = 0, dup = 0;

    if (sscanf(line, "ok %d %d", &sent, &dup) == 2)
        msg_update_text_fmt("QRZ: %d sent, %d already there", sent, dup);
    else
        msg_update_text_fmt("QRZ: %s", line);
}

static void qrz_cb(button_data_t *b) {
    if (!st.log[0] || access(st.log, R_OK) != 0) {
        msg_update_text_fmt("No log to upload");
        return;
    }
    if (job_start(qrz_done, HELPER " qrz-upload '%s'", st.log))
        msg_update_text_fmt("Uploading the log to QRZ...");
}

/* Keys and timer */

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    if (kbd_open)
        return;
    switch (key) {
        case LV_KEY_ESC:
            if (st.screen == SCR_HOME)
                main_screen_close_app();
            else
                show(SCR_HOME);
            break;
        case LV_KEY_UP:
        case LV_KEY_LEFT:
            if (st.screen == SCR_HOME)
                home_select(sel - 1);
            else
                list_select(sel - 1);
            break;
        case LV_KEY_DOWN:
        case LV_KEY_RIGHT:
            if (st.screen == SCR_HOME)
                home_select(sel + 1);
            else
                list_select(sel + 1);
            break;
        case LV_KEY_ENTER:
            if (st.screen == SCR_HOME) {
                home_open(sel);
            } else if (st.screen == SCR_PROG && !st.active && sel < near_n && near_prog == st.prog) {
                st.ref      = near[sel];
                st.have_ref = true;
                state_save();
                prog_info_update();
            } else if (st.screen == SCR_BANDS) {
                tune_cb(NULL);
            }
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

static void timer_cb(lv_timer_t *t) {
    static int  ticks;
    double      lat, lon;
    const char *src;
    int         age;
    time_t      now = time(NULL);
    struct tm   tm;

    gmtime_r(&now, &tm);
    position(&lat, &lon, &src, &age);
    lv_label_set_text_fmt(head_right, "%s   %.3f %s   %02d:%02dZ", src,
                          cparam_i_get(cfg_fg_freq) / 1e6, adif_mode(), tm.tm_hour, tm.tm_min);
    if (end_armed && now - end_armed_at > 4) {
        end_armed = false;
        buttons_refresh(&b_act);
    }
    ticks++;
    if (st.screen == SCR_PROG) {
        prog_info_update();
        if (ticks % 10 == 0)
            near_lookup(false);
    } else if (st.screen == SCR_BANDS && ticks % 5 == 0 && list) {
        int keep = sel;
        bands_fill();
        list_select(keep);
    }
}

static void construct_cb(lv_obj_t *parent) {
    lv_coord_t h = 480 - BUTTONS_H;

    if (!st_loaded) {
        state_load();
        st_loaded = true;
        if (st.active && st.log[0])
            qsos_n = field_log_read(st.log, qsos, MAX_QSO);
    }
    dialog.obj = dialog_init(parent);
    /* The whole screen above the buttons */
    lv_obj_set_pos(dialog.obj, 0, 0);
    lv_obj_set_size(dialog.obj, W, h);
    lv_obj_set_style_radius(dialog.obj, 0, 0);
    lv_obj_set_style_border_width(dialog.obj, 0, 0);
    lv_obj_set_style_bg_color(dialog.obj, TC(bg), 0);
    lv_obj_set_style_bg_opa(dialog.obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(dialog.obj, 0, 0);
    lv_obj_move_foreground(dialog.obj);

    lv_obj_t *head = panel(dialog.obj, 0, 0, W, HEAD_H);
    lv_obj_set_style_radius(head, 0, 0);
    lv_obj_set_style_bg_color(head, TC(bar), 0);
    lv_obj_t *x = label(head, &sony_24, TC(text_head), 12, 8, 0);
    lv_label_set_text(x, "X6100");
    title      = label(head, &sony_24, TC(text), 110, 8, 330);
    head_right = label(head, &sony_20, TC(text_dim), 440, 11, 350);
    lv_obj_set_style_text_align(head_right, LV_TEXT_ALIGN_RIGHT, 0);

    body = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(body);
    lv_obj_set_pos(body, 0, HEAD_H);
    body_h = h - HEAD_H;
    lv_obj_set_size(body, W, body_h);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    status = label(dialog.obj, &sony_20, TC(text_dim), 12, h - 32, W - 24);

    lv_group_add_obj(keyboard_group, dialog.obj);
    lv_obj_add_event_cb(dialog.obj, key_cb, LV_EVENT_KEY, NULL);
    lv_group_focus_obj(dialog.obj);
    lv_group_set_editing(keyboard_group, true);

    if (my_call()[0])
        field_rbn_start(my_call());
    kbd_open = false;
    show(st.screen);
    timer_cb(NULL);
    timer = lv_timer_create(timer_cb, 1000, NULL);
}

static void destruct_cb(void) {
    if (kbd_open) {
        textarea_window_close();
        kbd_open = false;
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
    /* Not on the phone's data while out of Field Ops; the activity of the last minutes stays */
    field_rbn_stop();
    state_save();
    list     = NULL;
    info_ref = NULL;
}
