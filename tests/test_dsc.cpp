// DSC receiver (src/navtex/dsc.c) on the NavTex demodulator: HF/MF calls made here (ITU-R M.493:
// dot pattern, phasing, DX/RX time diversity, EOS, ECC), 100 Bd FSK 915/1085 Hz at 11025 Hz with noise
#include <catch2/catch_test_macros.hpp>

#include "navtex/decoder.h"
#include "navtex/dsc.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

static const int    RATE = 11025;
static const double CENTER = 1000.0, SHIFT = 85.0;

/* gfft.h (fldigi) keeps 16 bytes of each FFT: not ours */
extern "C" const char *__lsan_default_suppressions() { return "leak:g_fft"; }

static std::vector<dsc_call_t> calls;

static void on_call(const dsc_call_t *c, void *) { calls.push_back(*c); }
static void on_bit(int mark, void *rx) { dsc_rx_bit((dsc_rx_t *)rx, !mark); }

static double gauss() {
    double u = (rand() + 1.0) / (RAND_MAX + 2.0), v = (rand() + 1.0) / (RAND_MAX + 2.0);
    return sqrt(-2 * log(u)) * cos(2 * M_PI * v);
}

/* 10 bits: 7 information bits LSB first (Y = 1), then the count of B (0) elements MSB first */
static void put_symbol(std::vector<int> &bits, int s) {
    int zeros = 0;
    for (int b = 0; b < 7; b++) {
        bits.push_back((s >> b) & 1);
        zeros += !((s >> b) & 1);
    }
    for (int b = 2; b >= 0; b--) bits.push_back((zeros >> b) & 1);
}

/* fmt fmt message EOS ECC EOS EOS, with phasing and the RX copies; dx_bad: characters whose DX
 * copy is sent damaged; ecc_xor spoils the ECC */
static std::vector<int> call_bits(int fmt, const std::vector<int> &msg, int eos, std::vector<int> dx_bad = {},
                                  int ecc_xor = 0) {
    std::vector<int> ch = {fmt, fmt};
    ch.insert(ch.end(), msg.begin(), msg.end());
    int ecc = fmt ^ eos;
    for (int s : msg) ecc ^= s;
    int e = (int)ch.size();
    ch.push_back(eos);
    ch.push_back(ecc ^ ecc_xor);
    ch.push_back(eos);
    ch.push_back(eos);

    std::vector<int> bits;
    for (int i = 0; i < 200; i++) bits.push_back(i % 2);   /* dot pattern */
    for (int p = 0; p <= 19 + 2 * e; p++) {
        int s;
        if (p % 2 == 0)
            s = p < 12 ? 125 : ch[(p - 12) / 2];
        else
            s = p <= 15 ? 111 - p / 2 : ch[(p - 17) / 2];
        size_t at = bits.size();
        put_symbol(bits, s);
        if (p % 2 == 0 && p >= 12)
            for (int j : dx_bad)
                if (j == (p - 12) / 2) bits[at + 1] ^= 1;   /* one bit wrong: the check bits catch it */
    }
    return bits;
}

/* Continuous phase FSK, Y (1) on the lower tone, noise for snr_db in 300 Hz; decoded as the app does */
static void run(const std::vector<std::vector<int>> &seqs, double snr_db, double noise_s = 0) {
    std::vector<float> audio(RATE, 0.0f);
    double             phase = 0, t = 0, spb = RATE / 100.0;
    for (const auto &bits : seqs) {
        for (int b : bits)
            for (t += spb; t >= 1; t -= 1) {
                audio.push_back((float)(0.5 * sin(phase)));
                phase += 2 * M_PI * (b ? CENTER - SHIFT : CENTER + SHIFT) / RATE;
            }
        audio.insert(audio.end(), RATE / 2, 0.0f);
    }
    audio.insert(audio.end(), (size_t)(RATE * (1 + noise_s)), 0.0f);
    double nz = sqrt(0.125 / pow(10, snr_db / 10) * (RATE / 2.0) / 300.0);
    for (float &v : audio) v += (float)(nz * gauss());

    static dsc_rx_t   rx;
    calls.clear();
    dsc_rx_init(&rx, on_call, nullptr);
    navtex_decoder_t *d = navtex_decoder_create(0);
    navtex_decoder_set_bit_callback(d, on_bit, &rx);
    for (size_t i = 0; i < audio.size(); i += 512)
        navtex_decoder_process(d, &audio[i], (unsigned)std::min<size_t>(512, audio.size() - i));
    navtex_decoder_destroy(d);
}

/* 247123456 distress, fire, 43°12'N 010°05'E at 11:58, J3E */
static const std::vector<int> DISTRESS = {24, 71, 23, 45, 60, 100, 4, 31, 20, 10, 5, 11, 58, 109};
/* 002470001 all ships safety, J3E on 8291 kHz */
static const std::vector<int> SAFETY = {108, 0, 24, 70, 0, 10, 109, 126, 8, 29, 10, 126, 126, 126};
/* 002470001 acknowledges 247123456's distress */
static const std::vector<int> ACK = {112, 0, 24, 70, 0, 10, 110, 24, 71, 23, 45, 60,
                                     100, 4, 31, 20, 10, 5, 11, 58, 109};

static void check_distress(const dsc_call_t &c) {
    CHECK(c.fmt == 112);
    CHECK(std::string(c.from) == "247123456");
    CHECK(std::string(dsc_cat_name(&c)) == "distress");
    CHECK(std::string(dsc_nature_name(c.nature)) == "FIRE / EXPLOSION");
    CHECK(std::string(c.pos) == "43\xc2\xb0" "12'N 010\xc2\xb0" "05'E");
    CHECK(std::string(c.utc) == "11:58");
    CHECK(c.subseq == 109);
    CHECK(c.eos == 127);
}

TEST_CASE("DSC distress, all ships safety and distress acknowledgement decode") {
    run({call_bits(112, DISTRESS, 127), call_bits(116, SAFETY, 127), call_bits(116, ACK, 127)}, 10);
    REQUIRE(calls.size() == 3);

    check_distress(calls[0]);
    CHECK(calls[0].ecc_ok);

    const dsc_call_t &s = calls[1];
    CHECK(s.fmt == 116);
    CHECK(s.cat == 108);
    CHECK(std::string(dsc_cat_name(&s)) == "safety");
    CHECK(std::string(s.from) == "002470001");
    CHECK(std::string(s.to).empty());
    CHECK(std::string(dsc_tc_name(s.tc1)) == "J3E tel.");
    CHECK(std::string(s.freq) == "8291 kHz");
    CHECK(s.ecc_ok);

    const dsc_call_t &a = calls[2];
    CHECK(std::string(dsc_cat_name(&a)) == "ack");
    CHECK(std::string(a.dmmsi) == "247123456");
    CHECK(std::string(a.pos) == "43\xc2\xb0" "12'N 010\xc2\xb0" "05'E");
    CHECK(a.ecc_ok);

    const char *prefix = nullptr;
    CHECK(std::string(dsc_country("247123456", &prefix)) == "Italy");
    CHECK(std::string(prefix) == "I");
    CHECK(std::string(dsc_country("002241078", nullptr)) == "Spain");   /* coast station */
}

TEST_CASE("DSC: damaged DX copies come from the RX ones, a bad ECC is flagged") {
    run({call_bits(112, DISTRESS, 127, {2, 3, 7, 9, 13})}, 15);
    REQUIRE(calls.size() == 1);
    check_distress(calls[0]);
    CHECK(calls[0].ecc_ok);

    run({call_bits(112, DISTRESS, 127, {}, 0x11)}, 15);
    REQUIRE(calls.size() == 1);
    check_distress(calls[0]);
    CHECK_FALSE(calls[0].ecc_ok);
}

TEST_CASE("DSC: noise alone gives nothing") {
    run({}, 0, 30);
    CHECK(calls.empty());
}
