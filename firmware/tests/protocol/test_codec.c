/*
 * tests/protocol/test_codec.c — Host-native tests for the v3 codec.
 *
 * Compiled with the host GCC (not riscv-none-elf-gcc). Tests:
 *   - Checksum computation / verification
 *   - Sub-command iteration
 *   - Response builder
 *   - Ping echo
 *
 * Exit 0 on pass, non-zero on fail.
 */
#include "protocol/codec.h"
#include "protocol/hid.h"
#include "protocol/commands.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

static int g_failures = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
        g_failures++; \
    } else { \
        printf("  [ OK ] %s\n", msg); \
    } \
} while (0)

/* The classic v3 echo packet (64 bytes), built by the official web UI
 * for a Ping: report_id=0x21, echo=0x03, checksum placeholder, sub-cmd
 * id=0x80, index=0x42, payload=[0xCD, 0xAB]. */
static void test_ping_echo(void)
{
    uint8_t pkt[PROTOCOL_PACKET_SIZE] = {0};
    pkt[HID_OFF_REPORT_ID] = HID_REPORT_ID_NORMAL;
    pkt[HID_OFF_ECHO]      = HID_ECHO;
    /* Build sub-cmd at offset 4: length=6, id=0x80, index=0x42, payload=[0xCD,0xAB] */
    uint8_t *cmd = &pkt[HID_OFF_CMDS];
    cmd[0U] = 6U;  cmd[1U] = 0U;     /* length LE */
    cmd[2U] = CMD_V3_PING;
    cmd[3U] = 0x42U;
    cmd[4U] = 0xCDU; cmd[5U] = 0xABU;
    /* Finalize checksum. */
    bool ok = codec_finalize_v3(pkt, sizeof(pkt));
    EXPECT(ok, "codec_finalize_v3 succeeds");
    EXPECT(codec_verify_v3(pkt, sizeof(pkt)), "checksum verifies after finalize");
}

static void test_checksum_known_value(void)
{
    /* Manually-computed reference: build a packet with known bytes and
     * verify the checksum matches a hand-rolled sum. */
    uint8_t pkt[8] = {
        0x21, 0x03, 0x00, 0x00,  /* report_id, echo, checksum (will fill) */
        0x06, 0x00, 0x80, 0x42   /* cmd len=6, id=0x80, index=0x42 */
    };
    /* Sum words: 0x0321 + 0x0000 + 0x0006 + 0x4280 = 0x45A7 */
    bool ok = codec_finalize_v3(pkt, sizeof(pkt));
    EXPECT(ok, "small-pkt finalize");
    /* Expected checksum: 0x0321 + 0 + 0 + 0x0006 + 0x4280 = 0x45A7 */
    uint16_t expected = 0x45A7U;
    uint16_t actual = (uint16_t)((uint16_t)pkt[2U] | ((uint16_t)pkt[3U] << 8));
    EXPECT(actual == expected, "checksum equals hand-rolled sum");
}

static void test_iter_single_cmd(void)
{
    uint8_t pkt[64] = {0};
    pkt[0U] = 0x21; pkt[1U] = 0x03;
    /* cmd: len=8, id=0x81, index=0x01, payload=4 bytes */
    pkt[4U] = 0x08; pkt[5U] = 0x00;
    pkt[6U] = CMD_V3_INFO;
    pkt[7U] = 0x01;
    pkt[8U] = 0xDE; pkt[9U] = 0xAD; pkt[10U] = 0xBE; pkt[11U] = 0xEF;
    (void)codec_finalize_v3(pkt, sizeof(pkt));

    const uint8_t *cur = &pkt[HID_OFF_CMDS];
    const uint8_t *end = &pkt[sizeof(pkt)];
    cmd_view_t v;
    bool got = cmd_iter_next(&cur, end, &v);
    EXPECT(got, "iter_single: first cmd found");
    EXPECT(v.id == CMD_V3_INFO, "iter_single: id == CMD_V3_INFO");
    EXPECT(v.index == 0x01, "iter_single: index correct");
    EXPECT(v.data_len == 4U, "iter_single: data_len = 4");
    EXPECT(v.data[0U] == 0xDEU, "iter_single: data[0]");
    EXPECT(!cmd_iter_next(&cur, end, &v), "iter_single: no more cmds");
}

static void test_iter_multi_cmd(void)
{
    uint8_t pkt[64] = {0};
    pkt[0U] = 0x21; pkt[1U] = 0x03;
    /* cmd A: len=6, id=0x80, idx=0x00, payload=[0x11,0x22] */
    pkt[4U] = 0x06; pkt[5U] = 0x00;
    pkt[6U] = 0x80; pkt[7U] = 0x00;
    pkt[8U] = 0x11; pkt[9U] = 0x22;
    /* cmd B: len=4, id=0x81, idx=0x00, payload=[] (header only) */
    pkt[10U] = 0x04; pkt[11U] = 0x00;
    pkt[12U] = 0x81; pkt[13U] = 0x00;
    (void)codec_finalize_v3(pkt, sizeof(pkt));

    const uint8_t *cur = &pkt[HID_OFF_CMDS];
    const uint8_t *end = &pkt[sizeof(pkt)];
    cmd_view_t a, b;
    EXPECT(cmd_iter_next(&cur, end, &a), "iter_multi: cmd A");
    EXPECT(a.id == 0x80, "iter_multi: A id");
    EXPECT(a.data_len == 2U, "iter_multi: A payload");
    EXPECT(cmd_iter_next(&cur, end, &b), "iter_multi: cmd B");
    EXPECT(b.id == 0x81, "iter_multi: B id");
    EXPECT(b.data_len == 0U, "iter_multi: B empty payload");
    EXPECT(!cmd_iter_next(&cur, end, &a), "iter_multi: end");
}

static void test_response_builder(void)
{
    uint8_t pkt[64];
    size_t n = codec_build_response(pkt, sizeof(pkt),
                                    CMD_V3_PING, 0x07,
                                    (const uint8_t[]){0xAA, 0xBB}, 2U);
    EXPECT(n > 0U, "response builder produces output");
    EXPECT(n <= sizeof(pkt), "response fits in buffer");
    EXPECT(pkt[HID_OFF_REPORT_ID] == HID_REPORT_ID_NORMAL, "response report_id");
    EXPECT(codec_verify_v3(pkt, n), "response checksum valid");
    /* Sub-cmd */
    EXPECT(pkt[HID_OFF_CMDS + 2U] == CMD_V3_PING, "response id echoed");
    EXPECT(pkt[HID_OFF_CMDS + 3U] == 0x07, "response index echoed");
    EXPECT(pkt[HID_OFF_CMDS + 4U] == 0xAA, "response payload[0]");
    EXPECT(pkt[HID_OFF_CMDS + 5U] == 0xBB, "response payload[1]");
}

static void test_ping_builder(void)
{
    uint8_t pkt[64];
    size_t n = codec_build_ping(pkt, sizeof(pkt), 0x42, 0xABCDU);
    EXPECT(n > 0U, "ping builder produces output");
    EXPECT(codec_verify_v3(pkt, n), "ping checksum valid");
    EXPECT(pkt[HID_OFF_CMDS + 2U] == CMD_V3_PING, "ping id");
    EXPECT(pkt[HID_OFF_CMDS + 3U] == 0x42, "ping index");
    EXPECT(pkt[HID_OFF_CMDS + 4U] == 0xCD, "ping token lo");
    EXPECT(pkt[HID_OFF_CMDS + 5U] == 0xAB, "ping token hi");
}

static void test_codec_rx_state(void)
{
    codec_rx_t rx;
    codec_rx_reset(&rx);
    const uint8_t *out = NULL;
    uint16_t out_len = 0U;
    /* Push bytes of a single sub-cmd: length=6, id=0x80, idx=0x00, payload=[0x11,0x22] */
    bool ready = false;
    for (uint8_t b = 0U; b < 5U; b++) {
        uint8_t byte = (uint8_t[]){0x06, 0x00, 0x80, 0x00, 0x11, 0x22}[b];
        ready = codec_rx_push(&rx, byte, &out, &out_len);
        EXPECT(!ready, "rx not yet complete");
    }
    ready = codec_rx_push(&rx, 0x22, &out, &out_len);
    EXPECT(ready, "rx complete at length bytes");
    EXPECT(out_len == 6U, "rx out_len correct");
    EXPECT(out[2U] == 0x80, "rx parsed id");
}

int main(void)
{
    printf("=== protocol/codec tests ===\n");
    test_ping_echo();
    test_checksum_known_value();
    test_iter_single_cmd();
    test_iter_multi_cmd();
    test_response_builder();
    test_ping_builder();
    test_codec_rx_state();

    if (g_failures == 0) {
        printf("\nAll codec tests passed.\n");
        return 0;
    }
    printf("\n%d failure(s).\n", g_failures);
    return 1;
}
