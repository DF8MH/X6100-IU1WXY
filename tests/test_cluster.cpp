extern "C" {
    #include "../src/cluster/spots.h"
}

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>

TEST_CASE("Cluster DX de line", "[cluster]") {
    cluster_spot_t s;
    REQUIRE(cluster_parse_line("DX de DL1ABC:     14023.5  VP8LP        CW 599 up 2        1238Z JO62\a\a", &s));
    REQUIRE(s.freq_hz == 14023500);
    REQUIRE(std::string(s.dx) == "VP8LP");
    REQUIRE(std::string(s.spotter) == "DL1ABC");
    REQUIRE(std::string(s.comment) == "CW 599 up 2");
    REQUIRE(s.utc_min == 12 * 60 + 38);

    REQUIRE(cluster_parse_line("DX de EA3CCC-#:  14205.0  PY2XB  1229Z", &s));
    REQUIRE(std::string(s.spotter) == "EA3CCC-#");
    REQUIRE(s.comment[0] == '\0');
}

TEST_CASE("Cluster sh/dx line", "[cluster]") {
    cluster_spot_t s;
    REQUIRE(cluster_parse_line("   14196.5  ZL2ABC      29-Sep-2026 1241Z  SSB LP 57             <IU1WXY>", &s));
    REQUIRE(s.freq_hz == 14196500);
    REQUIRE(std::string(s.dx) == "ZL2ABC");
    REQUIRE(std::string(s.spotter) == "IU1WXY");
    REQUIRE(std::string(s.comment) == "SSB LP 57");
    REQUIRE(s.when == 1790685660); /* 2026-09-29 12:41Z */
    REQUIRE(cluster_parse_line("DX de DL1ABC:  14023.5  VP8LP  CW  1238Z", &s));
    REQUIRE(s.when == 0); /* no date in live spots */

    /* DXSpider pads the day; CC Cluster and AR-Cluster rows */
    REQUIRE(cluster_parse_line("  14074.0  JA1XYZ       1-Oct-2026 0012Z FT8 -12 dB                  <DL1ABC>", &s));
    REQUIRE(std::string(s.comment) == "FT8 -12 dB");
    REQUIRE(s.when == 1790813520);
    REQUIRE(cluster_parse_line(" 7074.0 RA0ABC      30-Sep-26 2359Z  FT8 <JA1YYY-@>", &s));
    REQUIRE(std::string(s.spotter) == "JA1YYY-@");
    REQUIRE(s.utc_min == 23 * 60 + 59);
}

TEST_CASE("Cluster other lines", "[cluster]") {
    cluster_spot_t s;
    REQUIRE_FALSE(cluster_parse_line("IU1WXY de VE7CC-1 29-Sep-2026 1241Z dxspider >", &s));
    REQUIRE_FALSE(cluster_parse_line("WWV de VE7CC <18>:   SFI=150, A=5, K=1", &s));
    REQUIRE_FALSE(cluster_parse_line("login: ", &s));
    REQUIRE_FALSE(cluster_parse_line("", &s));
    REQUIRE_FALSE(cluster_parse_line("30-Sep-2026 1800Z   150   5   1  No Storms", &s));
    REQUIRE_FALSE(cluster_parse_line("2026 users online", &s));
}

TEST_CASE("Cluster store keeps the newest, sorted by time", "[cluster]") {
    static cluster_store_t st;
    cluster_spot_t         s;
    cluster_store_clear(&st);
    /* sh/dx replies come newest first */
    cluster_parse_line("14030.0  K1AAA   29-Sep-2026 1240Z  CW  <W1AW>", &s);
    cluster_store_add(&st, &s);
    cluster_parse_line("14020.0  K2BBB   29-Sep-2026 1230Z  CW  <W1AW>", &s);
    cluster_store_add(&st, &s);
    REQUIRE(st.count == 2);
    REQUIRE(std::string(st.spots[0].dx) == "K2BBB"); /* oldest first */
    /* an older spot of the same DX does not replace the newer one */
    cluster_parse_line("14030.5  K1AAA   29-Sep-2026 1200Z  old  <W1AW>", &s);
    cluster_store_add(&st, &s);
    REQUIRE(st.count == 2);
    REQUIRE(std::string(st.spots[1].comment) == "CW");

    /* full: the oldest goes */
    cluster_store_clear(&st);
    for (int i = 0; i < CLUSTER_MAX_SPOTS + 1; i++) {
        s         = {};
        s.freq_hz = 1800000 + i * 2000;
        s.utc_min = (uint16_t)(600 + i / 10);
        snprintf(s.dx, sizeof(s.dx), "K%dA", i);
        cluster_store_add(&st, &s);
    }
    REQUIRE(st.count == CLUSTER_MAX_SPOTS);
    REQUIRE(std::string(st.spots[0].dx) == "K1A");
}

TEST_CASE("Cluster store", "[cluster]") {
    static cluster_store_t st;
    cluster_spot_t         s;
    cluster_store_clear(&st);
    cluster_parse_line("DX de A:  14023.5  VP8LP  CW  1238Z", &s);
    cluster_store_add(&st, &s);
    cluster_parse_line("DX de B:  14024.0  VP8LP  CW  1240Z", &s);
    cluster_store_add(&st, &s);
    REQUIRE(st.count == 1); /* same DX within 1 kHz */
    REQUIRE(st.spots[0].utc_min == 12 * 60 + 40);
    cluster_parse_line("DX de B:  7024.0  VP8LP  CW  1240Z", &s);
    cluster_store_add(&st, &s);
    REQUIRE(st.count == 2);

    cluster_store_expire(&st, 13 * 60 + 30, 60);
    REQUIRE(st.count == 2);
    cluster_store_expire(&st, 14 * 60, 60);
    REQUIRE(st.count == 0);

    REQUIRE(cluster_age_min(23 * 60 + 50, 10) == 20); /* across midnight */
}

TEST_CASE("Cluster bands and modes", "[cluster]") {
    REQUIRE(std::string(cluster_band_name(cluster_band_of(14195000))) == "20m");
    REQUIRE(cluster_band_of(9500000) == -1);

    cluster_spot_t s = {};
    s.freq_hz        = 14074000;
    REQUIRE(cluster_guess_mode(&s) == CLUSTER_MODE_DIGI);
    s.freq_hz = 7150000;
    REQUIRE(cluster_guess_mode(&s) == CLUSTER_MODE_LSB);
    s.freq_hz = 14020000;
    REQUIRE(cluster_guess_mode(&s) == CLUSTER_MODE_CW);
    s.freq_hz = 14195000;
    strcpy(s.comment, "up 5 SSB");
    REQUIRE(cluster_guess_mode(&s) == CLUSTER_MODE_USB);
    strcpy(s.comment, "TNX QSO");
    REQUIRE(cluster_guess_mode(&s) == CLUSTER_MODE_USB);
    strcpy(s.comment, "CW 599");
    REQUIRE(cluster_guess_mode(&s) == CLUSTER_MODE_CW);
}
