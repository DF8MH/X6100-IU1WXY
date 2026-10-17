/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - voice from the phone (voice_tx.h)
 *
 *  The same path as the voice keyer: the audio goes to the codec and the modem bit keys the base,
 *  which takes the codec as its microphone. The mode stays as it is (LSB, USB, AM, FM), with the
 *  radio's own TX filter and the power set, no digital cap and no VFO shift as tx_player.c does.
 */

#include "voice_tx.h"
#include "voice_watch.h"

#include "audio.h"
#include "cJSON.h"
#include "cfg/cfg_api.h"
#include "msg.h"
#include "radio.h"
#include "remote_server.h"

#include "lvgl/lvgl.h"

#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define RATE        8000
#define BLOCK       160         /* 20 ms */
#define HEAD        12          /* "XV", flags, 0, seq (LE16), 0 0, token (LE32) */
#define FLAG_PTT    1

static uint32_t token;          /* the session; 0: none */
static bool     keyed;          /* read by voice_tx_open on the socket thread */

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000;
}

const char *voice_tx_open(uint32_t t) {
    if (__atomic_load_n(&keyed, __ATOMIC_ACQUIRE) && __atomic_load_n(&token, __ATOMIC_ACQUIRE) != t)
        return "Another device is transmitting";
    __atomic_store_n(&token, t, __ATOMIC_RELEASE);
    return NULL;
}

/* On the radio and on the phone */
static void tell(const char *text) {
    msg_schedule_text_fmt("%s", text);
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "t", "msg");
    cJSON_AddStringToObject(o, "text", text);
    remote_event(o);
}

/* NULL when the radio may key for the phone */
static const char *refuse(void) {
    int32_t mode = cparam_i_get(cfg_cur_mode);

    if (radio_get_state() != RADIO_RX || radio_get_modem())
        return "Phone PTT: the radio is already transmitting";
    if (mode != x6100_mode_lsb && mode != x6100_mode_usb && mode != x6100_mode_am && mode != x6100_mode_nfm)
        return "Phone PTT: voice only in LSB, USB, AM and FM";
    return NULL;
}

static void unkey(bool drain) {
    if (drain)
        audio_play_wait();      /* the last word, at most the 100 ms queued */
    else
        audio_play_flush();
    radio_set_modem(false);
    audio_play_lowlat_end();
    __atomic_store_n(&keyed, false, __ATOMIC_RELEASE);
}

static void *voice_thread(void *arg) {
    int           fd = *(int *)arg;
    voice_watch_t w  = {0};
    uint8_t       pkt[HEAD + BLOCK * 2 + 1];
    int16_t       in[BLOCK], out[BLOCK * AUDIO_PLAY_RATE / RATE + 2];
    double        pos  = 0;
    int16_t       prev = 0;

    while (true) {
        struct pollfd p = {.fd = fd, .events = POLLIN};
        vw_action_t   a = VW_NONE;
        bool          have = false;

        if (poll(&p, 1, 20) > 0) {
            ssize_t n = recv(fd, pkt, sizeof(pkt), 0);
            uint32_t t = pkt[8] | pkt[9] << 8 | pkt[10] << 16 | (uint32_t)pkt[11] << 24;
            uint32_t cur = __atomic_load_n(&token, __ATOMIC_ACQUIRE);
            if (n == HEAD + BLOCK * 2 && pkt[0] == 'X' && pkt[1] == 'V' && cur && t == cur) {
                a    = voice_watch_packet(&w, pkt[2] & FLAG_PTT, now_ms());
                have = true;
            }
        }
        if (a == VW_NONE)
            a = voice_watch_tick(&w, now_ms());

        /* SWR protection (radio.c) or the voice keyer took the modem bit */
        if (a == VW_NONE && keyed && !radio_get_modem()) {
            voice_watch_stop(&w);
            unkey(false);
            tell("Phone PTT: TX stopped by the radio");
            continue;
        }
        switch (a) {
            case VW_KEY: {
                const char *err = refuse();
                if (err || !audio_play_lowlat_begin()) {
                    voice_watch_stop(&w);
                    tell(err ? err : "Phone PTT: no audio output");
                    continue;
                }
                pos = 0, prev = 0;
                __atomic_store_n(&keyed, true, __ATOMIC_RELEASE);
                radio_set_modem(true);
                break;
            }
            case VW_UNKEY_RELEASE:
                unkey(true);
                break;
            case VW_UNKEY_SILENCE:
                unkey(false);
                tell("Phone PTT: TX stopped, no audio from the phone");
                break;
            case VW_UNKEY_LIMIT:
                unkey(false);
                tell("Phone PTT: TX stopped after 3 minutes");
                break;
            default:
                break;
        }
        if (keyed && have) {
            for (int i = 0; i < BLOCK; i++)
                in[i] = (int16_t)(pkt[HEAD + i * 2] | pkt[HEAD + i * 2 + 1] << 8);
            size_t k = voice_resample(in, BLOCK, out, (double)RATE / AUDIO_PLAY_RATE, &pos, &prev);
            audio_play_if_room(out, k);     /* full: what does not fit is dropped, the delay stays short */
        }
    }
    return NULL;
}

void voice_tx_init(void) {
    static int         fd;
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(VOICE_TX_PORT), .sin_addr.s_addr = htonl(INADDR_ANY)};
    pthread_t          th;

    fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0 || bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
        LV_LOG_ERROR("voice_tx: can't listen on UDP %d", VOICE_TX_PORT);
        if (fd >= 0)
            close(fd);
        return;
    }
    pthread_create(&th, NULL, voice_thread, &fd);
    pthread_detach(th);
}
