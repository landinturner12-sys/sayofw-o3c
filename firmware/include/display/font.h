/*
 * display/font.h — Public 5x7 ASCII font (subset 0x20..0x7E).
 *
 * Each glyph is 5 columns × 7 rows, packed as 5 bytes per glyph, msb-first
 * row order. Character '.' is mapped to '.'. Out-of-range chars render as
 * blank space.
 */
#ifndef SAYOFW_DISPLAY_FONT_H
#define SAYOFW_DISPLAY_FONT_H

#include <stdint.h>

#define FONT_GLYPH_W      5
#define FONT_GLYPH_H      7
#define FONT_GLYPH_BYTES  FONT_GLYPH_W
#define FONT_FIRST        0x20   /* space */
#define FONT_LAST         0x7E   /* '~' */
#define FONT_NUM_GLYPHS   (FONT_LAST - FONT_FIRST + 1)

extern const uint8_t font_5x7[FONT_NUM_GLYPHS][FONT_GLYPH_BYTES];

#endif /* SAYOFW_DISPLAY_FONT_H */
