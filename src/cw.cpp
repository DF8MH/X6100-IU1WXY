/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 *
 *  CW decoding with the receiver ported from upstream x6100_gui
 *  (src/cw_decoder/): FFT tone search, coherent tone tracker, adaptive
 *  detector, histogram timing and Morse tree.
 */

// First: the decoder headers include <complex>, which liquid.h (via cw.h)
// needs to see to use std::complex.
#include "cw_decoder/audio_resampler.h"

#include "cw.h"

#include "cfg/settings_manager.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

extern "C" {
    #include "lvgl/lvgl.h"
    #include "cw_decoder.h"
    #include "cw_tune_ui.h"
    #include "remote_server.h"
}

/* Frames (8 ms) between updates of the tune indicator */
#define TUNE_UPDATE_FRAMES  12

static bool ready = false;

/* Written on the LVGL thread, read on the audio thread */
static bool    trainer = false;
static bool    cw_decoder;
static bool    cw_tune;
static int32_t key_tone = 0;
static int32_t filter_low;
static int32_t filter_high;

/* Receiver settings: stored by the subscriptions (LVGL thread), applied by
 * the audio thread before the next block */
static std::atomic<float> rx_threshold;
static std::atomic<float> rx_hpf;
static std::atomic<float> rx_lpf;
static std::atomic<bool>  rx_peak_on;
static std::atomic<float> rx_peak_tone;
static std::atomic<float> rx_peak_q;
static std::atomic<bool>  rx_dirty{false};

/* Audio thread */
static cw::CwReceiver     *receiver;
static cw::AudioResampler *resampler;
static bool                keyed = false;
static float               tone_freq;
static std::array<float, 3> freq_hist;
static size_t              freq_hist_w = 0;
static size_t              freq_hist_n = 0;
static uint8_t             frame_counter = 0;
static int                 wpm_sent = 0;

static void on_val_bool_change(Subject *subj, void *user_data);
static void on_rx_param_change(Subject *subj, void *user_data);

static void on_text(const char *text);
static void on_keyed(bool on);
static void on_frame();

void cw_init() {
    cfg_sm.p_cw_decoder.subscribe_and_notify(on_val_bool_change, (void*)&cw_decoder);
    cfg_sm.p_cw_tune.subscribe_and_notify(on_val_bool_change, (void*)&cw_tune);

    cfg_sm.p_key_tone.subscribe(on_rx_param_change);
    cfg_sm.p_cw_decoder_snr.subscribe(on_rx_param_change);
    cfg_sm.p_cw_peak_on.subscribe(on_rx_param_change);
    cfg_sm.p_cw_peak_q.subscribe(on_rx_param_change);
    cfg_sm.cp_cur_filter_low.subscribe(on_rx_param_change);
    cfg_sm.cp_cur_filter_high.subscribe_and_notify(on_rx_param_change);
    tone_freq = key_tone;

    receiver = new cw::CwReceiver(on_text, on_keyed, on_frame);
    resampler = new cw::AudioResampler((float)AUDIO_CAPTURE_RATE / AUDIO_DECIM);

    ready = true;
}

static void apply_rx_params() {
    static bool  peak_on;
    static float peak_tone, peak_q;

    if (!rx_dirty.exchange(false)) {
        return;
    }
    receiver->change_threshold(rx_threshold);
    receiver->change_hpf_hz(rx_hpf);
    receiver->change_lpf_hz(rx_lpf);

    /* Changing the peak filter re-seeds the noise floor: only on a change */
    if (peak_on != rx_peak_on || peak_tone != rx_peak_tone || peak_q != rx_peak_q) {
        peak_on = rx_peak_on;
        peak_tone = rx_peak_tone;
        peak_q = rx_peak_q;
        receiver->change_peak_filter(peak_on, peak_tone, peak_q);
    }
}

void cw_put_audio_samples(unsigned int n, float *samples) {
    if (!ready) {
        return;
    }
    /* X6100 Link shows the tuning indicator on its own */
    if ((!cw_decoder) && (!cw_tune) && (!trainer) && !remote_subscribed()) {
        return;
    }
    apply_rx_params();
    resampler->process(n, samples, *receiver);
}

/* Decoded text: to the panel or the trainer, with the WPM when it changes */
static void on_text(const char *text) {
    if ((!cw_decoder) && (!trainer)) {
        return;
    }
    cw_decoder_text(text);

    int wpm = lroundf(receiver->get_measured_wpm());
    if (wpm != wpm_sent) {
        wpm_sent = wpm;
        cw_decoder_wpm(wpm);
    }
}

static void on_keyed(bool on) {
    keyed = on;
}

/* Every 8 ms: follow the tone while keyed and locked (median of 3, then
 * smoothed). The tone the detector is locked on: the FFT peak of a frame jumps
 * to any other station or noise peak in the passband; and only a tone held
 * coherently: noise that keys the detector (the Es of an empty band) does not
 * light the indicator. */
static void on_frame() {
    bool tuned = keyed && receiver->is_locked();

    if (tuned) {
        float freq = receiver->get_lock_freq();
        if ((freq >= filter_low) && (freq <= filter_high)) {
            freq_hist[freq_hist_w] = freq;
            freq_hist_w = (freq_hist_w + 1) % freq_hist.size();
            if (freq_hist_n < freq_hist.size()) {
                freq_hist_n++;
            }
            if (freq_hist_n == freq_hist.size()) {
                std::array<float, 3> s = freq_hist;
                std::sort(s.begin(), s.end());
                tone_freq += 0.5f * (s[1] - tone_freq);
            }
        }
    } else {
        freq_hist_n = 0;
    }

    if (++frame_counter >= TUNE_UPDATE_FRAMES) {
        frame_counter = 0;
        if (tuned) {
            cw_tune_set_freq(key_tone - tone_freq);
        }
    }
}

void cw_trainer_set(bool on) {
    trainer = on;
}

float cw_get_tone_freq(void) {
    return tone_freq;
}

int cw_get_wpm(void) {
    return wpm_sent;
}

static void on_val_bool_change(Subject *subj, void *user_data) {
    *(bool*)user_data = static_cast<SubjectT<int32_t>*>(subj)->get();
}

static void on_rx_param_change(Subject *subj, void *user_data) {
    key_tone = cfg_sm.p_key_tone.get();
    filter_low = cfg_sm.cp_cur_filter_low.get();
    filter_high = cfg_sm.cp_cur_filter_high.get();

    rx_threshold = cfg_sm.p_cw_decoder_snr.get();
    rx_hpf = filter_low;
    rx_lpf = filter_high;
    rx_peak_on = cfg_sm.p_cw_peak_on.get();
    rx_peak_tone = key_tone;
    rx_peak_q = cfg_sm.p_cw_peak_q.get();
    rx_dirty = true;
}
