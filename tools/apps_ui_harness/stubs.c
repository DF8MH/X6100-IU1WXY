/* Hand-written stubs with plausible values (the rest is auto-stubbed). */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
extern void *cfg_cur_filter_low, *cfg_cur_filter_high, *cfg_fg_freq, *cfg_cur_mode, *cfg_pwr;
extern void *cfg_tx_filter_low, *cfg_tx_filter_high, *cfg_ft8_auto, *cfg_ft8_hold_freq, *cfg_ft8_max_repeats,
            *cfg_ft8_protocol, *cfg_ft8_show_all, *cfg_wefax_align, *cfg_wefax_tilt;
static char dummies[16][64];
__attribute__((constructor)) static void init_ptrs(void) {
    void **pp[] = {&cfg_cur_filter_low, &cfg_cur_filter_high, &cfg_fg_freq, &cfg_cur_mode, &cfg_pwr, &cfg_tx_filter_low,
                   &cfg_tx_filter_high, &cfg_ft8_auto, &cfg_ft8_hold_freq, &cfg_ft8_max_repeats, &cfg_ft8_protocol,
                   &cfg_ft8_show_all, &cfg_wefax_align, &cfg_wefax_tilt};
    for (unsigned i = 0; i < sizeof pp / sizeof pp[0]; i++) *pp[i] = dummies[i];
}
int32_t harness_fg_freq = 14074000;
int32_t cparam_i_get(const void *p) {
    if (p == cfg_cur_filter_low) return 100;
    if (p == cfg_cur_filter_high) return 2900;
    if (p == cfg_fg_freq) return harness_fg_freq;
    if (p == cfg_cur_mode) return 7; /* USB-D */
    return 1;
}
void cparam_i_set(void *p, int32_t v) { if (p == cfg_fg_freq) harness_fg_freq = v; }
int wifi_get_status(void) { return 4; /* WIFI_CONNECTED */ }
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
