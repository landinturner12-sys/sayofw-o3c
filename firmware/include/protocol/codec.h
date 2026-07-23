/*
 * protocol/codec.h — Pure-data codec for v3 HID packets.
 *
 * No MCU dependencies — all functions are bit-fiddling on byte arrays.
 * Used both on-device (when parsing host packets) and on-host (when
 * generating test packets).
 *
 * The codec is stateless except for the optional `proto_ctx_t` used to
 * buffer stateful RX streams; that struct is exposed in the header only
 * for allocation, not for field access.
 */
#ifndef SAYOFW_PROTOCOL_CODEC_H
#define SAYOFW_PROTOCOL_CODEC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "protocol/hid.h"
#include "protocol/commands.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Checksums ===== */

/* v3 16-bit checksum: reinterpret packet as u16[] (little-endian), sum all
 * words including the placeholder at offset 0x02, then mask to 16 bits.
 * Returns the value that *should* appear at offset 0x02..0x03.
 * (Matches khang06's Connect.vue implementation.) */
uint16_t codec_checksum_v3(const uint8_t *pkt, size_t pkt_size);

/* Compute and write the checksum in-place. Returns true on success. */
bool codec_finalize_v3(uint8_t *pkt, size_t pkt_size);

/* Verify the checksum already written at pkt[2..3]. */
bool codec_verify_v3(const uint8_t *pkt, size_t pkt_size);

/* ===== Sub-command iterators ===== */

/* Result of decoding one sub-command. `data` points into the *original*
 * packet buffer (no copy). */
typedef struct {
    uint16_t length;     /* declared sub-cmd length (includes header) */
    uint8_t  id;
    uint8_t  index;
    const uint8_t *data; /* payload pointer, valid while parent pkt is alive */
    uint16_t data_len;   /* length - 4 */
} cmd_view_t;

/* Read the next sub-command starting at `cursor` (a pointer into pkt).
 * On success: advances *cursor past the command, returns true.
 * On out-of-bounds: returns false.
 *
 * Example:
 *   const uint8_t *p = pkt + HID_OFF_CMDS;
 *   const uint8_t *end = pkt + pkt_size;
 *   cmd_view_t v;
 *   while (cmd_iter_next(&p, end, &v)) {
 *       dispatch(v);
 *   }
 */
bool cmd_iter_next(const uint8_t **cursor, const uint8_t *end, cmd_view_t *out);

/* ===== Packet builders (write into caller-provided buffers) ===== */

/* Build a minimal response: report_id = HID_REPORT_ID_NORMAL, echo = 3,
 * checksum = 0, one sub-cmd with id `resp_id` and payload `payload..`.
 * Returns total packet size written, or 0 on overflow. */
size_t codec_build_response(uint8_t *pkt, size_t pkt_cap,
                            uint8_t resp_id, uint8_t resp_index,
                            const uint8_t *payload, uint16_t payload_len);

/* Build a Ping response (echoes the host's two-byte token back). */
size_t codec_build_ping(uint8_t *pkt, size_t pkt_cap,
                        uint8_t resp_index, uint16_t token);

/* ===== RX framing state machine ===== */

/* Stateful reader used by the protocol dispatcher to reassemble commands
 * that span multiple 64/1024-byte packets (rare; normal host tools send one
 * packet per command). Most callers can ignore this. */
typedef struct {
    uint8_t  buf[256];
    uint16_t len;
    uint16_t need;
} codec_rx_t;

void codec_rx_reset(codec_rx_t *r);
bool codec_rx_push(codec_rx_t *r, uint8_t byte, const uint8_t **out, uint16_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* SAYOFW_PROTOCOL_CODEC_H */
