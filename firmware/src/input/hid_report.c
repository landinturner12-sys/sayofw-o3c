/*
 * src/input/hid_report.c — HID report builder.
 *
 * Maintains the current keyboard report state and sends via hid_send().
 * For the O3C's 3 keys, consumer reports (media keys) are the default.
 */
#include "input/hid_report.h"
#include "sayofw.h"
#include <string.h>

/* Current keyboard report state (persistent across calls). */
static kbd_report_t g_kbd_report;

bool hid_report_send_keys(const bool *key_state, const uint16_t *hid_usages,
                          uint8_t num_keys)
{
    if (!key_state || !hid_usages) return false;
    /* O3C keys are consumer-page by default; send as consumer reports. */
    for (uint8_t i = 0; i < num_keys; i++) {
        if (key_state[i]) {
            return hid_report_send_consumer(hid_usages[i]);
        }
    }
    /* No key pressed: release */
    return hid_report_send_consumer(0);
}

bool hid_report_send_consumer(uint16_t usage)
{
    consumer_report_t rpt = { .usage = usage };
    /* Consumer reports use a different report ID in a real USB stack.
     * For now, send as raw bytes via hid_send. */
    return hid_send((const uint8_t *)&rpt, sizeof(rpt));
}

bool hid_report_key_event(uint8_t modifier, uint16_t keycode, bool press)
{
    if (press) {
        g_kbd_report.modifiers |= modifier;
        /* Find empty slot */
        for (uint8_t i = 0; i < 6; i++) {
            if (g_kbd_report.keys[i] == 0 || g_kbd_report.keys[i] == (uint8_t)keycode) {
                g_kbd_report.keys[i] = (uint8_t)keycode;
                break;
            }
        }
    } else {
        g_kbd_report.modifiers &= (uint8_t)~modifier;
        for (uint8_t i = 0; i < 6; i++) {
            if (g_kbd_report.keys[i] == (uint8_t)keycode) {
                g_kbd_report.keys[i] = 0;
                break;
            }
        }
    }
    return hid_send((const uint8_t *)&g_kbd_report, sizeof(g_kbd_report));
}

bool hid_report_release_all(void)
{
    memset(&g_kbd_report, 0, sizeof(g_kbd_report));
    return hid_send((const uint8_t *)&g_kbd_report, sizeof(g_kbd_report));
}
