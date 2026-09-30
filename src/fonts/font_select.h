#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { FONT_SONY, FONT_ROBOTO, FONT_DEJAVU };

/* Call once at boot, before any style or widget uses the fonts */
void fonts_apply(uint8_t choice);

#ifdef __cplusplus
}
#endif
