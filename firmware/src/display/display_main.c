/*
 * src/display/display_main.c — Top-level display glue: init + periodic flush.
 *
 *   display_init()  — driver + compositor bring-up
 *   display_flush() — repaint dirty layers, push to OLED
 *
 * Exposed via the umbrella `sayofw.h` (display_flush / display_init
 * are forward-declared there).
 */
#include "sayofw.h"
#include "sayofw_config.h"
#include "display/display.h"
#include "display/compositor.h"
#include "display/driver.h"

#include <stddef.h>

static compositor_t g_compositor;
static bool         g_inited = false;

extern void protocol_set_compositor(compositor_t *c);

void display_init(void)
{
    if (g_inited) { return; }
    driver_info_t info;
    if (!driver_init(&info)) {
        /* Stay uninit; flush will be a no-op. */
        return;
    }
    compositor_init(&g_compositor);
    /* Hand the compositor to the protocol dispatcher so display commands
     * mutate layer state directly. */
    protocol_set_compositor(&g_compositor);
    /* Initial frame: black. */
    (void)driver_clear();
    g_inited = true;
}

void display_flush(void)
{
    if (!g_inited) { return; }
    compositor_repaint(&g_compositor);

    uint16_t x0, x1; uint8_t y0, y1; bool any;
    compositor_take_dirty(&g_compositor, &x0, &y0, &x1, &y1, &any);

    const uint8_t *fb = display_get_framebuffer();
    if (any) {
        /* Coalesce to page-aligned rect. */
        uint8_t py0 = (uint8_t)(y0 >> 3);
        uint8_t py1 = (uint8_t)(y1 >> 3);
        (void)driver_flush_rect(fb, x0, py0, x1, py1);
    }
}
