/*
 * display/compositor.h — 16-layer display compositor.
 *
 * Layers paint bottom-up; later layers overwrite earlier ones (last write
 * wins, like a Photoshop layer model). Two layer kinds:
 *
 *   COLOR_FILL: solid on/off within a rect
 *   TEXT:       monospace 5x7 ASCII at (x,y), clipped to bounds
 *   BITMAP:     1bpp bitmap blit at (x,y)
 *
 * On any layer write, the dirty region is expanded and the framebuffer is
 * recomposited. On commit, only the dirty region is flushed to the OLED
 * (saves SPI bandwidth).
 */
#ifndef SAYOFW_DISPLAY_COMPOSITOR_H
#define SAYOFW_DISPLAY_COMPOSITOR_H

#include <stdint.h>
#include <stdbool.h>

#include "sayofw_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LAYER_KIND_NONE = 0,
    LAYER_KIND_FILL = 1,
    LAYER_KIND_TEXT = 2,
    LAYER_KIND_BITMAP = 3,
} layer_kind_t;

typedef struct {
    bool        enabled;
    layer_kind_t kind;
    /* source coordinates; bit 15 of y signals "vertically centered" if set
     * (host-push convenience for tiny OLEDs) */
    uint16_t    x;
    uint8_t     y;
    /* kind-specific payload */
    union {
        struct { uint16_t w; uint8_t h; bool on; } fill;
        struct { char text[DISPLAY_TEXT_CHARS + 1]; } text;
        struct {
            uint16_t w; uint8_t h;
            const uint8_t *bmp;   /* borrowed; caller owns the storage */
            uint16_t bmp_stride;
        } bitmap;
    } u;
} layer_t;

typedef struct {
    layer_t layers[DISPLAY_MAX_LAYERS];
    /* dirty region in fb coords; inclusive */
    uint16_t dirty_x0, dirty_x1;
    uint8_t  dirty_y0, dirty_y1;
    bool     has_dirty;
} compositor_t;

/* Lifecycle */
void compositor_init(compositor_t *c);
void compositor_reset(compositor_t *c);

/* Layer ops. `idx` in 0..DISPLAY_MAX_LAYERS-1. Returns true on success. */
bool compositor_set_fill(compositor_t *c, uint8_t idx,
                         uint16_t x, uint8_t y, uint16_t w, uint8_t h, bool on);
bool compositor_set_text(compositor_t *c, uint8_t idx,
                         uint16_t x, uint8_t y, const char *text);
bool compositor_set_bitmap(compositor_t *c, uint8_t idx,
                           uint16_t x, uint8_t y,
                           const uint8_t *bmp, uint16_t w, uint8_t h,
                           uint16_t stride);
bool compositor_disable(compositor_t *c, uint8_t idx);

/* Recomposite into the framebuffer. Caller must call after any layer
 * change. */
void compositor_repaint(compositor_t *c);

/* Mark the entire display dirty (forces full repaint on next flush). */
void compositor_invalidate_all(compositor_t *c);

/* Read back dirty rect. `(x0,y0,x1,y1)` all inclusive. Clears the dirty
 * flag. */
void compositor_take_dirty(compositor_t *c,
                           uint16_t *x0, uint8_t *y0,
                           uint16_t *x1, uint8_t *y1,
                           bool *any);

/* Reset all layers to disabled (used by DisplayClear command). */
void compositor_clear_host_layers(compositor_t *c);

#ifdef __cplusplus
}
#endif

#endif /* SAYOFW_DISPLAY_COMPOSITOR_H */
