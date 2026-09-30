/*******************************************************************************
 * Size: 8 px
 * Bpp: 4
 * Opts: --bpp 4 --size 8 --no-compress --font Roboto-Medium.ttf --range 32-126 --format lvgl -o roboto_12.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef ROBOTO_12
#define ROBOTO_12 1
#endif

#if ROBOTO_12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0x68, 0x68, 0x68, 0x34, 0x57,

    /* U+0022 "\"" */
    0xb9, 0x58, 0x72,

    /* U+0023 "#" */
    0x4, 0x6a, 0x4, 0xca, 0xd5, 0xa, 0x28, 0x6,
    0xda, 0xb2, 0x9, 0x82, 0x0,

    /* U+0024 "$" */
    0x0, 0x50, 0x0, 0x8e, 0x50, 0x5b, 0x1e, 0x2,
    0xd6, 0x0, 0x1, 0x6d, 0x7, 0x93, 0xe1, 0x7,
    0xd4, 0x0, 0x2, 0x0,

    /* U+0025 "%" */
    0x68, 0x80, 0x30, 0x48, 0x68, 0x10, 0x0, 0x69,
    0x92, 0x2, 0x7a, 0x28, 0x3, 0x8, 0xa4,

    /* U+0026 "&" */
    0x1c, 0xc5, 0x2, 0xc7, 0x60, 0x3d, 0xc0, 0x5b,
    0x48, 0xc8, 0x4c, 0xac, 0x80,

    /* U+0027 "'" */
    0xb1, 0x90,

    /* U+0028 "(" */
    0x3, 0x60, 0xc0, 0x48, 0x7, 0x60, 0x66, 0x3,
    0x90, 0xb, 0x10, 0x14,

    /* U+0029 ")" */
    0x80, 0x3, 0x90, 0xc, 0x0, 0xb3, 0xb, 0x20,
    0xc0, 0x57, 0x5, 0x0,

    /* U+002A "*" */
    0x9, 0x10, 0x7d, 0xa2, 0x38, 0x90, 0x0, 0x0,

    /* U+002B "+" */
    0x3, 0x90, 0x0, 0x3b, 0x0, 0x9d, 0xfd, 0x20,
    0x3b, 0x0,

    /* U+002C "," */
    0x75, 0xa1,

    /* U+002D "-" */
    0x8b, 0x30,

    /* U+002E "." */
    0x57,

    /* U+002F "/" */
    0x0, 0xd0, 0x5, 0x80, 0xc, 0x10, 0x2b, 0x0,
    0x94, 0x0, 0x60, 0x0,

    /* U+0030 "0" */
    0x2c, 0xb8, 0x8, 0x60, 0xd1, 0x95, 0xd, 0x28,
    0x60, 0xd1, 0x2c, 0xb8, 0x0,

    /* U+0031 "1" */
    0x3, 0x32, 0x9d, 0x2, 0xd0, 0x2d, 0x2, 0xd0,
    0x2d,

    /* U+0032 "2" */
    0x3c, 0xc8, 0x5, 0x30, 0xf0, 0x0, 0x87, 0x0,
    0x97, 0x0, 0x7e, 0xbb, 0x20,

    /* U+0033 "3" */
    0x3c, 0xc8, 0x3, 0x20, 0xe0, 0x5, 0xd8, 0x3,
    0x20, 0xe0, 0x4c, 0xc9, 0x0,

    /* U+0034 "4" */
    0x0, 0xba, 0x0, 0x7b, 0xa0, 0x3a, 0x5a, 0x9,
    0xbc, 0xe3, 0x0, 0x5a, 0x0,

    /* U+0035 "5" */
    0x2e, 0xcb, 0x4, 0xeb, 0x70, 0x12, 0xe, 0x13,
    0x50, 0xe1, 0x1c, 0xc8, 0x0,

    /* U+0036 "6" */
    0xa, 0xb6, 0x6, 0xca, 0x80, 0x87, 0xd, 0x27,
    0x80, 0xc3, 0x1b, 0xb9, 0x0,

    /* U+0037 "7" */
    0x8b, 0xbf, 0x20, 0x5, 0x80, 0x0, 0xd0, 0x0,
    0x59, 0x0, 0x9, 0x60, 0x0,

    /* U+0038 "8" */
    0x2c, 0xc9, 0x6, 0x90, 0xe0, 0x1e, 0xd8, 0x8,
    0x70, 0xd1, 0x3c, 0xca, 0x0,

    /* U+0039 "9" */
    0x3c, 0xc7, 0x9, 0x60, 0xe0, 0x3b, 0xae, 0x10,
    0x1, 0xf0, 0x1b, 0xc6, 0x0,

    /* U+003A ":" */
    0x57, 0x0, 0x0, 0x57,

    /* U+003B ";" */
    0x57, 0x0, 0x0, 0x48, 0x74,

    /* U+003C "<" */
    0x0, 0x44, 0x6b, 0x82, 0x6b, 0x82, 0x0, 0x44,

    /* U+003D "=" */
    0x5b, 0xba, 0x0, 0x0, 0x5b, 0xba,

    /* U+003E ">" */
    0x44, 0x0, 0x17, 0xb7, 0x28, 0xc7, 0x44, 0x0,

    /* U+003F "?" */
    0x6c, 0xd4, 0x30, 0x68, 0x3, 0xc1, 0x5, 0x20,
    0x9, 0x40,

    /* U+0040 "@" */
    0x3, 0x98, 0x87, 0x2, 0x92, 0x98, 0x46, 0x82,
    0xb1, 0xb0, 0x99, 0x1c, 0xb, 0xa, 0x57, 0xa7,
    0xb8, 0x30, 0x69, 0x86, 0x0,

    /* U+0041 "A" */
    0x1, 0xf4, 0x0, 0x7, 0x9a, 0x0, 0xd, 0xc,
    0x10, 0x4e, 0xbd, 0x80, 0xb3, 0x1, 0xd0,

    /* U+0042 "B" */
    0x6e, 0xbc, 0x26, 0x90, 0x76, 0x6d, 0xae, 0x26,
    0x90, 0x4a, 0x6d, 0xbc, 0x50,

    /* U+0043 "C" */
    0xa, 0xbc, 0x37, 0x80, 0x28, 0x95, 0x0, 0x7,
    0x80, 0x28, 0xa, 0xbc, 0x30,

    /* U+0044 "D" */
    0x6e, 0xbb, 0x26, 0x90, 0x3c, 0x69, 0x0, 0xe6,
    0x90, 0x3c, 0x6d, 0xbb, 0x20,

    /* U+0045 "E" */
    0x6e, 0xbb, 0x56, 0x90, 0x0, 0x6d, 0xbb, 0x6,
    0x90, 0x0, 0x6d, 0xbb, 0x50,

    /* U+0046 "F" */
    0x6e, 0xbb, 0x56, 0x90, 0x0, 0x6d, 0xbb, 0x6,
    0x90, 0x0, 0x69, 0x0, 0x0,

    /* U+0047 "G" */
    0xa, 0xbc, 0x57, 0x80, 0x15, 0x96, 0x2a, 0xb7,
    0x80, 0x1d, 0xa, 0xbc, 0x70,

    /* U+0048 "H" */
    0x69, 0x0, 0xe1, 0x69, 0x0, 0xe1, 0x6d, 0xbb,
    0xf1, 0x69, 0x0, 0xe1, 0x69, 0x0, 0xe1,

    /* U+0049 "I" */
    0x5a, 0x5a, 0x5a, 0x5a, 0x5a,

    /* U+004A "J" */
    0x0, 0xe, 0x0, 0xe, 0x0, 0xe, 0x52, 0x1e,
    0x5c, 0xc6,

    /* U+004B "K" */
    0x69, 0x8, 0x90, 0x69, 0x5b, 0x0, 0x6e, 0xf2,
    0x0, 0x69, 0x4c, 0x0, 0x69, 0x6, 0xb0,

    /* U+004C "L" */
    0x69, 0x0, 0x6, 0x90, 0x0, 0x69, 0x0, 0x6,
    0x90, 0x0, 0x6d, 0xbb, 0x20,

    /* U+004D "M" */
    0x6f, 0x10, 0x1f, 0x66, 0xd7, 0x7, 0xd6, 0x68,
    0xc0, 0xc8, 0x66, 0x97, 0xa7, 0x86, 0x69, 0x1e,
    0x19, 0x60,

    /* U+004E "N" */
    0x6d, 0x0, 0xe1, 0x6d, 0x90, 0xe1, 0x69, 0x94,
    0xe1, 0x69, 0xc, 0xe1, 0x69, 0x3, 0xf1,

    /* U+004F "O" */
    0xa, 0xbc, 0x40, 0x78, 0x1, 0xe0, 0x95, 0x0,
    0xe0, 0x78, 0x1, 0xe0, 0xa, 0xbc, 0x40,

    /* U+0050 "P" */
    0x6e, 0xbc, 0x56, 0x90, 0x2c, 0x6d, 0xbb, 0x46,
    0x90, 0x0, 0x69, 0x0, 0x0,

    /* U+0051 "Q" */
    0xa, 0xbc, 0x40, 0x78, 0x1, 0xe0, 0x95, 0x0,
    0xe0, 0x78, 0x1, 0xe0, 0xa, 0xbd, 0xa0, 0x0,
    0x0, 0x81,

    /* U+0052 "R" */
    0x6e, 0xbc, 0x46, 0x90, 0x5a, 0x6d, 0xbe, 0x36,
    0x90, 0x5a, 0x69, 0x2, 0xc0,

    /* U+0053 "S" */
    0x2b, 0xbc, 0x26, 0x90, 0x24, 0x7, 0xa9, 0x14,
    0x30, 0x69, 0x2b, 0xbc, 0x30,

    /* U+0054 "T" */
    0xac, 0xfb, 0x50, 0x2c, 0x0, 0x2, 0xc0, 0x0,
    0x2c, 0x0, 0x2, 0xc0, 0x0,

    /* U+0055 "U" */
    0x77, 0x0, 0xf7, 0x70, 0xf, 0x77, 0x0, 0xf6,
    0x80, 0x1e, 0x1b, 0xbc, 0x50,

    /* U+0056 "V" */
    0xb4, 0x2, 0xc0, 0x59, 0x8, 0x60, 0xd, 0xd,
    0x0, 0x8, 0x99, 0x0, 0x1, 0xf2, 0x0,

    /* U+0057 "W" */
    0xb3, 0xf, 0x12, 0xc7, 0x74, 0xe5, 0x67, 0x2a,
    0x94, 0xaa, 0x30, 0xcb, 0xc, 0xd0, 0xa, 0x80,
    0x7a, 0x0,

    /* U+0058 "X" */
    0x6a, 0x9, 0x80, 0xb7, 0xc0, 0x3, 0xf4, 0x0,
    0xb8, 0xc0, 0x7a, 0x8, 0x90,

    /* U+0059 "Y" */
    0xa5, 0x6, 0x91, 0xd1, 0xd1, 0x6, 0xd6, 0x0,
    0xf, 0x0, 0x0, 0xf0, 0x0,

    /* U+005A "Z" */
    0x7b, 0xbf, 0x40, 0x6, 0x90, 0x3, 0xc0, 0x1,
    0xc2, 0x0, 0x9e, 0xbb, 0x40,

    /* U+005B "[" */
    0x6c, 0x17, 0x70, 0x77, 0x7, 0x70, 0x77, 0x7,
    0x70, 0x77, 0x6, 0xc1,

    /* U+005C "\\" */
    0xb3, 0x0, 0x4a, 0x0, 0xd, 0x10, 0x6, 0x80,
    0x0, 0xd0, 0x0, 0x52,

    /* U+005D "]" */
    0xb8, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a, 0xb8,

    /* U+005E "^" */
    0x5, 0x10, 0x1d, 0x80, 0x93, 0xb0,

    /* U+005F "_" */
    0xab, 0xb6,

    /* U+0060 "`" */
    0x3a, 0x0,

    /* U+0061 "a" */
    0x3c, 0xb7, 0x2a, 0x9d, 0x95, 0x2d, 0x5d, 0xae,

    /* U+0062 "b" */
    0x87, 0x0, 0x8, 0x70, 0x0, 0x8c, 0xc9, 0x8,
    0x70, 0xd2, 0x87, 0xd, 0x28, 0xbb, 0xa0,

    /* U+0063 "c" */
    0x2c, 0xc7, 0x96, 0x6, 0x95, 0x5, 0x2c, 0xc7,

    /* U+0064 "d" */
    0x0, 0xf, 0x0, 0x0, 0xf0, 0x2d, 0xaf, 0x9,
    0x60, 0xf0, 0x95, 0xf, 0x3, 0xc9, 0xe0,

    /* U+0065 "e" */
    0x1b, 0xc8, 0x9c, 0xac, 0x98, 0x0, 0x2c, 0xb8,

    /* U+0066 "f" */
    0xb, 0x92, 0xc0, 0x9e, 0x63, 0xc0, 0x3c, 0x3,
    0xc0,

    /* U+0067 "g" */
    0x2d, 0xae, 0x9, 0x60, 0xe0, 0x96, 0xe, 0x3,
    0xda, 0xf0, 0x1a, 0xc8, 0x0,

    /* U+0068 "h" */
    0x87, 0x0, 0x8, 0x70, 0x0, 0x8b, 0xca, 0x8,
    0x70, 0xe0, 0x87, 0xe, 0x8, 0x70, 0xe0,

    /* U+0069 "i" */
    0x56, 0x0, 0x78, 0x78, 0x78, 0x78,

    /* U+006A "j" */
    0x5, 0x60, 0x0, 0x6, 0x90, 0x69, 0x6, 0x90,
    0x68, 0x3d, 0x40,

    /* U+006B "k" */
    0x87, 0x0, 0x8, 0x70, 0x0, 0x87, 0x88, 0x8,
    0xdd, 0x0, 0x87, 0xb4, 0x8, 0x72, 0xd0,

    /* U+006C "l" */
    0x78, 0x78, 0x78, 0x78, 0x78, 0x78,

    /* U+006D "m" */
    0x8b, 0xca, 0xad, 0x28, 0x70, 0xf0, 0x86, 0x87,
    0xf, 0x7, 0x78, 0x70, 0xf0, 0x77,

    /* U+006E "n" */
    0x8b, 0xca, 0x8, 0x70, 0xe0, 0x87, 0xe, 0x8,
    0x70, 0xe0,

    /* U+006F "o" */
    0x2c, 0xc8, 0x9, 0x60, 0xd1, 0x96, 0xd, 0x12,
    0xcb, 0x80,

    /* U+0070 "p" */
    0x8b, 0xb9, 0x8, 0x70, 0xd1, 0x87, 0xd, 0x28,
    0xcb, 0xa0, 0x87, 0x0, 0x0,

    /* U+0071 "q" */
    0x2d, 0xae, 0x96, 0xf, 0x96, 0xf, 0x3c, 0xaf,
    0x0, 0xf,

    /* U+0072 "r" */
    0x0, 0x8, 0xc7, 0x87, 0x8, 0x70, 0x87, 0x0,

    /* U+0073 "s" */
    0x3c, 0xc6, 0x6b, 0x52, 0x24, 0x8a, 0x4c, 0xb7,

    /* U+0074 "t" */
    0x4b, 0xa, 0xe4, 0x4b, 0x4, 0xb0, 0x1d, 0x40,

    /* U+0075 "u" */
    0x86, 0xe, 0x8, 0x60, 0xe0, 0x77, 0xe, 0x2,
    0xda, 0xe0,

    /* U+0076 "v" */
    0xb4, 0x2c, 0x49, 0x76, 0xd, 0xc0, 0x8, 0xa0,

    /* U+0077 "w" */
    0xb2, 0x87, 0x3a, 0x66, 0xbb, 0x76, 0x2c, 0x99,
    0xc1, 0xd, 0x34, 0xc0,

    /* U+0078 "x" */
    0x78, 0x78, 0xc, 0xc0, 0xc, 0xc0, 0x88, 0x79,

    /* U+0079 "y" */
    0xb4, 0x3c, 0x4a, 0x95, 0xc, 0xd0, 0x8, 0x70,
    0x6c, 0x10,

    /* U+007A "z" */
    0x7b, 0xe9, 0x3, 0xd1, 0x1d, 0x20, 0x9e, 0xb8,

    /* U+007B "{" */
    0x0, 0x0, 0x85, 0xe, 0x1, 0xd0, 0xb7, 0x1,
    0xd0, 0xe, 0x0, 0x85, 0x0, 0x0,

    /* U+007C "|" */
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x11,

    /* U+007D "}" */
    0x0, 0x9, 0x40, 0x3a, 0x2, 0xc0, 0xc, 0x52,
    0xc0, 0x3a, 0x9, 0x40, 0x0, 0x0,

    /* U+007E "~" */
    0x2b, 0x80, 0x75, 0x25, 0xc6
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 32, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 34, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 5, .adv_w = 42, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 8, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 21, .adv_w = 73, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 41, .adv_w = 94, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 56, .adv_w = 82, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 69, .adv_w = 22, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 71, .adv_w = 43, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 83, .adv_w = 44, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 95, .adv_w = 56, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 103, .adv_w = 71, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 113, .adv_w = 29, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 115, .adv_w = 42, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 117, .adv_w = 36, .box_w = 2, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 118, .adv_w = 52, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 130, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 143, .adv_w = 73, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 165, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 191, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 204, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 217, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 230, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 243, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 256, .adv_w = 34, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 260, .adv_w = 34, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 265, .adv_w = 65, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 273, .adv_w = 73, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 279, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 287, .adv_w = 62, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 297, .adv_w = 114, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 318, .adv_w = 83, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 333, .adv_w = 82, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 346, .adv_w = 81, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 359, .adv_w = 84, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 372, .adv_w = 74, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 385, .adv_w = 74, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 398, .adv_w = 86, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 411, .adv_w = 91, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 426, .adv_w = 37, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 431, .adv_w = 72, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 441, .adv_w = 83, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 456, .adv_w = 69, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 469, .adv_w = 112, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 487, .adv_w = 91, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 502, .adv_w = 87, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 517, .adv_w = 83, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 530, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 548, .adv_w = 85, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 561, .adv_w = 81, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 574, .adv_w = 74, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 587, .adv_w = 87, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 600, .adv_w = 81, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 615, .adv_w = 113, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 633, .adv_w = 81, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 646, .adv_w = 79, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 659, .adv_w = 76, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 672, .adv_w = 35, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 684, .adv_w = 53, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 696, .adv_w = 35, .box_w = 2, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 704, .adv_w = 55, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 710, .adv_w = 58, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 712, .adv_w = 41, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 714, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 722, .adv_w = 73, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 737, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 745, .adv_w = 73, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 760, .adv_w = 68, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 768, .adv_w = 45, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 777, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 790, .adv_w = 73, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 805, .adv_w = 33, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 811, .adv_w = 34, .box_w = 3, .box_h = 7, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 822, .adv_w = 67, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 837, .adv_w = 33, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 843, .adv_w = 111, .box_w = 7, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 857, .adv_w = 73, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 867, .adv_w = 73, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 877, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 890, .adv_w = 73, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 900, .adv_w = 44, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 908, .adv_w = 67, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 916, .adv_w = 45, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 924, .adv_w = 73, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 934, .adv_w = 65, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 942, .adv_w = 96, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 954, .adv_w = 65, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 962, .adv_w = 65, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 972, .adv_w = 65, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 980, .adv_w = 43, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 994, .adv_w = 32, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1001, .adv_w = 43, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1015, .adv_w = 85, .box_w = 5, .box_h = 2, .ofs_x = 0, .ofs_y = 1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    3, 3,
    3, 8,
    3, 34,
    3, 66,
    3, 68,
    3, 69,
    3, 70,
    3, 72,
    3, 78,
    3, 79,
    3, 80,
    3, 81,
    3, 82,
    3, 84,
    3, 88,
    8, 3,
    8, 8,
    8, 34,
    8, 66,
    8, 68,
    8, 69,
    8, 70,
    8, 72,
    8, 78,
    8, 79,
    8, 80,
    8, 81,
    8, 82,
    8, 84,
    8, 88,
    9, 55,
    9, 56,
    9, 58,
    13, 3,
    13, 8,
    15, 3,
    15, 8,
    27, 3,
    27, 8,
    34, 32,
    34, 85,
    34, 88,
    35, 53,
    35, 55,
    35, 58,
    36, 10,
    36, 53,
    37, 13,
    37, 15,
    37, 27,
    37, 34,
    37, 53,
    37, 55,
    37, 57,
    37, 58,
    37, 59,
    38, 53,
    38, 68,
    38, 69,
    38, 70,
    38, 72,
    38, 80,
    38, 82,
    38, 86,
    38, 87,
    38, 88,
    38, 90,
    39, 13,
    39, 15,
    39, 27,
    39, 34,
    39, 53,
    39, 66,
    39, 68,
    39, 69,
    39, 70,
    39, 72,
    39, 80,
    39, 82,
    39, 83,
    39, 86,
    39, 87,
    39, 90,
    43, 34,
    44, 14,
    44, 36,
    44, 40,
    44, 48,
    44, 50,
    44, 68,
    44, 69,
    44, 70,
    44, 72,
    44, 80,
    44, 82,
    44, 86,
    44, 87,
    44, 88,
    44, 90,
    45, 3,
    45, 8,
    45, 34,
    45, 36,
    45, 40,
    45, 48,
    45, 50,
    45, 53,
    45, 54,
    45, 55,
    45, 56,
    45, 58,
    45, 86,
    45, 87,
    45, 88,
    45, 90,
    48, 13,
    48, 15,
    48, 27,
    48, 34,
    48, 53,
    48, 55,
    48, 57,
    48, 58,
    48, 59,
    49, 13,
    49, 15,
    49, 27,
    49, 34,
    49, 57,
    49, 59,
    49, 66,
    49, 68,
    49, 69,
    49, 70,
    49, 72,
    49, 80,
    49, 82,
    49, 85,
    49, 87,
    49, 90,
    50, 53,
    50, 55,
    50, 56,
    50, 58,
    51, 53,
    51, 55,
    51, 58,
    53, 13,
    53, 14,
    53, 15,
    53, 27,
    53, 34,
    53, 36,
    53, 40,
    53, 48,
    53, 50,
    53, 52,
    53, 53,
    53, 55,
    53, 56,
    53, 58,
    53, 66,
    53, 68,
    53, 69,
    53, 70,
    53, 72,
    53, 78,
    53, 79,
    53, 80,
    53, 81,
    53, 82,
    53, 83,
    53, 84,
    53, 86,
    53, 87,
    53, 88,
    53, 89,
    53, 90,
    53, 91,
    54, 34,
    55, 10,
    55, 13,
    55, 14,
    55, 15,
    55, 27,
    55, 34,
    55, 36,
    55, 40,
    55, 48,
    55, 50,
    55, 62,
    55, 66,
    55, 68,
    55, 69,
    55, 70,
    55, 72,
    55, 80,
    55, 82,
    55, 83,
    55, 86,
    55, 87,
    55, 90,
    55, 94,
    56, 10,
    56, 13,
    56, 14,
    56, 15,
    56, 27,
    56, 34,
    56, 53,
    56, 62,
    56, 66,
    56, 68,
    56, 69,
    56, 70,
    56, 72,
    56, 80,
    56, 82,
    56, 83,
    56, 86,
    56, 94,
    57, 14,
    57, 36,
    57, 40,
    57, 48,
    57, 50,
    57, 55,
    57, 68,
    57, 69,
    57, 70,
    57, 72,
    57, 80,
    57, 82,
    57, 86,
    57, 87,
    57, 90,
    58, 7,
    58, 10,
    58, 11,
    58, 13,
    58, 14,
    58, 15,
    58, 27,
    58, 34,
    58, 36,
    58, 40,
    58, 43,
    58, 48,
    58, 50,
    58, 52,
    58, 53,
    58, 54,
    58, 55,
    58, 56,
    58, 58,
    58, 62,
    58, 66,
    58, 68,
    58, 69,
    58, 70,
    58, 71,
    58, 72,
    58, 78,
    58, 79,
    58, 80,
    58, 81,
    58, 82,
    58, 83,
    58, 84,
    58, 85,
    58, 86,
    58, 87,
    58, 89,
    58, 90,
    58, 91,
    58, 94,
    59, 34,
    59, 36,
    59, 40,
    59, 48,
    59, 50,
    59, 68,
    59, 69,
    59, 70,
    59, 72,
    59, 80,
    59, 82,
    59, 86,
    59, 87,
    59, 88,
    59, 90,
    60, 43,
    60, 54,
    66, 3,
    66, 8,
    66, 87,
    66, 90,
    67, 3,
    67, 8,
    67, 87,
    67, 89,
    67, 90,
    67, 91,
    68, 3,
    68, 8,
    70, 3,
    70, 8,
    70, 87,
    70, 90,
    71, 3,
    71, 8,
    71, 10,
    71, 62,
    71, 94,
    73, 3,
    73, 8,
    76, 68,
    76, 69,
    76, 70,
    76, 72,
    76, 82,
    78, 3,
    78, 8,
    79, 3,
    79, 8,
    80, 3,
    80, 8,
    80, 87,
    80, 89,
    80, 90,
    80, 91,
    81, 3,
    81, 8,
    81, 87,
    81, 89,
    81, 90,
    81, 91,
    83, 3,
    83, 8,
    83, 13,
    83, 15,
    83, 27,
    83, 71,
    83, 85,
    83, 87,
    83, 88,
    83, 90,
    87, 13,
    87, 15,
    87, 27,
    87, 66,
    87, 68,
    87, 69,
    87, 70,
    87, 71,
    87, 72,
    87, 80,
    87, 82,
    88, 13,
    88, 15,
    88, 27,
    89, 68,
    89, 69,
    89, 70,
    89, 72,
    89, 80,
    89, 82,
    90, 13,
    90, 15,
    90, 27,
    90, 66,
    90, 68,
    90, 69,
    90, 70,
    90, 71,
    90, 72,
    90, 80,
    90, 82,
    91, 68,
    91, 69,
    91, 70,
    91, 72,
    91, 80,
    91, 82,
    92, 43,
    92, 54
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -2, -2, -9, -4, -5, -5, -5, -5,
    -1, -1, -6, -1, -5, -6, 1, -2,
    -2, -9, -4, -5, -5, -5, -5, -1,
    -1, -6, -1, -5, -6, 1, 1, 2,
    1, -18, -18, -18, -18, -18, -18, -5,
    -1, -2, -2, -2, -6, -2, -2, -7,
    -7, -7, -1, -2, -1, -1, -3, -1,
    1, -1, -1, -1, -1, -1, -1, -1,
    -2, -1, -2, -19, -19, -19, -7, 1,
    -3, -2, -2, -2, -2, -2, -2, -2,
    -1, -2, -2, -1, -10, -3, -3, -3,
    -3, -2, -2, -2, -2, -2, -2, -1,
    -3, -4, -3, -17, -17, 1, -3, -3,
    -3, -3, -13, -2, -13, -6, -17, -1,
    -8, -5, -8, -7, -7, -7, -1, -2,
    -1, -1, -3, -1, -25, -25, -25, -11,
    -4, -2, -1, -1, -1, -1, -1, -1,
    -1, 1, 1, 1, -2, -2, -1, -3,
    -3, -1, -3, -14, -15, -14, -14, -7,
    -2, -2, -2, -2, -1, 1, 1, 1,
    1, -6, -6, -6, -6, -6, -5, -5,
    -5, -5, -6, -4, -5, -4, -5, -3,
    -5, -5, -4, -1, 1, -13, -7, -13,
    -13, -7, -1, -1, -1, -1, 1, -3,
    -3, -3, -3, -3, -3, -3, -2, -2,
    -1, -1, 1, 1, -10, -5, -10, -10,
    -3, 1, 1, -2, -2, -2, -2, -2,
    -2, -2, -1, -1, 1, -11, -2, -2,
    -2, -2, 1, -2, -2, -2, -2, -2,
    -2, -1, -2, -2, -3, 1, -4, -15,
    -9, -15, -15, -10, -2, -2, -8, -2,
    -2, -1, 1, -5, 1, 1, 1, 1,
    -5, -6, -6, -6, -2, -6, -4, -4,
    -6, -4, -6, -3, -5, -2, -3, -1,
    -2, -1, -2, 1, 1, -2, -2, -2,
    -2, -1, -1, -1, -1, -1, -1, -1,
    -2, -2, -2, -1, -1, -2, -2, -1,
    -1, -2, -2, -1, -1, -1, -1, -2,
    -2, -2, -2, -1, -1, 2, 2, 2,
    3, 2, -2, -2, -1, -1, -1, -1,
    -1, -2, -2, -2, -2, -2, -2, -1,
    -2, -1, -1, -2, -2, -1, -1, -1,
    -1, 1, 1, -10, -10, -10, 1, 1,
    1, 1, 1, -10, -10, -10, -1, -1,
    -1, -1, 1, -1, -1, -1, -6, -6,
    -6, -1, -1, -1, -1, -2, -1, -10,
    -10, -10, -1, -1, -1, -1, 1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 386,
    .glyph_ids_size = 0
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t roboto_12 = {
#else
lv_font_t roboto_12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 9,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if ROBOTO_12*/

