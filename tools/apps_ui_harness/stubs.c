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
int32_t cparam_i_get(const void *p) {
    if (p == cfg_cur_filter_low) return 100;
    if (p == cfg_cur_filter_high) return 2900;
    if (p == cfg_fg_freq) return harness_fg_freq;
    if (p == cfg_cur_mode) return 7; /* USB-D */
    if (p == cfg_cur_agc) return 3; /* AUTO */
    if (p == cfg_cur_att) return 0;
    return 1;
}
void cparam_i_set(void *p, int32_t v) { if (p == cfg_fg_freq) harness_fg_freq = v; }
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
int32_t param_i_get(const void *p) {
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
/* GPS with a fix */
int gps_status(void) { return 1; /* GPS_STATUS_WORKING */ }
bool gps_last_fix(double *lat, double *lon, int *age_s) { *lat = 45.0; *lon = 7.6; *age_s = 1; return true; }
