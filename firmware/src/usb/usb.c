/*
 * src/usb/usb.c — HID send/receive hooks.
 *
 * On the real target this drives the WCH USBHS device stack. The host
 * test build provides a fake "last_tx" buffer that records the most
 * recent response — used by protocol unit tests to verify dispatch
 * output.
 */
#include "sayofw.h"
#include "sayofw_config.h"

#include <string.h>

static hid_rx_cb_t g_rx_cb   = NULL;
static void       *g_rx_user = NULL;

/* Test-only recording buffer (filled by host tests via usb_test_init) */
#define USB_TEST_TX_MAX 256
static uint8_t  g_last_tx[USB_TEST_TX_MAX];
static uint16_t g_last_tx_len = 0U;

void hid_on_rx(hid_rx_cb_t cb, void *user)
{
    g_rx_cb   = cb;
    g_rx_user = user;
}

bool hid_send(const uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0U) { return false; }
    if (len > USB_TEST_TX_MAX) { return false; }
    memcpy(g_last_tx, buf, len);
    g_last_tx_len = len;
    return true;
}

/* Test introspection */
void usb_test_get_last_tx(uint8_t *dst, uint16_t *len)
{
    if (dst != NULL && len != NULL) {
        memcpy(dst, g_last_tx, g_last_tx_len);
        *len = g_last_tx_len;
    }
}

void usb_test_reset(void)
{
    g_last_tx_len = 0U;
    memset(g_last_tx, 0, sizeof(g_last_tx));
}

void usb_test_inject_rx(const uint8_t *buf, uint16_t len)
{
    if (g_rx_cb != NULL) { g_rx_cb(buf, len, g_rx_user); }
}
