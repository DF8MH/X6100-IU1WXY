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

#include <ctype.h>
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

/* AA00, AA00aa, AA00aa00: else left out (the site places the radio from its IP) */
static bool grid_ok(const char *g) {
    size_t n = strlen(g);

    if (n != 4 && n != 6 && n != 8) return false;
    for (size_t i = 0; i < n; i++) {
        char c   = (char)toupper((unsigned char)g[i]);
        char top = i < 2 ? 'R' : 'X';
        if (i % 4 < 2 ? (c < 'A' || c > top) : (c < '0' || c > '9')) return false;
    }
    return true;
}

#ifdef UPDATE_CHECK_TEST
#include <assert.h>
int main(void) {
    assert(update_newer("v0.112", "v0.111"));
    assert(update_newer("v0.100", "v0.99"));
    assert(!update_newer("v0.112", "v0.112"));
    assert(!update_newer("v0.99", "v0.112"));
    assert(update_newer("v1.0", "v0.200"));
    assert(grid_ok("JN35") && grid_ok("jn35sc") && grid_ok("JN35SC12"));
    assert(!grid_ok("") && !grid_ok("JN3") && !grid_ok("ZZ35") && !grid_ok("JN35SZ") && !grid_ok("35JN"));
    puts("ok");
    return 0;
}
#else

#include "cJSON.h"
#include "main.h"
#include "msg.h"
#include "params/params.h"
#include "station.h"

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

/* Device ping for the site's map and count of radios on (privacy notice): every 5 minutes and
 * at once when the network comes up, only with a default route. An anonymous random id kept
 * in /mnt, the firmware version, the QTH grid when valid. A failure waits for the next round. */
#define DEVICE_ID "/mnt/device_id"

static bool net_up(void) {
    FILE *f = fopen("/proc/net/route", "r");
    char  line[256], ifc[32];
    bool  up = false;

    if (!f) return false;
    while (!up && fgets(line, sizeof(line), f)) {
        unsigned long dest;
        up = sscanf(line, "%31s %lx", ifc, &dest) == 2 && dest == 0 && strcmp(ifc, "lo");
    }
    fclose(f);
    return up;
}

static void device_id(char *id, size_t size) {
    FILE *f = fopen(DEVICE_ID, "r");

    id[0] = 0;
    if (f) {
        if (fgets(id, (int)size, f)) id[strcspn(id, "\r\n")] = 0;
        fclose(f);
    }
    if (strlen(id) >= 8 && strspn(id, "0123456789abcdef") == strlen(id)) return;

    unsigned char r[16] = {0};
    if ((f = fopen("/dev/urandom", "r"))) {
        if (fread(r, 1, sizeof(r), f) != sizeof(r)) r[0] ^= 1;
        fclose(f);
    }
    for (int i = 0; i < 16 && 2 * i + 2 < (int)size; i++) sprintf(id + 2 * i, "%02x", r[i]);
    if ((f = fopen(DEVICE_ID, "w"))) {
        fprintf(f, "%s\n", id);
        fclose(f);
    }
}

static void *ping_thread(void *arg) {
    char id[40], qth[16], cmd[320];
    bool was_up = false;
    int  wait = 0;

    (void)arg;
    device_id(id, sizeof(id));
    while (1) {
        bool up = net_up();

        if (up && (!was_up || wait <= 0)) {
            snprintf(qth, sizeof(qth), "%s", station_qth());
            snprintf(cmd, sizeof(cmd),
                     "python3 -c \"import urllib.request as u;u.urlopen('https://x6100-link.it/api/ping.php"
                     "?id=%s&kind=radio&ver=%s%s%s',timeout=15)\" >/dev/null 2>&1",
                     id, strrchr(VERSION, ' ') + 1, grid_ok(qth) ? "&qth=" : "", grid_ok(qth) ? qth : "");
            if (system(cmd) == -1) { /* nothing to do: the next round tries again */ }
            wait = 300;
        }
        was_up = up;
        sleep(30);
        wait -= 30;
    }
    return NULL;
}

void update_check_start(void) {
    pthread_t t;

    if (pthread_create(&t, NULL, check_thread, NULL) == 0)
        pthread_detach(t);
    if (pthread_create(&t, NULL, ping_thread, NULL) == 0)
        pthread_detach(t);
}
#endif
