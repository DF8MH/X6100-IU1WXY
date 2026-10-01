extern "C" {
    #include "../src/wspr/wspr.h"
    #include "../src/wspr/wsprnet.h"
}

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

/* RX audio as dsp.cpp hands it to the apps: 11025 Hz float */
#define AUDIO_RATE 11025

/* One cycle of audio: the signal starts 1 s after the even minute */
static std::vector<float> make_cycle(const char *call, const char *grid, int dbm, float audio_hz, float snr_db) {
    uint8_t  sym[WSPR_SYMBOLS];
    uint32_t n;

    REQUIRE(wspr_encode(call, grid, dbm, sym));
    int16_t *tx = wspr_synth(sym, audio_hz, AUDIO_RATE, &n);
    REQUIRE(tx);

    std::vector<float>               audio(AUDIO_RATE * 116);
    std::mt19937                     rng(1);
    /* WSPR SNR is in 2500 Hz: noise power over the 5512 Hz band of the audio */
    float                            sig_rms = 0.8f / std::sqrt(2.0f);
    float                            noise = sig_rms / std::pow(10.0f, snr_db / 20.0f) * std::sqrt(AUDIO_RATE / 2 / 2500.0f);
    std::normal_distribution<float>  gauss(0.0f, noise);

    for (size_t k = 0; k < audio.size(); k++) {
        float s = 0.0f;
        if (k >= AUDIO_RATE && k - AUDIO_RATE < n) {
            s = tx[k - AUDIO_RATE] / 32767.0f;
        }
        audio[k] = 0.1f * (s + gauss(rng));
    }
    free(tx);
    return audio;
}

static int decode(const std::vector<float> &audio, wspr_spot_t *spots, int max) {
    wspr_bb_t          *bb = wspr_bb_create(AUDIO_RATE);
    std::vector<float>  i(WSPR_SAMPLES), q(WSPR_SAMPLES);

    for (size_t k = 0; k < audio.size(); k += 441) {
        wspr_bb_feed(bb, &audio[k], std::min<size_t>(441, audio.size() - k));
    }
    unsigned n = wspr_bb_take(bb, i.data(), q.data());
    wspr_bb_destroy(bb);
    REQUIRE(n > 115 * WSPR_RATE - 10);
    return wspr_decode_iq(i.data(), q.data(), n, 14095600, spots, max);
}

TEST_CASE("WSPR round trip: encode, audio, baseband, decode", "[wspr]") {
    wspr_spot_t spots[10];
    int         n = decode(make_cycle("IU1WXY", "JN35", 37, 1530.0f, -20.0f), spots, 10);

    REQUIRE(n == 1);
    REQUIRE(std::string(spots[0].call) == "IU1WXY");
    REQUIRE(std::string(spots[0].loc) == "JN35");
    REQUIRE(spots[0].dbm == 37);
    REQUIRE(std::fabs(spots[0].freq_hz - (14095600 + 1530)) < 2.0);
    REQUIRE(std::abs(spots[0].snr + 20) <= 3);
}

TEST_CASE("WSPR weak signal near the limit still decodes", "[wspr]") {
    wspr_spot_t spots[10];
    int         n = decode(make_cycle("K1ABC", "FN42", 30, 1450.0f, -27.0f), spots, 10);

    REQUIRE(n == 1);
    REQUIRE(std::string(spots[0].call) == "K1ABC");
}

TEST_CASE("WSPR encoder refuses what a type 1 message can't carry", "[wspr]") {
    uint8_t sym[WSPR_SYMBOLS];

    REQUIRE_FALSE(wspr_encode("PJ4/K1ABC", "FK52", 37, sym));
    REQUIRE_FALSE(wspr_encode("K1A", "FN42", 37, sym));
    REQUIRE_FALSE(wspr_encode("IU1WXY", "JN3", 37, sym));
    REQUIRE_FALSE(wspr_encode("IU1WXY", "ZZ99", 37, sym));
    REQUIRE(wspr_encode("iu1wxy", "jn35ab", 37, sym));
}

TEST_CASE("WSPR power levels", "[wspr]") {
    REQUIRE(wspr_dbm(5.0f) == 37);
    REQUIRE(wspr_dbm(1.0f) == 30);
    REQUIRE(wspr_dbm(0.2f) == 23);
    REQUIRE(wspr_dbm(0.0f) == 0);
}

TEST_CASE("WSPRnet request path", "[wspr]") {
    wspr_spot_t s = {-21, 0.3f, -1, 14097012.0, "K1ABC", "FN42", 37};
    char        path[512];

    wsprnet_path(path, sizeof(path), "IU1WXY/P", "JN35ta", 14095600, 1790856120, &s);   /* 2026-10-01 12:02 UTC */
    REQUIRE(std::string(path) ==
            "/post?function=wspr&rcall=IU1WXY%2FP&rgrid=JN35ta&rqrg=14.095600&date=261001&time=1202"
            "&sig=-21&dt=0.3&drift=-1&tqrg=14.097012&tcall=K1ABC&tgrid=FN42&dbm=37&version=X6100-IU1WXY&mode=2");
}
