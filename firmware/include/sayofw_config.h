/*
 * sayofw_config.h — Compile-time configuration
 *
 * Centralizes tunables so the same code can target different hardware
 * revisions and OLED panels without conditionals scattered through the
 * codebase. Tuned for the O3C default (128x32 SSD1306), but easy to
 * override at build time with -D flags.
 */
#ifndef SAYOFW_CONFIG_H
#define SAYOFW_CONFIG_H

/* ===== MCU identity ===== */
#ifndef MCU
#define MCU                        "CH32V307"
#endif
#ifndef MCU_FREQ_HZ
#define MCU_FREQ_HZ                144000000U
#endif

/* ===== Display ===== */
#ifndef DISPLAY_WIDTH_PX
#define DISPLAY_WIDTH_PX           128U
#endif
#ifndef DISPLAY_HEIGHT_PX
#define DISPLAY_HEIGHT_PX          32U
#endif
#ifndef DISPLAY_PAGES
/* Pages = ceil(height / 8). For 32px → 4 pages. */
#define DISPLAY_PAGES              ((DISPLAY_HEIGHT_PX + 7U) / 8U)
#endif
#ifndef DISPLAY_FRAMEBUFFER_BYTES
#define DISPLAY_FRAMEBUFFER_BYTES  (DISPLAY_WIDTH_PX * DISPLAY_PAGES)
#endif

/* SSD1306/SH1106 dual support. Driver auto-detects at init. */
#ifndef DISPLAY_DRIVER_AUTO
#define DISPLAY_DRIVER_AUTO        1
#endif
#ifndef DISPLAY_DRIVER_FORCE
/* 0 = SSD1306, 1 = SH1106; only used when DISPLAY_DRIVER_AUTO == 0 */
#define DISPLAY_DRIVER_FORCE       0
#endif

/* ===== Compositor ===== */
#ifndef DISPLAY_MAX_LAYERS
#define DISPLAY_MAX_LAYERS         16U
#endif
#ifndef DISPLAY_TEXT_CHARS
#define DISPLAY_TEXT_CHARS         32U   /* max chars in a text layer */
#endif
#ifndef DISPLAY_DIRTY_RECT
#define DISPLAY_DIRTY_RECT         1U    /* track dirty rects to limit SPI bytes */
#endif

/* ===== Protocol ===== */
#ifndef PROTOCOL_USAGE_PAGE
#define PROTOCOL_USAGE_PAGE        0xFF20U
#endif
#ifndef PROTOCOL_USAGE_V3
#define PROTOCOL_USAGE_V3           0x80U  /* OR with page for v3 */
#endif
#ifndef PROTOCOL_PACKET_SIZE
#define PROTOCOL_PACKET_SIZE        64U
#endif
#ifndef PROTOCOL_HS_PACKET_SIZE
#define PROTOCOL_HS_PACKET_SIZE     1024U
#endif
#ifndef PROTOCOL_RX_RING_SIZE
#define PROTOCOL_RX_RING_SIZE       16U
#endif

/* ===== Macro engine ===== */
#ifndef MACRO_SLOTS
#define MACRO_SLOTS                256U
#endif
#ifndef MACRO_STEPS_PER_SLOT
#define MACRO_STEPS_PER_SLOT       24U
#endif
#ifndef MACRO_TOTAL_STEPS
#define MACRO_TOTAL_STEPS          (MACRO_SLOTS * MACRO_STEPS_PER_SLOT)  /* 6144 */
#endif

/* ===== Flash config ===== */
#ifndef CONFIG_BANK_SIZE
#define CONFIG_BANK_SIZE           (64U * 1024U)
#endif
#ifndef CONFIG_NUM_BANKS
#define CONFIG_NUM_BANKS           2U     /* dual-bank A/B wear-leveled */
#endif
#ifndef CONFIG_BASE_ADDR
#define CONFIG_BASE_ADDR           0x0801A000U
#endif

#endif /* SAYOFW_CONFIG_H */
