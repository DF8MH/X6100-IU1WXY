/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - new release alert: once after start, as soon as there is a network,
 *  x6100-link.it/api/latest.json (the release IU1WXY marked to notify, as X6100 Link reads it);
 *  a newer one than this firmware shows a message. Settings > Info > Update alert turns it off.
 *  Python fetches it: BusyBox wget has no HTTPS.
 *
 *  Check of the version order on the host:
 *    gcc -DUPDATE_CHECK_TEST -Isrc -o /tmp/uc src/update_check.c && /tmp/uc
 */

#include "update_check.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* "v0.112" > "v0.99": number by number */
bool update_newer(const char *a, const char *b) {
    a += *a == 'v';
    b += *b == 'v';
    while (*a || *b) {
        long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);

        if (x != y) return x > y;
        a += *a == '.';
        b += *b == '.';
        if ((*a && (*a < '0' || *a > '9')) || (*b && (*b < '0' || *b > '9'))) break;
    }
    return false;
}

#ifdef UPDATE_CHECK_TEST
#include <assert.h>
int main(void) {
    assert(update_newer("v0.112", "v0.111"));
    assert(update_newer("v0.100", "v0.99"));
    assert(!update_newer("v0.112", "v0.112"));
    assert(!update_newer("v0.99", "v0.112"));
    assert(update_newer("v1.0", "v0.200"));
    puts("ok");
    return 0;
}
#else

#include "cJSON.h"
#include "main.h"
#include "msg.h"
#include "params/params.h"

#define FETCH "python3 -c \"import urllib.request as u;" \
              "print(u.urlopen('https://x6100-link.it/api/latest.json',timeout=15).read().decode())\" 2>/dev/null"

static void *check_thread(void *arg) {
    (void)arg;
    /* up to ~20 minutes for the WiFi to come up */
    for (int i = 0; i < 20 && params.update_alert.x; i++) {
        char  buf[512] = "";
        FILE *p;

        sleep(i ? 60 : 30);
        if ((p = popen(FETCH, "r"))) {
            if (!fgets(buf, sizeof(buf), p)) buf[0] = 0;
            pclose(p);
        }
        if (!buf[0]) continue;      /* no network yet */

        cJSON      *o = cJSON_Parse(buf);
        const char *v = cJSON_GetStringValue(cJSON_GetObjectItem(o, "version"));

        if (v && params.update_alert.x && update_newer(v, strrchr(VERSION, ' ') + 1))
            msg_schedule_long_text_fmt("Firmware %s available", v);
        cJSON_Delete(o);
        break;
    }
    return NULL;
}

void update_check_start(void) {
    pthread_t t;

    if (pthread_create(&t, NULL, check_thread, NULL) == 0)
        pthread_detach(t);
}
#endif
