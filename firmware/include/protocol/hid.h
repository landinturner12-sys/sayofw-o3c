/*
 * protocol/hid.h — HID v3 packet + command definitions.
 *
 * Protocol v3 extends v2 framing (see docs/sayobot-o3c-technical-reference.md
 * §3.2). Same packet layout, new usage page 0xFF20, new vendor command IDs
 * in the 0x80-0xBF range.
 *
 *   Packet (64 or 1024 bytes):
 *     offset  field
 *     0x00    report_id        (always 0x21 for normal, 0x22 for HS)
 *     0x01    echo             (always 0x03 — web UI quirk; we accept any)
 *     0x02    checksum (u16 LE) — sum of packet reinterpreted as u16[]
 *     0x04    cmd[0..N]        — one or more sub-commands
 *
 *   Sub-command:
 *     0x00 length (u16 LE, total incl. header)
 *     0x02 id                — see protocol/commands.h
 *     0x03 index             — host correlation; echoed in response
 *     0x04 data[]            — payload, length - 4 bytes
 */

#ifndef SAYOFW_PROTOCOL_HID_H
#define SAYOFW_PROTOCOL_HID_H

#include <stdint.h>
#include "sayofw_config.h"

#define HID_REPORT_ID_NORMAL    0x21U
#define HID_REPORT_ID_HS        0x22U
#define HID_ECHO                0x03U

/* Packet offsets */
#define HID_OFF_REPORT_ID       0x00U
#define HID_OFF_ECHO            0x01U
#define HID_OFF_CHECKSUM        0x02U
#define HID_OFF_CMDS            0x04U

/* Sub-cmd offsets */
#define HID_CMD_OFF_LEN         0x00U
#define HID_CMD_OFF_ID          0x02U
#define HID_CMD_OFF_INDEX       0x03U
#define HID_CMD_OFF_DATA        0x04U
#define HID_CMD_HEADER_SIZE     0x04U

/* Compute packed v3 packet buffer required for a given payload total size. */
#define HID_PACKET_BUF_SIZE(payload_size) \
    (HID_OFF_CMDS + (payload_size))

#endif /* SAYOFW_PROTOCOL_HID_H */
