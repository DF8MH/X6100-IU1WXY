#pragma once

#include "dialog.h"
#include "cluster/spots.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_cluster;

/* Starts the DX cluster client in the background (after the params are
 * loaded); it runs when a callsign is set. */
void dialog_cluster_start_client(void);

/* Tunes the radio to a spot: frequency and the mode its comment / the band plan suggests */
void dialog_cluster_tune_spot(const cluster_spot_t *s);

#ifdef __cplusplus
}
#endif
