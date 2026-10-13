// The digit the tuning step changes, under which the frequency strip draws its marker (src/freq_strip.c)
#include <catch2/catch_test_macros.hpp>

#include <cstdint>

extern "C" int freq_strip_step_char(const char *text, int32_t step, bool *big);

TEST_CASE("step marker: the digit of the step, past the dots", "[freq_strip]") {
    bool big;

    CHECK(freq_strip_step_char("7.018.200", 10, &big) == 7);   // tens of Hz
    CHECK_FALSE(big);
    CHECK(freq_strip_step_char("7.018.200", 500, &big) == 6);  // hundreds, a full mark
    CHECK(big);
    CHECK(freq_strip_step_char("7.018.200", 1, &big) == 8);
    CHECK(freq_strip_step_char("7.018.200", 1000, &big) == 4); // kHz units, before the dot
    CHECK(freq_strip_step_char("14.074.000", 5000, &big) == 5);
    CHECK(big);
    CHECK(freq_strip_step_char("7.018.200", 1000000, &big) == 0);  // MHz
    CHECK(freq_strip_step_char("7.018.200", 10000000, &big) == -1); // no such digit
}

TEST_CASE("step marker: split marks the first frequency", "[freq_strip]") {
    bool big;

    CHECK(freq_strip_step_char("7.018.200 / 7.020.000", 10, &big) == 7);
}
