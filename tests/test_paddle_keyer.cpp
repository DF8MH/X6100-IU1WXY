// The iambic keyer's element sequence (src/paddle_keyer.c), without the timing
#include <catch2/catch_test_macros.hpp>

#include "paddle_keyer.h"

TEST_CASE("paddle: a held paddle repeats, a squeeze alternates", "[paddle]") {
    iambic_t k = {};

    CHECK(iambic_next(&k, true, false) == PADDLE_DIT);
    CHECK(iambic_next(&k, true, false) == PADDLE_DIT);
    CHECK(iambic_next(&k, true, true) == PADDLE_DAH);
    CHECK(iambic_next(&k, true, true) == PADDLE_DIT);
    CHECK(iambic_next(&k, true, true) == PADDLE_DAH);
    CHECK(iambic_next(&k, false, true) == PADDLE_DIT);   // dit held during the dah: remembered
    CHECK(iambic_next(&k, false, true) == PADDLE_DAH);
    CHECK(iambic_next(&k, false, true) == PADDLE_DAH);
    CHECK(iambic_next(&k, false, false) == PADDLE_NONE);

    CHECK(iambic_next(&k, true, true) == PADDLE_DIT);   // from idle: dit first
}

TEST_CASE("paddle: a squeeze let go during a dah, mode A and B", "[paddle]") {
    for (bool b : {false, true}) {
        iambic_t k = {};

        k.mode_b = b;
        CHECK(iambic_next(&k, true, true) == PADDLE_DIT);
        CHECK(iambic_next(&k, true, true) == PADDLE_DAH);
        iambic_paddles(&k, false, false);                 // both let go during the dah
        CHECK(iambic_next(&k, false, false) == (b ? PADDLE_DIT : PADDLE_NONE));
        CHECK(iambic_next(&k, false, false) == PADDLE_NONE);
    }
}

TEST_CASE("paddle: dit memory, a tap during a dah", "[paddle]") {
    iambic_t k = {};

    CHECK(iambic_next(&k, false, true) == PADDLE_DAH);
    iambic_paddles(&k, false, false);                     // dah let go
    iambic_paddles(&k, true, false);                      // dit tapped
    iambic_paddles(&k, false, false);
    CHECK(iambic_next(&k, false, false) == PADDLE_DIT);   // also in mode A: no squeeze
    CHECK(iambic_next(&k, false, false) == PADDLE_NONE);
}
