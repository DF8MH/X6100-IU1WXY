/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Frequency row under the spectrum: the edges of the displayed span and
 *  round reference frequencies in between, lined up with the waterfall.
 */

#include "freq_scale.h"

#include "styles.h"
#include "util.h"

#define WIDTH    800
#define LABEL_W  150    /* freq_style width */
#define MAX_REFS 8
#define EDGE_GAP 100    /* no reference this close to an edge label */

static lv_obj_t *edges[2];
static lv_obj_t *refs[MAX_REFS];

static lv_obj_t *make_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_add_style(l, &freq_style, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static void set_freq(lv_obj_t *l, int32_t f, uint32_t color) {
    uint16_t mhz, khz, hz;

    split_freq(f, &mhz, &khz, &hz);
    lv_label_set_text_fmt(l, "%i.%03i", mhz, khz);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
}

void freq_scale_init(lv_obj_t *parent, lv_coord_t y) {
    edges[0] = make_label(parent, 0, y);
    lv_obj_set_style_text_align(edges[0], LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(edges[0], 10, 0);

    edges[1] = make_label(parent, WIDTH - LABEL_W, y);
    lv_obj_set_style_text_align(edges[1], LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_pad_right(edges[1], 10, 0);

    for (int i = 0; i < MAX_REFS; i++) {
        refs[i] = make_label(parent, 0, y);
        lv_obj_add_flag(refs[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void freq_scale_update(int32_t center, int32_t half_width, uint32_t color) {
    static const int32_t mult[] = {1, 2, 5};
    int32_t              left   = center - half_width;
    int32_t              span   = 2 * half_width;
    int32_t              step   = 0;
    int                  n      = 0;

    set_freq(edges[0], left, color);
    set_freq(edges[1], center + half_width, color);

    /* Round step (1, 2, 5 x 10^n Hz, at least 1 kHz): at most 7 per span */
    for (int32_t dec = 1000; !step; dec *= 10) {
        for (int i = 0; i < 3 && !step; i++) {
            if (dec * mult[i] * 7 >= span) {
                step = dec * mult[i];
            }
        }
    }

    for (int32_t f = (left / step + 1) * step; f < left + span && n < MAX_REFS; f += step) {
        lv_coord_t x = (int64_t)(f - left) * WIDTH / span;

        if (x < EDGE_GAP || x > WIDTH - EDGE_GAP) {
            continue;
        }
        set_freq(refs[n], f, color);
        lv_obj_set_x(refs[n], x - LABEL_W / 2);
        lv_obj_clear_flag(refs[n], LV_OBJ_FLAG_HIDDEN);
        n++;
    }
    for (; n < MAX_REFS; n++) {
        lv_obj_add_flag(refs[n], LV_OBJ_FLAG_HIDDEN);
    }
}
