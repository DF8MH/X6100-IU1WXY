/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <gps.h>          /* gpsd client library */
#include <stdbool.h>      /* for the C files that include only this */

typedef enum {
    GPS_STATUS_WAITING=0,
    GPS_STATUS_WORKING,
    GPS_STATUS_RESTARTING,
    GPS_STATUS_EXITED,
} gps_status_t;

void gps_init();

gps_status_t gps_status();

/* gpsd's last 2D or 3D position, any thread; age in seconds. False before the first fix */
bool gps_last_fix(double *lat, double *lon, int *age);

/* The phone's position (companion app, remote "location"), any thread; and the latest one */
void gps_phone_set(double lat, double lon);
bool gps_phone_fix(double *lat, double *lon, int *age_s);
/* The phone went (no X6100 Link connected): its position no longer counts */
void gps_phone_clear(void);
