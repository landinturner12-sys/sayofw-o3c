/*
 * src/display/driver.c — Frontend: dispatches to SSD1306 or SH1106.
 *
 * The real SPI bus is owned by `target/spi.c` (weak symbol hooks). For
 * host tests, the mock backend fills in identical function pointers so
 * the test can exercise the rest of the stack.
 */
#include "display/driver.h"
#include "display/display.h"
#include "sayofw_config.h"

#include <stddef.h>

/* Forward decls for backend functions (defined in driver_ssd1306.c). */
extern bool driver_ssd1306_init(driver_info_t *info_out);
extern bool driver_sh1106_init(driver_info_t *info_out);
extern bool driver_ssd1306_flush_rect(const uint8_t *fb,
                                      uint16_t x0, uint8_t page_y0,
                                      uint16_t x1, uint8_t page_y1);
extern bool driver_sh1106_flush_rect(const uint8_t *fb,
                                     uint16_t x0, uint8_t page_y0,
                                     uint16_t x1, uint8_t page_y1);
extern bool driver_ssd1306_set_power(bool on);
extern bool driver_sh1106_set_power(bool on);

static driver_info_t g_info = {
    .chip   = DRIVER_CHIP_UNKNOWN,
    .width  = DISPLAY_WIDTH_PX,
    .height = DISPLAY_HEIGHT_PX,
    .pages  = DISPLAY_PAGES,
};

bool driver_init(driver_info_t *info_out)
{
    bool ok;
#if DISPLAY_DRIVER_AUTO
    ok = driver_ssd1306_init(&g_info);
#else
    ok = driver_sh1106_init(&g_info);
#endif
    if (!ok) { return false; }
    if (info_out != NULL) {
        *info_out = g_info;
    }
    return true;
}

bool driver_flush_rect(const uint8_t *fb,
                       uint16_t x0, uint8_t page_y0,
                       uint16_t x1, uint8_t page_y1)
{
    if (fb == NULL) { return false; }
    if (g_info.chip == DRIVER_CHIP_UNKNOWN) { return false; }
    return (g_info.chip == DRIVER_CHIP_SH1106)
         ? driver_sh1106_flush_rect(fb, x0, page_y0, x1, page_y1)
         : driver_ssd1306_flush_rect(fb, x0, page_y0, x1, page_y1);
}

bool driver_flush_all(const uint8_t *fb)
{
    if (fb == NULL) { return false; }
    return driver_flush_rect(fb, 0U, 0U, DISPLAY_WIDTH_PX - 1U, DISPLAY_PAGES - 1U);
}

bool driver_set_power(bool on)
{
    if (g_info.chip == DRIVER_CHIP_SH1106) { return driver_sh1106_set_power(on); }
    return driver_ssd1306_set_power(on);
}

bool driver_clear(void)
{
    uint8_t *fb = display_get_framebuffer();
    display_clear_all(fb);
    return driver_flush_all(fb);
}

driver_chip_t driver_get_chip(void) { return g_info.chip; }
