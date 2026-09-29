/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Bluetooth headphones: btaudio-ctl runner and "Mute speaker".
 */

#include "btaudio.h"

#include "cfg/settings_manager.h"

#include <string>
#include <thread>

extern "C" {
#include "scheduler.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
}

#define BTAUDIO_CTL   "/usr/sbin/btaudio-ctl"
#define BTAUDIO_CONF  "/mnt/btaudio.conf"
#define HP_STATE      "/tmp/btaudio-hp"     /* written by btaudio-ctl: headphones connected */
#define SAVED_AF_VOL  "/mnt/btaudio.afvol"  /* AF volume before muting, survives a power off */

struct result_t {
    std::string       out;
    int               status;
    btaudio_done_cb_t cb;
    void             *user;
};

static bool mute_on   = false;
static int  saved_vol = -1;

static void done(void *arg) {
    result_t *r = *(result_t **)arg;
    if (r->cb) r->cb(r->out.c_str(), r->status, r->user);
    delete r;
}

void btaudio_run(const char *args, btaudio_done_cb_t cb, void *user) {
    std::string cmd = std::string(BTAUDIO_CTL " ") + args + " 2>&1";
    std::thread([cmd, cb, user]() {
        result_t *r = new result_t{"", -1, cb, user};
        FILE *fp = popen(cmd.c_str(), "r");
        if (fp) {
            char buf[256];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) r->out.append(buf, n);
            r->status = pclose(fp);
        }
        scheduler_put(done, &r, sizeof(r));
    }).detach();
}

static void save_vol(int vol) {
    saved_vol = vol;
    if (vol < 0) {
        unlink(SAVED_AF_VOL);
        return;
    }
    FILE *fp = fopen(SAVED_AF_VOL, "w");
    if (fp) {
        fprintf(fp, "%d\n", vol);
        fclose(fp);
    }
}

static void mute_timer_cb(lv_timer_t *t) {
    bool hp = access(HP_STATE, F_OK) == 0;

    if (hp && mute_on && saved_vol < 0) {
        save_vol(cfg_sm.p_volume.get());
        cfg_sm.p_volume.set(0);
    } else if ((!hp || !mute_on) && saved_vol >= 0) {
        cfg_sm.p_volume.set(saved_vol);
        save_vol(-1);
    }
}

void btaudio_init(void) {
    FILE *fp = fopen(BTAUDIO_CONF, "r");
    if (fp) {
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "BTAUDIO_MUTE=", 13) == 0) mute_on = strstr(line, "yes") != NULL;
        }
        fclose(fp);
    }
    fp = fopen(SAVED_AF_VOL, "r");
    if (fp) {
        if (fscanf(fp, "%d", &saved_vol) != 1) saved_vol = -1;
        fclose(fp);
    }
    lv_timer_create(mute_timer_cb, 1000, NULL);
}

bool btaudio_mute_get(void) {
    return mute_on;
}

void btaudio_mute_set(bool on) {
    mute_on = on;
    btaudio_run(on ? "mute on" : "mute off", NULL, NULL);
}
