/*
 * display/driver.h — Abstract OLED panel driver (SSD1306 / SH1106).
 *
 * The driver owns the SPI bus, command queue, and flush state. Compositor
 * hands it a framebuffer + dirty rect; the driver translates that to panel
 * command sequences.
 *
 * Two implementations may be selected at compile time:
 *   - driver_ssd1306.c (most common 128x32/64 OLED)
 *   - driver_sh1106.c  (some clones; slight page-address quirk)
 *
 * For host tests, a mock driver (driver_mock.c) is compiled and selected
 * via the same interface.
 */
#ifndef SAYOFW_DISPLAY_DRIVER_H
#define SAYOFW_DISPLAY_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "sayofw_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRIVER_CHIP_UNKNOWN = 0,
    DRIVER_CHIP_SSD1306,
    DRIVER_CHIP_SH1106,
} driver_chip_t;

typedef struct {
    driver_chip_t chip;
    uint16_t      width;
    uint8_t       height;
    uint8_t       pages;
} driver_info_t;

/* Lifecycle. Returns true on success. */
bool driver_init(driver_info_t *info_out);

/* Flush `rect` (inclusive page byte rect x0..x1, page y0..y1) within the
 * `fb` framebuffer to the panel. Size of rect must be page-aligned. */
bool driver_flush_rect(const uint8_t *fb,
                       uint16_t x0, uint8_t page_y0,
                       uint16_t x1, uint8_t page_y1);

/* Flush the entire framebuffer. */
bool driver_flush_all(const uint8_t *fb);

/* Turn the panel on/off (sleep mode). */
bool driver_set_power(bool on);

/* Clear the panel RAM (sets all pixels to 0). */
bool driver_clear(void);

/* Read the auto-detected chip (UNKNOWN if init hasn't run). */
driver_chip_t driver_get_chip(void);

/* Compile-time selection. Override with -DDRIVER_BACKEND=driver_mock. */
#ifndef DRIVER_BACKEND
#if DISPLAY_DRIVER_AUTO
#define DRIVER_BACKEND driver_ssd1306
#else
#define DRIVER_BACKEND driver_sh1106
#endif
#endif

extern bool driver_ssd1306_init(driver_info_t *info_out);
extern bool driver_sh1106_init(driver_info_t *info_out);
extern bool driver_mock_init(driver_info_t *info_out);

#ifdef __cplusplus
}
#endif

#endif /* SAYOFW_DISPLAY_DRIVER_H */
