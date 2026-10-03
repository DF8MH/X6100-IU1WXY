extern "C" {
    #include "../src/field/flog.h"
    #include "../src/field/rbn.h"
    #include "../src/field/refs.h"
}

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstring>
#include <string>

TEST_CASE("Field refs nearest", "[field]") {
    const char *path = "test_field_refs.tsv";
    FILE       *f    = fopen(path, "w");

    fputs("P\tIT-0001\tFar park\t46.00000\t10.00000\t\t\n"
          "S\tI/LO-001\tNear summit\t45.47000\t9.19000\t1200\t4\n"
          "P\tIT-0002\tNear park\t45.46500\t9.19000\t\t\n"
          "P\tIT-0003\tMiddle park\t45.60000\t9.19000\t\t\n", f);
    fclose(f);

    field_ref_t r[2];
    REQUIRE(field_refs_near(path, 'P', 45.4642, 9.1900, r, 2) == 2);
    REQUIRE(std::string(r[0].ref) == "IT-0002");
    REQUIRE(std::string(r[1].ref) == "IT-0003");
    REQUIRE(r[0].km < 0.2f);

    REQUIRE(field_refs_near(path, 'S', 45.4642, 9.1900, r, 2) == 1);
    REQUIRE(std::string(r[0].name) == "Near summit");
    REQUIRE(r[0].alt_m == 1200);
    REQUIRE(r[0].points == 4);

    field_ref_t one;
    REQUIRE(field_refs_find(path, "i/lo-001", &one));
    REQUIRE(one.type == 'S');
    REQUIRE_FALSE(field_refs_find(path, "IT-9999", &one));
    REQUIRE(field_refs_near("missing.tsv", 'P', 0, 0, r, 2) == -1);
    remove(path);
}

TEST_CASE("Field ADIF log", "[field]") {
    const char     *path = "test_field_log.adi";
    field_station_t st   = {"IU1WXY/P", "JN35ta", 'S', "I/LO-001"};
    field_qso_t     q    = {1790685660, "DL1ABC", 14062000, "CW", "599", "579"};
    char            rec[512];

    field_adif_record(rec, sizeof(rec), &q, &st);
    REQUIRE(std::string(rec).find("<CALL:6>DL1ABC ") != std::string::npos);
    REQUIRE(std::string(rec).find("<QSO_DATE:8>20260929 <TIME_ON:6>124100 <BAND:3>20m <FREQ:9>14.062000 <MODE:2>CW") != std::string::npos);
    REQUIRE(std::string(rec).find("<MY_SOTA_REF:8>I/LO-001") != std::string::npos);
    REQUIRE(std::string(rec).find("<eor>") != std::string::npos);

    remove(path);
    REQUIRE(field_log_add(path, &q, &st));
    st.prog = 'P';
    strcpy(st.ref, "IT-0002");
    strcpy(q.call, "F5XYZ");
    q.t += 60;
    REQUIRE(field_log_add(path, &q, &st));

    field_qso_t back[4];
    REQUIRE(field_log_read(path, back, 4) == 2);
    REQUIRE(std::string(back[0].call) == "DL1ABC");
    REQUIRE(back[1].t == 1790685720);
    REQUIRE(back[1].freq_hz == 14062000);
    REQUIRE(std::string(back[1].rst_r) == "579");

    field_adif_record(rec, sizeof(rec), &q, &st);
    REQUIRE(std::string(rec).find("<MY_SIG:4>POTA <MY_SIG_INFO:7>IT-0002 <MY_POTA_REF:7>IT-0002") != std::string::npos);
    REQUIRE(std::string(field_band(7030000)) == "40m");
    REQUIRE(std::string(field_band(15000000)).empty());
    remove(path);
}

TEST_CASE("Field RBN", "[field]") {
    REQUIRE(field_same_call("IU1WXY", "iu1wxy/p"));
    REQUIRE(field_same_call("I/IU1WXY/P", "IU1WXY"));
    REQUIRE_FALSE(field_same_call("IU1WXX", "IU1WXY"));

    field_rbn_start("");    /* sets no callsign, starts nothing */
    field_rbn_hit_t h[4];
    time_t          now = time(NULL);

    /* No callsign: activity only */
    field_rbn_feed_line("DX de EA5WU-#:     7024.0  IU1WXY       CW    12 dB  22 WPM  CQ      1234Z", now);
    REQUIRE(field_rbn_heard(600, h, 4) == 0);

    int act[FIELD_RBN_BANDS];
    field_rbn_activity(act);
    REQUIRE(act[3] == 1);   /* 40 m */

    /* The callsign stays after stop: the hits are counted from the lines */
    field_rbn_start("IU1WXY/P");
    field_rbn_stop();
    field_rbn_feed_line("DX de EA5WU-#:     7024.0  IU1WXY       CW    12 dB  22 WPM  CQ      1234Z", now);
    field_rbn_feed_line("DX de DL8ABC-2-#:  7024.1  IU1WXY       CW    -3 dB  21 WPM  CQ      1234Z", now);
    field_rbn_feed_line("DX de DL8ABC-#:   14025.0  K1ABC        CW    20 dB  25 WPM  CQ      1234Z", now);
    REQUIRE(field_rbn_heard(600, h, 4) == 2);
    REQUIRE(std::string(h[0].skimmer) == "DL8ABC");
    REQUIRE(h[0].snr == -3);
    REQUIRE(std::string(h[1].skimmer) == "EA5WU");
    REQUIRE(h[1].snr == 12);
    REQUIRE(h[1].wpm == 22);
    REQUIRE(h[1].freq_hz == 7024000);
    field_rbn_activity(act);
    REQUIRE(act[3] == 3);
    REQUIRE(act[5] == 1);   /* 20 m */
}
