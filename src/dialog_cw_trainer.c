/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CW trainer. Runs only with the keyer in train mode (sidetone, no TX).
 *  Learn (Koch): the radio keys a group with the sidetone, the user keys it
 *  back with the paddle and the decoder (listening to the sidetone) tells
 *  if it was right. Practice: the keyed text is shown large.
 *
 *  Safety: key train is forced on while the app is open and restored when
 *  it closes, whatever closes it (destruct_cb runs for the close button,
 *  another app, a key page, or a mode change caught by the timer).
 *  The encoder is also told to never key while train is off.
 */

#include "dialog_cw_trainer.h"

#include "buttons.h"
#include "cfg/cfg_api.h"
#include "cw.h"
#include "cw_decoder.h"
#include "cw_encoder.h"
#include "dialog.h"
#include "events.h"
#include "keyboard.h"
#include "main_screen.h"
#include "msg.h"
#include "panel.h"
#include "params/params.h"
#include "radio.h"
#include "scheduler.h"
#include "styles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TIMER_MS    100
#define GUARD_MS    600     /* the end of our own sidetone is still being decoded */
#define ANSWER_MS   3000    /* silence that ends an answer shorter than the group */
#define NEXT_OK_MS  1000    /* auto mode: pause before the next group */
#define NEXT_BAD_MS 2500    /* longer after a wrong answer, to read it */
#define WINDOW      10      /* results the accuracy is measured over */
#define PASS_PCT    90
#define HEARD_MAX   32
#define TEXT_MAX    400

#define X           16
#define W           (796 - 2 * X)
#define BOX_Y       44
#define BOX_H       (DIALOG_H - BOX_Y - 34)
#define SIDE_W      270

#define COLOR_HEAD  0x90f0d0
#define COLOR_DIM   0x90a0b0
#define COLOR_OK    0x60ff60
#define COLOR_BAD   0xff6060
#define COLOR_NEW   0xffd24a

/* Koch order (as LCWO): the first `level` characters are in use */
static const struct {
    char        c;
    const char *morse;
} koch[] = {
    {'K', "-.-"},   {'M', "--"},    {'R', ".-."},   {'S', "..."},    {'U', "..-"},    {'A', ".-"},   {'P', ".--."},
    {'T', "-"},     {'L', ".-.."},  {'O', "---"},   {'W', ".--"},    {'I', ".."},     {'.', ".-.-.-"}, {'N', "-."},
    {'J', ".---"},  {'E', "."},     {'F', "..-."},  {'0', "-----"},  {'Y', "-.--"},   {',', "--..--"}, {'V', "...-"},
    {'G', "--."},   {'5', "....."}, {'/', "-..-."}, {'Q', "--.-"},   {'9', "----."},  {'Z', "--.."},  {'H', "...."},
    {'3', "...--"}, {'8', "---.."}, {'B', "-..."},  {'?', "..--.."}, {'4', "....-"},  {'2', "..---"},  {'7', "--..."},
    {'C', "-.-."},  {'1', ".----"}, {'D', "-.."},   {'6', "-...."},  {'X', "-..-"},
};
#define KOCH_N ((int)(sizeof(koch) / sizeof(koch[0])))

typedef enum { MODE_LEARN, MODE_PRACTICE } tmode_t;
typedef enum { ST_READY, ST_PLAYING, ST_LISTEN, ST_RESULT } state_t;

static tmode_t mode;
static state_t st;
static bool    active;      /* between construct and destruct */

/* Radio state to give back */
static bool           saved;
static int32_t        prev_train;
static bool           mode_changed;
static x6100_mode_t   prev_mode;

/* Learn */
static char     target[8];
static char     heard[HEARD_MAX];
static int      heard_n;        /* decoded characters in heard[] */
static bool     heard_end;      /* word gap after the answer */
static uint32_t listen_t, last_rx_t;
static bool     last_ok;
static bool     autoplay;       /* Start: groups follow each other until Pause */
static uint32_t result_t;
static uint8_t  hist[WINDOW];   /* 1 = right, newest overwrites the oldest */
static int      hist_n, hist_pos;
static int      tried, right;

/* Practice */
static char text[TEXT_MAX];
static int  wpm_seen;

static lv_obj_t   *head, *head_right, *box, *big, *line1, *line2, *side_score, *side_chars, *hint, *text_lbl;
static lv_timer_t *timer;

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void key_cb(lv_event_t *e);
static void refresh_ui(void);

static void play_cb(button_data_t *b);
static void skip_cb(button_data_t *b);
static void mode_cb(button_data_t *b);
static void close_cb(button_data_t *b);
static void level_up_cb(button_data_t *b);
static void level_down_cb(button_data_t *b);
static void group_cb(button_data_t *b);
static void speed_up_cb(button_data_t *b);
static void speed_down_cb(button_data_t *b);
static void clear_cb(button_data_t *b);
static const char *play_label(void);
static const char *mode_label(void);
static const char *level_up_label(void);
static const char *level_down_label(void);
static const char *group_label(void);
static const char *speed_up_label(void);
static const char *speed_down_label(void);

static button_data_t btn_p1 = {.type = BTN_TEXT, .label = "(CWT 1:3)", .press = button_next_page_cb, .hold = button_prev_page_cb};
static button_data_t btn_p2 = {.type = BTN_TEXT, .label = "(CWT 2:3)", .press = button_next_page_cb, .hold = button_prev_page_cb};
static button_data_t btn_p3 = {.type = BTN_TEXT, .label = "(CWT 3:3)", .press = button_next_page_cb, .hold = button_prev_page_cb};
static button_data_t btn_play  = {.type = BTN_TEXT_FN, .label_fn = play_label, .press = play_cb};
static button_data_t btn_skip  = {.type = BTN_TEXT, .label = "Skip", .press = skip_cb};
static button_data_t btn_mode  = {.type = BTN_TEXT_FN, .label_fn = mode_label, .press = mode_cb};
static button_data_t btn_close = {.type = BTN_TEXT, .label = "Close", .press = close_cb};
static button_data_t btn_lvl_u = {.type = BTN_TEXT_FN, .label_fn = level_up_label, .press = level_up_cb};
static button_data_t btn_lvl_d = {.type = BTN_TEXT_FN, .label_fn = level_down_label, .press = level_down_cb};
static button_data_t btn_group = {.type = BTN_TEXT_FN, .label_fn = group_label, .press = group_cb};
static button_data_t btn_close2 = {.type = BTN_TEXT, .label = "Close", .press = close_cb};
static button_data_t btn_spd_u = {.type = BTN_TEXT_FN, .label_fn = speed_up_label, .press = speed_up_cb};
static button_data_t btn_spd_d = {.type = BTN_TEXT_FN, .label_fn = speed_down_label, .press = speed_down_cb};
static button_data_t btn_clear = {.type = BTN_TEXT, .label = "Clear", .press = clear_cb};
static button_data_t btn_close3 = {.type = BTN_TEXT, .label = "Close", .press = close_cb};

static buttons_page_t page_1 = {{&btn_p1, &btn_play, &btn_skip, &btn_mode, &btn_close}};
static buttons_page_t page_2 = {{&btn_p2, &btn_lvl_u, &btn_lvl_d, &btn_group, &btn_close2}};
static buttons_page_t page_3 = {{&btn_p3, &btn_spd_u, &btn_spd_d, &btn_clear, &btn_close3}};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .key_cb       = key_cb,
    .btn_page     = &page_1,
};

dialog_t *dialog_cw_trainer = &dialog;

const char *dialog_cw_trainer_target(void) {
    return target;
}

/* Safety */

/* Key train on. False when it could not be set: nothing must be keyed then. */
static bool force_train(void) {
    if (!param_i_get(cfg_key_train)) {
        param_i_set(cfg_key_train, 1);
    }
    return param_i_get(cfg_key_train) != 0;
}

static bool is_cw(void) {
    int32_t m = cparam_i_get(cfg_cur_mode);
    return m == x6100_mode_cw || m == x6100_mode_cwr;
}

/* Decoder */

static void on_text(void *arg);
static void on_wpm(void *arg);

/* Audio thread: hand over to the LVGL thread */
static void sink_text(const char *t) {
    scheduler_put(on_text, (void *)t, strlen(t) + 1);
}

static void sink_wpm(int w) {
    scheduler_put(on_wpm, &w, sizeof(w));
}

static const char *morse_of(char c) {
    for (int i = 0; i < KOCH_N; i++) {
        if (koch[i].c == c) return koch[i].morse;
    }
    return "";
}

/* Morse of every character of s, separated by spaces */
static void morse_line(const char *s, char *out, size_t size) {
    out[0] = 0;
    for (; *s; s++) {
        size_t n = strlen(out);
        snprintf(out + n, size - n, "%s%s", n ? "  " : "", morse_of(*s));
    }
}

static void on_wpm(void *arg) {
    if (!active) return;
    wpm_seen = *(int *)arg;
    if (mode == MODE_PRACTICE) refresh_ui();
}

static void on_text(void *arg) {
    const char *t = arg;
    if (!active) return;

    if (mode == MODE_PRACTICE) {
        size_t n = strlen(text), l = strlen(t);
        if (n + l >= TEXT_MAX) {    /* drop the oldest quarter */
            memmove(text, text + TEXT_MAX / 4, n - TEXT_MAX / 4 + 1);
            n = strlen(text);
        }
        snprintf(text + n, TEXT_MAX - n, "%s", t);
        refresh_ui();
        return;
    }

    /* Learn: only while listening, and not our own sidetone */
    if (st != ST_LISTEN || lv_tick_elaps(listen_t) < GUARD_MS) return;
    last_rx_t = lv_tick_get();
    if (t[0] == ' ') {
        heard_end = heard_n > 0;
    } else if (heard_n < 6) {
        size_t n = strlen(heard);
        snprintf(heard + n, HEARD_MAX - n, "%s", strcmp(t, "<?>") == 0 ? "?" : t);
        heard_n++;
    }
    refresh_ui();
}

/* Learn */

static int level(void) {
    int l = param_i_get(cfg_cwt_level);
    return l < 2 ? 2 : (l > KOCH_N ? KOCH_N : l);
}

static void reset_score(void) {
    hist_n = hist_pos = 0;
}

static void set_level(int l) {
    if (l < 2 || l > KOCH_N) return;
    param_i_set(cfg_cwt_level, l);
    reset_score();
}

static void new_target(void) {
    int len = param_i_get(cfg_cwt_group);
    int l   = level();
    for (int i = 0; i < len; i++) {
        /* the newest character comes up more often */
        int k     = (rand() % 3 == 0) ? l - 1 : rand() % l;
        target[i] = koch[k].c;
    }
    target[len] = 0;
}

static void play(void) {
    if (!force_train()) {
        msg_update_text_fmt("Key train is off: not playing");
        return;
    }
    heard[0] = 0;
    heard_n  = 0;
    heard_end = false;
    st       = ST_PLAYING;
    cw_encoder_send(target, false);
}

static void evaluate(void) {
    last_ok = strcmp(heard, target) == 0;
    tried++;
    right += last_ok;
    hist[hist_pos] = last_ok;
    hist_pos       = (hist_pos + 1) % WINDOW;
    if (hist_n < WINDOW) hist_n++;
    st = ST_RESULT;
    result_t = lv_tick_get();

    if (hist_n == WINDOW) {
        int good = 0;
        for (int i = 0; i < WINDOW; i++) good += hist[i];
        if (good * 100 >= PASS_PCT * WINDOW && level() < KOCH_N) {
            set_level(level() + 1);
            msg_update_text_fmt("Level %i: new character %c", level(), koch[level() - 1].c);
        }
    }
}

static void tick_cb(lv_timer_t *t) {
    (void)t;

    /* The paddle must not transmit: train back on, whoever turned it off */
    if (!param_i_get(cfg_key_train)) {
        cw_encoder_stop();
        force_train();
    }
    /* Mode changed away (band memory, CAT): the trainer ends */
    if (!is_cw()) {
        dialog_destruct();
        return;
    }

    if (mode != MODE_LEARN) return;
    if (st == ST_PLAYING && cw_encoder_state() == CW_ENCODER_IDLE) {
        st        = ST_LISTEN;
        listen_t  = lv_tick_get();
        last_rx_t = listen_t;
        refresh_ui();
    } else if (st == ST_LISTEN && heard_n > 0 &&
               (heard_n >= (int)strlen(target) || heard_end || lv_tick_elaps(last_rx_t) > ANSWER_MS)) {
        evaluate();
        refresh_ui();
    } else if (st == ST_RESULT && autoplay && lv_tick_elaps(result_t) > (last_ok ? NEXT_OK_MS : NEXT_BAD_MS)) {
        new_target();
        play();
        refresh_ui();
    }
}

/* Buttons */

static void play_cb(button_data_t *b) {
    (void)b;
    if (mode != MODE_LEARN) return;
    if (st == ST_LISTEN) {          /* Replay */
        play();
    } else if (autoplay) {          /* Pause: the current group still ends */
        autoplay = false;
    } else {                        /* Start */
        autoplay = true;
        if (st != ST_PLAYING) {
            new_target();
            play();
        }
    }
    refresh_ui();
}

static void skip_cb(button_data_t *b) {
    (void)b;
    if (mode != MODE_LEARN) return;
    cw_encoder_stop();
    new_target();
    play();
    refresh_ui();
}

static void mode_cb(button_data_t *b) {
    autoplay = false;
    (void)b;
    cw_encoder_stop();
    mode      = mode == MODE_LEARN ? MODE_PRACTICE : MODE_LEARN;
    st        = ST_READY;
    target[0] = 0;
    text[0]   = 0;
    refresh_ui();
}

static void close_cb(button_data_t *b) {
    (void)b;
    dialog_destruct();
}

static void level_up_cb(button_data_t *b) {
    (void)b;
    if (mode != MODE_LEARN) return;
    set_level(level() + 1);
    refresh_ui();
}

static void level_down_cb(button_data_t *b) {
    (void)b;
    if (mode != MODE_LEARN) return;
    set_level(level() - 1);
    refresh_ui();
}

static void group_cb(button_data_t *b) {
    (void)b;
    if (mode != MODE_LEARN) return;
    param_i_set(cfg_cwt_group, param_i_get(cfg_cwt_group) % 5 + 1);
    refresh_ui();
}

/* The key speed is also the paddle speed of the keyer */
static void change_speed(int d) {
    int v = param_i_get(cfg_key_speed) + d;
    if (v < 5 || v > 50) return;
    param_i_set(cfg_key_speed, v);
    refresh_ui();
}

static void speed_up_cb(button_data_t *b) {
    (void)b;
    change_speed(1);
}

static void speed_down_cb(button_data_t *b) {
    (void)b;
    change_speed(-1);
}

static void clear_cb(button_data_t *b) {
    (void)b;
    text[0] = 0;
    refresh_ui();
}

static const char *play_label(void) {
    if (st == ST_LISTEN) return "Replay";
    return autoplay ? "Pause" : "Start";
}

static const char *mode_label(void) {
    return mode == MODE_LEARN ? "Mode:\nLearn" : "Mode:\nPractice";
}

static char label_buf[3][24];

static const char *level_up_label(void) {
    snprintf(label_buf[0], sizeof(label_buf[0]), "Level +\n%i", level());
    return label_buf[0];
}

static const char *level_down_label(void) {
    snprintf(label_buf[1], sizeof(label_buf[1]), "Level -\n%i", level());
    return label_buf[1];
}

static const char *group_label(void) {
    snprintf(label_buf[2], sizeof(label_buf[2]), "Group\n%i", (int)param_i_get(cfg_cwt_group));
    return label_buf[2];
}

static char speed_buf[2][24];

static const char *speed_up_label(void) {
    snprintf(speed_buf[0], sizeof(speed_buf[0]), "Speed +\n%i WPM", (int)param_i_get(cfg_key_speed));
    return speed_buf[0];
}

static const char *speed_down_label(void) {
    snprintf(speed_buf[1], sizeof(speed_buf[1]), "Speed -\n%i WPM", (int)param_i_get(cfg_key_speed));
    return speed_buf[1];
}

/* Screen */

static void set_btn(button_data_t *b, bool disabled) {
    if (b->disabled != disabled) buttons_disabled(b, disabled);
}

static void refresh_btn(button_data_t *b) {
    if (b->disp_btn) buttons_refresh(b);
}

static void show(lv_obj_t *o, bool on) {
    if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_ui(void) {
    bool learn = mode == MODE_LEARN;
    int  wpm   = param_i_get(cfg_key_speed);
    char m1[48], m2[48], buf[160];

    set_btn(&btn_play, !learn);
    set_btn(&btn_skip, !learn);
    set_btn(&btn_lvl_u, !learn);
    set_btn(&btn_lvl_d, !learn);
    set_btn(&btn_group, !learn);
    set_btn(&btn_clear, learn);
    refresh_btn(&btn_play);
    refresh_btn(&btn_mode);
    refresh_btn(&btn_lvl_u);
    refresh_btn(&btn_lvl_d);
    refresh_btn(&btn_group);
    refresh_btn(&btn_spd_u);
    refresh_btn(&btn_spd_d);

    show(box, true);
    show(big, learn);
    show(line1, learn);
    show(line2, learn);
    show(text_lbl, !learn);
    show(side_score, learn);
    show(side_chars, learn);

    if (!learn) {
        lv_label_set_text(head, "CW TRAINER - #ffffff Practice#");
        snprintf(buf, sizeof(buf), "%i WPM (keyer %i)", wpm_seen, wpm);
        lv_label_set_text(head_right, buf);
        lv_label_set_text(text_lbl, text[0] ? text : "Key with the paddle: the text appears here");
        lv_obj_set_style_text_color(text_lbl, lv_color_hex(text[0] ? 0xffffff : COLOR_DIM), 0);
        lv_obj_update_layout(box);
        lv_obj_scroll_to_y(box, LV_COORD_MAX, LV_ANIM_OFF);
        lv_label_set_text(hint, "No transmission: key train is on. Clear: page 3");
        return;
    }

    lv_label_set_text(head, "CW TRAINER - #ffffff Learn (Koch)#");
    snprintf(buf, sizeof(buf), "%i WPM - group %i", wpm, (int)param_i_get(cfg_cwt_group));
    lv_label_set_text(head_right, buf);

    lv_label_set_text(line1, "");
    lv_label_set_text(line2, "");
    switch (st) {
        case ST_READY:
            lv_label_set_text(big, "Press Play");
            lv_obj_set_style_text_color(big, lv_color_hex(COLOR_HEAD), 0);
            lv_label_set_text(hint, "Start: groups follow each other, key each one back with the paddle");
            break;
        case ST_PLAYING:
            lv_label_set_text(big, "Listen...");
            lv_obj_set_style_text_color(big, lv_color_hex(COLOR_NEW), 0);
            lv_label_set_text(hint, "The radio keys the group (sidetone only)");
            /* The group stays on screen while playing and answering, to memorise it */
            morse_line(target, m1, sizeof(m1));
            snprintf(buf, sizeof(buf), "Sent:   #ffd24a %s#   %s", target, m1);
            lv_label_set_text(line1, buf);
            lv_label_set_text(line2, "");
            break;
        case ST_LISTEN:
            lv_label_set_text(big, heard[0] ? heard : "Your turn");
            lv_obj_set_style_text_color(big, lv_color_hex(heard[0] ? 0xffffff : COLOR_NEW), 0);
            lv_label_set_text(hint, "Key the group with the paddle. Replay: hear it again");
            morse_line(target, m1, sizeof(m1));
            snprintf(buf, sizeof(buf), "Sent:   #ffd24a %s#   %s", target, m1);
            lv_label_set_text(line1, buf);
            morse_line(heard, m2, sizeof(m2));
            snprintf(buf, sizeof(buf), "Heard: #ffffff %s#   %s", heard, m2);
            lv_label_set_text(line2, heard[0] ? buf : "");
            break;
        case ST_RESULT:
            lv_label_set_text(big, last_ok ? "OK" : "WRONG");
            lv_obj_set_style_text_color(big, lv_color_hex(last_ok ? COLOR_OK : COLOR_BAD), 0);
            morse_line(target, m1, sizeof(m1));
            morse_line(heard, m2, sizeof(m2));
            snprintf(buf, sizeof(buf), "Sent:   #ffd24a %s#   %s", target, m1);
            lv_label_set_text(line1, buf);
            snprintf(buf, sizeof(buf), "Heard: #%s %s#   %s", last_ok ? "60ff60" : "ff6060", heard, m2);
            lv_label_set_text(line2, buf);
            lv_label_set_text(hint, autoplay ? "Next group follows. Pause: stop after this one" : "Start: resume. Skip: another group");
            break;
    }

    int good = 0;
    for (int i = 0; i < hist_n; i++) good += hist[i];
    snprintf(buf, sizeof(buf), "Score  %i / %i\nLast %i: %i / %i\nNext level at %i%%", right, tried, WINDOW, good, hist_n, PASS_PCT);
    lv_label_set_text(side_score, buf);

    /* Characters in use, the newest highlighted */
    char *p = buf;
    int   l = level();
    for (int i = 0; i < l; i++) {
        p += sprintf(p, i == l - 1 ? "#ffd24a %c#" : "%c", koch[i].c);
        *p++ = ' ';
    }
    *p = 0;
    char cbuf[200];
    snprintf(cbuf, sizeof(cbuf), "Level %i\n%s", l, buf);
    lv_label_set_text(side_chars, cbuf);
}

static void key_cb(lv_event_t *e) {
    switch (*((uint32_t *)lv_event_get_param(e))) {
        case LV_KEY_LEFT:
        case LV_KEY_DOWN:
            change_speed(-1);
            break;
        case LV_KEY_RIGHT:
        case LV_KEY_UP:
            change_speed(1);
            break;
        case LV_KEY_ENTER:
        case KEYBOARD_F4:
            play_cb(NULL);
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
        default:
            break;
    }
}

/* MFK press: the encoder sends a click, not ENTER. */
static void click_cb(lv_event_t *e) {
    (void)e;
    lv_indev_t *indev = lv_indev_get_act();
    if (indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) play_cb(NULL);
}

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_recolor(l, true);
    return l;
}

static lv_obj_t *panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t w) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, BOX_H);
    lv_obj_set_pos(o, x, BOX_Y);
    lv_obj_set_style_bg_color(o, lv_color_hex(0x121a22), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0x445566), 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_radius(o, 8, 0);
    lv_obj_set_style_pad_all(o, 12, 0);
    return o;
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);

    /* Train first: nothing below may key the radio without it */
    prev_train = param_i_get(cfg_key_train);
    saved      = true;
    force_train();
    cw_encoder_require_train(true);

    /* CW mode: the decoder listens only there */
    mode_changed = !is_cw();
    if (mode_changed) {
        prev_mode = (x6100_mode_t)cparam_i_get(cfg_cur_mode);
        cparam_i_set(cfg_cur_mode, x6100_mode_cw);
    }
    main_screen_lock_mode(true);
    panel_hide();

    srand(time(NULL));
    mode = MODE_LEARN;
    st   = ST_READY;
    autoplay = false;
    target[0] = heard[0] = text[0] = 0;
    heard_n = tried = right = wpm_seen = 0;
    reset_score();

    btn_p1.next = &page_2;
    btn_p1.prev = &page_3;
    btn_p2.next = &page_3;
    btn_p2.prev = &page_1;
    btn_p3.next = &page_1;
    btn_p3.prev = &page_2;

    head = label(dialog.obj, &sony_20, COLOR_HEAD);
    lv_obj_set_pos(head, X, 10);
    head_right = label(dialog.obj, &sony_20, COLOR_HEAD);
    lv_obj_set_width(head_right, 300);
    lv_obj_set_style_text_align(head_right, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(head_right, X + W - 300, 10);

    /* Learn: result on the left, score and characters on the right. Practice: the
     * text uses the whole box and scrolls. */
    box = panel(dialog.obj, X, W);
    lv_obj_add_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    text_lbl = label(box, &sony_44, 0xffffff);
    lv_obj_set_width(text_lbl, W - 24);
    lv_label_set_long_mode(text_lbl, LV_LABEL_LONG_WRAP);

    big = label(box, &sony_44, COLOR_HEAD);
    lv_obj_set_width(big, W - SIDE_W - 40);
    lv_obj_set_pos(big, 0, 0);
    line1 = label(box, &sony_28, COLOR_DIM);
    lv_obj_set_pos(line1, 0, 90);
    line2 = label(box, &sony_28, COLOR_DIM);
    lv_obj_set_pos(line2, 0, 140);
    side_score = label(box, &sony_22, 0xd0e0f0);
    lv_obj_set_width(side_score, SIDE_W - 12);
    lv_obj_set_pos(side_score, W - SIDE_W - 12, 0);
    side_chars = label(box, &sony_22, 0xd0e0f0);
    lv_label_set_long_mode(side_chars, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(side_chars, SIDE_W - 12);
    lv_obj_set_pos(side_chars, W - SIDE_W - 12, 100);

    hint = label(dialog.obj, &sony_18, COLOR_DIM);
    lv_obj_set_pos(hint, X, DIALOG_H - 30);

    lv_group_add_obj(keyboard_group, dialog.obj);
    lv_obj_add_event_cb(dialog.obj, key_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(dialog.obj, click_cb, LV_EVENT_CLICKED, NULL);
    lv_group_focus_obj(dialog.obj);
    lv_group_set_editing(keyboard_group, true);

    active = true;
    cw_decoder_set_sink(sink_text, sink_wpm);
    cw_trainer_set(true);
    timer = lv_timer_create(tick_cb, TIMER_MS, NULL);
    refresh_ui();
}

/* Every exit: the close button, another app, a key page, the mode timer */
static void destruct_cb(void) {
    active = false;
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
    cw_trainer_set(false);
    cw_decoder_set_sink(NULL, NULL);
    cw_encoder_stop();
    cw_encoder_require_train(false);

    if (mode_changed && is_cw()) {  /* not when the user changed the mode */
        cparam_i_set(cfg_cur_mode, prev_mode);
    }
    mode_changed = false;
    if (saved) {
        param_i_set(cfg_key_train, prev_train);
        saved = false;
    }
    lv_group_remove_obj(dialog.obj);
    main_screen_lock_mode(false);
    panel_update_visibility(false);
}
