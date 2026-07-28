# SayoFW Code Review — Findings

Scope: firmware C, web flasher JS, docs. Tested with native tests where applicable.

## CRITICAL — blocks device function or bricks hardware

### F1. Init order breaks first packets — `firmware/src/main.c:24-32`
`protocol_init()` runs **before** `display_init()`. Inside `display_init`,
`protocol_set_compositor(&g_compositor)` is called. Any HID packet arriving
between these two calls is dispatched with `g_compositor == NULL`, silently
no-oping display commands. Reorder so `display_init()` runs first, or make
`protocol_set_compositor(NULL)` safe and call it early.

### F2. Bitmap pointer lifetime — `firmware/src/protocol/dispatch.c:248`
`h_display_bmp` does:
```c
const uint8_t *bmp = &v->data[5];
(void)compositor_set_bitmap(g_compositor, layer, x, y, bmp, w, h, stride);
```
`v->data` points into the USB RX buffer. The compositor stores the pointer
(`c->layers[].u.bitmap.bmp = bmp`). The next `compositor_repaint` reads from
freed/recycled RAM. **Use-after-free**: dangling pointer read on every redraw
after a BMP layer is set. Copy the bitmap data into a compositor-owned buffer.

### F3. `boot_count` never incremented — `firmware/src/storage/config.c`
Field exists in `config_blob_t` and is described as "boot-time stamp /
counter". Never written except `0U` in the no-valid-bank branch. Either
implement the increment or delete the field to avoid surprising future
contributors.

### F4. SysTick / `sys_tick_ms` never increments on target — `firmware/src/system/sys.c`
`sys_init()` has only a comment "Left as a TODO for the integration phase."
The `#if HOST_TESTING` branch is also dead (HOST_TESTING is unconditionally
defined to 0). On real hardware, `g_tick_ms` stays 0 forever:
- `keys_scan` debounce timer never elapses (always `>= 15ms` since `now=0`).
- `macro_vm_tick` `OP_DELAY` would never expire because `now_ms == 0`,
  every DELAY's `delay_until_ms = 0 + arg1`, so `now >= until` immediately
  → effectively zero delays.
- Every keys_consume_press behavior degrades to "no debounce".

### F5. Linker FLASH region overlaps config storage — `firmware/linker/ch32v307_app.ld:23` and `firmware/include/sayofw_config.h:91`
Linker: `FLASH ORIGIN = 0x08004000, LENGTH = 144K` → ends at `0x08028000`.
Config: `CONFIG_BASE_ADDR = 0x0801A000`, `CONFIG_BANK_SIZE = 64K`
→ ends at `0x0802A000`. **Overlap of 56 KB**. A sufficiently large
firmware (text+data > ~96 KB) will be linked into the config region and
brick the device on save, or clobber the bootloader-managed metadata at
`0x08029F80`. Either shrink FLASH to `LENGTH = 96K` and raise
`CONFIG_BASE_ADDR` accordingly, or move CONFIG above `0x08028000`.

### F6. Placeholder binary deployed — `docs/flasher/firmware/sayofw_o3c.bin`
The deployed firmware is `SAYOFW_PLACEHOLDER_v0.1.0\n` followed by 0xFF.
`firmware.json` exposes this with `sha256: "placeholder"` and `note`
explaining it. The web flasher performs **no sanity check** (no SHA match,
no size sanity vs. advertised flash capacity). A user clicking "Flash" will
erase their app region and write 1 KB of garbage, then likely fail verify
with their bootloader refusing to boot. CI workflow `deploy.yml` does not
build the firmware before publishing. At minimum: refuse to flash if
`sha256 === "placeholder"`, or fail the workflow if `firmware/build/`
artifacts are absent.

## HIGH — functional bugs

### F7. `h_display_rect` ignores width/height — `firmware/src/protocol/dispatch.c:120`
Reads 6 bytes, casts to `(uint16_t)x, (uint16_t)y, bool on`, then calls
`compositor_set_fill(..., 1, 1, on)`. The TODO comment in the function
admits this. The handler is effectively a set-pixel command with an extra
`y_hi` byte silently dropped. Implement the documented payload format
(layer, x_lo, x_hi, y_lo, y_hi, w_lo, w_hi, h_lo, h_hi, on) — or remove the
command from `commands.h` until ready.

### F8. `SysTick_Handler` is not weak — `firmware/src/system/sys.c:32-36`
The vendor SDK (ch32v30x.h) provides `SysTick_Handler`. Defining a strong
version here will cause a duplicate-symbol link error when the SDK is
included. Either mark `__attribute__((weak))` (the comment above
`hal_key_read` in `keys.c` already follows this pattern) or move to a
target-only file.

### F9. Init order for `_start` and `SystemInit` — `firmware/src/boot/startup_ch32v30x.S:8`
Comment promises "Call SystemInit (clock tree)". Code does not. PLLs are
never configured before `main()`. `sys_init` was meant to do this but is
a stub. Either move PLL config into startup.S (vendor `ch32v30x.S`
typically does this) or actually implement `sys_init`.

### F10. `request_bootloader_entry` not in header — `firmware/src/protocol/dispatch.c:159`
Declared `__attribute__((weak))` inside `.c` only. No prototype in any
header. Target overrides will trigger implicit-declaration warnings under
`-Werror`. Add to a public header.

### F11. `OP_LOOP_START` overflow handling — `firmware/src/macro/vm.c:113-122`
When `loop_sp >= VM_LOOP_STACK_DEPTH`, the start is silently swallowed
but `vm->pc++` still advances, and `OP_LOOP_END` later tries to find a
match. Net effect: the inner loop runs once (without loop semantics) then
hits LOOP_END as unmatched → skipped (line: `else { vm->pc++; }`). Silent
correctness loss. Either enforce nesting limit at upload time (in
`h_macro_define`) or set a runtime error flag.

### F12. `OP_LOOP_START` arg1 = 0 ambiguous — `firmware/include/macro/vm.h:25-26`
Comment says "iteration count (0=infinite)". Also no semantic for "do
zero times". Convention is fine but should be documented in the wire-spec.

### F13. `host_layers` half comment wrong — `firmware/src/display/compositor.c:204-208`
Comment says "Host layers are the upper half (8..15)" but code disables
**all 16** layers. Either fix the implementation to only clear upper half
or correct the comment. Currently the lower half is being silently wiped
on `DisplayClear` — if device-side features (status icons) ever populate
layers 0..7, they'll vanish on a host clear.

### F14. `compute_crc32` reads OOB if `size > CONFIG_PAYLOAD_MAX` — `firmware/src/storage/config.c:30-39, 95-103`
No clamp on `tmp.size` before passing to `compute_crc32` in `storage_init`
and `storage_commit`. A flipped byte in flash could read past the buffer.
Add `if (tmp.size > CONFIG_PAYLOAD_MAX) continue;`.

### F15. `hal_flash_rase_checked` typo — `firmware/src/storage/config.c:20`
Probably intentional but `rase` will confuse anyone grepping. Either fix
the typo or alias both names.

### F16. `protocol_dispatch` doesn't NAK on checksum fail — `firmware/src/protocol/dispatch.c:303-305`
Silent drop. Host tools can't distinguish "no response because nothing to
do" from "your packet was rejected". The existing `RESP_ERR_CHECKSUM`
define is unused. Send a 1-byte error response so hosts can retry or
surface the error.

## MEDIUM — design / portability

### F17. `g_tx_len` dead variable — `firmware/src/protocol/dispatch.c:36`
Written every response, never read. Delete or wire it into a status
register for the test hooks.

### F18. `PROTOCOL_RX_RING_SIZE`, `PROTOCOL_USAGE_PAGE`, `PROTOCOL_USAGE_V3` unused — `firmware/include/sayofw_config.h:51-59`
Three defines never referenced. Either delete or implement (ring buffer
for RX fragmentation; usage page for runtime HID).

### F19. `keys_init` zero-arg OK, but `just_changed` lifecycle fragile — `firmware/src/input/keys.c:35-44`
`just_changed` is reset on every scan where state didn't change. If host
consumes events slower than the scan loop, events are lost (without
queueing). Acceptable for 3 keys but not for future N-key matrix. Add a
ring or a sticky flag with explicit clear.

### F20. `display_flush` doesn't clamp dirty to panel — `firmware/src/display/display_main.c:40-55`
`compositor_take_dirty` can return `y1 = 254` if a host pushes text at
`y=200`. Driver clamps `page_y1`, but `x1` is also unclamped; x≥128 is
clamped inside the driver. Add clamping at the compositor API boundary so
clients don't have to guess.

### F21. `h_v2_sysinfo` response shape wrong — `firmware/src/protocol/dispatch.c:108-119`
Comment says "u16 width, u16 height, u8 refresh, u8 reserved x3, u8
reserved x4" = 12 bytes total. The code writes 12 bytes, all zeros after
the width/height/refresh. v2 hosts that interpret byte 5..11 as flags
will read garbage if the stock firmware had specific encodings. Validate
against khang06's reference before shipping.

### F22. `codec_rx_push` discards overflow silently — `firmware/src/protocol/codec.c:130-152`
On overflow, `r->len = 0` and returns false. No telemetry to caller, no
reset ack. Add a `r->overflow_count` field so production firmware can
log/diagnose RX overruns.

### F23. `font.h` doc says "msb-first row order" — `firmware/include/display/font.h:6`
Actually column-major: each byte is a column, each bit a row, bit 0 = top.
The comment describes the bitmap format (`display_draw_bitmap`) but is in
the font header. Either change to "byte = column, bit = row" or move the
comment to `display.h`.

### F24. `protocol_get_macro_vm` exposed but not declared — `firmware/src/protocol/dispatch.c:269`
Same implicit-declaration risk as F10.

### F25. `display_init()`/`display_flush()` not declared in `sayofw.h` — `firmware/src/main.c:30-32`
`main.c` reinvents `extern void display_init(void);` locally. Add to the
umbrella header.

## LOW — style / nits

### F26. `h_display_text` and friends cast `const char *` from `uint8_t *` — `firmware/src/protocol/dispatch.c:99`
UB-adjacent if the host sends invalid UTF-8 that happens to also be a
trapping pattern. Not actionable, but worth noting.

### F27. `display_draw_char` hard-codes off-by-one for off-pixel erase — `firmware/src/display/display.c:148-152`
Duplicates the bit-set logic instead of calling a hypothetical
`display_clear_pixel`. Cosmetic.

### F28. `display_get_framebuffer()` returns module-private static — `firmware/src/display/display.c:175`
The compositor reaches into it. Fine, but tightly couples the two — a
"two framebuffers (double-buffer)" optimization would require surgery.

## Web flasher JS

### W1. `xorKey` derivation is single-byte — `docs/flasher/static/js/wch-isp.js:69-72`
Only `(sum & 0xff)` is kept. Some WCH ISP variants use the full u32 sum
or two-byte rolling. Match the variant in use or expose a knob.

### W2. `CMD_ISP_END` reset semantics — `docs/flasher/static/js/wch-isp.js:124`
Sends `[1]`, then a `transferIn` that may throw on the device
disconnect. The catch is there but the UI doesn't surface
"device disconnected during reset" distinctly from "reset ack". Minor UX.

### W3. `program` chunking fixed at 56 — `docs/flasher/static/js/wch-isp.js:11`
Hardcoded. WCH ISP can usually accept up to ~56 bytes with the XOR header
accounted for. Add a constant explaining the limit and a comment pointing
at the WCH ISP spec.

### W4. No fallback for non-USB-capable browsers — `docs/flasher/static/js/main.js:29-34`
The warning banner exists but the rest of the page still shows the flash
button as enabled if `checkBrowser` returns early. Check `#btn-connect`'s
disabled state when navigator.usb is absent. Currently correct because
`checkBrowser` returns without re-enabling, but a defensive early return
in `doConnect` would help.

### W5. UI hardcoded to VID 0x4348 / PID 0x55e0 — `docs/flasher/static/js/wch-isp.js:3-4`
The O3C application firmware reports VID 0x8089 / PID 0x0009 (README).
If the user wants to re-enter bootloader mode from a malfunctioning app
firmware via the web flasher (the documented use case), the browser will
not match — the device never shows up as a 0x4348 device unless BOOT0
is held. This is a usability, not correctness, issue, but the README
implies the web flasher is a recovery tool. Consider adding a "manually
select VID/PID" picker.

### W6. `fwData` cached on first load — `docs/flasher/static/js/main.js:78`
Once loaded, `loadFirmware` returns the cached blob even after a `fetch`
that returned a new file from disk (cache invalidation is manual). For
iterative development on the firmware, a user flashing multiple times in
quick succession may accidentally flash an older binary after a `make`.
Either re-fetch or bust the cache.

### W7. `erase` unit confusion — `docs/flasher/static/js/main.js:97`
"Number of sectors" passed to `isp.erase(sectors)` is
`Math.ceil(fw.length / 256)`. The WCH ISP `CMD_ERASE` typically takes
**page** count (256 B), which matches here. But `SECTOR_SIZE = 256`
naming in `wch-isp.js` is misleading — flash "sectors" on STM32/WCH are
usually 4 KB. Rename to `PAGE_SIZE` or document.

### W8. `setKey` uses 30-byte payload but WCH expects 30 — `docs/flasher/static/js/wch-isp.js:62-65`
The WCH ISP `CMD_ISP_KEY` payload is documented as 30 bytes in some
revisions and 8 bytes in others. khang06's gist specifies 30. Confirm
against the O3C bootloader version before relying on this.

## Docs / README

### D1. Status table stale — `README.md:60-72`
- "Macro VM — Header, implementation in progress" — actual `vm.c`
  implements the full 12-opcode VM with delays and loops.
- "Config Storage — Stub, flash persistence pending" — actual
  `storage/config.c` has dual-bank commit + CRC + verify.
- "Web UI (SvelteKit) — Planned" — `docs/flasher/` is vanilla JS+WebUSB,
  no SvelteKit code in repo.

### D2. CLI tool mentioned but absent — `README.md:54,71,234,525`
`sayocli` is repeatedly referenced. No Rust code anywhere in the repo.
Either remove references, add a stub binary, or update the docs to "not
yet started".

### D3. `webui/` directory is empty — `webui/{flasher/src,src,static}/`
README claims a SvelteKit app lives here. Reality: `webui/flasher/src/`
and `webui/src/` exist but contain only directories with no `.ts`/`.svelte`
files. Either delete `webui/` or populate it. The actually-shipped
flasher is in `docs/flasher/`.

### D4. Structure tree mismatches reality — `README.md:355-372`
- `webui/flasher/` shown with WebHID flasher; in reality contains only
  `src/{components,lib,assets,styles}` (all empty).
- `firmware/tools/` shown as "Dev utilities (planned)" — exists as empty
  directory.
- `firmware/scripts/` exists as empty directory (not in tree).

### D5. Wrong PUSH instructions — `README.md:218-224`
The push instructions for HID-based control reference `sayocli` (which
doesn't exist) for sending bootloader command. Use `wchisp` or the web
flasher (the available path).

### D6. `WCH-Link` flashing method not supported by `wlink` — `README.md:148-180`
Steps are reasonable but missing: vendor link-tool install method,
`wlink` requires the WCH-Link hardware. Add explicit "you need a WCH-Link
adapter for this path".

### D7. `DISPLAY_USAGE_PAGE` advertised but never sent — `README.md`/`docs/api/TECHNICAL_REFERENCE.md`
README and protocol docs describe usage page `0xFF20` for HID reports.
The actual `HID_REPORT_ID_NORMAL = 0x21` is what the codec uses; the
usage page is never encoded. Update docs to match the implementation, or
add an HID descriptor with the proper usage page.

### D8. `CNAME` placeholder — `docs/flasher/CNAME`
Value should be checked before deployment; CI doesn't validate.

### D9. `firmware.json` exposes placeholder SHA — `docs/flasher/firmware/firmware.json`
SHA is `"placeholder"`. Web flasher should refuse to flash when this is
the case (related to F6).

### D10. Deploy workflow doesn't build firmware — `.github/workflows/deploy.yml`
Workflow publishes `docs/flasher/` directly with whatever `.bin` is in
the repo. Add a build step (checkout + riscv toolchain + `make all`)
that copies the fresh binary into `docs/flasher/firmware/` and updates
`firmware.json` with SHA + size before upload.

## Verification performed

- `make test` runs native tests: **all pass** (4 binaries, 0 failures).
- Tests cover codec, display math, compositor integration, dispatch
  pipeline. **They do not** catch:
  - F1 (init order): unit tests don't call `main`.
  - F2 (lifetime): dispatch test only checks `c.layers[0].enabled`, not
    that the BMP repaint uses valid memory after the packet buffer is
    freed.
  - F4 (SysTick): host tests stub `sys_tick_ms = 0`.
  - F5 (linker overlap): only `riscv-none-elf-ld` would catch this; the
    toolchain is not installed in the test env (verified via
    `which riscv-none-elf-gcc` → not found).
  - F6/F10/F11/F13/F14/F15/F16: not exercised by tests.

## Recommended fixes, in priority order

1. F2 (bitmap lifetime) — security/dangling pointer.
2. F5 (linker overlap) — silently bricks storage.
3. F4 (SysTick) — without this, runtime is non-functional.
4. F6 (placeholder binary) — protect users from bricking via UI.
5. F1 (init order) — first-packet loss.
6. F8 (weak SysTick handler) — link error when integrating vendor SDK.
7. F11 (loop overflow), F14 (CRC OOB), F16 (silent NAK) — correctness.
8. Then F7, F13, F15, F22, F24.
9. Docs cleanup: D1, D2, D3, D4, D7 — align the public story with code.