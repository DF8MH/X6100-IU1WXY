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
}

TEST_CASE("Cluster other lines", "[cluster]") {
    cluster_spot_t s;
    REQUIRE_FALSE(cluster_parse_line("IU1WXY de VE7CC-1 29-Sep-2026 1241Z dxspider >", &s));
    REQUIRE_FALSE(cluster_parse_line("WWV de VE7CC <18>:   SFI=150, A=5, K=1", &s));
    REQUIRE_FALSE(cluster_parse_line("login: ", &s));
    REQUIRE_FALSE(cluster_parse_line("", &s));
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
