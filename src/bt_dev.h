/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Bluetooth devices over BlueZ D-Bus (Settings -> Wireless): list, scan,
 *  pair, connect, disconnect, remove, and the pairing agent.
 *  Ported from openx6100 (bt_dev.c by SP2KMO and randal007).
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BT_DEV_MAX 12

typedef struct {
    char addr[18];
    char name[32];
    bool paired;
    bool connected;
    bool audio;     /* headphones or speaker (A2DP sink) */
    bool input;     /* keyboard or other HID device */
} bt_dev_t;

/* Worker and agent threads; the agent answers pairing only while the
 * pairing window is open (btcat-ctl discoverable on, or a pairing started
 * here). Call once at startup. */
void bt_dev_init(void);

/* Asynchronous refresh of the snapshot read by bt_dev_count/get. */
void bt_dev_refresh(void);
int  bt_dev_count(void);
bool bt_dev_get(int idx, bt_dev_t *out);

/* Connect; a device not paired yet is paired first (headphones in pairing
 * mode, keyboard after its pairing key: a code to type shows in
 * bt_dev_activity). */
void bt_dev_connect(const char *addr);
void bt_dev_disconnect(const char *addr);
/* Disconnect and remove the pairing. */
void bt_dev_forget(const char *addr);

/* 16 s inquiry, LE and classic in turns. Connect ends it early. */
void bt_dev_scan(void);
bool bt_dev_scanning(void);

/* A connect, disconnect or remove is pending or running. */
bool bt_dev_busy(void);

/* What the worker is doing or the last result, "" when idle. */
void bt_dev_activity(char *out, int size);

#ifdef __cplusplus
}
#endif
