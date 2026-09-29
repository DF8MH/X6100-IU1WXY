/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  DX cluster telnet client: a worker thread that logs in with the
 *  callsign, asks for the latest spots and keeps the spot store updated.
 *  It reconnects on its own until cluster_client_stop().
 */

#pragma once

#include "spots.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLUSTER_CONF         "/mnt/cluster.conf"
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

/* Writes CLUSTER_CONF (atomically: a temporary file, then rename). */
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

/* Copies the spots (oldest first) when they changed since *gen; returns
 * the count, or -1 when nothing changed. */
int cluster_client_snapshot(cluster_spot_t *out, int max, uint32_t *gen);

cluster_state_t cluster_client_state(char *detail, size_t size);

void cluster_client_clear(void);

/* Feeds a line as if it came from the cluster (tests, UI harness). */
void cluster_client_feed_line(const char *line);

#ifdef __cplusplus
}
#endif
