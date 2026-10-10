/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - new release alert (update_check.c)
 */

#pragma once

#include <stdbool.h>

/* Once after start, in its own thread: a message when x6100-link.it has a newer release */
void update_check_start(void);

/* Version a after b ("v0.112" > "v0.99") */
bool update_newer(const char *a, const char *b);
