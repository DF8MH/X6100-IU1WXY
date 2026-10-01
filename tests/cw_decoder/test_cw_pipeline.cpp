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

// No signal, 60 s each: nothing may be decoded. The radio with no antenna
// printed an E every few seconds (noise peaks and clicks taken as dots).
TEST_CASE("cw pipeline: no signal decodes nothing") {
    const size_t                    n = static_cast<size_t>(RATE) * 60;
    std::mt19937                    rng(7);
    std::normal_distribution<float> n01(0.0f, 1.0f);
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);
    const float lsb = 1.0f / 32768.0f;
    auto quantize = [lsb](float x) { return std::round(x / lsb) * lsb; };
    // Steady carrier at the given level over the noise, in 60 Hz (the
    // coherent detection bandwidth): A^2/2 = sigma^2 * 60 / (RATE / 2) * level
    auto birdie = [&](std::vector<float> &a, float sigma, float hz, float level_db) {
        const float amp = std::sqrt(2.0f * sigma * sigma * 60.0f / (RATE / 2.0f) * std::pow(10.0f, level_db / 10.0f));
        for (size_t i = 0; i < a.size(); ++i)
            a[i] += amp * std::sin(2.0f * static_cast<float>(M_PI) * hz * static_cast<float>(i) / RATE);
    };

    struct Case {
        const char        *name;
        std::vector<float> audio;
    };
    std::vector<Case> cases;
    {
        std::vector<float> a(n);
        for (float &x : a)
            x = 0.05f * n01(rng);
        cases.push_back({"white noise", a});
    }
    {
        std::vector<float> a(n);
        for (float &x : a)
            x = quantize(3.0f * lsb * n01(rng));
        cases.push_back({"very low noise, 3 LSB rms", a});
    }
    {
        std::vector<float> a(n);
        for (float &x : a)
            x = quantize(0.7f * lsb * n01(rng));
        cases.push_back({"quantized near-silence, 0.7 LSB rms", a});
    }
    {
        std::vector<float> a(n);
        for (size_t i = 0; i < n; ++i) {
            a[i] = 3.0f * lsb * n01(rng);
            if (u01(rng) < 1.0f / (RATE * 5.0f)) // a click every ~5 s
                for (size_t k = 0; k < 6 && i + k < n; ++k)
                    a[i + k] += 0.02f * ((k % 2) ? -1.0f : 1.0f) * std::exp(-static_cast<float>(k) / 1.5f);
        }
        cases.push_back({"sparse clicks on low noise", a});
    }
    for (float level : {0.0f, 6.0f, 15.0f}) {
        std::vector<float> a(n);
        for (float &x : a)
            x = 0.01f * n01(rng);
        birdie(a, 0.01f, 700.0f, level);
        cases.push_back({level == 0.0f ? "birdie 700 Hz at the noise level" : level == 6.0f ? "birdie 700 Hz +6 dB" : "birdie 700 Hz +15 dB", a});
    }
    {
        std::vector<float> a(n);
        for (float &x : a)
            x = 0.01f * n01(rng);
        birdie(a, 0.01f, 830.0f, 6.0f);
        cases.push_back({"birdie 830 Hz +6 dB", a});
    }

    for (auto &c : cases) {
        Result r = run(c.audio);
        INFO(c.name << ": decoded '" << r.text << "', " << r.wpm << " WPM");
        CHECK(cw_synth::normalize(r.text).empty());
        CHECK(r.wpm == 20.0f); // nothing learned: still the default speed
    }
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
