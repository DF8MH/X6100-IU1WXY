/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  DX cluster telnet client: a worker thread that logs in with the
 *  callsign, asks for the latest spots and keeps the spot store updated
 *  (all bands, the last hour). It runs from boot, independent of the app,
 *  and reconnects on its own, with a backoff, until cluster_client_stop().
 */

#pragma once

#include "spots.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLUSTER_CONF /* overridden by the tests and the UI harness */
#define CLUSTER_CONF         "/mnt/cluster.conf"
#endif
#define CLUSTER_DEFAULT_HOST "dxc.ve7cc.net"
#define CLUSTER_DEFAULT_PORT 23

typedef enum {
    CLUSTER_STOPPED,
    CLUSTER_CONNECTING,
    CLUSTER_CONNECTED,
    CLUSTER_FAILED, /* waits and retries */
} cluster_state_t;

/* Host and port from CLUSTER_CONF ("host=..." and "port=..." lines),
 * or the defaults. */
void cluster_config_load(char *host, size_t host_size, int *port);

/* Saved servers: "server=name|host|port" lines (the web page writes them).
 * Without any, the default list; the one in use is added when not listed. */
#define CLUSTER_MAX_SERVERS 16
typedef struct {
    char name[32];
    char host[128];
    int  port;
} cluster_server_t;

int cluster_servers_load(cluster_server_t *out, int max);

/* Sets the server in use in CLUSTER_CONF, keeping the saved servers
 * (atomically: a temporary file, then rename). */
bool cluster_config_save(const char *host, int port);

/* Modification time of CLUSTER_CONF, 0 when missing: the app reloads the
 * server when the file changes (e.g. from the web configurator). */
time_t cluster_config_mtime(void);

/* "host" or "host:port" -> host, port (23 when missing). */
bool cluster_parse_server(const char *text, char *host, size_t host_size, int *port);

/* Starts the client, or makes the running one reconnect when the server
 * or the callsign changed. */
void cluster_client_start(const char *host, int port, const char *callsign);
void cluster_client_stop(void);

/* Retries at once when waiting after a failure (e.g. the WiFi is back). */
void cluster_client_retry(void);

/* Copies the spots (oldest first) when they changed since *gen; returns
 * the count, or -1 when nothing changed. */
int cluster_client_snapshot(cluster_spot_t *out, int max, uint32_t *gen);

cluster_state_t cluster_client_state(char *detail, size_t size);


/* Feeds a line as if it came from the cluster (tests, UI harness). */
void cluster_client_feed_line(const char *line);

#ifdef __cplusplus
}
#endif
