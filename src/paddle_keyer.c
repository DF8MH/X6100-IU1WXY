/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  The keyboard paddle (see paddle_keyer.h). A thread of its own reads [ and ] from a second
 *  copy of the keyboard's events, with their real press and release, and keys the radio as
 *  the CW messages do (radio_set_morse_key). Elements are timed on CLOCK_MONOTONIC deadlines.
 */

#include "paddle_keyer.h"

void iambic_paddles(iambic_t *k, bool dit, bool dah) {
    if (k->last == PADDLE_DIT && dah) {
        k->dah_mem = true;
    }
    if (k->last == PADDLE_DAH && dit) {
        k->dit_mem = true;
    }
    if (dit && dah) {
        k->squeeze = true;
    }
}

paddle_el_t iambic_next(iambic_t *k, bool dit, bool dah) {
    paddle_el_t next;

    iambic_paddles(k, dit, dah);
    /* A squeeze let go: mode A ends with the element just sent, B sends the other one too */
    if (!k->mode_b && k->squeeze && !dit && !dah) {
        k->dit_mem = k->dah_mem = false;
    }
    if (k->dit_mem || k->dah_mem) {
        next = k->dit_mem ? PADDLE_DIT : PADDLE_DAH;
    } else if (dit) {   /* a squeeze from idle starts with a dit */
        next = PADDLE_DIT;
    } else if (dah) {
        next = PADDLE_DAH;
    } else {
        next = PADDLE_NONE;
    }
    k->dit_mem = k->dah_mem = k->squeeze = false;
    k->last = next;
    iambic_paddles(k, dit, dah);   /* still squeezed as the new element starts */
    return next;
}

#ifndef PADDLE_IAMBIC_ONLY   /* tests/test_paddle_keyer.cpp: the element sequence alone */

#include "cfg/cfg_api.h"
#include "cw_encoder.h"
#include "radio.h"

#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

static atomic_int new_fd = -1;  /* a keyboard opened, for the thread to take */
static atomic_bool halt;        /* paddle_keyer_stop() */
static int        fd     = -1;
static bool       left, right;  /* [ and ] held */
static bool       keyed;
static iambic_t   k;

void paddle_keyer_stop(void) {
    atomic_store(&halt, true);
}

bool paddle_keyer_active(void) {
    int32_t m = cparam_i_get(cfg_cur_mode);

    return m == x6100_mode_cw || m == x6100_mode_cwr;
}

static bool dit(void) {
    return param_i_get(cfg_key_mode) == x6100_key_auto_right ? right : left;
}

static bool dah(void) {
    return param_i_get(cfg_key_mode) == x6100_key_auto_right ? left : right;
}

static void key(bool on) {
    if (on != keyed) {
        keyed = on;
        radio_set_morse_key(on);
    }
}

/* Reads the paddle until the deadline; with none, until something comes (a held paddle looks
 * again soon: it may be waiting for a CW message it stopped) */
static void wait_until(const struct timespec *deadline) {
    for (;;) {
        int             n = atomic_exchange(&new_fd, -1);
        struct timespec t = {0, left || right ? 20000000L : 500000000L};

        if (n >= 0) {
            if (fd >= 0) close(fd);
            fd   = n;
            left = right = false;
        }
        if (deadline) {
            struct timespec now;

            clock_gettime(CLOCK_MONOTONIC, &now);
            t.tv_sec  = deadline->tv_sec - now.tv_sec;
            t.tv_nsec = deadline->tv_nsec - now.tv_nsec;
            if (t.tv_nsec < 0) {
                t.tv_sec--;
                t.tv_nsec += 1000000000L;
            }
            if (t.tv_sec < 0) return;
        }

        struct pollfd p = {fd, POLLIN, 0};

        if (ppoll(&p, 1, &t, NULL) > 0) {
            struct input_event in;

            if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) {   /* the keyboard is gone */
                close(fd);
                fd   = -1;
                left = right = false;
            }
            while (fd >= 0 && read(fd, &in, sizeof(in)) == sizeof(in)) {
                if (in.type != EV_KEY || in.value == 2) continue;   /* not auto-repeat */
                if (in.code == KEY_LEFTBRACE) {
                    left = in.value;
                } else if (in.code == KEY_RIGHTBRACE) {
                    right = in.value;
                } else {
                    continue;
                }
                iambic_paddles(&k, dit(), dah());
            }
        }
        if (!deadline) return;
    }
}

static void wait_nsec(struct timespec *t, long nsec) {
    t->tv_nsec += nsec;
    t->tv_sec += t->tv_nsec / 1000000000L;
    t->tv_nsec %= 1000000000L;
    wait_until(t);
}

static void * keyer_thread(void *arg) {
    (void)arg;
    for (;;) {
        struct timespec t;
        paddle_el_t     el;

        wait_until(NULL);
        if (!paddle_keyer_active() || cw_encoder_state() != CW_ENCODER_IDLE) {
            key(false);   /* a message on air: the press stopped it (keyboard.c), the paddle goes after */
            continue;
        }
        if (atomic_load(&halt)) {
            key(false);
            while (left || right) {
                wait_until(NULL);
            }
            atomic_store(&halt, false);
            continue;
        }
        if (param_i_get(cfg_key_mode) == x6100_key_manual) {
            key(left);    /* a straight key */
            continue;
        }
        k = (iambic_t){.mode_b = param_i_get(cfg_iambic_mode) == x6100_iambic_b};
        clock_gettime(CLOCK_MONOTONIC, &t);
        while (!atomic_load(&halt) && (el = iambic_next(&k, dit(), dah())) != PADDLE_NONE) {
            long dit_nsec = 1200000000L / param_i_get(cfg_key_speed);

            key(true);
            wait_nsec(&t, el == PADDLE_DIT ? dit_nsec : (long)(dit_nsec * param_f_get(cfg_key_ratio)));
            key(false);
            wait_nsec(&t, dit_nsec);
        }
    }
    return NULL;
}

void paddle_keyer_open(const char *path) {
    static pthread_t thread;
    static bool      started;
    int              n = open(path, O_RDONLY | O_NONBLOCK);

    if (n >= 0) {
        int old = atomic_exchange(&new_fd, n);

        if (old >= 0) close(old);
    }
    if (!started) {
        started = pthread_create(&thread, NULL, keyer_thread, NULL) == 0;
    }
}

#endif
