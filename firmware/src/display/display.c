/*
 * src/display/display.c — Framebuffer math.
 *
 * Pure pixel/bit operations. Owns the global framebuffer (module-private
 * static), and exposes the layout functions used by the compositor.
 *
 * Coordinate system: x in 0..WIDTH-1, y in 0..HEIGHT-1.
 * Byte layout: page-major; page = y >> 3; within page, bit 0 = top row.
 */
#include "display/display.h"
#include "display/font.h"
#include "sayofw_config.h"

#include <string.h>

static uint8_t g_fb[DISPLAY_FRAMEBUFFER_BYTES];

/* ===== Pixel ops ===== */
bool display_set_pixel(uint8_t *fb, uint16_t x, uint8_t y)
{
    if (fb == NULL || x >= DISPLAY_WIDTH_PX || y >= DISPLAY_HEIGHT_PX) {
        return false;
    }
    const uint16_t page = (uint16_t)(y >> 3);
    const uint8_t  bit  = (uint8_t)(y & 0x07U);
    const uint16_t idx  = (uint16_t)(page * DISPLAY_WIDTH_PX + x);
    fb[idx] |= (uint8_t)(1U << bit);
    return true;
}

bool display_get_pixel(const uint8_t *fb, uint16_t x, uint8_t y)
{
    if (fb == NULL || x >= DISPLAY_WIDTH_PX || y >= DISPLAY_HEIGHT_PX) {
        return false;
    }
    const uint16_t page = (uint16_t)(y >> 3);
    const uint8_t  bit  = (uint8_t)(y & 0x07U);
    const uint16_t idx  = (uint16_t)(page * DISPLAY_WIDTH_PX + x);
    return (fb[idx] & (1U << bit)) != 0U;
}

void display_clear_all(uint8_t *fb)
{
    if (fb != NULL) { memset(fb, 0, DISPLAY_FRAMEBUFFER_BYTES); }
}

void display_fill_all(uint8_t *fb)
{
    if (fb != NULL) { memset(fb, 0xFF, DISPLAY_FRAMEBUFFER_BYTES); }
}

void display_fill_rect(uint8_t *fb, uint16_t x0, uint8_t y0,
                       uint16_t x1, uint8_t y1, bool on)
{
    if (fb == NULL) { return; }
    if (x1 >= DISPLAY_WIDTH_PX)  { x1 = DISPLAY_WIDTH_PX - 1U; }
    if (y1 >= DISPLAY_HEIGHT_PX) { y1 = DISPLAY_HEIGHT_PX - 1U; }
    if (x0 > x1 || y0 > y1) { return; }
    for (uint16_t y = y0; y <= y1; y++) {
        const uint16_t page = (uint16_t)(y >> 3);
        const uint8_t  bit  = (uint8_t)(y & 0x07U);
        for (uint16_t x = x0; x <= x1; x++) {
            const uint16_t idx = (uint16_t)(page * DISPLAY_WIDTH_PX + x);
            if (on) { fb[idx] |=  (uint8_t)(1U << bit); }
            else    { fb[idx] &= (uint8_t)~(1U << bit); }
        }
    }
}

void display_draw_bitmap(uint8_t *fb,
                         int16_t x, int8_t y,
                         const uint8_t *bmp,
                         uint16_t bmp_w, uint8_t bmp_h)
{
    if (fb == NULL || bmp == NULL || bmp_w == 0U || bmp_h == 0U) { return; }
    /* Bitmap is msb-first row order (rows top to bottom). */
    for (uint16_t bx = 0U; bx < bmp_w; bx++) {
        int16_t px = (int16_t)(x + (int16_t)bx);
        if (px < 0 || px >= (int16_t)DISPLAY_WIDTH_PX) { continue; }
        for (uint8_t by = 0U; by < bmp_h; by++) {
            int16_t py = (int16_t)(y + (int16_t)by);
            if (py < 0 || py >= (int16_t)DISPLAY_HEIGHT_PX) { continue; }
            /* Source bit (msb-first per byte). */
            uint16_t row_byte = (uint16_t)((uint16_t)by * ((bmp_w + 7U) / 8U));
            uint16_t row_off  = (uint16_t)(row_byte + (bx >> 3U));
            uint8_t  bit_src  = (uint8_t)(7U - (bx & 0x07U));
            bool on = (bmp[row_off] & (1U << bit_src)) != 0U;
            if (on) {
                (void)display_set_pixel(fb, (uint16_t)px, (uint8_t)py);
            } else {
                /* Erase (set pixel off). */
                const uint16_t page = (uint16_t)(py >> 3);
                const uint8_t  bit  = (uint8_t)(py & 0x07U);
                const uint16_t idx  = (uint16_t)(page * DISPLAY_WIDTH_PX + (uint16_t)px);
                fb[idx] &= (uint8_t)~(1U << bit);
            }
        }
    }
}

bool display_compute_dirty(const uint8_t *fb,
                           uint16_t *x0, uint8_t *y0,
                           uint16_t *x1, uint8_t *y1)
{
    if (fb == NULL) { return false; }
    /* Find first and last non-zero page byte (column-wise) and first/last
     * set bit within those columns. We scan pages independently for speed. */
    uint16_t min_x = DISPLAY_WIDTH_PX;
    uint16_t max_x = 0U;
    uint8_t  min_y = DISPLAY_HEIGHT_PX;
    uint8_t  max_y = 0U;
    for (uint8_t p = 0U; p < DISPLAY_PAGES; p++) {
        for (uint16_t x = 0U; x < DISPLAY_WIDTH_PX; x++) {
            uint8_t b = fb[(uint16_t)(p * DISPLAY_WIDTH_PX) + x];
            if (b == 0U) { continue; }
            if (x < min_x) { min_x = x; }
            if (x > max_x) { max_x = x; }
            for (uint8_t bit = 0U; bit < 8U; bit++) {
                if ((b & (1U << bit)) != 0U) {
                    uint8_t y = (uint8_t)((uint8_t)(p << 3) | bit);
                    if (y < min_y) { min_y = y; }
                    if (y > max_y) { max_y = y; }
                }
            }
        }
    }
    if (max_x < min_x || max_y < min_y) { return false; }
    *x0 = min_x; *x1 = max_x; *y0 = min_y; *y1 = max_y;
    return true;
}

/* ===== Text ===== */
void display_draw_char(uint8_t *fb, uint16_t x, uint8_t y, char c)
{
    if (fb == NULL || x >= DISPLAY_WIDTH_PX || y >= DISPLAY_HEIGHT_PX) { return; }
    if ((uint8_t)c < FONT_FIRST || (uint8_t)c > FONT_LAST) { return; }
    const uint8_t *glyph = font_5x7[(uint8_t)c - FONT_FIRST];
    for (uint8_t col = 0U; col < FONT_GLYPH_W; col++) {
        uint16_t px = (uint16_t)(x + col);
        if (px >= DISPLAY_WIDTH_PX) { break; }
        uint8_t colbits = glyph[col];
        for (uint8_t row = 0U; row < FONT_GLYPH_H; row++) {
            int16_t py = (int16_t)y + (int16_t)row;
            if (py < 0 || py >= (int16_t)DISPLAY_HEIGHT_PX) { continue; }
            bool on = (colbits & (1U << row)) != 0U;
            if (on) {
                (void)display_set_pixel(fb, px, (uint8_t)py);
            } else {
                const uint16_t page = (uint16_t)(py >> 3);
                const uint8_t  bit  = (uint8_t)(py & 0x07U);
                const uint16_t idx  = (uint16_t)(page * DISPLAY_WIDTH_PX + px);
                fb[idx] &= (uint8_t)~(1U << bit);
            }
        }
    }
}

void display_draw_text(uint8_t *fb, uint16_t x, uint8_t y, const char *s)
{
    if (fb == NULL || s == NULL) { return; }
    uint16_t cx = x;
    while (*s != '\0') {
        display_draw_char(fb, cx, y, *s);
        cx = (uint16_t)(cx + FONT_GLYPH_W + 1U);  /* 1px space */
        if (cx >= DISPLAY_WIDTH_PX) { break; }
        s++;
    }
}

/* ===== Framebuffer accessors ===== */
uint8_t *display_get_framebuffer(void) { return g_fb; }
size_t display_get_framebuffer_size(void) { return sizeof(g_fb); }
