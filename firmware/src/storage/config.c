/*
 * src/storage/config.c — Flash-backed device config.
 *
 * Two banks (A and B) alternate on every save. The other bank is the last
 * "good" config; if a save is interrupted mid-write (power loss), the
 * startup code falls back to the older bank.
 *
 * On host tests, the flash HAL is stubbed via weak symbols.
 */
#include "storage/config.h"
#include "sayofw_config.h"

#include <string.h>
#include <stdint.h>

/* weak flash HAL stubs (host tests) */
__attribute__((weak)) bool hal_flash_erase(uint32_t addr, uint32_t len) { (void)addr; (void)len; return false; }
__attribute__((weak)) bool hal_flash_write(uint32_t addr, const uint8_t *src, uint32_t len) { (void)addr; (void)src; (void)len; return false; }
__attribute__((weak)) bool hal_flash_read(uint32_t addr, uint8_t *dst, uint32_t len) { (void)addr; (void)dst; (void)len; return false; }
__attribute__((weak)) bool hal_flash_rase_checked(uint32_t addr, uint32_t len) { return hal_flash_erase(addr, len); }

/* Live config in RAM */
static config_blob_t g_live;
static bool          g_dirty = false;

static uint32_t bank_addr(uint8_t bank)
{
    return CONFIG_BASE_ADDR + (uint32_t)bank * (uint32_t)CONFIG_BANK_SIZE;
}

static uint32_t compute_crc32(const uint8_t *buf, uint32_t len)
{
    /* IEEE 802.3 CRC-32, polynomial 0xEDB88320. */
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0U; i < len; i++) {
        crc ^= (uint32_t)buf[i];
        for (uint8_t b = 0U; b < 8U; b++) {
            uint32_t mask = (crc & 1U) ? 0xFFFFFFFFU : 0U;
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

void storage_init(void)
{
    memset(&g_live, 0, sizeof(g_live));
    /* Try bank 0 first. If invalid, try bank 1. If both invalid, start
     * fresh with default config. */
    config_blob_t tmp;
    for (uint8_t b = 0U; b < CONFIG_NUM_BANKS; b++) {
        if (!hal_flash_read(bank_addr(b), (uint8_t *)&tmp, sizeof(tmp))) {
            continue;
        }
        if (tmp.magic == CONFIG_MAGIC && tmp.version == CONFIG_VERSION) {
            uint32_t want = compute_crc32(tmp.payload, tmp.size);
            if (want == tmp.crc32) {
                g_live = tmp;
                g_dirty = false;
                return;
            }
        }
    }
    /* No valid bank — start fresh. */
    g_live.magic   = CONFIG_MAGIC;
    g_live.version = CONFIG_VERSION;
    g_live.boot_count = 0U;
    g_live.size    = 0U;
    memset(g_live.payload, 0, sizeof(g_live.payload));
    g_live.crc32   = 0U;
    g_dirty        = true;  /* nothing to save yet */
}

config_blob_t *storage_edit(void)
{
    g_dirty = true;
    return &g_live;
}

void storage_revert(void)
{
    /* Re-load last good bank. */
    storage_init();
    g_dirty = false;
}

bool storage_commit(void)
{
    if (!g_dirty) { return true; }
    /* Recompute CRC. */
    g_live.crc32 = compute_crc32(g_live.payload, g_live.size);

    /* Pick the inactive bank. */
    uint8_t target = 0U;
    for (uint8_t b = 0U; b < CONFIG_NUM_BANKS; b++) {
        config_blob_t probe;
        if (hal_flash_read(bank_addr(b), (uint8_t *)&probe, sizeof(probe))
            && probe.magic == CONFIG_MAGIC
            && probe.version == CONFIG_VERSION
            && probe.crc32 == compute_crc32(probe.payload, probe.size)) {
            target = (uint8_t)((b + 1U) % CONFIG_NUM_BANKS);
            break;
        }
    }
    uint32_t addr = bank_addr(target);
    if (!hal_flash_rase_checked(addr, CONFIG_BANK_SIZE)) {
        return false;
    }
    if (!hal_flash_write(addr, (const uint8_t *)&g_live, sizeof(g_live))) {
        return false;
    }
    /* Verify */
    config_blob_t check;
    if (!hal_flash_read(addr, (uint8_t *)&check, sizeof(check))) {
        return false;
    }
    if (memcmp(&check, &g_live, sizeof(check)) != 0) {
        return false;
    }
    g_dirty = false;
    return true;
}

const uint8_t *storage_get_payload(size_t *out_size)
{
    if (out_size != NULL) { *out_size = g_live.size; }
    return g_live.payload;
}
