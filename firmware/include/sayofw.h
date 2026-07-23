/*
 * sayofw.h — Public API for the Sayobot O3C custom firmware.
 *
 * Single umbrella include for all host-callable firmware APIs. The headers
 * here are designed to be host-testable: implementations are split into
 * "pure" modules (protocol codec, compositor math, display grid) that have
 * no MCU dependencies, and "io" modules (USB, SPI) that delegate to
 * vendor HAL through function pointers / weak symbols.
 *
 * Usage:
 *   #include "sayofw.h"
 *
 *   int main(void) {
 *       sys_init();
 *       display_init();
 *       protocol_init();
 *       for (;;) {
 *           protocol_pump();      // drain incoming HID packets
 *           input_scan();         // scan + debounce keys
 *           display_flush();      // push dirty region to OLED
 *       }
 *   }
 */
#ifndef SAYOFW_H
#define SAYOFW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Version ===== */
#define SAYOFW_VERSION_MAJOR 0
#define SAYOFW_VERSION_MINOR 1
#define SAYOFW_VERSION_PATCH 0

/* ===== Display ===== */
#include "display/display.h"
#include "display/compositor.h"
#include "display/driver.h"

#include "display/font.h"

/* ===== Protocol ===== */
#include "protocol/hid.h"
#include "protocol/codec.h"
#include "protocol/commands.h"

/* ===== Input ===== */
#include "input/keys.h"

/* ===== Storage ===== */
#include "storage/config.h"

/* ===== System init ===== */
void sys_init(void);
uint32_t sys_tick_ms(void);

/* ===== USB hooks (provided by target usb.c, stubbed in host tests) ===== */
typedef struct {
    uint8_t *buf;
    uint16_t len;
} hid_report_t;

/* Receive callback registered by protocol module — fires when host sends
 * a complete v3 packet. Implementations must consume `buf` before returning. */
typedef void (*hid_rx_cb_t)(const uint8_t *buf, uint16_t len, void *user);

/* Send a HID report on usage page 0xFF20 (vendor v3). Non-blocking; returns
 * false if endpoint is busy. Target implementation: usb.c. */
bool hid_send(const uint8_t *buf, uint16_t len);
void hid_on_rx(hid_rx_cb_t cb, void *user);

#ifdef __cplusplus
}
#endif

#endif /* SAYOFW_H */
