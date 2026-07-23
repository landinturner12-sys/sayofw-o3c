/*
 * input/keys.h — Key matrix scanner + debouncer.
 *
 * The O3C has 3 mechanical keys. We expose a simple polling API for now;
 * an interrupt-driven scanner can swap in without changing the public API.
 */

#ifndef SAYOFW_INPUT_KEYS_H
#define SAYOFW_INPUT_KEYS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_KEYS  3U

typedef struct {
    bool pressed[NUM_KEYS];
    bool just_changed[NUM_KEYS];
    uint32_t last_change_ms[NUM_KEYS];
} key_state_t;

/* Initialize hardware (GPIO, optional EXTI). Safe to call once at boot. */
void keys_init(key_state_t *s);

/* Poll the matrix; updates debouncer state. Call from main loop. */
void keys_scan(key_state_t *s);

/* Return true exactly once per press, then false until release+press. */
bool keys_consume_press(key_state_t *s, uint8_t idx);

/* Map a key index to a HID usage (consumer ctrl / kbd). Defaults are
 * sensible; the macro engine overrides these at runtime. */
uint16_t keys_default_hid_usage(uint8_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SAYOFW_INPUT_KEYS_H */
