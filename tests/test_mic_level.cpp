// The microphone gain from the ALC in voice (src/mic_level.c): the decisions only
#include <catch2/catch_test_macros.hpp>

#include "mic_level.h"

TEST_CASE("mic level: talking with the ALC stuck high takes the gain down, a carrier does not", "[mic]") {
    mic_level_t l;
    mic_level_start(&l, 5.0f);
    int total = 0;
    for (int i = 0; i < 40; i++) total += mic_level_reading(&l, i % 2 ? 9.0f : 6.0f, i % 2 ? 5.3f : 3.0f);
    CHECK(total == -MIC_LEVEL_DOWN_MAX); // down, and no more than the cap in one transmission

    mic_level_start(&l, 5.0f);
    total = 0;
    for (int i = 0; i < 40; i++) total += mic_level_reading(&l, 9.5f, 5.3f); // TUNE: steady
    CHECK(total == 0);
}

TEST_CASE("mic level: the ALC only on the peaks is left as it is", "[mic]") {
    mic_level_t l;
    mic_level_start(&l, 5.0f);
    int total = 0;
    for (int i = 0; i < 40; i++) total += mic_level_reading(&l, i % 5 == 0 ? 4.0f : 0.0f, i % 2 ? 5.0f : 1.5f);
    CHECK(total == 0);
    CHECK(mic_level_end(&l) == 0);
}

TEST_CASE("mic level: talking with the ALC never reaching 1 takes it up, a silent mic does not", "[mic]") {
    mic_level_t l;
    mic_level_start(&l, 5.0f);
    for (int i = 0; i < 12; i++) CHECK(mic_level_reading(&l, i % 3 ? 0.0f : 0.6f, i % 2 ? 5.0f : 0.5f) == 0);
    CHECK(mic_level_end(&l) == MIC_LEVEL_UP); // the power reaches the setting, the ALC hardly shows

    mic_level_start(&l, 5.0f);
    for (int i = 0; i < 12; i++) mic_level_reading(&l, 0.0f, 0.0f); // nothing from the mic
    CHECK(mic_level_end(&l) == 0);
}
