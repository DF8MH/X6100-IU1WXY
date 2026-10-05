/**
 * @file drm.h
 *
 */

#ifndef DRM_H
#define DRM_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#ifndef LV_DRV_NO_CONF
#ifdef LV_CONF_INCLUDE_SIMPLE
#include "lv_drv_conf.h"
#else
#include "../../lv_drv_conf.h"
#endif
#endif

#if USE_DRM

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/* false: no usable DRM device, nothing was changed on the screen (use fbdev) */
bool drm_init(void);
/* flush_cb, partial rendering: area-sized color_p */
void drm_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_p);
/* The screen as /dev/fb0 would hold it (480x800 BGRX) into path; false if not active */
bool drm_dump(const char *path);

#endif  /*USE_DRM*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*DRM_H*/
