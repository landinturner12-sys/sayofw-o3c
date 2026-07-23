/*
 * src/display/driver_ssd1306.c — SSD1306 driver.
 *
 * SPI commands used (subset):
 *   0xAE / 0xAF — display OFF / ON
 *   0x20 0x00   — memory addressing mode = horizontal
 *   0x21 A B    — column address range [A..B] inclusive
 *   0x22 A B    — page address range [A..B] inclusive
 *   0x7E        — normal (non-inverted)
 *   0xA8 H      — multiplex ratio = H+1
 *   0x8D 0x14   — enable charge pump (required when VCC = 3.3V)
 *   0xB0..B7    — set page start address (when in page mode)
 *   0x00..0x0F  — set lower column nibble
 *   0x10..0x1F  — set upper column nibble
 *
 * On the target, this is compiled against the vendor SDK; the SPI bus
 * writes are routed through `hal_spi_write` (weak). On host tests,
 * `hal_spi_write` is a recording stub and the whole driver is exercised
 * via the existing framebuffer.
 */
#include "display/driver.h"
#include "display/display.h"
#include "sayofw_config.h"

#include <string.h>

/* ===== SPI hooks (weak) ===== */
__attribute__((weak)) bool hal_spi_write(const uint8_t *buf, uint16_t len) { (void)buf; (void)len; return false; }
__attribute__((weak)) void hal_spi_dc_set(bool data_mode)   { (void)data_mode; }
__attribute__((weak)) void hal_spi_cs_set(bool selected)    { (void)selected; }
__attribute__((weak)) void hal_delay_ms(uint32_t ms)         { (void)ms; }

/* ===== Command helpers ===== */
static bool cmd_byte(uint8_t b)
{
    hal_spi_dc_set(false);
    hal_spi_cs_set(true);
    bool ok = hal_spi_write(&b, 1U);
    hal_spi_cs_set(false);
    return ok;
}

static bool cmd_seq(const uint8_t *p, uint16_t n)
{
    hal_spi_dc_set(false);
    hal_spi_cs_set(true);
    bool ok = hal_spi_write(p, n);
    hal_spi_cs_set(false);
    return ok;
}

static bool data_seq(const uint8_t *p, uint16_t n)
{
    hal_spi_dc_set(true);
    hal_spi_cs_set(true);
    bool ok = hal_spi_write(p, n);
    hal_spi_cs_set(false);
    return ok;
}

/* ===== Init sequence ===== */
bool driver_ssd1306_init(driver_info_t *info_out)
{
    static const uint8_t init_cmds[] = {
        0xAE,            /* display off */
        0xD5, 0x80,      /* set display clock: default ratio */
        0xA8, (uint8_t)(DISPLAY_HEIGHT_PX - 1U),  /* multiplex */
        0xD3, 0x00,      /* display offset = 0 */
        0x40,            /* start line = 0 */
        0x8D, 0x14,      /* enable charge pump */
        0x20, 0x00,      /* horizontal addressing mode */
        0xA1,            /* segment remap (mirror X) */
        0xC8,            /* COM output scan remap (mirror Y) */
        0xDA, 0x02,      /* COM pins: alt for 32px */
        0x81, 0x8F,      /* contrast */
        0xD9, 0xF1,      /* pre-charge */
        0xDB, 0x40,      /* VCOMH deselect level */
        0xA4,            /* display on (resume to RAM) */
        0xA6,            /* normal (non-inverted) */
        0xAF,            /* display on */
    };
    for (uint8_t i = 0U; i < (uint8_t)sizeof(init_cmds); i++) {
        if (!cmd_byte(init_cmds[i])) { return false; }
    }
    hal_delay_ms(100U);
    if (info_out != NULL) {
        info_out->chip   = DRIVER_CHIP_SSD1306;
        info_out->width  = DISPLAY_WIDTH_PX;
        info_out->height = DISPLAY_HEIGHT_PX;
        info_out->pages  = DISPLAY_PAGES;
    }
    return true;
}

bool driver_ssd1306_flush_rect(const uint8_t *fb,
                               uint16_t x0, uint8_t page_y0,
                               uint16_t x1, uint8_t page_y1)
{
    if (fb == NULL) { return false; }
    if (x1 >= DISPLAY_WIDTH_PX)  { x1 = DISPLAY_WIDTH_PX - 1U; }
    if (page_y1 >= DISPLAY_PAGES) { page_y1 = DISPLAY_PAGES - 1U; }
    if (x0 > x1 || page_y0 > page_y1) { return false; }
    /* Set column range */
    uint8_t col_seq[3] = {
        (uint8_t)0x21,
        (uint8_t)(x0 & 0x7FU),
        (uint8_t)(x1 & 0x7FU)
    };
    if (!cmd_seq(col_seq, 3U)) { return false; }
    /* Set page range */
    uint8_t page_seq[3] = {
        (uint8_t)0x22,
        (uint8_t)(page_y0 & 0x07U),
        (uint8_t)(page_y1 & 0x07U)
    };
    if (!cmd_seq(page_seq, 3U)) { return false; }
    /* Write data pages */
    for (uint8_t p = page_y0; p <= page_y1; p++) {
        const uint8_t *row = &fb[(uint16_t)(p * DISPLAY_WIDTH_PX) + x0];
        uint16_t w = (uint16_t)(x1 - x0 + 1U);
        if (!data_seq(row, w)) { return false; }
    }
    return true;
}

bool driver_ssd1306_set_power(bool on)
{
    return cmd_byte(on ? 0xAFU : 0xAEU);
}

/* ===== SH1106 = same protocol with one quirk: page addressing only, so
 *        we have to write 1 page at a time. Reuse ssd1306 internals. ===== */
bool driver_sh1106_init(driver_info_t *info_out)
{
    /* SH1106 init is similar but with a 0xB0 page set + 0x0n/0x1n col high/low
     * before each data write. Init register values match SSD1306 closely. */
    static const uint8_t init_cmds[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF,
    };
    for (uint8_t i = 0U; i < (uint8_t)sizeof(init_cmds); i++) {
        if (!cmd_byte(init_cmds[i])) { return false; }
    }
    hal_delay_ms(100U);
    if (info_out != NULL) {
        info_out->chip   = DRIVER_CHIP_SH1106;
        info_out->width  = DISPLAY_WIDTH_PX;
        info_out->height = DISPLAY_HEIGHT_PX;
        info_out->pages  = DISPLAY_PAGES;
    }
    return true;
}

bool driver_sh1106_flush_rect(const uint8_t *fb,
                              uint16_t x0, uint8_t page_y0,
                              uint16_t x1, uint8_t page_y1)
{
    if (fb == NULL) { return false; }
    if (x1 >= DISPLAY_WIDTH_PX)   { x1 = DISPLAY_WIDTH_PX - 1U; }
    if (page_y1 >= DISPLAY_PAGES) { page_y1 = DISPLAY_PAGES - 1U; }
    /* SH1106 has a 2-pixel column offset: data at column 0 is on screen
     * at column 2. We compensate by adding 2 to the start column. */
    for (uint8_t p = page_y0; p <= page_y1; p++) {
        uint8_t hdr[3] = {
            (uint8_t)(0xB0U | (p & 0x07U)),                              /* page */
            (uint8_t)(0x10U | (((x0 + 2U) >> 4) & 0x0FU)),               /* col hi */
            (uint8_t)(((x0 + 2U) & 0x0FU))                              /* col lo */
        };
        if (!cmd_seq(hdr, 3U)) { return false; }
        const uint8_t *row = &fb[(uint16_t)(p * DISPLAY_WIDTH_PX) + x0];
        uint16_t w = (uint16_t)(x1 - x0 + 1U);
        if (!data_seq(row, w)) { return false; }
    }
    return true;
}

bool driver_sh1106_set_power(bool on)
{
    return cmd_byte(on ? 0xAFU : 0xAEU);
}

/* ===== Mock backend (host tests) ===== */
bool driver_mock_init(driver_info_t *info_out)
{
    if (info_out != NULL) {
        info_out->chip   = DRIVER_CHIP_SSD1306;
        info_out->width  = DISPLAY_WIDTH_PX;
        info_out->height = DISPLAY_HEIGHT_PX;
        info_out->pages  = DISPLAY_PAGES;
    }
    return true;
}
