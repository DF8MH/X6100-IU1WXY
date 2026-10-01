/* Hand-written stubs with plausible values (the rest is auto-stubbed). */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "wifi.h"
#include "btaudio.h"
#include "scheduler.h"
extern void *cfg_cur_filter_low, *cfg_cur_filter_high, *cfg_fg_freq, *cfg_cur_mode, *cfg_pwr;
extern void *cfg_tx_filter_low, *cfg_tx_filter_high, *cfg_ft8_auto, *cfg_ft8_hold_freq, *cfg_ft8_max_repeats,
            *cfg_ft8_protocol, *cfg_ft8_show_all, *cfg_wefax_align, *cfg_wefax_tilt,
            *cfg_cur_agc, *cfg_cur_att;
static char dummies[16][64];
__attribute__((constructor)) static void init_ptrs(void) {
    void **pp[] = {&cfg_cur_filter_low, &cfg_cur_filter_high, &cfg_fg_freq, &cfg_cur_mode, &cfg_pwr, &cfg_tx_filter_low,
                   &cfg_tx_filter_high, &cfg_ft8_auto, &cfg_ft8_hold_freq, &cfg_ft8_max_repeats, &cfg_ft8_protocol,
                   &cfg_ft8_show_all, &cfg_wefax_align, &cfg_wefax_tilt, &cfg_cur_agc, &cfg_cur_att};
    for (unsigned i = 0; i < sizeof pp / sizeof pp[0]; i++) *pp[i] = dummies[i];
}
int32_t harness_fg_freq = 14074000;
int32_t harness_mode = 3; /* USB-D, the trainer switches to CW */
int32_t cparam_i_get(const void *p) {
    if (p == cfg_cur_filter_low) return 100;
    if (p == cfg_cur_filter_high) return 2900;
    if (p == cfg_fg_freq) return harness_fg_freq;
    if (p == cfg_cur_mode) return harness_mode;
    if (p == cfg_cur_agc) return 3; /* AUTO */
    if (p == cfg_cur_att) return 0;
    return 1;
}
void cparam_i_set(void *p, int32_t v) { if (p == cfg_fg_freq) harness_fg_freq = v; if (p == cfg_cur_mode) harness_mode = v; }
/* wfview server: enabled, one fake client logged in */
bool wfview_server_get_enabled(void) { return true; }
void wfview_server_set_enabled(bool on) { (void)on; }
void wfview_server_set_credentials(const char *u, const char *p) { (void)u; (void)p; }
const char *wfview_server_user(void) { return "x6100"; }
const char *wfview_server_pass(void) { return "x6100"; }
const char *wfview_server_client(void) { return "DESKTOP-wfview"; }
wifi_status_t wifi_get_status(void) { return WIFI_CONNECTED; }

/* Settings -> Wireless: fake networks and Bluetooth devices */
static gboolean any_password(const char *p) { return TRUE; }
wifi_ap_arr_t wifi_get_available_access_points(void) {
    static const wifi_ap_info_t aps[] = {
        {"Vodafone-A1B2", "", 45, 0, false, false, any_password},
        {"MyWiFi", "", 90, 0, true, true, any_password},
        {"MyWiFi-5G", "", 70, 0, false, true, any_password},
        {"FreeWiFi", "", 20, 0, false, false, NULL},
    };
    wifi_ap_arr_t r = {malloc(sizeof aps), 4, true};
    memcpy(r.ap_arr, aps, sizeof aps);
    return r;
}
void wifi_aps_info_delete(wifi_ap_arr_t a) { free(a.ap_arr); }
bool wifi_get_ipaddr(char **ip, char **gw) { strcpy(*ip, "10.0.0.62"); strcpy(*gw, "10.0.0.2"); return true; }
bool btaudio_mute_get(void) { return true; }
typedef struct { btaudio_done_cb_t cb; void *user; const char *out; } fake_run_t;
static void fake_done(void *arg) { fake_run_t *r = arg; if (r->cb) r->cb(r->out, 0, r->user); }
void btaudio_run(const char *args, btaudio_done_cb_t cb, void *user) {
    fake_run_t r = {cb, user, ""};
    printf("[btaudio-ctl] %s\n", args);
    if (!strcmp(args, "list"))
        r.out = "DEV\t00:11:22:33:44:01\t1\thp\t1\tBT Headphones\n"
                "DEV\t00:11:22:33:44:02\t0\tcat\t0\tTablet\n"
                "HP\t1\t60\nRX\tyes\nMUTE\tyes\n";
    else if (!strcmp(args, "scan"))
        r.out = "NEW\t11:22:33:44:55:66\tSoundcore Life Q30\n";
    scheduler_put(fake_done, &r, sizeof r);
}
extern void *cfg_band_split;
extern void *cfg_cluster_column;
int32_t harness_cluster_column;
/* CW trainer: level, group and keyer speed are kept here, train is on */
extern void *cfg_cwt_level, *cfg_cwt_group, *cfg_key_speed, *cfg_key_train;
int32_t harness_cwt_level = 2, harness_cwt_group = 1, harness_key_speed = 15;
/* WSPR app settings, kept here */
extern void *cfg_wspr_tx_pct, *cfg_wspr_tx_freq, *cfg_wspr_upload;
int32_t harness_wspr_tx_pct = 0, harness_wspr_tx_freq = 1500, harness_wspr_upload = 0;
void param_i_set(void *p, int32_t v) {
    if (p == cfg_wspr_tx_pct) harness_wspr_tx_pct = v;
    if (p == cfg_wspr_tx_freq) harness_wspr_tx_freq = v;
    if (p == cfg_wspr_upload) harness_wspr_upload = v;
    if (p == cfg_cwt_level) harness_cwt_level = v;
    if (p == cfg_cwt_group) harness_cwt_group = v;
    if (p == cfg_key_speed) harness_key_speed = v;
}
/* The decoder sink of the trainer: the harness feeds "decoded" text */
static void (*sink_text)(const char *);
static void (*sink_wpm)(int);
void cw_decoder_set_sink(void (*text)(const char *), void (*wpm)(int)) { sink_text = text; sink_wpm = wpm; }
void harness_decode_wpm(int w) { if (sink_wpm) sink_wpm(w); }
void harness_decode(const char *t) { if (sink_text) sink_text(t); }
int32_t param_i_get(const void *p) {
    if (p == cfg_wspr_tx_pct) return harness_wspr_tx_pct;
    if (p == cfg_wspr_tx_freq) return harness_wspr_tx_freq;
    if (p == cfg_wspr_upload) return harness_wspr_upload;
    if (p == cfg_cwt_level) return harness_cwt_level;
    if (p == cfg_cwt_group) return harness_cwt_group;
    if (p == cfg_key_speed) return harness_key_speed;
    if (p == cfg_key_train) return 1;
    if (p == cfg_cluster_column) return harness_cluster_column;
    if (p == cfg_band_split) return 0;
    if (p == cfg_tx_filter_low) return 160;
    if (p == cfg_tx_filter_high) return 3000;
    return 1;
}
float param_f_get(const void *p) { return 10.0f; }
int32_t subject_i_get(const void *s) { return 1; }
const char *cfg_digital_label_get(void) { return "20m"; }
bool cfg_digital_load(int8_t dir, int type) { return true; }
static void v(const char *t, const char *fmt, va_list ap) { printf("[%s] ", t); vprintf(fmt, ap); printf("\n"); }
void msg_update_text_fmt(const char *fmt, ...) { va_list ap; va_start(ap, fmt); v("msg", fmt, ap); va_end(ap); }
void msg_schedule_text_fmt(const char *fmt, ...) { va_list ap; va_start(ap, fmt); v("msg", fmt, ap); va_end(ap); }
const char *x6100_control_get_fw_version_str(void) { return "1.1.9"; }
const char *util_canonize_callsign(const char *c, bool s) { return c; }
/* Top bar: subscriptions notify at once (only in the topbar mode) */
typedef void (*observer_cb)(void *subj, void *user_data);
bool harness_notify;
void *subject_subscribe_delayed_and_notify(void *subj, observer_cb fn, void *user_data) {
    if (harness_notify) fn(subj, user_data);
    return NULL;
}
void lpf(float *x, float current, float beta, float initial) { *x = current; }
/* GPS with a fix (harness_gps_fix = false: no fix since 5 minutes) */
bool harness_gps_fix = true;
int gps_status(void) { return 1; /* GPS_STATUS_WORKING */ }
bool gps_last_fix(double *lat, double *lon, int *age_s) {
    *lat = 45.0; *lon = 7.6; *age_s = harness_gps_fix ? 1 : 300;
    return true;
}
void split_freq(int32_t freq, uint16_t *mhz, uint16_t *khz, uint16_t *hz) {
    *mhz = freq / 1000000;
    *khz = (freq / 1000) % 1000;
    *hz  = freq % 1000;
}

/* Linked with --wrap=connect: every outgoing connection is refused */
#include <errno.h>
#include <sys/socket.h>
int __wrap_connect(int fd, const struct sockaddr *addr, socklen_t len) {
    (void)fd; (void)addr; (void)len;
    errno = ECONNREFUSED;
    return -1;
}
/* BASE rev 12: Settings shows every option (struct of 4 bytes, rev last) */
uint32_t x6100_control_get_base_ver(void) { return 12u << 24; }
