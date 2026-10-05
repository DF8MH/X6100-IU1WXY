/**
 * @file drm.c
 *
 * Single plane DRM/KMS output: LVGL renders landscape 800x480 areas, they are
 * rotated 90 ccw into a cached portrait shadow, and on the last area of a
 * frame the dirty rectangles are copied to the back dumb buffer, which is
 * shown with a page flip at vblank (no tearing).
 *
 * The back buffer misses the previous frame too (it went to the other
 * buffer), so the rectangles of the previous frame are copied again: only
 * dirty areas are moved, never the whole screen.
 */

/*********************
 *      INCLUDES
 *********************/
#include "drm.h"
#if USE_DRM

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#include <xf86drm.h>
#include <xf86drmMode.h>

/*********************
 *      DEFINES
 *********************/
#define LAND_W      800             /* LVGL, landscape */
#define LAND_H      480
#define PANEL_W     LAND_H          /* panel, portrait */
#define PANEL_H     LAND_W
#define MAX_RECTS   64              /* per frame, more: whole screen */
#define FLIP_TIMEOUT_MS 100

#define err(msg, ...) fprintf(stderr, "drm: " msg "\n", ##__VA_ARGS__)

/**********************
 *      TYPEDEFS
 **********************/
struct drm_buffer {
    uint32_t    handle;
    uint32_t    pitch;
    uint32_t    size;
    uint32_t    fb_id;
    uint8_t     *map;
};

typedef struct {
    lv_area_t   rects[MAX_RECTS];   /* portrait coordinates */
    uint32_t    cnt;
    bool        full;
} rects_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static int                  fd = -1;
static uint32_t             conn_id, crtc_id;
static drmModeModeInfo      mode;
static struct drm_buffer    bufs[2];
static int                  back;           /* index of the buffer not on screen */
static bool                 crtc_set;
static volatile bool        flip_pending;
static uint32_t             *shadow;        /* portrait copy of the screen, cached RAM */
static rects_t              frame[2];       /* this frame, previous frame */
static int                  cur;

/**********************
 *   STATIC FUNCTIONS
 **********************/

static bool find_output(void) {
    drmModeRes *res = drmModeGetResources(fd);

    if (!res) {
        return false;
    }

    bool ok = false;

    for (int i = 0; i < res->count_connectors && !ok; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);

        if (!conn) {
            continue;
        }
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            int m = 0;

            for (int j = 0; j < conn->count_modes; j++) {
                if (conn->modes[j].type & DRM_MODE_TYPE_PREFERRED) {
                    m = j;
                    break;
                }
            }
            mode = conn->modes[m];

            /* CRTC: the one already driving the connector, else the first possible one */
            crtc_id = 0;
            for (int j = 0; j < conn->count_encoders && !crtc_id; j++) {
                drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[j]);

                if (!enc) {
                    continue;
                }
                if (enc->encoder_id == conn->encoder_id && enc->crtc_id) {
                    crtc_id = enc->crtc_id;
                }
                for (int k = 0; k < res->count_crtcs && !crtc_id; k++) {
                    if (enc->possible_crtcs & (1 << k)) {
                        crtc_id = res->crtcs[k];
                    }
                }
                drmModeFreeEncoder(enc);
            }
            conn_id = conn->connector_id;
            ok = crtc_id != 0;
        }
        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
    return ok;
}

static bool alloc_buf(struct drm_buffer *buf) {
    struct drm_mode_create_dumb creq = { .width = PANEL_W, .height = PANEL_H, .bpp = 32 };
    struct drm_mode_map_dumb    mreq = { 0 };

    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
        err("create dumb: %s", strerror(errno));
        return false;
    }
    buf->handle = creq.handle;
    buf->pitch = creq.pitch;
    buf->size = creq.size;

    mreq.handle = creq.handle;
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
        err("map dumb: %s", strerror(errno));
        return false;
    }
    buf->map = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
    if (buf->map == MAP_FAILED) {
        err("mmap: %s", strerror(errno));
        return false;
    }
    memset(buf->map, 0, creq.size);

    /* depth 24, 32 bpp: XRGB8888, the byte order of lv_color32_t (B, G, R, A) */
    if (drmModeAddFB(fd, PANEL_W, PANEL_H, 24, 32, buf->pitch, buf->handle, &buf->fb_id)) {
        err("add fb: %s", strerror(errno));
        return false;
    }
    return true;
}

static bool open_card(const char *path) {
    uint64_t has_dumb = 0;

    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    /* drmSetMaster: fails if another process owns the display */
    if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) == 0 && has_dumb &&
        drmSetMaster(fd) == 0 && find_output())
    {
        if (mode.hdisplay == PANEL_W && mode.vdisplay == PANEL_H) {
            return true;
        }
        err("%s: mode %ux%u, %ux%u expected", path, mode.hdisplay, mode.vdisplay, PANEL_W, PANEL_H);
    }
    close(fd);
    fd = -1;
    return false;
}

static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec, unsigned int tv_usec, void *data) {
    flip_pending = false;
}

static void wait_flip(void) {
    drmEventContext ctx = { .version = 2, .page_flip_handler = page_flip_handler };
    struct pollfd   pfd = { .fd = fd, .events = POLLIN };

    while (flip_pending) {
        if (poll(&pfd, 1, FLIP_TIMEOUT_MS) <= 0) {
            err("page flip timeout");
            flip_pending = false;       /* never hang the GUI on a lost event */
            break;
        }
        drmHandleEvent(fd, &ctx);
    }
}

/* Landscape area (w x h pixels, packed) -> portrait shadow, 90 ccw:
 * landscape (x, y) goes to portrait column y, row LAND_W - 1 - x */
static void rotate_area(const lv_area_t *a, const uint32_t *src) {
    int32_t w = lv_area_get_width(a);
    int32_t h = lv_area_get_height(a);
    int32_t x = 0, y = 0;

#ifdef __ARM_NEON
    /* 4x4 blocks: transpose in registers, store 4 portrait rows of 4 pixels */
    for (y = 0; y + 4 <= h; y += 4) {
        for (x = 0; x + 4 <= w; x += 4) {
            const uint32_t *s = src + y * w + x;
            uint32_t *d = shadow + (LAND_W - 1 - (a->x1 + x)) * PANEL_W + a->y1 + y;

            uint32x4x2_t t0 = vtrnq_u32(vld1q_u32(s), vld1q_u32(s + w));
            uint32x4x2_t t1 = vtrnq_u32(vld1q_u32(s + 2 * w), vld1q_u32(s + 3 * w));

            vst1q_u32(d, vcombine_u32(vget_low_u32(t0.val[0]), vget_low_u32(t1.val[0])));
            vst1q_u32(d - PANEL_W, vcombine_u32(vget_low_u32(t0.val[1]), vget_low_u32(t1.val[1])));
            vst1q_u32(d - 2 * PANEL_W, vcombine_u32(vget_high_u32(t0.val[0]), vget_high_u32(t1.val[0])));
            vst1q_u32(d - 3 * PANEL_W, vcombine_u32(vget_high_u32(t0.val[1]), vget_high_u32(t1.val[1])));
        }
        /* right edge of this band */
        for (int32_t yy = y; yy < y + 4; yy++) {
            for (int32_t xx = x; xx < w; xx++) {
                shadow[(LAND_W - 1 - (a->x1 + xx)) * PANEL_W + a->y1 + yy] = src[yy * w + xx];
            }
        }
    }
#endif

    /* bottom rows (all rows without NEON) */
    for (; y < h; y++) {
        for (x = 0; x < w; x++) {
            shadow[(LAND_W - 1 - (a->x1 + x)) * PANEL_W + a->y1 + y] = src[y * w + x];
        }
    }
}

static void rects_add(rects_t *r, const lv_area_t *portrait) {
    if (r->cnt < MAX_RECTS) {
        r->rects[r->cnt++] = *portrait;
    } else {
        r->full = true;
    }
}

static void copy_rect(struct drm_buffer *buf, const lv_area_t *p) {
    size_t n = lv_area_get_width(p) * 4;

    for (int32_t y = p->y1; y <= p->y2; y++) {
        memcpy(buf->map + y * buf->pitch + p->x1 * 4, shadow + y * PANEL_W + p->x1, n);
    }
}

static void copy_rects(struct drm_buffer *buf, const rects_t *r) {
    static const lv_area_t all = { 0, 0, PANEL_W - 1, PANEL_H - 1 };

    if (r->full) {
        copy_rect(buf, &all);
    } else {
        for (uint32_t i = 0; i < r->cnt; i++) {
            copy_rect(buf, &r->rects[i]);
        }
    }
}

/* Last area of a frame: back buffer <- dirty rects of this and the previous frame, then flip */
static void present(void) {
    static bool failed;
    rects_t     *now = &frame[cur], *prev = &frame[cur ^ 1];
    int         ret;

    wait_flip();                        /* the back buffer was on screen until this flip */

    copy_rects(&bufs[back], prev);
    copy_rects(&bufs[back], now);

    if (crtc_set) {
        /* shown at the next vblank, the event clears flip_pending */
        ret = drmModePageFlip(fd, crtc_id, bufs[back].fb_id, DRM_MODE_PAGE_FLIP_EVENT, NULL);
        flip_pending = ret == 0;
    } else {
        /* first frame: modeset, synchronous */
        ret = drmModeSetCrtc(fd, crtc_id, bufs[back].fb_id, 0, 0, &conn_id, 1, &mode);
        crtc_set = ret == 0;
    }

    if (ret) {
        if (!failed) {
            err("%s: %s", crtc_set ? "page flip" : "set crtc", strerror(errno));
        }
        failed = true;
        /* Not shown, buffers not swapped: both get the whole screen once flips work again */
        now->full = true;
        prev->full = true;
        return;
    }
    failed = false;
    back ^= 1;

    /* this frame becomes the previous one */
    cur ^= 1;
    frame[cur].cnt = 0;
    frame[cur].full = false;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

bool drm_init(void) {
    const char *path = getenv("DRM_CARD");

    if (path ? !open_card(path) : !open_card("/dev/dri/card0") && !open_card("/dev/dri/card1")) {
        err("no usable device, fbdev is used");
        return false;
    }

    shadow = calloc(PANEL_W * PANEL_H, 4);
    if (!shadow || !alloc_buf(&bufs[0]) || !alloc_buf(&bufs[1])) {
        err("buffers not allocated, fbdev is used");
        for (int i = 0; i < 2; i++) {
            if (bufs[i].map && bufs[i].map != MAP_FAILED) {
                munmap(bufs[i].map, bufs[i].size);
            }
        }
        free(shadow);
        close(fd);              /* also frees the dumb buffers and their fbs */
        fd = -1;
        return false;
    }

    /* Both buffers are empty: the first two frames copy the whole screen */
    frame[0].full = true;
    frame[1].full = true;

    fprintf(stderr, "drm: %ux%u, connector %u, crtc %u\n", mode.hdisplay, mode.vdisplay, conn_id, crtc_id);
    return true;
}

void drm_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_p) {
    if (area->x1 >= 0 && area->y1 >= 0 && area->x2 < LAND_W && area->y2 < LAND_H) {
        lv_area_t p = { area->y1, LAND_W - 1 - area->x2, area->y2, LAND_W - 1 - area->x1 };

        rotate_area(area, (const uint32_t *) color_p);
        rects_add(&frame[cur], &p);
    }

    if (lv_disp_flush_is_last(drv)) {
        present();
    }
    lv_disp_flush_ready(drv);
}

#endif
