/* Runtime font choice: the code keeps using &sony_N; for Roboto/DejaVu the contents of each
 * sony_N are replaced by the alt font before the UI is built. The alt fonts are ASCII only
 * (lv_font_conv --bpp 4 --no-compress --range 32-126 --format lvgl, size two steps below
 * Sony's); icon glyphs come from the untouched Sony copy through the fallback. */
#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "font_select.h"

#define SIZES(X) X(14) X(16) X(18) X(20) X(22) X(24) X(26) X(28) X(30) \
                 X(32) X(34) X(36) X(38) X(40) X(42) X(44) X(60)

#define DECL(n) extern lv_font_t sony_##n; extern const lv_font_t roboto_##n, dejavu_##n;
SIZES(DECL)

#define SONY(n) &sony_##n,
#define ROBOTO(n) &roboto_##n,
#define DEJAVU(n) &dejavu_##n,
static lv_font_t *const sony[] = { SIZES(SONY) };
static const lv_font_t *const roboto[] = { SIZES(ROBOTO) };
static const lv_font_t *const dejavu[] = { SIZES(DEJAVU) };
#define N_FONTS (sizeof(sony) / sizeof(sony[0]))

static lv_font_t orig[N_FONTS];
static bool applied;

int buttons_h = 54;  /* = BUTTONS_H_MIN */

void fonts_apply(uint8_t choice) {
    if (applied || (choice != FONT_ROBOTO && choice != FONT_DEJAVU)) return;
    applied = true;
    if (choice == FONT_DEJAVU) buttons_h = 62;
    const lv_font_t *const *alt = choice == FONT_ROBOTO ? roboto : dejavu;

    for (unsigned i = 0; i < N_FONTS; i++) {
        orig[i] = *sony[i];
        *sony[i] = *alt[i];
        sony[i]->fallback = &orig[i];
    }
}
