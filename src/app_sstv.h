/*
 * SSTV app (app_sstv.c, app_sstv_tx.c, app_sstv_gallery.c)
 */
#pragma once

#include "dialog.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_sstv;

/* X6100 Link: a command for the app (LVGL thread), its "ax" state (any thread) */
void dialog_sstv_remote(const char *cmd, int32_t val, const char *text);
int  dialog_sstv_remote_state(char *buf, size_t size);

/* At start-up: the bundled TX pictures and texts into /mnt/sstv, once */
void sstv_install_files(void);

#ifdef __cplusplus
}
#endif
