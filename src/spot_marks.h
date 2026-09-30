/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  DX cluster spots on the main screen: callsign tags in an extra strip
 *  under the frequency labels (SPOT_MARKS_H, 2 staggered rows), and a
 *  short marker line at the spot frequency (bottom of the spectrum, or
 *  top of the waterfall when the spectrum is off). Coloured by age.
 */

#pragma once

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Height of the tag strip the frequency row grows by */
#define SPOT_MARKS_H 44

/* Marks on parent (800 px wide); call after the waterfall is created so
 * they are on top. Lines: line_h pixels at line_y; tags: from tag_y. */
void spot_marks_init(lv_obj_t *parent, lv_coord_t line_y, lv_coord_t line_h, lv_coord_t tag_y);
void spot_marks_layout(lv_coord_t line_y, lv_coord_t line_h, lv_coord_t tag_y);

/* Span shown: center - half_width .. center + half_width */
void spot_marks_update(int32_t center, int32_t half_width);

#ifdef __cplusplus
}
#endif
