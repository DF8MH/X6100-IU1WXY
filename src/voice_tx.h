/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - voice from the phone (X6100 Link): UDP packets of 20 ms of audio go
 *  to the codec as the voice keyer's do (MSG, dialog_msg_voice.c), the radio keyed as long as
 *  the phone holds its PTT (voice_watch.h). docs/REMOTE_API.md in the app repo has the packet.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VOICE_TX_PORT 50010

void voice_tx_init(void);

/* The session of a phone: its packets carry this token. NULL, or the error for the phone */
const char *voice_tx_open(uint32_t token);

#ifdef __cplusplus
}
#endif
