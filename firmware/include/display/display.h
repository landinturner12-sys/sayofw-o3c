/*
 * display/display.h — Geometry + framebuffer layout for the OLED panel.
 *
 * The display is organised as WIDTH × HEIGHT pixels, 1 bit per pixel, packed
 * into PAGES = ceil(HEIGHT/8) vertical bytes per horizontal byte. This is
 * the standard SSD1306/SH1106 "page addressing" mode layout.
 *
 *   byte index  = (page * WIDTH) + col
 *   bit  index  = y % 8
 *   pixel (x,y) = (byte >> (y % 8)) & 1, with bit 0 = top of page
 *
 * The compositor produces a framebuffer in this exact layout; the driver
 * flushes it to the panel.
 */
#ifndef SAYOFW_DISPLAY_DISPLAY_H
#define SAYOFW_DISPLAY_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "sayofw_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t fb_byte_t;

/* Logical pixel set/get. Returns false if x or y out of range. */
bool display_set_pixel(uint8_t *fb, uint16_t x, uint8_t y);
bool display_get_pixel(const uint8_t *fb, uint16_t x, uint8_t y);

/* Bulk ops (optimized). */
void display_clear_all(uint8_t *fb);                       /* zero entire fb */
void display_fill_all(uint8_t *fb);                        /* set every pixel */
void display_fill_rect(uint8_t *fb, uint16_t x0, uint8_t y0,
                       uint16_t x1, uint8_t y1, bool on);  /* inclusive */

/* Draw a 1bpp bitmap (msb-first row order, like the canonical OLED format).
 * `bmp_w` and `bmp_h` may exceed display dims — clipping is automatic. */
void display_draw_bitmap(uint8_t *fb,
                         int16_t x, int8_t y,
                         const uint8_t *bmp,
                         uint16_t bmp_w, uint8_t bmp_h);

/* Compute bounding rect of non-zero page bytes. Used by the dirty-rect
 * flush optimisation. Returns (x0,y0,x1,y1) inclusive. If fb is empty,
 * returns false and rect is zero-filled. */
bool display_compute_dirty(const uint8_t *fb,
                           uint16_t *x0, uint8_t *y0,
                           uint16_t *x1, uint8_t *y1);

/* 5x7 font (ASCII 0x20..0x7E). Single external dep; see display/font.h. */
#include "display/font.h"

void display_draw_char(uint8_t *fb, uint16_t x, uint8_t y, char c);
void display_draw_text(uint8_t *fb, uint16_t x, uint8_t y, const char *s);

/* The framebuffer is a module-private allocation owned by the compositor.
 * This accessor returns it for unit tests. */
uint8_t *display_get_framebuffer(void);
size_t display_get_framebuffer_size(void);

#ifdef __cplusplus
}
#endif

#endif /* SAYOFW_DISPLAY_DISPLAY_H */
