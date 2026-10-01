// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Synthetic CW audio for the decoder tests: known text keyed at a given WPM
// (PARIS timing, 5 ms raised-cosine edges), optional timing jitter and tone
// drift, plus white Gaussian noise at a given SNR in 2500 Hz.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace cw_synth {

struct Params {
    float sample_rate = 11025.0f; // what cw_put_audio_samples() receives
    float wpm         = 20.0f;
    float tone_hz     = 650.0f;
    float drift_hz    = 0.0f;  // linear tone drift over the whole signal
    float jitter      = 0.0f;  // std dev of each element/gap length, fraction of it
    float snr_db      = 20.0f; // tone power / noise power in 2500 Hz
    float amplitude   = 0.1f;  // tone peak (full scale 1.0)
    float lead_s      = 2.0f;  // noise before and after the text (the receiver
                               // detects nothing in its first second)
    uint32_t seed     = 1;
};

inline const std::map<char, std::string> &morse_table() {
    static const std::map<char, std::string> t = {
        {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},   {'E', "."},     {'F', "..-."},
        {'G', "--."},   {'H', "...."},  {'I', ".."},    {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
        {'M', "--"},    {'N', "-."},    {'O', "---"},   {'P', ".--."},  {'Q', "--.-"},  {'R', ".-."},
        {'S', "..."},   {'T', "-"},     {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
        {'Y', "-.--"},  {'Z', "--.."},  {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
        {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
        {'/', "-..-."}, {'?', "..--.."}, {'=', "-...-"}, {',', "--..--"}, {'.', ".-.-.-"},
    };
    return t;
}

// Key-down intervals (start, end) in seconds, after the lead-in.
inline std::vector<std::pair<double, double>> keying(const std::string &text, const Params &p, std::mt19937 &rng) {
    std::normal_distribution<double> n01(0.0, 1.0);
    const double unit = 1.2 / p.wpm;
    auto len = [&](double units) { return std::max(0.3 * unit, units * unit * (1.0 + p.jitter * n01(rng))); };

    std::vector<std::pair<double, double>> marks;
    double t = p.lead_s;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == ' ') {
            t += len(4); // 3 already added after the letter
            continue;
        }
        const std::string &code = morse_table().at(c);
        for (size_t k = 0; k < code.size(); ++k) {
            const double d = len(code[k] == '.' ? 1 : 3);
            marks.emplace_back(t, t + d);
            t += d + (k + 1 < code.size() ? len(1) : 0.0);
        }
        t += len(3);
    }
    return marks;
}

inline std::vector<float> make(const std::string &text, const Params &p) {
    std::mt19937 rng(p.seed);
    auto marks = keying(text, p, rng);
    const double total = marks.back().second + p.lead_s;
    const size_t n     = static_cast<size_t>(total * p.sample_rate);

    // Noise std dev from the SNR in 2500 Hz: tone power A^2/2, white noise
    // of variance s^2 spread over fs/2.
    const double tone_pw  = p.amplitude * p.amplitude / 2.0;
    const double noise_pw = tone_pw / std::pow(10.0, p.snr_db / 10.0);
    const double sigma    = std::sqrt(noise_pw * (p.sample_rate / 2.0) / 2500.0);
    std::normal_distribution<double> noise(0.0, sigma);

    const double rise = 0.005;
    std::vector<float> out(n);
    double phase = 0.0;
    size_t m     = 0;
    for (size_t i = 0; i < n; ++i) {
        const double t = i / static_cast<double>(p.sample_rate);
        while (m < marks.size() && t > marks[m].second + rise)
            m++;
        double env = 0.0;
        if (m < marks.size()) {
            const double a = marks[m].first, b = marks[m].second;
            if (t >= a && t < a + rise)
                env = 0.5 - 0.5 * std::cos(M_PI * (t - a) / rise);
            else if (t >= a + rise && t < b)
                env = 1.0;
            else if (t >= b && t < b + rise)
                env = 0.5 + 0.5 * std::cos(M_PI * (t - b) / rise);
        }
        const double f = p.tone_hz + p.drift_hz * t / total;
        phase += 2.0 * M_PI * f / p.sample_rate;
        out[i] = static_cast<float>(p.amplitude * env * std::sin(phase) + noise(rng));
    }
    return out;
}

// Levenshtein distance between the reference and the decoded text.
inline size_t edit_distance(const std::string &a, const std::string &b) {
    std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j)
        prev[j] = j;
    for (size_t i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (size_t j = 1; j <= b.size(); ++j)
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

// Trims and collapses spaces, so only the characters and word breaks count.
inline std::string normalize(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == ' ' && (out.empty() || out.back() == ' '))
            continue;
        out.push_back(c);
    }
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    return out;
}

// Character accuracy: 1 - edit distance / reference length (floored at 0).
inline float accuracy(const std::string &ref, const std::string &decoded) {
    const std::string d = normalize(decoded);
    const float       e = static_cast<float>(edit_distance(ref, d));
    return std::max(0.0f, 1.0f - e / static_cast<float>(ref.size()));
}

} // namespace cw_synth
