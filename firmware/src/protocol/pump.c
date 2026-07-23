/*
 * src/protocol/pump.c — RX pump (USB HID reports → dispatcher).
 *
 * The actual USB stack pushes a complete 64/1024-byte report to the
 * `hid_on_rx` callback registered in main(). That callback is wired here.
 *
 * This is the "front end" glue — it knows nothing about codec internals.
 */
#include "sayofw.h"
#include "protocol/codec.h"

#include <stddef.h>

static hid_rx_cb_t g_rx_cb = NULL;
static void       *g_rx_user = NULL;


void protocol_init(void);
void protocol_pump(void);

static void on_report(const uint8_t *buf, uint16_t len, void *user)
{
    (void)user;
    extern void protocol_dispatch(const uint8_t *pkt, uint16_t len);
    protocol_dispatch(buf, len);
}

void protocol_init(void)
{
    hid_on_rx(on_report, NULL);
}

void protocol_pump(void)
{
    /* On target: USB stack drains in its own ISR and calls on_report. The
     * pump is a hook for future commands that need to run synchronously
     * (e.g. polling a CTS pin, draining an event queue). For now: no-op. */
}

/* ===== Weak default for bootloader-entry hook (target overrides) ===== */
