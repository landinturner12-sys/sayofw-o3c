/*
 * tests/display/test_compositor.c — Compositor integration tests.
 *
 * Verifies that setting layers results in the right pixels in the
 * framebuffer, dirty-rect expansion across multiple writes, and clear
 * behavior.
 */
#include "display/compositor.h"
#include "display/display.h"
#include "sayofw_config.h"
#include "protocol/codec.h"
#include "protocol/hid.h"
#include "protocol/commands.h"

#include <stdio.h>
#include <string.h>

/* Stubs for USB */
bool hid_send(const uint8_t *buf, uint16_t len) { (void)buf; (void)len; return true; }
void protocol_set_compositor_dummy(void) {}

static int g_failures = 0;
#define EXPECT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
        g_failures++; \
    } else { \
        printf("  [ OK ] %s\n", msg); \
    } \
} while (0)

static void test_text_layer_renders(void)
{
    compositor_t c;
    compositor_init(&c);
    EXPECT(compositor_set_text(&c, 0, 0, 0, "Hi"), "set text layer 0");
    compositor_repaint(&c);
    EXPECT(display_get_pixel(display_get_framebuffer(), 0, 0), "H on screen");
}

static void test_fill_layer_renders(void)
{
    compositor_t c;
    compositor_init(&c);
    EXPECT(compositor_set_fill(&c, 0, 10, 5, 5, 5, true), "set fill layer");
    compositor_repaint(&c);
    EXPECT(display_get_pixel(display_get_framebuffer(), 10, 5), "fill on");
    EXPECT(display_get_pixel(display_get_framebuffer(), 14, 9), "fill on BR");
}

static void test_layer_overwrite(void)
{
    compositor_t c;
    compositor_init(&c);
    /* Layer 0: text "Hi" at (0,0) */
    (void)compositor_set_text(&c, 0, 0, 0, "Hi");
    /* Layer 1: fill black at (0,0)..(50,10) → overwrites layer 0 */
    (void)compositor_set_fill(&c, 1, 0, 0, 50, 10, false);
    compositor_repaint(&c);
    /* The 'H' pixel (0,0) is in the fill rect, and fill is off → off. */
    EXPECT(!display_get_pixel(display_get_framebuffer(), 0, 0), "overwritten by fill");
}

static void test_dirty_rect_after_layer_change(void)
{
    compositor_t c;
    compositor_init(&c);
    (void)compositor_set_text(&c, 0, 50, 10, "X");
    uint16_t x0, x1; uint8_t y0, y1; bool any;
    compositor_take_dirty(&c, &x0, &y0, &x1, &y1, &any);
    EXPECT(any, "dirty after set_text");
    EXPECT(x0 == 50, "dirty x0");
    EXPECT(y0 == 10, "dirty y0");
    /* After take, has_dirty should clear. */
    compositor_take_dirty(&c, &x0, &y0, &x1, &y1, &any);
    EXPECT(!any, "no dirty after take");
}

static void test_clear_all_layers(void)
{
    compositor_t c;
    compositor_init(&c);
    (void)compositor_set_text(&c, 0, 0, 0, "Hi");
    (void)compositor_set_text(&c, 1, 20, 0, "OK");
    compositor_repaint(&c);
    compositor_clear_host_layers(&c);
    /* After clear and repaint, all pixels should be off. */
    compositor_repaint(&c);
    /* Sample a few known positions. */
    EXPECT(!display_get_pixel(display_get_framebuffer(), 0, 0), "clear: H off");
    EXPECT(!display_get_pixel(display_get_framebuffer(), 20, 0), "clear: O off");
}

int main(void)
{
    printf("=== compositor tests ===\n");
    test_text_layer_renders();
    test_fill_layer_renders();
    test_layer_overwrite();
    test_dirty_rect_after_layer_change();
    test_clear_all_layers();

    if (g_failures == 0) {
        printf("\nAll compositor tests passed.\n");
        return 0;
    }
    printf("\n%d failure(s).\n", g_failures);
    return 1;
}
