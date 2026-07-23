/*
 * hal/hal.h — Hardware Abstraction Layer.
 *
 * All hardware-touching functions are declared here with weak default
 * stubs. The target build (CH32V307) overrides them with real peripheral
 * drivers. Host tests use the default stubs (no-ops / return false).
 *
 * This header consolidates the scattered weak declarations into one place.
 */
#ifndef SAYOFW_HAL_H
#define SAYOFW_HAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== GPIO ===== */
/* Read a key switch state. idx: 0..NUM_KEYS-1. true = pressed. */
bool hal_key_read(uint8_t idx);

/* Write a GPIO pin (used for OLED DC/CS/RST). */
void hal_gpio_write(uint8_t pin, bool high);

/* ===== SPI (OLED) ===== */
/* Send raw bytes over SPI. Returns true on success. */
bool hal_spi_write(const uint8_t *buf, uint16_t len);
/* Set D/C line: false = command, true = data. */
void hal_spi_dc_set(bool data_mode);
/* Set CS line: true = selected (active low driven). */
void hal_spi_cs_set(bool selected);

/* ===== Flash ===== */
bool hal_flash_erase(uint32_t addr, uint32_t len);
bool hal_flash_write(uint32_t addr, const uint8_t *src, uint32_t len);
bool hal_flash_read(uint32_t addr, uint8_t *dst, uint32_t len);

/* ===== Timing ===== */
void hal_delay_ms(uint32_t ms);

/* ===== HID report injection (from macro VM) ===== */
void macro_emit_key(uint8_t modifier, uint16_t keycode, bool press);
void macro_emit_consumer(uint16_t usage);
void macro_emit_mouse(int8_t dx, int8_t dy, uint8_t buttons);

#ifdef __cplusplus
}
#endif

#endif /* SAYOFW_HAL_H */
