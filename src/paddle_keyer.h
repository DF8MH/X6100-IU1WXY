/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  A paddle on the keyboard's [ and ] keys (the N6ARA TinyMIDI in Keyboard Mode sends them): in
 *  CW / CW-R they key the radio through an iambic keyer that follows the KEY settings.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PADDLE_NONE = 0,
    PADDLE_DIT,
    PADDLE_DAH
} paddle_el_t;

typedef struct {
    bool        mode_b;
    bool        dit_mem, dah_mem;   /* the other paddle touched during an element */
    bool        squeeze;            /* both held during the element */
    paddle_el_t last;               /* the element on (with its space), NONE when idle */
} iambic_t;

/* The paddles as they are now, while the last element and its space are on */
void iambic_paddles(iambic_t *k, bool dit, bool dah);

/* The element after the last one and its space: NONE, the keyer stops */
paddle_el_t iambic_next(iambic_t *k, bool dit, bool dah);

/* The keyboard just opened (keyboard.c): the keyer reads its own copy of the events */
void paddle_keyer_open(const char *path);

/* SWR protection tripped: the key opens and the paddle is ignored until both are let go */
void paddle_keyer_stop(void);

/* CW / CW-R: [ and ] are the paddle, not keys */
bool paddle_keyer_active(void);

#ifdef __cplusplus
}
#endif
