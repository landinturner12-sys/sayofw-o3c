/*
 * storage/config.h — Flash-backed device config.
 *
 * Two banks (A and B) alternate on every save. The other bank is the last
 * "good" config; if a save is interrupted mid-write (power loss), the boot
 * loader or startup code falls back to the older bank. Provides classic
 * A/B wear-leveling with minimal logic.
 *
 * Config layout:
 *   +0x00 magic           4B  'S','F','W','C'
 *   +0x04 version         u32 (currently 1)
 *   +0x08 last_boot_ok    u32  (boot-time stamp / counter)
 *   +0x0C size            u32
 *   +0x10 payload[]       opaque bytes (key map, macros, default display)
 *   +CRC32                u32
 *
 * The HAL flash primitives (`flash_erase`, `flash_write`) are provided by
 * target/hal.c and stubbed in host tests.
 */

#ifndef SAYOFW_STORAGE_CONFIG_H
#define SAYOFW_STORAGE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "sayofw_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_MAGIC      0x43465753U  /* 'CFWS' little-endian */
#define CONFIG_VERSION    1U
#define CONFIG_PAYLOAD_MAX 4096U
#define CONFIG_TOTAL_MAX  (16U + CONFIG_PAYLOAD_MAX + 4U)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t boot_count;
    uint32_t size;
    uint8_t  payload[CONFIG_PAYLOAD_MAX];
    uint32_t crc32;
} __attribute__((packed)) config_blob_t;

/* Initialize the storage layer (find first valid bank or fall back to defaults). */
void storage_init(void);

/* Begin a config edit. Returns a pointer to the live RAM copy. */
config_blob_t *storage_edit(void);

/* Commit the current edit: pick the other bank, erase it, write, verify. */
bool storage_commit(void);

/* Revert edit (drop unsaved changes). */
void storage_revert(void);

/* Read the current effective config payload (NULL if empty). */
const uint8_t *storage_get_payload(size_t *out_size);

/* Target-provided flash primitives (weakly linked in host tests). */
extern bool hal_flash_erase(uint32_t addr, uint32_t len);
extern bool hal_flash_write(uint32_t addr, const uint8_t *src, uint32_t len);
extern bool hal_flash_read(uint32_t addr, uint8_t *dst, uint32_t len);
/* F15 (originally `hal_flash_rase_checked` — typo renamed to
 * `hal_flash_erase_checked`). The weak default delegates to
 * hal_flash_erase; target overrides may add read-back verification. */
extern bool hal_flash_erase_checked(uint32_t addr, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* SAYOFW_STORAGE_CONFIG_H */
