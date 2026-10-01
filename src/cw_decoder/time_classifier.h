/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  CW decoder ported from upstream x6100_gui (gdyuldin/x6100_gui,
 *  src/cw_decoder/time_classifier.h, b27bbe5).
 */

#pragma once

#include <array>

#include "cw_config.h"

namespace cw {

// Grants the tests access to the adaptation state without widening the class
// interface. Defined only in the test tree (tests/cw_decoder/).
struct TimeClassifierTestAccess;

// Duration-histogram size and resolution. The resolution is deliberately finer
// than the BIN_SIZE_MS frame duration: the histogram holds closed-interval
// durations (marks and spaces), not whole frames.
constexpr size_t HIST_BINS   = 300;
constexpr float  HIST_BIN_MS = BIN_SIZE_MS / 2.0f;

// Tracked unit (dot length) bounds; the default is ~20 WPM.
constexpr float UNIT_MS_DEFAULT = static_cast<float>(WPM_K) / 20.0f;
constexpr float UNIT_MS_MIN     = static_cast<float>(WPM_K) / 50.0f; // 50 WPM
constexpr float UNIT_MS_MAX     = static_cast<float>(WPM_K) / 10.0f; // 10 WPM

// Morse boundaries as multiples of the unit (dot/dash 2u, element/letter 2u,
// letter/word 5u).
constexpr float DOT_DASH_TH_K    = 2.0f;
constexpr float ELEM_LETTER_TH_K = 1.7f;  // make elem/letter K shorter to avoid combining letters
constexpr float LETTER_WORD_TH_K = 5.0f;

// Histogram leak; measured by tests/cw_decoder/tools/hist_forget_sweep.cpp.
constexpr float HIST_FORGET = 0.9f;

// A mark with a word gap (or more) on both sides is a one-element word (E,
// T) or a noise blip: noise peaks and clicks come alone, 16-40 ms long. It is
// kept only if it lasts LONE_MARK_K units at the tracked speed and is clearly
// above the detection floor (noise peaks barely cross it).
constexpr float LONE_MARK_K  = 0.7f;
constexpr float LONE_MARK_DB = 3.0f; // and peak this far above the detection floor
// A mark longer than this many units is a carrier, not keying: dropped.
constexpr float MAX_MARK_K = 8.0f;

// Splits a keyed envelope into Morse intervals. The ON/OFF decision (level,
// hysteresis, glitch merge) is owned by the Detector; the classifier only
// times the intervals it is given. The hop that contains a level crossing is
// split at the crossing: its pre-crossing part closes the old interval and its
// post-crossing part opens the new one, so each hop's time is counted exactly
// once and marks and spaces merge into one histogram peak at the true unit.
// The unit is the centre of mass of the lowest cluster, not a discrete bin, so
// a multimodal cluster (dots, element spaces, jitter) averages out. A silence
// that outlives the word gap with no following mark is the end of a message: it
// is reported once as a word space so the decoder flushes the last character.
class TimeClassifier {
  public:
    // `edge` is the Detector's crossing for this frame: +1 OFF->ON, -1 ON->OFF,
    // 0 held. `dt` (ms) is the crossing offset behind the start of the
    // reporting hop (>= 0); it is used only when `edge != 0`.
    // `strong` (with edge -1): the closing mark peaked LONE_MARK_DB above the
    // detection floor; a mark alone in the silence must.
    Token feed(int edge, float dt, bool strong = true);
    float get_current_wpm() const;
    bool  is_signal_active() const;

    // A retune is a new station: drop the in-flight edges and the committed
    // state, but keep the learned speed and the histogram.
    void on_retune();

  private:
    friend struct TimeClassifierTestAccess;

    void  update_unit();
    void  update_boundaries();
    void  decay_histogram(std::array<float, HIST_BINS> &hist);
    void  add_sample(float duration_ms);
    float estimate_unit() const;
    Token classify_and_update(bool closed_on, float closed_duration_ms, bool learn);

    std::array<float, HIST_BINS> hist_{};

    // In-range intervals recorded since the histogram was last rebuilt; gates the
    // relock so a sparse post-rebuild histogram cannot start a speed hunt.
    int hist_samples_ = 0;

    // Committed state: the value is_signal_active() reports.
    bool  is_now_on_           = false;
    float current_duration_ms_ = 0.0f;

    // Latches the one-shot end-of-message word space so a long silence flushes
    // the last character once; re-armed by the next mark.
    bool idle_reported_ = true; // nothing to close before the first mark

    // The in-flight interval was cut by a retune: its length is not learned.
    bool truncated_ = false;

    // The current mark follows a word gap or the end of a message; when it
    // ends it is held until the next mark (see LONE_MARK_K).
    bool  lone_       = true;
    bool  held_       = false;
    float held_ms_    = 0.0f;
    bool  held_learn_ = false;
    bool  held_strong_ = false;
    Token queued_     = CW_NONE; // token due on the next hop

    float unit_ms_ = UNIT_MS_DEFAULT;
    // Fast re-lock: a candidate far from the current unit is committed after it
    // repeats for SPEED_CONFIRM_FRAMES, which rebuilds the histogram from the
    // new speed instead of dragging the old one across.
    float candidate_unit_ms_ = -1.0f;
    int   relock_frames_     = 0;
    float hist_forget_       = HIST_FORGET;

    // Decision boundaries derived from unit_ms_ (start values ~20 WPM).
    float threshold_dot_dash_    = DOT_DASH_TH_K * UNIT_MS_DEFAULT;
    float threshold_elem_letter_ = ELEM_LETTER_TH_K * UNIT_MS_DEFAULT;
    float threshold_letter_word_ = LETTER_WORD_TH_K * UNIT_MS_DEFAULT;
};

} // namespace cw
