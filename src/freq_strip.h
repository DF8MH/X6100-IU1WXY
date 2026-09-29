/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Frequency strip: a thin bar under the top bar, shown while an app
 *  dialog covers the main screen frequency row.
 */

#pragma once

#include "lvgl/lvgl.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *freq_strip_init(lv_obj_t *parent);
void      freq_strip_show(bool show);

/* Shows text instead of the band while the VFO stays on freq_hz. */
void      freq_strip_set_label(const char *text, int32_t freq_hz);

#ifdef __cplusplus
}
#endif
