/*
 * tests/display/test_display.c — Host-native tests for the display module.
 *
 * Tests:
 *   - set_pixel / get_pixel round trip
 *   - clear_all / fill_all
 *   - fill_rect clipping
 *   - draw_text "Hello" appears at known coords
 *   - draw_bitmap (a 5x5 filled square at origin)
 *   - compute_dirty
 */
#include "display/display.h"
#include "display/font.h"
#include "sayofw_config.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;
#define EXPECT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
        g_failures++; \
    } else { \
        printf("  [ OK ] %s\n", msg); \
    } \
} while (0)

static void test_set_get_pixel(void)
{
    uint8_t fb[DISPLAY_FRAMEBUFFER_BYTES];
    display_clear_all(fb);
    EXPECT(!display_get_pixel(fb, 5, 7), "empty fb: pixel off");
    EXPECT(display_set_pixel(fb, 5, 7), "set pixel in range");
    EXPECT(display_get_pixel(fb, 5, 7), "pixel now on");
    EXPECT(!display_set_pixel(fb, DISPLAY_WIDTH_PX, 0), "out-of-range rejected");
    EXPECT(!display_set_pixel(fb, 0, DISPLAY_HEIGHT_PX), "out-of-range rejected");
}

static void test_clear_fill_all(void)
{
    uint8_t fb[DISPLAY_FRAMEBUFFER_BYTES];
    display_fill_all(fb);
    /* Every byte == 0xFF */
    bool all = true;
    for (size_t i = 0; i < sizeof(fb); i++) { if (fb[i] != 0xFF) { all = false; break; } }
    EXPECT(all, "fill_all sets every byte to 0xFF");

    display_clear_all(fb);
    all = true;
    for (size_t i = 0; i < sizeof(fb); i++) { if (fb[i] != 0x00) { all = false; break; } }
    EXPECT(all, "clear_all sets every byte to 0x00");
}

static void test_fill_rect(void)
{
    uint8_t fb[DISPLAY_FRAMEBUFFER_BYTES];
    display_clear_all(fb);
    display_fill_rect(fb, 10, 5, 19, 8, true);
    /* 10×4 rect, set; outside should remain clear. */
    EXPECT(display_get_pixel(fb, 10, 5),  "rect TL pixel on");
    EXPECT(display_get_pixel(fb, 19, 8),  "rect BR pixel on");
    EXPECT(!display_get_pixel(fb, 9, 5),  "outside-left off");
    EXPECT(!display_get_pixel(fb, 10, 4), "outside-above off");
    EXPECT(!display_get_pixel(fb, 20, 5), "outside-right off");
    EXPECT(!display_get_pixel(fb, 10, 9), "outside-below off");
}

static void test_draw_text_hello(void)
{
    uint8_t fb[DISPLAY_FRAMEBUFFER_BYTES];
    display_clear_all(fb);
    display_draw_text(fb, 0, 0, "Hi");
    /* 'H' first column has a tall vertical bar (top-left of glyph). */
    EXPECT(display_get_pixel(fb, 0, 0),  "H top-left pixel on");
    EXPECT(display_get_pixel(fb, 0, 6),  "H bottom-left pixel on");
    /* The 'H' glyph's right vertical bar is at column index 4, with
     * bits 6..0 set in font_5x7['H'-0x20][4] = 0x7F. So col=4, row=0..6
     * are on. */
    EXPECT(display_get_pixel(fb, 4, 0),  "H right-bar top pixel on");
}

static void test_draw_bitmap_5x5_square(void)
{
    /* 5x5 all-on bitmap, stride = 1 byte. */
    static const uint8_t bmp[5] = { 0xF8, 0xF8, 0xF8, 0xF8, 0xF8 };
    uint8_t fb[DISPLAY_FRAMEBUFFER_BYTES];
    display_clear_all(fb);
    display_draw_bitmap(fb, 0, 0, bmp, 5, 5);
    /* Top row of the 5x5 → all on */
    for (uint16_t x = 0; x < 5; x++) {
        EXPECT(display_get_pixel(fb, x, 0), "bitmap top row on");
    }
    /* Outside the 5x5 → off */
    EXPECT(!display_get_pixel(fb, 5, 0), "outside-right off");
    EXPECT(!display_get_pixel(fb, 0, 5), "outside-below off");
}

static void test_dirty_rect(void)
{
    uint8_t fb[DISPLAY_FRAMEBUFFER_BYTES];
    display_clear_all(fb);
    uint16_t x0, x1; uint8_t y0, y1;
    bool any = display_compute_dirty(fb, &x0, &y0, &x1, &y1);
    EXPECT(!any, "empty fb → no dirty");

    display_set_pixel(fb, 20, 10);
    any = display_compute_dirty(fb, &x0, &y0, &x1, &y1);
    EXPECT(any, "single pixel → dirty");
    EXPECT(x0 == 20 && x1 == 20, "single pixel x range");
    EXPECT(y0 == 10 && y1 == 10, "single pixel y range");

    display_set_pixel(fb, 5, 25);
    any = display_compute_dirty(fb, &x0, &y0, &x1, &y1);
    EXPECT(any, "two pixels → dirty");
    EXPECT(x0 == 5 && x1 == 20, "two pixels x range");
    EXPECT(y0 == 10 && y1 == 25, "two pixels y range");
}

static void test_set_pixel_far(void)
{
    uint8_t fb[DISPLAY_FRAMEBUFFER_BYTES];
    display_clear_all(fb);
    /* Pixel at (127, 31) — far corner */
    EXPECT(display_set_pixel(fb, 127, 31), "set far corner");
    EXPECT(display_get_pixel(fb, 127, 31), "get far corner");
}



int main(void)
{
    printf("=== display tests ===\n");
    test_set_get_pixel();
    test_clear_fill_all();
    test_fill_rect();
    test_draw_text_hello();
    test_draw_bitmap_5x5_square();
    test_dirty_rect();
    test_set_pixel_far();

    if (g_failures == 0) {
        printf("\nAll display tests passed.\n");
        return 0;
    }
    printf("\n%d failure(s).\n", g_failures);
    return 1;
}
