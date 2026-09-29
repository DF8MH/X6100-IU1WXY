/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  DX cluster telnet client.
 */

#define _GNU_SOURCE /* strcasestr */

#include "client.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define CONNECT_TIMEOUT_MS 10000
#define RETRY_MIN_MS       15000
#define RETRY_MAX_MS       240000 /* backoff doubles up to this */
#define STABLE_MS          60000  /* a session this long resets the backoff */
#define IDLE_TIMEOUT_MS    600000 /* nothing received: the link may be dead */
#define SHOW_DX_COUNT      "50"
#define LOGIN_FALLBACK_MS  4000
#define SHOW_DX_DELAY_MS   2000
#define MAX_AGE_MIN        60
#define LINE_MAX_LEN       512

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static bool            thread_running; /* under lock */
static volatile bool   stop_requested;
static volatile bool   reconnect_requested; /* new server or callsign */

#define ABORTED() (stop_requested || reconnect_requested)

static cluster_store_t store;
static uint32_t        store_gen = 1;
static cluster_state_t state     = CLUSTER_STOPPED;
static char            state_detail[64];

static char host_name[128];
static int  host_port;
static char login_call[CLUSTER_CALL_LEN];

static int64_t now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static uint16_t utc_now_min(void) {
    time_t    now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    return (uint16_t)(tm.tm_hour * 60 + tm.tm_min);
}

static void set_state(cluster_state_t s, const char *detail) {
    pthread_mutex_lock(&lock);
    state = s;
    snprintf(state_detail, sizeof(state_detail), "%s", detail ? detail : "");
    pthread_mutex_unlock(&lock);
}

void cluster_config_load(char *host, size_t host_size, int *port) {
    snprintf(host, host_size, "%s", CLUSTER_DEFAULT_HOST);
    *port = CLUSTER_DEFAULT_PORT;

    FILE *f = fopen(CLUSTER_CONF, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "host=", 5) == 0 && line[5]) {
            snprintf(host, host_size, "%s", line + 5);
        } else if (strncmp(line, "port=", 5) == 0) {
            int p = atoi(line + 5);
            if (p > 0 && p < 65536) *port = p;
        }
    }
    fclose(f);
}

int cluster_servers_load(cluster_server_t *out, int max) {
    char host[128];
    int  port, n = 0;
    cluster_config_load(host, sizeof(host), &port);

    FILE *f = fopen(CLUSTER_CONF, "r");
    char  line[320];
    while (f && fgets(line, sizeof(line), f) && n < max) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "server=", 7) != 0) continue;
        char *bar1 = strchr(line + 7, '|');
        char *bar2 = bar1 ? strchr(bar1 + 1, '|') : NULL;
        if (!bar2) continue;
        *bar1 = *bar2 = '\0';
        int p         = atoi(bar2 + 1);
        if (!line[7] || !bar1[1] || p <= 0 || p > 65535) continue;
        snprintf(out[n].name, sizeof(out[n].name), "%s", line + 7);
        snprintf(out[n].host, sizeof(out[n].host), "%s", bar1 + 1);
        out[n].port = p;
        n++;
    }
    if (f) fclose(f);

    /* No saved list: well-known public clusters */
    static const cluster_server_t defaults[] = {
        {"VE7CC", "dxc.ve7cc.net", 23},
        {"W3LPL", "dxc.w3lpl.net", 7373},
        {"K1TTT", "k1ttt.net", 7373},
        {"NC7J", "dxc.nc7j.com", 7373},
        {"W1NR", "dxc.w1nr.net", 23},
        {"WA9PIE", "dxc.wa9pie.net", 8000},
        {"K4ZR", "k4zr.no-ip.org", 7300},
        {"DB0SUE", "db0sue.de", 8000},
        {"GB7DJK", "gb7djk.dxcluster.net", 7300},
        {"AE5E", "dxspots.com", 23},
    };
    if (n == 0) {
        while (n < max && n < (int)(sizeof(defaults) / sizeof(defaults[0]))) {
            out[n] = defaults[n];
            n++;
        }
    }
    bool listed = false;
    for (int i = 0; i < n; i++) listed |= strcmp(out[i].host, host) == 0 && out[i].port == port;
    if (!listed && n < max) {
        snprintf(out[n].name, sizeof(out[n].name), "%s", host);
        snprintf(out[n].host, sizeof(out[n].host), "%s", host);
        out[n].port = port;
        n++;
    }
    return n;
}

/* Sets the server in use; the saved "server=" lines are kept. */
bool cluster_config_save(const char *host, int port) {
    static char servers[CLUSTER_MAX_SERVERS * 320];
    servers[0] = '\0';
    FILE *in   = fopen(CLUSTER_CONF, "r");
    char  line[320];
    while (in && fgets(line, sizeof(line), in))
        if (strncmp(line, "server=", 7) == 0 && strlen(servers) + strlen(line) + 2 < sizeof(servers)) {
            line[strcspn(line, "\r\n")] = '\0';
            strcat(servers, line);
            strcat(servers, "\n");
        }
    if (in) fclose(in);

    FILE *f = fopen(CLUSTER_CONF ".tmp", "w");
    if (!f) return false;
    fprintf(f, "host=%s\nport=%d\n%s", host, port, servers);
    bool ok = fflush(f) == 0 && fsync(fileno(f)) == 0;
    ok      = fclose(f) == 0 && ok;
    return ok && rename(CLUSTER_CONF ".tmp", CLUSTER_CONF) == 0;
}

time_t cluster_config_mtime(void) {
    struct stat st;
    return stat(CLUSTER_CONF, &st) == 0 ? st.st_mtime : 0;
}

bool cluster_parse_server(const char *text, char *host, size_t host_size, int *port) {
    while (*text == ' ') text++;
    const char *colon = strrchr(text, ':');
    size_t      n     = colon ? (size_t)(colon - text) : strlen(text);
    while (n > 0 && text[n - 1] == ' ') n--;
    if (n == 0 || n >= host_size) return false;
    for (size_t i = 0; i < n; i++)
        if (!(isalnum((unsigned char)text[i]) || text[i] == '.' || text[i] == '-')) return false;
    int p = CLUSTER_DEFAULT_PORT;
    if (colon) {
        char *end;
        long  v = strtol(colon + 1, &end, 10);
        while (*end == ' ') end++;
        if (end == colon + 1 || *end || v <= 0 || v > 65535) return false;
        p = (int)v;
    }
    memcpy(host, text, n);
    host[n] = '\0';
    *port   = p;
    return true;
}

void cluster_client_feed_line(const char *line) {
    cluster_spot_t spot;
    if (!cluster_parse_line(line, &spot)) return;
    if (spot.when && time(NULL) - spot.when > MAX_AGE_MIN * 60) return; /* old sh/dx row */
    pthread_mutex_lock(&lock);
    cluster_store_add(&store, &spot);
    cluster_store_expire(&store, utc_now_min(), MAX_AGE_MIN);
    store_gen++;
    pthread_mutex_unlock(&lock);
}

int cluster_client_snapshot(cluster_spot_t *out, int max, uint32_t *gen) {
    static uint16_t last_expire = 0xffff;
    int             n           = -1;
    uint16_t        now         = utc_now_min();

    pthread_mutex_lock(&lock);
    if (now != last_expire) {
        int before = store.count;
        cluster_store_expire(&store, now, MAX_AGE_MIN);
        if (store.count != before) store_gen++;
        last_expire = now;
    }
    if (*gen != store_gen) {
        n = store.count < max ? store.count : max;
        memcpy(out, store.spots + (store.count - n), (size_t)n * sizeof(*out));
        *gen = store_gen;
    }
    pthread_mutex_unlock(&lock);
    return n;
}

cluster_state_t cluster_client_state(char *detail, size_t size) {
    pthread_mutex_lock(&lock);
    cluster_state_t s = state;
    if (detail) snprintf(detail, size, "%s", state_detail);
    pthread_mutex_unlock(&lock);
    return s;
}

/* Sleeps in small steps so that stop requests are served quickly. */
static void wait_ms(int ms) {
    for (int t = 0; t < ms && !ABORTED(); t += 500) usleep(500000);
}

static int open_socket(const char *host, int port_num) {
    struct addrinfo hints = {0}, *res = NULL;
    char            port[8];
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port, sizeof(port), "%d", port_num);

    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0) {
        set_state(CLUSTER_FAILED, "no DNS (WiFi?)");
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *ai = res; ai && fd < 0 && !ABORTED(); ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        if (errno == EINPROGRESS) {
            struct pollfd pfd = {fd, POLLOUT, 0};
            int           so  = 1;
            socklen_t     len = sizeof(so);
            for (int t = 0; t < CONNECT_TIMEOUT_MS && !ABORTED(); t += 250) {
                if (poll(&pfd, 1, 250) != 0) break;
            }
            if ((pfd.revents & POLLOUT) && getsockopt(fd, SOL_SOCKET, SO_ERROR, &so, &len) == 0 && so == 0) break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0 && !ABORTED()) set_state(CLUSTER_FAILED, "no answer");
    return fd;
}

static void send_text(int fd, const char *text) {
    size_t n = strlen(text);
    while (n > 0) {
        ssize_t w = send(fd, text, n, MSG_NOSIGNAL);
        if (w > 0) {
            text += w;
            n -= (size_t)w;
            continue;
        }
        if (w < 0 && (errno == EAGAIN || errno == EINTR)) {
            struct pollfd pfd = {fd, POLLOUT, 0};
            if (poll(&pfd, 1, 1000) > 0) continue;
        }
        return;
    }
}

/* Telnet commands (IAC ...) are dropped, never answered: clusters work
 * without option negotiation. */
static size_t strip_telnet(unsigned char *buf, size_t n) {
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == 0xFF && i + 1 < n) {
            unsigned char cmd = buf[i + 1];
            if (cmd == 0xFF) {
                buf[o++] = 0xFF;
                i++;
            } else if (cmd >= 0xFB && cmd <= 0xFE) {
                i += 2;
            } else {
                i++;
            }
            continue;
        }
        buf[o++] = buf[i];
    }
    return o;
}

static bool looks_like_login_prompt(const char *text) {
    return strcasestr(text, "login") || strcasestr(text, "call") || strcasestr(text, "enter your");
}

/* Returns true when the session lasted long enough to reset the backoff. */
static bool session(int fd, const char *call) {
    char          line[LINE_MAX_LEN];
    size_t        len        = 0;
    bool          logged_in  = false;
    int64_t       started    = now_ms();
    int64_t       login_at   = 0;
    bool          asked_spots = false;
    bool          spider      = false; /* DXSpider wants "sh/dx 50", the others "sh/dx/50" */
    int64_t       last_rx     = started;
    unsigned char buf[1024];

    set_state(CLUSTER_CONNECTED, "");

    while (!ABORTED()) {
        struct pollfd pfd = {fd, POLLIN, 0};
        int           r   = poll(&pfd, 1, 1000);
        int64_t       now = now_ms();

        if (!logged_in && now - started > LOGIN_FALLBACK_MS) {
            send_text(fd, call);
            send_text(fd, "\r\n");
            logged_in = true;
            login_at  = now;
        }
        if (logged_in && !asked_spots && now - login_at > SHOW_DX_DELAY_MS) {
            /* The latest spots at once, not only the new ones from now on. */
            send_text(fd, spider ? "sh/dx " SHOW_DX_COUNT "\r\n" : "sh/dx/" SHOW_DX_COUNT "\r\n");
            asked_spots = true;
        }
        if (now - last_rx > IDLE_TIMEOUT_MS) break;
        if (r < 0 && errno != EINTR) break;
        if (r <= 0) continue;

        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n == 0) break;
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            break;
        }
        last_rx = now;
        n       = (ssize_t)strip_telnet(buf, (size_t)n);

        for (ssize_t i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\n' || c == '\r') {
                if (len > 0) {
                    line[len] = '\0';
                    if (strcasestr(line, "DXSpider")) spider = true;
                    cluster_client_feed_line(line);
                    len = 0;
                }
                continue;
            }
            if (c == '\a') continue;
            if (len < sizeof(line) - 1) line[len++] = c;
        }

        /* The login prompt has no newline after it. */
        if (!logged_in && len > 0) {
            line[len] = '\0';
            if (looks_like_login_prompt(line)) {
                send_text(fd, call);
                send_text(fd, "\r\n");
                logged_in = true;
                login_at  = now_ms();
                len       = 0;
            }
        }
    }
    close(fd);
    if (!ABORTED()) set_state(CLUSTER_FAILED, "connection closed");
    return now_ms() - started > STABLE_MS;
}

static void *thread_cb(void *arg) {
    (void)arg;
    int delay = RETRY_MIN_MS;
    for (;;) {
        while (!stop_requested) {
            char host[sizeof(host_name)], call[sizeof(login_call)];
            int  port;
            pthread_mutex_lock(&lock);
            reconnect_requested = false; /* under the lock that start() sets it with */
            memcpy(host, host_name, sizeof(host));
            memcpy(call, login_call, sizeof(call));
            port = host_port;
            pthread_mutex_unlock(&lock);

            set_state(CLUSTER_CONNECTING, "");
            int fd = open_socket(host, port);
            if (fd >= 0 && session(fd, call)) delay = RETRY_MIN_MS;
            /* No network: retry calmly, with a longer wait each time. */
            if (!reconnect_requested) {
                wait_ms(delay);
                delay = delay * 2 < RETRY_MAX_MS ? delay * 2 : RETRY_MAX_MS;
            }
            if (reconnect_requested) delay = RETRY_MIN_MS;
        }
        /* Restarted while stopping: carry on instead of exiting. */
        pthread_mutex_lock(&lock);
        if (stop_requested) {
            thread_running = false;
            state          = CLUSTER_STOPPED;
            state_detail[0] = '\0';
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        pthread_mutex_unlock(&lock);
    }
}

/* The thread is detached and never joined: a DNS lookup or a connect can
 * block for seconds, and closing the app must not wait for it. */
void cluster_client_start(const char *host, int port, const char *callsign) {
    pthread_mutex_lock(&lock);
    if (strcmp(host_name, host) != 0 || strcmp(login_call, callsign) != 0 || host_port != port)
        reconnect_requested = true; /* the running session picks up the change */
    snprintf(host_name, sizeof(host_name), "%s", host);
    snprintf(login_call, sizeof(login_call), "%s", callsign);
    host_port      = port;
    stop_requested = false;
    if (!thread_running) {
        pthread_t      t;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&t, &attr, thread_cb, NULL) == 0) {
            thread_running = true;
        } else {
            state = CLUSTER_FAILED;
            snprintf(state_detail, sizeof(state_detail), "no thread");
        }
        pthread_attr_destroy(&attr);
    }
    pthread_mutex_unlock(&lock);
}

void cluster_client_stop(void) {
    stop_requested = true;
}

void cluster_client_retry(void) {
    pthread_mutex_lock(&lock);
    if (thread_running && state == CLUSTER_FAILED) reconnect_requested = true;
    pthread_mutex_unlock(&lock);
}
