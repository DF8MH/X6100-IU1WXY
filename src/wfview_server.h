/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Icom LAN (wfview) server on UDP 50001-50003 */
void wfview_server_init();
bool wfview_server_get_enabled(void);
void wfview_server_set_enabled(bool on);
void wfview_server_set_credentials(const char *user, const char *pass);
const char *wfview_server_user(void);
const char *wfview_server_pass(void);
/* Name of the logged-in client, NULL when none */
const char *wfview_server_client(void);

#ifdef __cplusplus
}
#endif
