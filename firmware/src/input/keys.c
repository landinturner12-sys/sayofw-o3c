/*
 * src/input/keys.c — Key matrix scanner with debounce.
 *
 * Hardware-agnostic: the actual GPIO reads go through `hal_key_read`
 * (weak). On host tests, `hal_key_read` returns 0 (no keys pressed).
 */
#include "input/keys.h"
#include "sayofw_config.h"

#include <stddef.h>

extern uint32_t sys_tick_ms(void);

__attribute__((weak)) bool hal_key_read(uint8_t idx) { (void)idx; return false; }

#define DEBOUNCE_MS  15U

void keys_init(key_state_t *s)
{
    if (s == NULL) { return; }
    for (uint8_t i = 0U; i < NUM_KEYS; i++) {
        s->pressed[i]         = false;
        s->just_changed[i]    = false;
        s->last_change_ms[i]  = 0U;
    }
}

void keys_scan(key_state_t *s)
{
    if (s == NULL) { return; }
    uint32_t now = sys_tick_ms();
    for (uint8_t i = 0U; i < NUM_KEYS; i++) {
        bool now_pressed = hal_key_read(i);
        if (now_pressed != s->pressed[i]) {
            if ((now - s->last_change_ms[i]) >= DEBOUNCE_MS) {
                s->pressed[i]         = now_pressed;
                s->just_changed[i]    = true;
                s->last_change_ms[i]  = now;
            }
        } else {
            s->just_changed[i] = false;
        }
    }
}

bool keys_consume_press(key_state_t *s, uint8_t idx)
{
    if (s == NULL || idx >= NUM_KEYS) { return false; }
    if (!s->pressed[idx] || !s->just_changed[idx]) { return false; }
    s->just_changed[idx] = false;
    return true;
}

uint16_t keys_default_hid_usage(uint8_t idx)
{
    /* Three O3C keys → volume up / down / mute (consumer page). */
    static const uint16_t map[NUM_KEYS] = { 0x00E9U, 0x00EAU, 0x00E2U };
    return (idx < NUM_KEYS) ? map[idx] : 0U;
}
