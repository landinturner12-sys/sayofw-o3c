/*
 * tests/flasher/test_dispatch_text.c — End-to-end dispatch test.
 *
 * Builds a "DisplayText" HID packet by hand, feeds it through the
 * dispatch path, and verifies the compositor received the text on the
 * expected layer.
 */
#include "protocol/codec.h"
#include "protocol/hid.h"
#include "protocol/commands.h"
#include "display/compositor.h"
#include "display/display.h"
#include "sayofw.h"

#include <stdio.h>
#include <string.h>

/* Stubs for USB (host test build) */
static uint8_t  g_last_tx[256];
static uint16_t g_last_tx_len = 0U;

bool hid_send(const uint8_t *buf, uint16_t len)
{
    if (len > sizeof(g_last_tx)) { return false; }
    memcpy(g_last_tx, buf, len);
    g_last_tx_len = len;
    return true;
}

static int g_failures = 0;
#define EXPECT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
        g_failures++; \
    } else { \
        printf("  [ OK ] %s\n", msg); \
    } \
} while (0)

/* Forward decls from firmware source */
void protocol_dispatch(const uint8_t *pkt, uint16_t len);
void protocol_set_compositor(compositor_t *c);

static void build_display_text_packet(uint8_t *pkt, uint8_t layer,
                                      uint8_t x, uint8_t y, const char *text)
{
    memset(pkt, 0, 64);
    pkt[HID_OFF_REPORT_ID] = HID_REPORT_ID_NORMAL;
    pkt[HID_OFF_ECHO]      = HID_ECHO;
    size_t tlen = strlen(text);
    if (tlen > 32) { tlen = 32; }
    uint16_t cmd_len = (uint16_t)(HID_CMD_HEADER_SIZE + 3U + tlen);
    uint8_t *cmd = &pkt[HID_OFF_CMDS];
    cmd[0U] = (uint8_t)(cmd_len & 0xFFU);
    cmd[1U] = (uint8_t)((cmd_len >> 8) & 0xFFU);
    cmd[2U] = CMD_V3_DISPLAY_TEXT;
    cmd[3U] = 0x01;  /* index */
    cmd[4U] = layer;
    cmd[5U] = x;
    cmd[6U] = y;
    memcpy(&cmd[7U], text, tlen);
    (void)codec_finalize_v3(pkt, 64);
}

static void test_display_text_push(void)
{
    compositor_t c;
    compositor_init(&c);
    protocol_set_compositor(&c);

    uint8_t pkt[64];
    build_display_text_packet(pkt, 0, 0, 0, "Hi");
    protocol_dispatch(pkt, 64);

    /* The compositor should have a text layer 0 = "Hi" */
    EXPECT(c.layers[0].enabled, "layer 0 enabled");
    EXPECT(c.layers[0].kind == LAYER_KIND_TEXT, "layer 0 is text");
    EXPECT(strcmp(c.layers[0].u.text.text, "Hi") == 0, "layer 0 text is 'Hi'");

    /* Response was sent (just verify it was sent and has CMD_V3_DISPLAY_TEXT id) */
    EXPECT(g_last_tx_len > 0U, "response sent");
    EXPECT(g_last_tx[HID_OFF_CMDS + 2U] == CMD_V3_DISPLAY_TEXT, "response id");
}

static void test_ping_echo(void)
{
    uint8_t pkt[64];
    memset(pkt, 0, 64);
    pkt[HID_OFF_REPORT_ID] = HID_REPORT_ID_NORMAL;
    pkt[HID_OFF_ECHO]      = HID_ECHO;
    uint8_t *cmd = &pkt[HID_OFF_CMDS];
    cmd[0U] = 6U; cmd[1U] = 0U;
    cmd[2U] = CMD_V3_PING;
    cmd[3U] = 0x07;
    cmd[4U] = 0xCA; cmd[5U] = 0xFE;
    (void)codec_finalize_v3(pkt, 64);
    g_last_tx_len = 0U;

    protocol_dispatch(pkt, 64);
    EXPECT(g_last_tx_len > 0U, "ping sent response");
    EXPECT(g_last_tx[HID_OFF_CMDS + 2U] == CMD_V3_PING, "ping response id");
    EXPECT(g_last_tx[HID_OFF_CMDS + 4U] == 0xCA, "ping response token lo");
    EXPECT(g_last_tx[HID_OFF_CMDS + 5U] == 0xFE, "ping response token hi");
}

static void test_display_clear(void)
{
    compositor_t c;
    compositor_init(&c);
    protocol_set_compositor(&c);
    (void)compositor_set_text(&c, 0, 0, 0, "Hello");

    uint8_t pkt[64];
    memset(pkt, 0, 64);
    pkt[0U] = 0x21; pkt[1U] = 0x03;
    uint8_t *cmd = &pkt[4];
    cmd[0U] = 4U; cmd[1U] = 0U;
    cmd[2U] = CMD_V3_DISPLAY_CLEAR;
    cmd[3U] = 0x00;
    (void)codec_finalize_v3(pkt, 64);

    protocol_dispatch(pkt, 64);
    EXPECT(!c.layers[0].enabled, "layer 0 disabled by DisplayClear");
    EXPECT(c.has_dirty, "DisplayClear marks full dirty");
}

int main(void)
{
    printf("=== dispatch tests ===\n");
    test_display_text_push();
    test_ping_echo();
    test_display_clear();

    if (g_failures == 0) {
        printf("\nAll dispatch tests passed.\n");
        return 0;
    }
    printf("\n%d failure(s).\n", g_failures);
    return 1;
}
