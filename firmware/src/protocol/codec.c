/*
 * src/protocol/codec.c — Pure-data v3 packet codec.
 *
 * No MCU deps. Safe to compile for the host (test) target.
 */
#include "protocol/codec.h"
#include "protocol/hid.h"
#include "protocol/commands.h"

#include <string.h>

uint16_t codec_checksum_v3(const uint8_t *pkt, size_t pkt_size)
{
    /* Sum packet reinterpreted as u16[] (little-endian), then mask.
     * Matches khang06's v2/v3 checksum algorithm exactly. */
    uint16_t sum = 0U;
    /* pkt_size is always even (HID report). */
    for (size_t i = 0; i < pkt_size; i += 2U) {
        uint16_t w = (uint16_t)((uint16_t)pkt[i] | ((uint16_t)pkt[i + 1U] << 8));
        sum = (uint16_t)(sum + w);
    }
    return sum;
}

bool codec_finalize_v3(uint8_t *pkt, size_t pkt_size)
{
    if (pkt == NULL || pkt_size < HID_OFF_CMDS) {
        return false;
    }
    if ((pkt_size % 2U) != 0U) {
        return false;  /* v3 packets are always even-length */
    }
    /* Zero the checksum slot before computing so the field participates in
     * the sum exactly once. */
    pkt[HID_OFF_CHECKSUM + 0U] = 0U;
    pkt[HID_OFF_CHECKSUM + 1U] = 0U;
    uint16_t csum = codec_checksum_v3(pkt, pkt_size);
    pkt[HID_OFF_CHECKSUM + 0U] = (uint8_t)(csum & 0xFFU);
    pkt[HID_OFF_CHECKSUM + 1U] = (uint8_t)((csum >> 8) & 0xFFU);
    return true;
}

bool codec_verify_v3(const uint8_t *pkt, size_t pkt_size)
{
    if (pkt == NULL || pkt_size < HID_OFF_CMDS) {
        return false;
    }
    if ((pkt_size % 2U) != 0U) {
        return false;
    }
    /* Compute sum of all u16 words (including the checksum slot), then
     * subtract the checksum word.  Equivalent to zeroing the slot first
     * but avoids mutating the const buffer. */
    uint16_t full_sum = codec_checksum_v3(pkt, pkt_size);
    uint16_t stored   = (uint16_t)((uint16_t)pkt[HID_OFF_CHECKSUM + 0U] |
                                   ((uint16_t)pkt[HID_OFF_CHECKSUM + 1U] << 8));
    uint16_t actual   = (uint16_t)(full_sum - stored);
    uint16_t expected = stored;
    return expected == actual;
}

bool cmd_iter_next(const uint8_t **cursor, const uint8_t *end, cmd_view_t *out)
{
    if (cursor == NULL || *cursor == NULL || end == NULL || out == NULL) {
        return false;
    }
    const uint8_t *p = *cursor;
    if ((size_t)(end - p) < HID_CMD_HEADER_SIZE) {
        return false;  /* not enough room for header */
    }
    /* length is little-endian u16 at p[0..1] */
    uint16_t length = (uint16_t)((uint16_t)p[0U] | ((uint16_t)p[1U] << 8));
    if (length < HID_CMD_HEADER_SIZE) {
        return false;  /* malformed: header alone is 4 bytes */
    }
    if ((size_t)(end - p) < (size_t)length) {
        return false;  /* truncated */
    }
    out->length    = length;
    out->id        = p[HID_CMD_OFF_ID];
    out->index     = p[HID_CMD_OFF_INDEX];
    out->data      = &p[HID_CMD_OFF_DATA];
    out->data_len  = (uint16_t)(length - HID_CMD_HEADER_SIZE);

    *cursor = p + length;  /* advance past this cmd */
    return true;
}

size_t codec_build_response(uint8_t *pkt, size_t pkt_cap,
                            uint8_t resp_id, uint8_t resp_index,
                            const uint8_t *payload, uint16_t payload_len)
{
    if (pkt == NULL) { return 0U; }
    size_t total = (size_t)HID_OFF_CMDS + (size_t)HID_CMD_HEADER_SIZE + (size_t)payload_len;
    if (total > pkt_cap) { return 0U; }
    /* Pad to 4-byte boundary to match v2 spec. */
    size_t padded = (total + 3U) & ~(size_t)3U;
    if (padded > pkt_cap) { return 0U; }

    memset(pkt, 0, pkt_cap);
    pkt[HID_OFF_REPORT_ID] = HID_REPORT_ID_NORMAL;
    pkt[HID_OFF_ECHO]      = HID_ECHO;

    /* Sub-cmd at offset HID_OFF_CMDS */
    uint8_t *cmd = &pkt[HID_OFF_CMDS];
    uint16_t cmd_len = (uint16_t)(HID_CMD_HEADER_SIZE + payload_len);
    cmd[0U] = (uint8_t)(cmd_len & 0xFFU);
    cmd[1U] = (uint8_t)((cmd_len >> 8) & 0xFFU);
    cmd[2U] = resp_id;
    cmd[3U] = resp_index;
    if (payload != NULL && payload_len > 0U) {
        memcpy(&cmd[HID_CMD_OFF_DATA], payload, payload_len);
    }
    /* Finalize over the FULL padded buffer (pkt_cap, since we zeroed it
     * above) so the trailing zero pad contributes 0 to the checksum —
     * matching what the host verifier does on received 64-byte reports. */
    (void)codec_finalize_v3(pkt, pkt_cap);
    return padded;
}

size_t codec_build_ping(uint8_t *pkt, size_t pkt_cap, uint8_t resp_index, uint16_t token)
{
    uint8_t payload[2];
    payload[0U] = (uint8_t)(token & 0xFFU);
    payload[1U] = (uint8_t)((token >> 8) & 0xFFU);
    return codec_build_response(pkt, pkt_cap,
                                CMD_V3_PING,  /* echo CMD_V3_PING on response */
                                resp_index,
                                payload, (uint16_t)sizeof(payload));
}

void codec_rx_reset(codec_rx_t *r)
{
    if (r == NULL) { return; }
    memset(r, 0, sizeof(*r));
}

bool codec_rx_push(codec_rx_t *r, uint8_t byte, const uint8_t **out, uint16_t *out_len)
{
    if (r == NULL) { return false; }
    if (r->len >= sizeof(r->buf)) {
        r->len = 0U;  /* overflow — reset */
    }
    r->buf[r->len++] = byte;
    if (r->len < HID_CMD_HEADER_SIZE) {
        return false;  /* not enough yet */
    }
    /* Peek at length field. */
    uint16_t length = (uint16_t)((uint16_t)r->buf[0U] | ((uint16_t)r->buf[1U] << 8));
    if (length < HID_CMD_HEADER_SIZE || length > sizeof(r->buf)) {
        r->len = 0U;  /* malformed */
        return false;
    }
    if (r->len < length) {
        r->need = length;
        return false;  /* need more bytes */
    }
    if (out != NULL)    { *out    = r->buf;  }
    if (out_len != NULL){ *out_len = length; }
    /* shift leftover (for simplicity assume one cmd per push) */
    r->len = 0U;
    return true;
}
