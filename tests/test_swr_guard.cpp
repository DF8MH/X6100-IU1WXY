// SWR protection (src/swr_guard.c): over the limit for SWR_GUARD_MS stops the TX, spikes don't
#include <catch2/catch_test_macros.hpp>
#include "swr_guard.h"

TEST_CASE("SWR guard trips only on a lasting high SWR") {
    swr_guard_t g = {};

    // A key-up spike, then a good reading: nothing
    REQUIRE_FALSE(swr_guard_check(&g, 0.5f, 4.0f, 2.5f, 0));
    REQUIRE_FALSE(swr_guard_check(&g, 0.5f, 1.1f, 2.5f, 100));
    REQUIRE_FALSE(swr_guard_check(&g, 0.5f, 4.0f, 2.5f, 350));
    // No power (CW space) keeps the time going
    REQUIRE_FALSE(swr_guard_check(&g, 0.0f, 1.0f, 2.5f, 500));
    REQUIRE_FALSE(swr_guard_check(&g, 0.5f, 4.0f, 2.5f, 350 + SWR_GUARD_MS - 1));
    REQUIRE(swr_guard_check(&g, 0.5f, 3.4f, 2.5f, 350 + SWR_GUARD_MS));
    // Tripped: starts again
    REQUIRE_FALSE(swr_guard_check(&g, 0.5f, 3.4f, 2.5f, 700));
    // A transmission later (a gap): its first high reading starts again too
    REQUIRE_FALSE(swr_guard_check(&g, 0.5f, 3.4f, 2.5f, 700 + SWR_GUARD_GAP_MS + 1));

    // At the limit is fine, and off never trips
    swr_guard_t h = {};
    REQUIRE_FALSE(swr_guard_check(&h, 0.5f, 2.5f, 2.5f, 0));
    REQUIRE_FALSE(swr_guard_check(&h, 0.5f, 2.5f, 2.5f, 400));
    REQUIRE_FALSE(swr_guard_check(&h, 0.5f, 5.0f, 1.0f, 500));
    REQUIRE_FALSE(swr_guard_check(&h, 0.5f, 5.0f, 1.0f, 900));
}
