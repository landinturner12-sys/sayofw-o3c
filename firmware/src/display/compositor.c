/*
 * src/display/compositor.c — 16-layer compositor.
 *
 * Layer model: bottom-up paint; last write wins. The compositor owns
 * `compositor_t` state and writes into the global framebuffer owned by
 * `display.c`. On any layer change, the dirty rect is expanded; on
 * `compositor_flush`, only the dirty rect is pushed to the panel driver.
 */
#include "display/compositor.h"
#include "display/display.h"
#include "sayofw_config.h"

#include <string.h>

void compositor_init(compositor_t *c)
{
    if (c == NULL) { return; }
    memset(c, 0, sizeof(*c));
}

void compositor_reset(compositor_t *c)
{
    if (c == NULL) { return; }
    for (uint8_t i = 0U; i < DISPLAY_MAX_LAYERS; i++) {
        c->layers[i].enabled = false;
        c->layers[i].kind    = LAYER_KIND_NONE;
    }
    c->has_dirty = false;
}

static void expand_dirty(compositor_t *c,
                        uint16_t x0, uint8_t y0, uint16_t x1, uint8_t y1)
{
    if (x0 > x1 || y0 > y1) { return; }
    if (!c->has_dirty) {
        c->dirty_x0 = x0; c->dirty_x1 = x1;
        c->dirty_y0 = y0; c->dirty_y1 = y1;
        c->has_dirty = true;
        return;
    }
    if (x0 < c->dirty_x0) { c->dirty_x0 = x0; }
    if (x1 > c->dirty_x1) { c->dirty_x1 = x1; }
    if (y0 < c->dirty_y0) { c->dirty_y0 = y0; }
    if (y1 > c->dirty_y1) { c->dirty_y1 = y1; }
}

bool compositor_set_fill(compositor_t *c, uint8_t idx,
                         uint16_t x, uint8_t y, uint16_t w, uint8_t h, bool on)
{
    if (c == NULL || idx >= DISPLAY_MAX_LAYERS) { return false; }
    c->layers[idx].enabled = true;
    c->layers[idx].kind    = LAYER_KIND_FILL;
    c->layers[idx].x = x;
    c->layers[idx].y = y;
    c->layers[idx].u.fill.w  = w;
    c->layers[idx].u.fill.h  = h;
    c->layers[idx].u.fill.on = on;
    expand_dirty(c, x, y, (uint16_t)(x + w - 1U), (uint8_t)(y + h - 1U));
    return true;
}

bool compositor_set_text(compositor_t *c, uint8_t idx,
                         uint16_t x, uint8_t y, const char *text)
{
    if (c == NULL || idx >= DISPLAY_MAX_LAYERS) { return false; }
    c->layers[idx].enabled = true;
    c->layers[idx].kind    = LAYER_KIND_TEXT;
    c->layers[idx].x = x;
    c->layers[idx].y = y;
    if (text != NULL) {
        size_t n = 0U;
        while (text[n] != '\0' && n < DISPLAY_TEXT_CHARS) {
            c->layers[idx].u.text.text[n] = text[n];
            n++;
        }
        c->layers[idx].u.text.text[n] = '\0';
    } else {
        c->layers[idx].u.text.text[0U] = '\0';
    }
    /* Approx text bbox: chars × 6 px wide, 7 px tall. */
    size_t len = (text != NULL) ? strlen(text) : 0U;
    if (len > DISPLAY_TEXT_CHARS) { len = DISPLAY_TEXT_CHARS; }
    uint16_t w = (uint16_t)(len * 6U);
    expand_dirty(c, x, y, (uint16_t)(x + w - 1U), (uint8_t)(y + 6U));
    return true;
}

bool compositor_set_bitmap(compositor_t *c, uint8_t idx,
                           uint16_t x, uint8_t y,
                           const uint8_t *bmp, uint16_t w, uint8_t h,
                           uint16_t stride)
{
    if (c == NULL || idx >= DISPLAY_MAX_LAYERS) { return false; }
    if (bmp == NULL || w == 0U || h == 0U) { return false; }
    c->layers[idx].enabled = true;
    c->layers[idx].kind    = LAYER_KIND_BITMAP;
    c->layers[idx].x = x;
    c->layers[idx].y = y;
    c->layers[idx].u.bitmap.w         = w;
    c->layers[idx].u.bitmap.h         = h;
    c->layers[idx].u.bitmap.bmp_stride = stride;
    /* Copy the bitmap into compositor-owned storage. The previous
     * implementation stored the caller's pointer, which becomes a
     * dangling reference once the USB RX buffer is recycled (use-after-
     * free on every repaint). See code review finding F2. */
    uint16_t row_bytes = (uint16_t)((w + 7U) / 8U);
    size_t   total     = (size_t)row_bytes * (size_t)h;
    if (total > sizeof(c->layers[idx].u.bitmap.bmp)) {
        /* Truncate to fit; the caller sent too much. We still accept and
         * render whatever fits, rather than fail the whole packet. */
        total = sizeof(c->layers[idx].u.bitmap.bmp);
    }
    memcpy(c->layers[idx].u.bitmap.bmp, bmp, total);
    /* store effective stride (row_bytes) so the renderer can rely on it */
    c->layers[idx].u.bitmap.bmp_stride = row_bytes;
    expand_dirty(c, x, y, (uint16_t)(x + w - 1U), (uint8_t)(y + h - 1U));
    return true;
}

bool compositor_disable(compositor_t *c, uint8_t idx)
{
    if (c == NULL || idx >= DISPLAY_MAX_LAYERS) { return false; }
    if (c->layers[idx].enabled) {
        expand_dirty(c, c->layers[idx].x, c->layers[idx].y,
                     (uint16_t)(c->layers[idx].x + 5U),
                     (uint8_t)(c->layers[idx].y + 7U));
    }
    c->layers[idx].enabled = false;
    c->layers[idx].kind    = LAYER_KIND_NONE;
    return true;
}

void compositor_repaint(compositor_t *c)
{
    if (c == NULL) { return; }
    uint8_t *fb = display_get_framebuffer();
    if (c->has_dirty) {
        /* Erase the dirty region before re-painting. */
        display_fill_rect(fb,
            c->dirty_x0, c->dirty_y0,
            c->dirty_x1, c->dirty_y1, false);
    } else {
        /* No dirty rect → fresh paint: clear everything once. */
        display_clear_all(fb);
    }
    for (uint8_t i = 0U; i < DISPLAY_MAX_LAYERS; i++) {
        if (!c->layers[i].enabled) { continue; }
        switch (c->layers[i].kind) {
        case LAYER_KIND_FILL: {
            const layer_t *L = &c->layers[i];
            display_fill_rect(fb, L->x, L->y,
                              (uint16_t)(L->x + L->u.fill.w - 1U),
                              (uint8_t)(L->y + L->u.fill.h - 1U),
                              L->u.fill.on);
            break;
        }
        case LAYER_KIND_TEXT: {
            const layer_t *L = &c->layers[i];
            display_draw_text(fb, L->x, L->y, L->u.text.text);
            break;
        }
        case LAYER_KIND_BITMAP: {
            const layer_t *L = &c->layers[i];
            display_draw_bitmap(fb, (int16_t)L->x, (int8_t)L->y,
                                L->u.bitmap.bmp,
                                L->u.bitmap.w, L->u.bitmap.h);
            break;
        }
        default: break;
        }
    }
}

void compositor_invalidate_all(compositor_t *c)
{
    if (c == NULL) { return; }
    c->dirty_x0 = 0U; c->dirty_x1 = DISPLAY_WIDTH_PX - 1U;
    c->dirty_y0 = 0U; c->dirty_y1 = DISPLAY_HEIGHT_PX - 1U;
    c->has_dirty = true;
}

void compositor_take_dirty(compositor_t *c,
                           uint16_t *x0, uint8_t *y0,
                           uint16_t *x1, uint8_t *y1,
                           bool *any)
{
    if (c == NULL) { return; }
    if (any  != NULL) { *any  = c->has_dirty; }
    if (x0   != NULL) { *x0   = c->dirty_x0; }
    if (y0   != NULL) { *y0   = c->dirty_y0; }
    if (x1   != NULL) { *x1   = c->dirty_x1; }
    if (y1   != NULL) { *y1   = c->dirty_y1; }
    c->has_dirty = false;
}

void compositor_clear_host_layers(compositor_t *c)
{
    if (c == NULL) { return; }
    /* For the O3C all 16 layers are host-pushable; there are no
     * reserved device-side layers. (If a future device allocates
     * device-side layers in 0..7, change DISPLAY_HOST_LAYER_START.) */
    for (uint8_t i = 0U; i < DISPLAY_MAX_LAYERS; i++) {
        (void)compositor_disable(c, i);
    }
    compositor_invalidate_all(c);
}
