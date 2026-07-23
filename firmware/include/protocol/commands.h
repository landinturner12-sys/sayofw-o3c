/*
 * protocol/commands.h — v3 command IDs and request/response structures.
 *
 * The 0x00-0x3F range is reserved for stock v2 compatibility (we answer
 * the same IDs when possible). 0x80-0xBF is the v3 vendor-extended range
 * used by this firmware.
 */

#ifndef SAYOFW_PROTOCOL_COMMANDS_H
#define SAYOFW_PROTOCOL_COMMANDS_H

#include <stdint.h>

/* ===== Stock v2 commands (subset we answer) ===== */
#define CMD_V2_INFO                  0x00U   /* SysInfo response */
#define CMD_V2_DEVICE_NAME           0x01U
#define CMD_V2_SYSINFO               0x02U
#define CMD_V2_SETTING               0x03U
#define CMD_V2_BLE                   0x04U
#define CMD_V2_LOCK                  0x05U
#define CMD_V2_UNLOCK                0x06U

/* ===== Screen/image v2 (subset) ===== */
#define CMD_V2_SCRIPT_PREVIEW        0x19U
#define CMD_V2_SCRIPT_STEP           0x1AU
#define CMD_V2_IMAGE                 0x20U
#define CMD_V2_DISPLAY_DUMP          0x25U  /* framebuffer dump */
#define CMD_V2_SCREEN_START          0x31U
#define CMD_V2_SCREEN_MAIN           0x32U
#define CMD_V2_SCREEN_SLEEP          0x33U
#define CMD_V2_TEXT_GBK_ASCII        0x17U
#define CMD_V2_TEXT_U16              0x18U

/* ===== Bootloader ===== */
#define CMD_BOOTLOADER               0xFFU

/* ===== v3 (vendor-extended, this firmware) ===== */
#define CMD_V3_PING                  0x80U   /* arg: token u16; echo 0x80 */
#define CMD_V3_INFO                  0x81U   /* firm version, capabilities */
#define CMD_V3_DISPLAY_TEXT          0x82U   /* host-push text to layer */
#define CMD_V3_DISPLAY_RECT          0x83U   /* host-push filled rect */
#define CMD_V3_DISPLAY_BMP           0x84U   /* host-push 1bpp bitmap */
#define CMD_V3_DISPLAY_CLEAR         0x85U   /* clear host layer */
#define CMD_V3_DISPLAY_FLUSH         0x86U   /* commit now */
#define CMD_V3_DISPLAY_MODE          0x87U   /* toggle text/graphic mode */
#define CMD_V3_MACRO_DEFINE          0x88U   /* upload bytecode slot */
#define CMD_V3_MACRO_RUN             0x89U   /* exec slot by index */
#define CMD_V3_MACRO_STOP            0x8AU
#define CMD_V3_CONFIG_EXPORT         0x8BU
#define CMD_V3_CONFIG_IMPORT         0x8CU
#define CMD_V3_DISPLAY_FOREGROUND    0x8DU   /* set fg color for text/rect */

/* ===== Response status codes (vendor-defined) ===== */
#define RESP_OK                      0x00U
#define RESP_DATA                    0x01U
#define RESP_INFO                    0x02U
#define RESP_ERR_DATA                0x03U
#define RESP_ERR_CHECKSUM            0x04U
#define RESP_ERR_UNKNOWN_CMD         0x05U
#define RESP_ERR_BAD_LEN             0x06U
#define RESP_ERR_NOT_SUPPORTED       0x07U

#endif /* SAYOFW_PROTOCOL_COMMANDS_H */
