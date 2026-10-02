#!/usr/bin/env python3
"""LVGL 8 font C files (4 bpp, ASCII 32-126, no compression, no kerning) from a TTF,
like lv_font_conv --bpp 4 --no-compress --range 32-126 --format lvgl, with Pillow only.

Usage: font_gen.py FONT.ttf NAME SONY=PX ...   writes NAME_SONY.c (PX pixels) in the cwd
The Arial fonts (Arial Bold, as RS-BA1's buttons):
  cd src/fonts && ../../tools/font_gen.py /mnt/c/Windows/Fonts/arialbd.ttf arial 14=11 16=13 18=14 \
    20=15 22=18 24=19 26=21 28=22 30=24 32=26 34=27 36=29 38=30 40=32 42=34 44=35 60=48
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

FIRST, LAST = 32, 126


def glyphs(font):
    out = []
    for c in range(FIRST, LAST + 1):
        ch = chr(c)
        adv = round(font.getlength(ch) * 16)
        x0, y0, x1, y1 = font.getbbox(ch, anchor='ls')     # y down, baseline at 0
        if ch == ' ' or x1 <= x0 or y1 <= y0:
            out.append((ch, adv, 0, 0, 0, 0, []))
            continue
        im = Image.new('L', (x1 - x0, y1 - y0))
        ImageDraw.Draw(im).text((-x0, -y0), ch, font=font, fill=255, anchor='ls')
        bx = im.getbbox()                                  # trim empty rows/columns
        if not bx:
            out.append((ch, adv, 0, 0, 0, 0, []))
            continue
        im = im.crop(bx)
        px = [(v * 15 + 127) // 255 for v in im.getdata()]
        out.append((ch, adv, im.width, im.height, x0 + bx[0], -(y0 + bx[3]), px))
    return out


def c_char(ch):
    return {'"': '\\"', '\\': '\\\\'}.get(ch, ch)


def write(ttf, name, sony, px):
    font = ImageFont.truetype(ttf, px)
    gl = glyphs(font)
    inked = [g for g in gl if g[2]]
    ascent = max(g[5] + g[3] for g in inked)
    descent = min(g[5] for g in inked)
    fn = f'{name}_{sony}'
    o = [f'''/*******************************************************************************
 * Size: {px} px
 * Bpp: 4
 * Opts: tools/font_gen.py {os.path.basename(ttf)} {name} {sony}={px}
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef {fn.upper()}
#define {fn.upper()} 1
#endif

#if {fn.upper()}

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {{''']
    dsc, idx, first = [], 0, True
    for ch, adv, w, h, ox, oy, pxs in gl:
        o.append(('' if first else ',\n') + f'\n    /* U+{ord(ch):04X} "{c_char(ch)}" */')
        if pxs:
            if len(pxs) % 2: pxs = pxs + [0]
            b = [pxs[i] << 4 | pxs[i + 1] for i in range(0, len(pxs), 2)]
            o.append('\n' + ',\n'.join('    ' + ', '.join(f'{v:#x}' for v in b[i:i + 8]) for i in range(0, len(b), 8)))
            first = False
        dsc.append(f'    {{.bitmap_index = {idx}, .adv_w = {adv}, .box_w = {w}, .box_h = {h}, .ofs_x = {ox}, .ofs_y = {oy}}}')
        idx += len(pxs) // 2
    o.append(f'''
}};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{
    {{.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0}} /* id = 0 reserved */,
{',\n'.join(dsc)}
}};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const lv_font_fmt_txt_cmap_t cmaps[] =
{{
    {{
        .range_start = {FIRST}, .range_length = {LAST - FIRST + 1}, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }}
}};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {{
#else
static lv_font_fmt_txt_dsc_t font_dsc = {{
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
}};

/*-----------------
 *  PUBLIC FONT
 *----------------*/

#if LVGL_VERSION_MAJOR >= 8
const lv_font_t {fn} = {{
#else
lv_font_t {fn} = {{
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = {ascent - descent},          /*The maximum line height required by the font*/
    .base_line = {-descent},             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = {-round(px * 0.1)},
    .underline_thickness = {max(1, round(px * 0.07))},
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
}};

#endif /*#if {fn.upper()}*/
''')
    with open(fn + '.c', 'w') as f:
        f.write(''.join(o))


if __name__ == '__main__':
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    for arg in sys.argv[3:]:
        sony, px = arg.split('=')
        write(sys.argv[1], sys.argv[2], sony, int(px))
