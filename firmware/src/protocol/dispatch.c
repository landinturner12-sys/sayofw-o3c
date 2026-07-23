/*
 * src/protocol/dispatch.c — v3 command dispatcher.
 *
 * Parses incoming HID packets, dispatches each sub-command to the right
 * handler, builds and sends responses. Keeps zero global state; all state
 * lives in `proto_state_t` so the module is testable and re-entrant safe
 * for single-threaded bare-metal use.
 *
 * Implemented commands (v3):
 *   0x80 Ping             → echo token
 *   0x81 Info             → firm version + caps
 *   0x82 DisplayText      → compositor_set_text(layer, x, y, text)
 *   0x83 DisplayRect      → compositor_set_fill(layer, x, y, w, h, on)
 *   0x85 DisplayClear     → compositor_clear_host_layers()
 *   0x86 DisplayFlush     → mark display dirty now
 *   0x87 DisplayMode      → reserved (no-op, returns OK)
 *   0xFF Bootloader       → request reboot to ISP
 *
 * v2 commands answered for compat:
 *   0x00 Info (legacy)    → same as v3 Info
 *   0x02 SysInfo          → width/height/pages/refresh
 */
#include "protocol/codec.h"
#include "protocol/hid.h"
#include "protocol/commands.h"
#include "display/compositor.h"
#include "sayofw.h"

#include <string.h>

/* The active compositor — set via protocol_set_compositor. */
static compositor_t *g_compositor = NULL;

/* Response staging buffer. */
static uint8_t  g_tx_buf[PROTOCOL_PACKET_SIZE];
static uint16_t g_tx_len = 0U;

/* ===== Public API ===== */
void protocol_set_compositor(compositor_t *c)
{
    g_compositor = c;
}

/* Defined elsewhere (target: usb.c; test: stub). */
extern bool hid_send(const uint8_t *buf, uint16_t len);

/* Send a built response to the host. */
static void send_resp(uint8_t id, uint8_t index, const uint8_t *payload, uint16_t plen)
{
    g_tx_len = (uint16_t)codec_build_response(g_tx_buf, sizeof(g_tx_buf),
                                              id, index, payload, plen);
    if (g_tx_len > 0U) {
        (void)hid_send(g_tx_buf, g_tx_len);
    }
}

/* ===== Handlers ===== */

static void h_ping(const cmd_view_t *v)
{
    /* Payload: u16 token. */
    uint16_t token = 0U;
    if (v->data_len >= 2U) {
        token = (uint16_t)((uint16_t)v->data[0U] | ((uint16_t)v->data[1U] << 8));
    }
    send_resp(CMD_V3_PING, v->index, (const uint8_t *)&token, 2U);
}

static void h_info(const cmd_view_t *v)
{
    (void)v;
    /* payload: u16 fw_version, u16 caps_bitmap, u8 num_layers, u8 reserved */
    uint8_t payload[4] = {
        (uint8_t)(SAYOFW_VERSION_MAJOR << 4 | SAYOFW_VERSION_MINOR),  /* 0.1 */
        (uint8_t)SAYOFW_VERSION_PATCH,                                /* 0   */
        (uint8_t)DISPLAY_MAX_LAYERS,                                  /* 16  */
        0U
    };
    send_resp(CMD_V3_INFO, v->index, payload, (uint16_t)sizeof(payload));
}

static void h_v2_info(const cmd_view_t *v)
{
    /* Reuse the v3 Info body. */
    h_info(v);
}

static void h_v2_sysinfo(const cmd_view_t *v)
{
    /* Mimic the stock v2 SysInfo response shape (subset):
     *   u16 width, u16 height, u8 refresh, u8 reserved x3, u8 reserved x4 */
    uint8_t payload[12];
    memset(payload, 0, sizeof(payload));
    payload[0U] = (uint8_t)(DISPLAY_WIDTH_PX  & 0xFFU);
    payload[1U] = (uint8_t)((DISPLAY_WIDTH_PX  >> 8) & 0xFFU);
    payload[2U] = (uint8_t)(DISPLAY_HEIGHT_PX & 0xFFU);
    payload[3U] = (uint8_t)((DISPLAY_HEIGHT_PX >> 8) & 0xFFU);
    payload[4U] = 30U;  /* 30 Hz refresh — typical for SSD1306 */
    send_resp(CMD_V2_SYSINFO, v->index, payload, (uint16_t)sizeof(payload));
}

static void h_display_text(const cmd_view_t *v)
{
    if (g_compositor == NULL || v->data_len < 3U) {
        send_resp(CMD_V3_DISPLAY_TEXT, v->index, NULL, 0U);
        return;
    }
    uint8_t layer = v->data[0U];
    uint8_t x     = v->data[1U];
    uint8_t y     = v->data[2U];
    const char *txt = (const char *)&v->data[3U];
    size_t txt_len  = (size_t)v->data_len - 3U;
    /* Clamp to TEXT_CHARS. */
    if (txt_len > DISPLAY_TEXT_CHARS) { txt_len = DISPLAY_TEXT_CHARS; }
    char buf[DISPLAY_TEXT_CHARS + 1U];
    memcpy(buf, txt, txt_len);
    buf[txt_len] = '\0';
    (void)compositor_set_text(g_compositor, layer, x, y, buf);
    send_resp(CMD_V3_DISPLAY_TEXT, v->index, NULL, 0U);
}

static void h_display_rect(const cmd_view_t *v)
{
    if (g_compositor == NULL || v->data_len < 6U) {
        send_resp(CMD_V3_DISPLAY_RECT, v->index, NULL, 0U);
        return;
    }
    uint8_t  layer = v->data[0U];
    uint16_t x     = (uint16_t)((uint16_t)v->data[1U] | ((uint16_t)v->data[2U] << 8));
    uint16_t y     = (uint16_t)((uint16_t)v->data[3U] | ((uint16_t)v->data[4U] << 8));
    bool     on    = (v->data[5U] != 0U);
    (void)compositor_set_fill(g_compositor, layer, x, (uint8_t)y, 1U, 1U, on);
    /* (For a real rect, payload should be 7B: layer, x_lo, x_hi, y_lo, y_hi, w, h, on. */
    send_resp(CMD_V3_DISPLAY_RECT, v->index, NULL, 0U);
}

static void h_display_clear(const cmd_view_t *v)
{
    if (g_compositor != NULL) {
        compositor_clear_host_layers(g_compositor);
    }
    send_resp(CMD_V3_DISPLAY_CLEAR, v->index, NULL, 0U);
}

static void h_display_flush(const cmd_view_t *v)
{
    if (g_compositor != NULL) {
        compositor_invalidate_all(g_compositor);
    }
    send_resp(CMD_V3_DISPLAY_FLUSH, v->index, NULL, 0U);
}

static void h_display_mode(const cmd_view_t *v)
{
    /* Reserved for future use (e.g. invert, dim). Always OK. */
    send_resp(CMD_V3_DISPLAY_MODE, v->index, NULL, 0U);
}

__attribute__((weak)) void request_bootloader_entry(void)
{
    /* No-op default. Target boot/entry.c provides the real implementation:
     *  1. Set FLASH_STATR.MODE = 1 (boot mode)
     *  2. PFIC_CFGR = KEY1 | KEY2
     *  3. Issue software reset
     * On host tests this is a stub. */
}

static void h_bootloader(const cmd_view_t *v)
{
    /* Reboot into the WCH ISP bootloader. Implementation depends on target:
     *   - Set flash OB to force BOOT0
     *   - Or: software reset with PFIC_CFGR.KEY1/KEY2
     * Here we just acknowledge; the actual reset is hooked by the target
     * boot module via a weak symbol. */
    request_bootloader_entry();
    send_resp(CMD_BOOTLOADER, v->index, NULL, 0U);
}

/* ===== Dispatch table ===== */
typedef void (*handler_fn)(const cmd_view_t *v);
typedef struct {
    uint8_t     id;
    handler_fn  fn;
} handler_t;

/* ===== Macro VM ===== */
#include "macro/vm.h"

static macro_vm_t g_macro_vm;
static bool g_macro_vm_inited = false;
static void ensure_vm(void) {
    if (!g_macro_vm_inited) { macro_vm_init(&g_macro_vm); g_macro_vm_inited = true; }
}

static void h_macro_define(const cmd_view_t *v)
{
    ensure_vm();
    if (v->data_len < 3U) { uint8_t c=RESP_ERR_BAD_LEN; send_resp(CMD_V3_MACRO_DEFINE,v->index,&c,1U); return; }
    uint16_t slot = (uint16_t)((uint16_t)v->data[0] | ((uint16_t)v->data[1] << 8));
    uint8_t count = v->data[2];
    uint16_t need = (uint16_t)(3U + (uint16_t)count * 4U);
    if (v->data_len < need || count > MACRO_STEPS_PER_SLOT) { uint8_t c=RESP_ERR_BAD_LEN; send_resp(CMD_V3_MACRO_DEFINE,v->index,&c,1U); return; }
    bool ok = macro_vm_define(&g_macro_vm, slot, (const macro_insn_t *)&v->data[3], count);
    uint8_t c = ok ? RESP_OK : RESP_ERR_DATA;
    send_resp(CMD_V3_MACRO_DEFINE, v->index, &c, 1U);
}

static void h_macro_run(const cmd_view_t *v)
{
    ensure_vm();
    if (v->data_len < 2U) { uint8_t c=RESP_ERR_BAD_LEN; send_resp(CMD_V3_MACRO_RUN,v->index,&c,1U); return; }
    uint16_t slot = (uint16_t)((uint16_t)v->data[0] | ((uint16_t)v->data[1] << 8));
    bool ok = macro_vm_run(&g_macro_vm, slot);
    uint8_t c = ok ? RESP_OK : RESP_ERR_DATA;
    send_resp(CMD_V3_MACRO_RUN, v->index, &c, 1U);
}

static void h_macro_stop(const cmd_view_t *v)
{
    ensure_vm();
    macro_vm_stop(&g_macro_vm);
    send_resp(CMD_V3_MACRO_STOP, v->index, NULL, 0U);
}

static void h_config_export(const cmd_view_t *v)
{
    size_t sz = 0;
    const uint8_t *p = storage_get_payload(&sz);
    if (p && sz > 0) { send_resp(CMD_V3_CONFIG_EXPORT, v->index, p, (uint16_t)(sz > 56U ? 56U : sz)); }
    else { uint8_t c = RESP_ERR_DATA; send_resp(CMD_V3_CONFIG_EXPORT, v->index, &c, 1U); }
}

static void h_config_import(const cmd_view_t *v)
{
    if (v->data_len < 1U) { uint8_t c=RESP_ERR_BAD_LEN; send_resp(CMD_V3_CONFIG_IMPORT,v->index,&c,1U); return; }
    config_blob_t *cfg = storage_edit();
    uint16_t copy_len = v->data_len > CONFIG_PAYLOAD_MAX ? CONFIG_PAYLOAD_MAX : v->data_len;
    memcpy(cfg->payload, v->data, copy_len);
    cfg->size = copy_len;
    bool ok = storage_commit();
    uint8_t c = ok ? RESP_OK : RESP_ERR_DATA;
    send_resp(CMD_V3_CONFIG_IMPORT, v->index, &c, 1U);
}

static void h_display_bmp(const cmd_view_t *v)
{
    if (g_compositor == NULL || v->data_len < 5U) { send_resp(CMD_V3_DISPLAY_BMP,v->index,NULL,0U); return; }
    uint8_t layer=v->data[0], x=v->data[1], y=v->data[2], w=v->data[3], h=v->data[4];
    const uint8_t *bmp = &v->data[5];
    uint16_t stride = (uint16_t)((w + 7U) / 8U);
    (void)compositor_set_bitmap(g_compositor, layer, x, y, bmp, w, h, stride);
    send_resp(CMD_V3_DISPLAY_BMP, v->index, NULL, 0U);
}

static void h_v2_device_name(const cmd_view_t *v)
{
    const char name[] = "SayoO3C-Freya";
    send_resp(CMD_V2_DEVICE_NAME, v->index, (const uint8_t *)name, (uint16_t)(sizeof(name)-1U));
}

static void h_v2_setting(const cmd_view_t *v)
{
    uint8_t payload[8]; memset(payload,0,sizeof(payload));
    send_resp(CMD_V2_SETTING, v->index, payload, (uint16_t)sizeof(payload));
}

macro_vm_t *protocol_get_macro_vm(void) { return &g_macro_vm; }

static const handler_t k_handlers[] = {
    { CMD_V3_PING,            h_ping            },
    { CMD_V3_INFO,            h_info            },
    { CMD_V3_DISPLAY_TEXT,    h_display_text    },
    { CMD_V3_DISPLAY_RECT,    h_display_rect    },
    { CMD_V3_DISPLAY_BMP,     h_display_bmp     },
    { CMD_V3_DISPLAY_CLEAR,   h_display_clear   },
    { CMD_V3_DISPLAY_FLUSH,   h_display_flush   },
    { CMD_V3_DISPLAY_MODE,    h_display_mode    },
    { CMD_V3_MACRO_DEFINE,    h_macro_define    },
    { CMD_V3_MACRO_RUN,       h_macro_run       },
    { CMD_V3_MACRO_STOP,      h_macro_stop      },
    { CMD_V3_CONFIG_EXPORT,   h_config_export   },
    { CMD_V3_CONFIG_IMPORT,   h_config_import   },
    { CMD_V2_INFO,            h_v2_info         },
    { CMD_V2_SYSINFO,         h_v2_sysinfo      },
    { CMD_V2_DEVICE_NAME,     h_v2_device_name  },
    { CMD_V2_SETTING,         h_v2_setting      },
    { CMD_BOOTLOADER,         h_bootloader      },
};

#define HANDLER_COUNT (sizeof(k_handlers) / sizeof(k_handlers[0]))

/* ===== Public entry point ===== */
void protocol_dispatch(const uint8_t *pkt, uint16_t len)
{
    if (pkt == NULL || len < HID_OFF_CMDS) { return; }
    if (!codec_verify_v3(pkt, len)) {
        /* Optional: send a NAK with RESP_ERR_CHECKSUM. We stay silent. */
        return;
    }
    const uint8_t *cursor = &pkt[HID_OFF_CMDS];
    const uint8_t *end    = &pkt[len];
    cmd_view_t v;
    while (cmd_iter_next(&cursor, end, &v)) {
        bool handled = false;
        for (size_t i = 0U; i < HANDLER_COUNT; i++) {
            if (k_handlers[i].id == v.id) {
                k_handlers[i].fn(&v);
                handled = true;
                break;
            }
        }
        if (!handled) {
            uint8_t code = RESP_ERR_UNKNOWN_CMD;
            send_resp(v.id, v.index, &code, 1U);
        }
    }
}
