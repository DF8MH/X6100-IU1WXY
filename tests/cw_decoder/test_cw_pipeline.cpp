// SPDX-License-Identifier: LGPL-2.1-or-later
//
// End-to-end test of the decoder as the app runs it (src/cw.cpp): 11025 Hz
// audio -> AudioResampler -> CwReceiver, on synthetic CW with noise, jitter
// and drift. Checks the character accuracy and the WPM estimate.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "audio_resampler.h"
#include "cw_receiver.h"
#include "cw_synth.h"

namespace {

const std::string TEXT = "CQ CQ DE IU1WXY IU1WXY K THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 5NN 73";

struct Result {
    std::string text;
    float       wpm;
    double      seconds; // processing time
};

// The rate src/cw.cpp gets from dsp.cpp (AUDIO_CAPTURE_RATE / AUDIO_DECIM)
constexpr float RATE = 11025.0f;

Result run(const std::vector<float> &audio) {
    Result             r;
    cw::CwReceiver     rx{[&r](const char *t) { r.text += t; }, [](bool) {}};
    cw::AudioResampler rs{RATE};
    rx.change_threshold(8.0f); // p_cw_decoder_snr default

    std::vector<float> buf(audio);
    const auto         t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < buf.size(); i += 256)
        rs.process(std::min<size_t>(256, buf.size() - i), buf.data() + i, rx);
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    r.wpm     = rx.get_measured_wpm();
    return r;
}

float mean_accuracy(cw_synth::Params p, int seeds, float *wpm_out = nullptr) {
    float acc = 0.0f, wpm = 0.0f;
    for (int s = 1; s <= seeds; ++s) {
        p.seed   = s;
        Result r = run(cw_synth::make(TEXT, p));
        acc += cw_synth::accuracy(TEXT, r.text);
        wpm += r.wpm;
    }
    if (wpm_out)
        *wpm_out = wpm / seeds;
    return acc / seeds;
}

} // namespace

TEST_CASE("cw pipeline: clean signal decodes exactly at 15-30 WPM") {
    for (float wpm : {15.0f, 20.0f, 25.0f, 30.0f}) {
        cw_synth::Params p;
        p.wpm    = wpm;
        p.snr_db = 30.0f;
        Result r = run(cw_synth::make(TEXT, p));
        INFO("wpm " << wpm << " decoded '" << r.text << "'");
        CHECK(cw_synth::normalize(r.text) == TEXT);
        CHECK(std::fabs(r.wpm - wpm) <= 0.1f * wpm);
    }
}

// The tone starts 50 Hz off the tracker's initial NCO (670 Hz): a strong
// signal there used to keep the NCO "alive" through the leak, never retuned.
TEST_CASE("cw pipeline: noise, jitter and drift") {
    for (float wpm : {15.0f, 20.0f, 25.0f, 30.0f}) {
        for (float snr : {20.0f, 10.0f, 0.0f, -3.0f}) {
            cw_synth::Params p;
            p.wpm      = wpm;
            p.snr_db   = snr;
            p.tone_hz  = 620.0f;
            p.drift_hz = 30.0f;
            p.jitter   = 0.05f;
            float wpm_est;
            float acc = mean_accuracy(p, 3, &wpm_est);
            INFO("wpm " << wpm << " snr " << snr << " accuracy " << acc << " wpm est " << wpm_est);
            CHECK(acc >= (snr >= 0.0f ? 0.95f : 0.85f));
            CHECK(std::fabs(wpm_est - wpm) <= 0.15f * wpm);
        }
    }
}

TEST_CASE("cw pipeline: noise alone decodes nothing") {
    std::mt19937                     rng(7);
    std::normal_distribution<float> noise(0.0f, 0.05f);
    std::vector<float>               audio(static_cast<size_t>(RATE) * 60);
    for (float &x : audio)
        x = noise(rng);
    Result r = run(audio);
    INFO("decoded '" << r.text << "'");
    CHECK(cw_synth::normalize(r.text).size() <= 1);
}

TEST_CASE("cw pipeline: CPU cost per second of audio") {
    cw_synth::Params p;
    p.snr_db           = 5.0f;
    auto        audio  = cw_synth::make(TEXT, p);
    Result      r      = run(audio);
    const double audio_s = audio.size() / RATE;
    std::printf("cw pipeline: %.2f ms per second of audio (host)\n", 1000.0 * r.seconds / audio_s);
    CHECK(r.seconds < audio_s * 0.05); // generous: well under real time even with sanitizers
}
