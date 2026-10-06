/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  CW decoder ported from upstream x6100_gui (gdyuldin/x6100_gui,
 *  src/cw_decoder/cw_receiver.cpp, b27bbe5).
 */

#include "cw_receiver.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace cw {

namespace {

// Carrier floor at the NCO: a steady carrier (birdie) on the tone lifts the
// low percentile of power/noise, which is ~0.051 (5th percentile) for noise
// alone. Above FLOOR_K times that, the noise reference is raised with it, so
// the carrier is not keyed by its own noise. CW keying leaves the 5th
// percentile in its gaps. A batch estimate over the first second after a
// reset or a retune, then multiplicative tracking steps; a detector held ON
// longer than any dash raises it quickly.
constexpr float FLOOR_PCT     = 0.05f;
constexpr float FLOOR_Q_NOISE = 0.05129f; // -ln(1 - FLOOR_PCT)
constexpr float FLOOR_K       = 2.0f;
constexpr float FLOOR_STEP    = 0.02f;
constexpr int   CARRIER_HOPS  = 60;    // 480 ms, longer than a 10 WPM dash
constexpr float CARRIER_RISE  = 1.05f; // per hop

} // namespace

CwReceiver::CwReceiver(EmitTextFn emit_text, EmitOnOffFn emit_on_off, FrameFn on_frame)
    : decoder_(std::move(emit_text)), emit_on_off(std::move(emit_on_off)), on_frame_(std::move(on_frame)) {
    update_search_region();
    change_threshold(5.0f);
}

void CwReceiver::update_search_region() {
    region_from_ = std::max(size_t(0), static_cast<size_t>(hpf_hz_ * static_cast<float>(FFT_SIZE) / SAMPLE_RATE));
    region_to_ = std::min(SPECTRUM_SIZE - 1, static_cast<size_t>(lpf_hz_ * static_cast<float>(FFT_SIZE) / SAMPLE_RATE));
}

void CwReceiver::change_threshold(float db_val) {
    detector_.set_threshold(db_val);
}

void CwReceiver::change_hpf_hz(float hz) {
    hpf_hz_ = hz;
    update_search_region();
}

void CwReceiver::change_lpf_hz(float hz) {
    lpf_hz_ = hz;
    update_search_region();
}

void CwReceiver::change_peak_filter(bool on, float key_tone_hz, float q) {
    peak_filter_.configure(on, key_tone_hz, q);
    // Whitening changes the units of the per-bin noise; re-seed the EMA so the
    // detector does not mix pre- and post-filter estimates.
    noise_bin_smoothed_ = -1.0f;
    noise_integr_       = -1.0f;
    batch_hops_         = 0;
    muted_              = true;
    floor_q_            = 0.0f;
}

FrameObservation CwReceiver::analyze_frame(const ComplexSpectrum &fft_output) {
    // Power spectrum (I^2 + Q^2). When the external peak filter is active the
    // bins are whitened by its known response, so the percentile noise estimate
    // and the peak ratio are independent of the filter's colouring.
    if (peak_filter_.enabled()) {
        const std::array<float, SPECTRUM_SIZE> &h2 = peak_filter_.h2();
        for (size_t i = region_from_; i <= region_to_; ++i) {
            power_spectrum_[i] = std::norm(fft_output[i]) / h2[i];
        }
    } else {
        for (size_t i = region_from_; i <= region_to_; ++i) {
            power_spectrum_[i] = std::norm(fft_output[i]);
        }
    }

    // Coarse maximum inside the search region
    size_t max_idx = region_from_;
    float  max_val = power_spectrum_[region_from_];
    for (size_t i = region_from_ + 1; i <= region_to_; ++i) {
        if (power_spectrum_[i] > max_val) {
            max_val = power_spectrum_[i];
            max_idx = i;
        }
    }

    // Per-bin noise estimate: 25% percentile, then EMA-smoothed across frames.
    size_t region_length = region_to_ - region_from_ + 1;
    for (size_t i = 0; i < region_length; ++i) {
        percentile_scratch_[i] = power_spectrum_[region_from_ + i];
    }

    size_t pct_offset = static_cast<size_t>(0.25f * static_cast<float>(region_length));
    if (pct_offset >= region_length) {
        pct_offset = region_length - 1;
    }
    std::nth_element(percentile_scratch_.begin(), percentile_scratch_.begin() + pct_offset,
                     percentile_scratch_.begin() + region_length);
    float noise_bin_inst = percentile_scratch_[pct_offset];

    if (noise_bin_smoothed_ < 0.0f) {
        noise_bin_smoothed_ = noise_bin_inst;
    } else {
        float beta = (noise_bin_inst > noise_bin_smoothed_) ? 0.003f : 0.01f;
        noise_bin_smoothed_ += beta * (noise_bin_inst - noise_bin_smoothed_);
    }

    // Parabolic interpolation over 3 points in log scale
    float precise_bin = static_cast<float>(max_idx);

    if (max_idx > region_from_ && max_idx < region_to_) {
        float y1  = 10.0f * std::log10(power_spectrum_[max_idx - 1] + 1e-15f);
        float y2  = 10.0f * std::log10(power_spectrum_[max_idx] + 1e-15f);
        float y3  = 10.0f * std::log10(power_spectrum_[max_idx + 1] + 1e-15f);
        float den = y1 - 2.0f * y2 + y3;

        if (den < -1.5f) {
            float delta = 0.5f * (y1 - y3) / den;
            precise_bin = static_cast<float>(max_idx) + delta;
        }
    }
    current_freq_hz_ = precise_bin * SAMPLE_RATE / static_cast<float>(FFT_SIZE);

    float snr_lin = (noise_bin_smoothed_ > 0.0f) ? max_val / noise_bin_smoothed_ : 0.0f;

    // Candidate gate is the FFT SNR, independent of the user floor: a raised
    // detection floor must not stop the frequency from being tracked.
    bool is_strong = (snr_lin >= db_to_lin(FFT_STRONG_DB));
    if (is_strong) {
        if (prev_peak_bin_ >= 0 && std::abs(static_cast<int>(max_idx) - prev_peak_bin_) <= 1) {
            stable_frames_++;
        } else {
            stable_frames_ = 1;
        }
        prev_peak_bin_ = static_cast<int>(max_idx);
    } else {
        stable_frames_ = 0;
        prev_peak_bin_ = -1;
    }

    // Is the station at the NCO in this frame, or only the leak of a stronger
    // one elsewhere? (see CoherentToneTracker::consider_retune)
    float nco_rel = 0.0f;
    const long nco_bin = std::lround(tone_tracker_.get_freq_hz() * static_cast<float>(FFT_SIZE) / SAMPLE_RATE);
    if (nco_bin >= static_cast<long>(region_from_) && nco_bin <= static_cast<long>(region_to_) && max_val > 0.0f) {
        nco_rel = power_spectrum_[nco_bin] / max_val;
    }

    diag_peak_freq_hz_  = current_freq_hz_;
    diag_snr_lin_       = snr_lin;
    diag_stable_frames_ = stable_frames_;

    return {.freq_hz = current_freq_hz_, .snr_lin = snr_lin, .stable_frames = stable_frames_, .nco_rel = nco_rel};
}

void CwReceiver::process_audio_frame(size_t n, float *samples) {
    bool retuned = false;

    for (size_t i = 0; i < n; ++i) {
        if (spgram_.execute(samples[i])) {
            FrameObservation obs = analyze_frame(spgram_.get_fft_output());
            if (noise_bin_smoothed_ > 0.0f) {
                noise_integr_ = noise_bin_smoothed_ * NOISE_ALIGN;
                tone_tracker_.set_noise(noise_integr_);
                if (tone_tracker_.consider_retune(obs)) {
                    classifier_.on_retune();
                    batch_hops_ = 0; // a new tone: estimate its carrier floor again
                    retuned = true;
                }
            }
        }

        float power = tone_tracker_.process(samples[i]);
        if (power >= 0.0f && noise_integr_ > 0.0f) {
            const float r = power / noise_integr_;
            if (batch_hops_ < BATCH_HOPS) {
                // A batch estimate of the carrier floor over the first second
                // (after a reset or a retune), then tracking from there.
                batch_r_[batch_hops_++] = r;
                if (batch_hops_ == BATCH_HOPS) {
                    auto q = batch_r_.begin() + static_cast<long>(FLOOR_PCT * BATCH_HOPS);
                    std::nth_element(batch_r_.begin(), q, batch_r_.end());
                    floor_q_ = std::max({floor_q_, *q, 1e-3f * FLOOR_Q_NOISE});
                    muted_   = false;
                }
            }
            if (muted_) {
                // After a reset the noise floors settle: nothing is detected.
                power = 0.0f;
            } else if (on_hops_ > CARRIER_HOPS) {
                // Keyed far longer than a dash: a carrier. Catch up quickly.
                floor_q_ *= CARRIER_RISE;
            } else {
                floor_q_ *= (r > floor_q_) ? (1.0f + FLOOR_STEP * FLOOR_PCT) : (1.0f - FLOOR_STEP * (1.0f - FLOOR_PCT));
                floor_q_ = std::max(floor_q_, 1e-3f * FLOOR_Q_NOISE); // never stuck at 0
            }
            const float noise = noise_integr_ * std::max(1.0f, floor_q_ / (FLOOR_K * FLOOR_Q_NOISE));
            auto res = detector_.feed(power, noise);
            if (res.edge > 0)
                mark_peak_ = power / noise;
            else if (detector_.is_on() || res.edge < 0)
                mark_peak_ = std::max(mark_peak_, power / noise);
            const bool strong = mark_peak_ >= db_to_lin(detector_.floor_db() + LONE_MARK_DB);
            Token token = classifier_.feed(res.edge, res.dt, strong);
            if (token != CW_NONE) {
                decoder_.handle_token(token);
            }
            if (res.edge != 0) {
                is_signal_detected = (res.edge > 0);
                emit_on_off(is_signal_detected);
            }
            on_hops_ = is_signal_detected ? on_hops_ + 1 : 0;

            // Diagnostics for the offline inspector; written after the detector
            // and classifier have run, so the reported state is already the new
            // one.
            diag_snr_db_    = 10.0f * std::log10(power / noise_integr_ + 1e-12f);
            diag_nco_hz_    = tone_tracker_.get_freq_hz();
            diag_t_on_lin_  = detector_.on_threshold_lin();
            diag_t_off_lin_ = detector_.off_threshold_lin();
            diag_is_active_ = is_signal_detected;
            diag_retuned_   = retuned;
            retuned         = false;
            if (on_frame_) {
                on_frame_();
            }
        }
    }
}

float CwReceiver::get_measured_wpm() const {
    return classifier_.get_current_wpm();
}

float CwReceiver::get_tone_freq() const {
    return current_freq_hz_;
}

float CwReceiver::get_lock_freq() const {
    return tone_tracker_.get_freq_hz();
}

} // namespace cw
