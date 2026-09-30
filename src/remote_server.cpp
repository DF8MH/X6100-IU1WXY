/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Web remote: control socket for the webserver's /remote page.
 *  UNIX socket, one JSON object per line. Requests:
 *    {"cmd":"get"}                 -> one state line
 *    {"cmd":"sub"}                 -> state lines (5/s) and spectrum lines (10/s) until the client closes
 *    {"cmd":"freq","val":14074000} {"cmd":"tune","val":-100} {"cmd":"mode","val":"USB"} {"cmd":"band","val":40}
 *    {"cmd":"pre|att|nb|nr","val":0|1} {"cmd":"agc"} (next) {"cmd":"rfgain","val":0..100} {"cmd":"filter","val":Hz}
 *                                  -> {"t":"ok"} or {"t":"err","err":"..."}
 *  Commands are applied on the LVGL thread through the scheduler. There is no PTT:
 *  the remote can't transmit, and nothing is changed while the radio transmits.
 */

#include "remote_server.h"

#include "cfg/db.h"
#include "cfg/settings_manager.h"
#include "util.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>

extern "C" {
    #include "cJSON.h"
    #include "main_screen.h"
    #include "meter.h"
    #include "radio.h"
    #include "scheduler.h"
    #include "waterfall.h"

    #include "lvgl/lvgl.h"
    #include <errno.h>
    #include <fcntl.h>
    #include <math.h>
    #include <poll.h>
    #include <pthread.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/socket.h>
    #include <sys/stat.h>
    #include <sys/un.h>
    #include <time.h>
    #include <unistd.h>
}

#ifndef REMOTE_SOCK
#define REMOTE_SOCK     "/tmp/x6100_remote.sock"
#endif
#define MAX_CLIENTS     6
#define MAX_SUBS        3
#define IN_SIZE         256
#define OUT_SIZE        (32 * 1024)
#define NBINS           800
#define FRAME_MS        100
#define STATE_MS        200
#define IDLE_MS         5000    /* a command client must be done within this */
#define STALL_MS        10000   /* a subscriber that takes nothing for this long is dropped */
#define MAX_PENDING     16      /* commands queued on the LVGL thread */

typedef struct {
    int      fd;
    bool     sub;
    size_t   in_len;
    char     in[IN_SIZE];
    size_t   out_len;
    char     out[OUT_SIZE];
    uint64_t last_ms;   /* connect time, then last write progress */
} client_t;

typedef enum {
    CMD_FREQ, CMD_TUNE, CMD_MODE, CMD_BAND, CMD_PRE, CMD_ATT, CMD_AGC, CMD_NB, CMD_NR, CMD_RFGAIN, CMD_FILTER
} cmd_id_t;

typedef struct {
    cmd_id_t id;
    int32_t  val;
} cmd_t;

/* x6100_mode_t and x6100_agc_t order */
static const char *mode_names[] = {"LSB", "LSB-D", "USB", "USB-D", "CW", "CW-R", "AM", "NFM"};
static const char *agc_names[]  = {"OFF", "SLOW", "FAST", "AUTO"};

static client_t          clients[MAX_CLIENTS];
static int               wake_fd[2] = {-1, -1};
static std::atomic<int>  subscribers{0};
static std::atomic<int>  pending{0};
static uint64_t          pending_ms;

/* Band id last seen for each "40m ..." name */
static std::mutex                 last_band_mutex;
static std::map<int32_t, int32_t> last_band;

/* Last spectrum line from the DSP thread */
static std::mutex        spec_mutex;
static uint8_t           spec_bins[NBINS];
static uint32_t          spec_center, spec_width;
static bool              spec_new;

static uint64_t now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000;
}

/* LVGL thread */

/* 40 -> "40m", "40m CW", "40m SSB", not "4m" */
static bool band_matches(const std::string &name, int32_t m) {
    char pre[8];
    int  l = snprintf(pre, sizeof(pre), "%im", m);
    return name.compare(0, l, pre) == 0 && (name.size() == (size_t)l || name[l] == ' ');
}

/* Back to the sub-band last seen from the web (40m CW / 40m SSB), else the active band with the
 * lowest start, the first one band up/down reaches. Its own memory is loaded. */
static void set_band(int32_t m) {
    BandInfoLoadResult cur = BandsTable::get_by_id(cfg_sm.current_band_id());

    if (cur.rc == SUCCESS && band_matches(cur.value.name, m))
        return;
    int32_t last = BAND_UNDEFINED;
    {
        std::lock_guard<std::mutex> lock(last_band_mutex);
        auto                        it = last_band.find(m);
        if (it != last_band.end())
            last = it->second;
    }
    int32_t  id    = BAND_UNDEFINED;
    uint32_t start = UINT32_MAX;
    for (const BandInfo &b : BandsTable::all_bands()) {
        if (b.type != BAND_ACTIVE || !band_matches(b.name, m) || !cfg_sm.is_valid_hw_freq(b.start_freq))
            continue;
        if (b.id == last) {
            id = b.id;
            break;
        }
        if (b.start_freq < start) {
            id    = b.id;
            start = b.start_freq;
        }
    }
    if (id != BAND_UNDEFINED)
        cfg_sm.p_band_id.set(id);
}

static void apply_cmd(void *arg) {
    cmd_t  *c = (cmd_t *)arg;
    int32_t f;

    if (pending > 0)
        pending--;
    if (radio_get_state() != RADIO_RX)
        return;
    switch (c->id) {
        case CMD_FREQ:
        case CMD_TUNE:
            if (main_screen_freq_locked())
                break;
            f = c->val;
            if (c->id == CMD_TUNE)
                f = align_int(cfg_sm.cp_fg_freq.get() + c->val, abs(c->val));
            if (cfg_sm.is_valid_hw_freq(f))
                main_screen_set_freq(f);
            break;
        case CMD_MODE:
            if (!main_screen_mode_locked())
                cfg_sm.cp_cur_mode.set(c->val);
            break;
        case CMD_BAND:
            if (!main_screen_band_locked())
                set_band(c->val);
            break;
        case CMD_PRE:
            cfg_sm.cp_cur_pre.set(c->val);
            break;
        case CMD_ATT:
            cfg_sm.cp_cur_att.set(c->val);
            break;
        case CMD_AGC:
            main_screen_agc_next();
            break;
        case CMD_NB:
            cfg_sm.p_nb.set(c->val);
            break;
        case CMD_NR:
            cfg_sm.p_nr.set(c->val);
            break;
        case CMD_RFGAIN:
            cfg_sm.p_rfgain.set(c->val);
            break;
        case CMD_FILTER:
            cfg_sm.cp_cur_filter_bw.set(c->val);
            break;
    }
}

/* Socket thread */

static void client_close(client_t *c) {
    close(c->fd);
    c->fd = -1;
    if (c->sub) {
        c->sub = false;
        subscribers--;
    }
}

static void client_flush(client_t *c) {
    if (c->fd < 0 || !c->out_len)
        return;
    ssize_t n = send(c->fd, c->out, c->out_len, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (n > 0) {
        c->out_len -= n;
        memmove(c->out, c->out + n, c->out_len);
        c->last_ms = now_ms();
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        client_close(c);
    }
}

/* Whole lines only: a slow client loses lines, never gets half of one */
static void client_send(client_t *c, const char *line, size_t len) {
    if (c->fd < 0 || c->out_len + len > OUT_SIZE)
        return;
    if (!c->out_len)
        c->last_ms = now_ms();
    memcpy(c->out + c->out_len, line, len);
    c->out_len += len;
    client_flush(c);
}

static void client_err(client_t *c, const char *err) {
    char buf[128];
    int  l = snprintf(buf, sizeof(buf), "{\"t\":\"err\",\"err\":\"%s\"}\n", err);
    client_send(c, buf, l);
}

static const char *band_name() {
    static int32_t cached_id = BAND_UNDEFINED - 1;
    static char    name[32];
    int32_t        id = cfg_sm.current_band_id();

    if (id != cached_id) {
        cached_id = id;
        name[0]   = 0;
        BandInfoLoadResult r = BandsTable::get_by_id(id);
        if (r.rc == SUCCESS) {
            /* Edited on the web: keep it plain for the JSON string */
            size_t i;
            for (i = 0; i < r.value.name.size() && i < sizeof(name) - 1; i++) {
                char ch = r.value.name[i];
                name[i] = (ch >= ' ' && ch <= '~' && ch != '"' && ch != '\\') ? ch : '?';
            }
            name[i] = 0;
            std::lock_guard<std::mutex> lock(last_band_mutex);
            int32_t                     m = atoi(name);
            if (m > 0 && band_matches(r.value.name, m))
                last_band[m] = id;
        }
    }
    return name;
}

static int state_line(char *buf, size_t size) {
    int32_t mode = cfg_sm.cp_cur_mode.get();
    int32_t agc  = cfg_sm.cp_cur_agc.get();

    return snprintf(buf, size,
                    "{\"t\":\"state\",\"vfo\":\"%s\",\"freq\":%i,\"freq_bg\":%i,\"mode\":\"%s\",\"band\":\"%s\","
                    "\"filter\":%i,\"rfgain\":%i,\"vol\":%i,\"pre\":%i,\"att\":%i,\"agc\":\"%s\",\"nb\":%i,\"nr\":%i,"
                    "\"db\":%i,\"tx\":%s}\n",
                    cfg_sm.p_band_current_vfo.get() == X6100_VFO_A ? "A" : "B",
                    cfg_sm.cp_fg_freq.get(), cfg_sm.cp_bg_freq.get(),
                    (mode >= 0 && mode <= x6100_mode_nfm) ? mode_names[mode] : "?", band_name(),
                    cfg_sm.cp_cur_filter_bw.get(), cfg_sm.p_rfgain.get(), cfg_sm.p_volume.get(),
                    cfg_sm.cp_cur_pre.get() != 0, cfg_sm.cp_cur_att.get() != 0,
                    (agc >= 0 && agc <= x6100_agc_auto) ? agc_names[agc] : "?",
                    cfg_sm.p_nb.get() != 0, cfg_sm.p_nr.get() != 0,
                    meter_get_raw_db(), radio_get_state() == RADIO_RX ? "false" : "true");
}

static void send_state(client_t *c) {
    char buf[512];
    int  l = state_line(buf, sizeof(buf));

    if (l > 0 && (size_t)l < sizeof(buf))
        client_send(c, buf, l);
}

static void send_spectrum() {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8_t           bins[NBINS];
    uint32_t          center, width;
    char              buf[64 + NBINS * 2];
    int               l;

    {
        std::lock_guard<std::mutex> lock(spec_mutex);
        if (!spec_new)
            return;
        spec_new = false;
        memcpy(bins, spec_bins, NBINS);
        center = spec_center;
        width  = spec_width;
    }
    l = snprintf(buf, sizeof(buf), "{\"t\":\"spec\",\"c\":%u,\"w\":%u,\"d\":\"", center, width);
    for (size_t i = 0; i < NBINS; i += 3) {     /* base64, the last group padded */
        uint32_t v = bins[i] << 16 | (i + 1 < NBINS ? bins[i + 1] << 8 : 0) | (i + 2 < NBINS ? bins[i + 2] : 0);
        buf[l++] = b64[v >> 18 & 63];
        buf[l++] = b64[v >> 12 & 63];
        buf[l++] = i + 1 < NBINS ? b64[v >> 6 & 63] : '=';
        buf[l++] = i + 2 < NBINS ? b64[v & 63] : '=';
    }
    l += snprintf(buf + l, sizeof(buf) - l, "\"}\n");
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].sub)
            client_send(&clients[i], buf, l);
}

static bool get_int(cJSON *req, int32_t min, int32_t max, int32_t *out) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(req, "val");

    /* NaN and inf fail the range check */
    if (!cJSON_IsNumber(v) || !(v->valuedouble >= min && v->valuedouble <= max) || v->valuedouble != floor(v->valuedouble))
        return false;
    *out = (int32_t)v->valuedouble;
    return true;
}

/* Validated command, or the error for the client */
static const char *parse_cmd(cJSON *req, const char *name, cmd_t *cmd) {
    static const struct {
        const char *name;
        cmd_id_t    id;
        int32_t     min, max;
    } ints[] = {
        {"freq", CMD_FREQ, SettingsManager::HF_MIN_FREQ, 500000000},
        {"tune", CMD_TUNE, -10000, 10000},
        {"band", CMD_BAND, 1, 999},
        {"pre", CMD_PRE, 0, 1},
        {"att", CMD_ATT, 0, 1},
        {"nb", CMD_NB, 0, 1},
        {"nr", CMD_NR, 0, 1},
        {"rfgain", CMD_RFGAIN, 0, 100},
        {"filter", CMD_FILTER, 50, 6000},
    };

    if (!strcmp(name, "agc")) {
        cmd->id = CMD_AGC;
        return NULL;
    }
    if (!strcmp(name, "mode")) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(req, "val");
        for (int32_t i = 0; i < (int32_t)(sizeof(mode_names) / sizeof(mode_names[0])); i++)
            if (cJSON_IsString(v) && !strcmp(v->valuestring, mode_names[i])) {
                cmd->id  = CMD_MODE;
                cmd->val = i;
                return main_screen_mode_locked() ? "Mode locked by an app" : NULL;
            }
        return "Invalid mode";
    }
    for (size_t i = 0; i < sizeof(ints) / sizeof(ints[0]); i++) {
        if (strcmp(name, ints[i].name))
            continue;
        if (!get_int(req, ints[i].min, ints[i].max, &cmd->val) || (ints[i].id == CMD_TUNE && !cmd->val))
            return "Invalid value";
        cmd->id = ints[i].id;
        if (cmd->id == CMD_FREQ && !cfg_sm.is_valid_hw_freq(cmd->val))
            return "Frequency out of range";
        if ((cmd->id == CMD_FREQ || cmd->id == CMD_TUNE) && main_screen_freq_locked())
            return "Frequency locked";
        if (cmd->id == CMD_BAND && main_screen_band_locked())
            return "Band locked by an app";
        return NULL;
    }
    return "Unknown command";
}

static void handle_line(client_t *c, const char *line) {
    cJSON      *req  = cJSON_Parse(line);
    cJSON      *name = cJSON_GetObjectItemCaseSensitive(req, "cmd");
    const char *err  = NULL;
    cmd_t       cmd;

    if (!cJSON_IsString(name)) {
        err = "Bad request";
    } else if (!strcmp(name->valuestring, "get")) {
        send_state(c);
    } else if (!strcmp(name->valuestring, "sub")) {
        if (!c->sub && subscribers >= MAX_SUBS) {
            err = "Too many remote pages open";
        } else if (!c->sub) {
            c->sub = true;
            subscribers++;
            send_state(c);
        }
    } else if (!(err = parse_cmd(req, name->valuestring, &cmd))) {
        uint64_t now = now_ms();
        /* A command the scheduler dropped never comes back: don't stay busy for it */
        if (pending >= MAX_PENDING && now - pending_ms > 2000)
            pending = 0;
        if (radio_get_state() != RADIO_RX) {
            err = "Radio is transmitting";
        } else if (pending >= MAX_PENDING) {
            err = "Busy, try again";
        } else {
            pending++;
            pending_ms = now;
            scheduler_put(apply_cmd, &cmd, sizeof(cmd));
            client_send(c, "{\"t\":\"ok\"}\n", 11);
        }
    }
    if (err)
        client_err(c, err);
    cJSON_Delete(req);
}

static void client_read(client_t *c) {
    ssize_t n = recv(c->fd, c->in + c->in_len, IN_SIZE - c->in_len, MSG_DONTWAIT);

    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
        client_close(c);
        return;
    }
    if (n < 0)
        return;
    c->in_len += n;
    char *line = c->in, *end = c->in + c->in_len, *nl;
    while (c->fd >= 0 && (nl = (char *)memchr(line, '\n', end - line))) {
        *nl = 0;
        handle_line(c, line);
        line = nl + 1;
    }
    c->in_len = end - line;
    memmove(c->in, line, c->in_len);
    if (c->fd >= 0 && c->in_len == IN_SIZE)
        client_close(c);    /* no line fits: not our client */
}

static void *server_thread(void *arg) {
    int           listen_fd = *(int *)arg;
    struct pollfd pfd[2 + MAX_CLIENTS];
    int           slot[2 + MAX_CLIENTS];
    uint64_t      next_state = 0;

    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;
    while (true) {
        int n   = 0;
        pfd[n++] = (struct pollfd){.fd = listen_fd, .events = POLLIN};
        pfd[n++] = (struct pollfd){.fd = wake_fd[0], .events = POLLIN};
        for (int i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].fd >= 0) {
                slot[n]  = i;
                pfd[n++] = (struct pollfd){.fd = clients[i].fd, .events = (short)(POLLIN | (clients[i].out_len ? POLLOUT : 0))};
            }
        /* Sleeps until a connection when nobody is connected */
        if (poll(pfd, n, n > 2 ? 50 : -1) < 0) {
            if (errno != EINTR)
                usleep(100000);
            continue;
        }
        if (pfd[0].revents & POLLIN) {
            int fd = accept4(listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (fd >= 0) {
                int i;
                for (i = 0; i < MAX_CLIENTS && clients[i].fd >= 0; i++) {}
                if (i < MAX_CLIENTS) {
                    clients[i].fd      = fd;
                    clients[i].sub     = false;
                    clients[i].in_len  = 0;
                    clients[i].out_len = 0;
                    clients[i].last_ms = now_ms();
                } else {
                    close(fd);
                }
            }
        }
        if (pfd[1].revents & POLLIN) {
            char buf[64];
            while (read(wake_fd[0], buf, sizeof(buf)) > 0) {}
            send_spectrum();
        }
        for (int k = 2; k < n; k++) {
            client_t *c = &clients[slot[k]];
            if (pfd[k].revents & POLLOUT)
                client_flush(c);
            if (c->fd >= 0 && (pfd[k].revents & POLLIN))
                client_read(c);
            else if (c->fd >= 0 && (pfd[k].revents & (POLLERR | POLLHUP | POLLNVAL)))
                client_close(c);
        }

        uint64_t now = now_ms();
        bool     state = subscribers > 0 && now >= next_state;
        if (state)
            next_state = now + STATE_MS;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            client_t *c = &clients[i];
            if (c->fd < 0)
                continue;
            if (c->sub ? (c->out_len && now - c->last_ms > STALL_MS) : now - c->last_ms > IDLE_MS)
                client_close(c);
            else if (c->sub && state)
                send_state(c);
        }
    }
    return NULL;
}

void remote_scope_data(const float *psd_db, size_t n, uint32_t center_hz, uint32_t width_hz, bool tx) {
    static uint64_t last_ms;

    if (!subscribers.load(std::memory_order_relaxed) || !n)
        return;
    uint64_t now = now_ms();
    if (now - last_ms < FRAME_MS)
        return;
    last_ms = now;

    /* Same center and levels as the GUI waterfall (waterfall_data) */
    int32_t lo = cfg_sm.cp_mode_lo_offset.get();
    if (center_hz == 0)
        center_hz = cfg_sm.cp_fg_freq.get() - cfg_sm.p_band_if_shift.get() + lo;
    else if (tx)
        center_hz += lo;
    float min, max;
    waterfall_get_levels(tx, &min, &max);

    /* Max-pool to NBINS, 0..255 over the waterfall levels */
    uint8_t bins[NBINS];
    for (size_t i = 0; i < NBINS; i++) {
        size_t from = i * n / NBINS, to = LV_MAX((i + 1) * n / NBINS, from + 1);
        float  v    = psd_db[from];
        for (size_t j = from + 1; j < to && j < n; j++)
            v = LV_MAX(v, psd_db[j]);
        v       = (v - min) * 255.0f / (max - min);
        bins[i] = v > 0.0f ? (v < 255.0f ? v : 255) : 0;    /* NaN -> 0 */
    }
    {
        /* Never wait for the socket thread here */
        std::unique_lock<std::mutex> lock(spec_mutex, std::try_to_lock);
        if (!lock.owns_lock())
            return;
        memcpy(spec_bins, bins, NBINS);
        spec_center = center_hz;
        spec_width  = width_hz;
        spec_new    = true;
    }
    char b = 0;
    if (write(wake_fd[1], &b, 1) < 0) {
        /* pipe full: the socket thread is already woken */
    }
}

void remote_server_init() {
    static int         listen_fd;
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    pthread_t          th;

    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", REMOTE_SOCK);
    unlink(REMOTE_SOCK);
    listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listen_fd < 0 || bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(listen_fd, 8) < 0) {
        LV_LOG_ERROR("remote: can't listen on %s", REMOTE_SOCK);
        if (listen_fd >= 0)
            close(listen_fd);
        return;
    }
    chmod(REMOTE_SOCK, 0600);   /* the webserver runs as root */
    if (pipe2(wake_fd, O_NONBLOCK | O_CLOEXEC) < 0) {
        LV_LOG_ERROR("remote: pipe failed");
        close(listen_fd);
        return;
    }
    pthread_create(&th, NULL, server_thread, &listen_fd);
    pthread_detach(th);
}
