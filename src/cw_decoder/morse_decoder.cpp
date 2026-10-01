/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  CW decoder ported from upstream x6100_gui (gdyuldin/x6100_gui,
 *  src/cw_decoder/morse_decoder.cpp, b27bbe5). Here every character is
 *  emitted by its own call.
 */

#include "morse_decoder.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace cw {

MorseDecoder::MorseDecoder(EmitTextFn emit) : emit_(std::move(emit)) {
}

void MorseDecoder::append(char c) {
    if (last_char == ' ' && c == ' ') {
        return;
    }
    last_char = c;
    const char text[2] = {c, '\0'};
    if (emit_)
        emit_(text);
}

void MorseDecoder::append(const std::string_view &s) {
    last_char = s.back();
    char text[16];
    const size_t n = std::min(s.size(), sizeof(text) - 1);
    std::memcpy(text, s.data(), n);
    text[n] = '\0';
    if (emit_)
        emit_(text);
}

bool MorseDecoder::check_and_handle_exceptions()  {
    std::string_view current_input(raw_buffer_.data(), buffer_len_);

    for (const auto& ex : exceptions_) {
        if (current_input == ex.morse_str) {
            append(ex.text_out);
            return true; // Exception processed
        }
    }
    return false; // No exception in vocab
}

// Emits the character collected so far (if any) and starts a new one.
void MorseDecoder::decode_pending() {
    if (buffer_len_ > 0 && !check_and_handle_exceptions()) {
        char c = (tree_index_ > 0 && tree_index_ < morse_tree.size()) ? morse_tree[tree_index_] : ' ';
        if (c == ' ') {
            append(UNKNOWN);
        } else {
            append(c);
        }
    }
    tree_index_ = 0;
    buffer_len_ = 0;
}

void MorseDecoder::handle_token(Token token) {
    switch (token) {
        case CW_DOT:
            if (buffer_len_ < raw_buffer_.size()) raw_buffer_[buffer_len_++] = '.';
            // Descend left. Tree bound guard (max reachable index 126).
            if (tree_index_ < 63) {
                tree_index_ = tree_index_ * 2 + 1;
            }
            break;

        case CW_DASH:
            if (buffer_len_ < raw_buffer_.size()) raw_buffer_[buffer_len_++] = '-';
            // Descend right.
            if (tree_index_ < 63) {
                tree_index_ = tree_index_ * 2 + 2;
            }
            break;

        case CW_LETTER_SPACE:
            decode_pending();
            break;

        case CW_WORD_SPACE:
            // Word pause. First flush a pending letter (if any).
            decode_pending();
            append(' '); // Print the inter-word space
            break;

        case CW_ELEMENT_SPACE:
        case CW_NONE:
        default:
            // Intra-character pauses and empty tokens are ignored by the tree
            break;
    }
}

} // namespace cw
