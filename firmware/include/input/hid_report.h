/*
 * input/hid_report.h — HID keyboard/consumer/mouse report builder.
 *
 * Builds USB HID reports from key events. The actual USB send is via
 * hid_send() (provided by usb.c).
 */
#ifndef SAYOFW_INPUT_HID_REPORT_H
#define SAYOFW_INPUT_HID_REPORT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Standard 8-byte HID keyboard report (Boot protocol compatible). */
typedef struct {
    uint8_t modifiers;    /* Ctrl/Shift/Alt/GUI bitmask */
    uint8_t reserved;
    uint8_t keys[6];      /* up to 6 simultaneous keycodes */
} __attribute__((packed)) kbd_report_t;

/* 2-byte HID consumer control report (media keys). */
typedef struct {
    uint16_t usage;       /* Consumer usage code (e.g. 0x00E9 = Vol Up) */
} __attribute__((packed)) consumer_report_t;

/* Build and send a keyboard report for current key states.
 * key_state: array of NUM_KEYS booleans (pressed or not).
 * hid_usages: array of NUM_KEYS HID usage codes for each key.
 * Returns true if report was sent successfully. */
bool hid_report_send_keys(const bool *key_state, const uint16_t *hid_usages,
                          uint8_t num_keys);

/* Send a single consumer control usage (media key). usage=0 releases. */
bool hid_report_send_consumer(uint16_t usage);

/* Press or release a specific keycode (used by macro VM). */
bool hid_report_key_event(uint8_t modifier, uint16_t keycode, bool press);

/* Release all keys (emergency clear). */
bool hid_report_release_all(void);

#ifdef __cplusplus
}
#endif

#endif /* SAYOFW_INPUT_HID_REPORT_H */
