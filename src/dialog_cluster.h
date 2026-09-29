#pragma once

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_cluster;

/* Starts the DX cluster client in the background (after the params are
 * loaded); it runs when a callsign is set. */
void dialog_cluster_start_client(void);

#ifdef __cplusplus
}
#endif
