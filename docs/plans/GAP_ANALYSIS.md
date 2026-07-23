# SayoBot O3C Firmware — Gap Analysis Report

> **Date**: 2025-07-23
> **Scope**: All plan documents (`docs/`) vs. implemented code (`firmware/`, `webui/`)
> **Verdict**: **~30% implemented** — core scaffolding done, most planned features missing

---

## Executive Summary

The project has solid architectural foundations — protocol codec, display compositor, key scanner, and dispatcher are implemented with host-native tests. However, the majority of planned functionality (macro VM, config storage, real USB stack, CLI tool, Web UI) exists only as headers/stubs or is entirely absent. The firmware cannot run on real hardware in its current state.

---

## 1. Development Phases (Plan Appendix B) — Status

| Phase | Planned | Status | Detail |
|-------|---------|--------|--------|
| **P0: Toolchain** | Build system, linker script, startup asm, blink LED | ⚠️ PARTIAL | Makefile ✅, linker script ✅, startup.S ✅, but `sys_init()` has no real clock/PLL setup — just zeros a counter. No LED blink. No vendor SDK integrated. |
| **P1: USB HID** | Enumerate as HID device, respond to Ping | ⚠️ PARTIAL | Protocol codec ✅, Ping handler ✅, but `usb.c` is a **test stub only** — no real USBHS device stack. Device cannot enumerate on a bus. |
| **P2: Key Input** | Scan 3 keys, send HID keyboard reports | ⚠️ PARTIAL | `keys.c` scanner with debounce ✅, `keys_default_hid_usage()` ✅, but `hal_key_read()` is a weak stub returning `false`. No GPIO config. No HID keyboard report generation/sending. |
| **P3: Display** | Init OLED, render text, accept DisplayTextLayer from host | ⚠️ PARTIAL | Compositor ✅ (16-layer, text/fill/bitmap), SSD1306 driver ✅ (init sequence, page writes), font_5x7 ✅, DisplayText command dispatch ✅. But `hal_spi_send()` / `hal_gpio_write()` are weak stubs. No real SPI init. |
| **P4: CLI (sayocli)** | `sayocli display`, `sayocli status`, `sayocli ping` | ❌ **NOT STARTED** | No CLI tool exists anywhere in the repo. Plan specifies Rust + hidapi. Zero code. |
| **P5: Macro VM** | Bytecode interpreter, DSL compiler | ❌ **NOT STARTED** | `sayofw_config.h` defines `MACRO_SLOTS`/`MACRO_STEPS_PER_SLOT` constants. `commands.h` defines `CMD_V3_MACRO_DEFINE` (0x83) and `CMD_V3_MACRO_RUN` (0x84). Dispatcher has **stub handlers** that do nothing. No VM, no bytecode interpreter, no DSL compiler. |
| **P6: Config Storage** | Flash save/load, dual-bank wear-leveling, export/import | ⚠️ STUB | `storage/config.h` declares full API (`config_init`, `config_save`, `config_load`, `config_factory_reset`, `config_export`, `config_import`). `config.c` implements `config_init` (zeros struct), `config_load`/`config_save` are **no-ops** (always return true). No flash driver, no wear-leveling, no export/import. |
| **P7: Web UI** | SvelteKit + WebHID dashboard, key mapper, macro editor | ❌ **NOT STARTED** | `webui/` directory tree exists but **all subdirectories are empty** — no source files, no `package.json`, no components, no SvelteKit scaffolding. |
| **P8: Polish** | Display designer, LED config, FW update trigger, docs | ❌ **NOT STARTED** | None of these features exist. |

---

## 2. Module Breakdown (Plan §2) — Status

### 2.1 Implemented Modules

| Module | Files | Completeness | Notes |
|--------|-------|-------------|-------|
| **Protocol Codec** | `codec.h/c` | ✅ **Complete** | v3 checksum, finalize, verify, sub-command iterator, response builder, ping builder, streaming RX state machine. Well-tested. |
| **Protocol Dispatcher** | `dispatch.c` | ⚠️ 40% | Handles `Ping`, `Info`, `DisplayText`, `DisplayClear`. Macro commands are stubs. No `Save`, `DeviceName`, `SysInfo`, `Setting`, `MemoryRead/Write`. |
| **Protocol Pump** | `pump.c` | ✅ **Complete** | Ring buffer RX queue, drains to dispatcher. |
| **HID Transport** | `hid.h`, `usb.c` | ⚠️ 20% | Header defines report IDs, offsets, echo byte. `usb.c` is test-only (memcpy to buffer). No real USB device stack. |
| **Display Compositor** | `compositor.h/c` | ✅ **Complete** | 16-layer system, fill/text/bitmap kinds, dirty-rect tracking, flatten-to-framebuffer. Tested. |
| **Display Driver** | `driver.h`, `driver.c`, `driver_ssd1306.c` | ⚠️ 70% | SSD1306 init sequence correct, page-write logic correct, auto-detect placeholder. HAL calls are weak stubs (no real SPI). |
| **Display Manager** | `display.h`, `display.c`, `display_main.c` | ✅ **Complete** | `display_init()` wires compositor + driver, `display_flush()` pushes dirty pages. Startup screen rendering. |
| **Font** | `font.h`, `font_5x7.c` | ✅ **Complete** | Full ASCII 5×7 bitmap font (printable range 0x20–0x7E). |
| **Key Scanner** | `keys.h`, `keys.c` | ⚠️ 60% | Debounce logic ✅, consume-press API ✅, default HID usage map ✅. Missing: real GPIO reads, HID report generation, layer/remap support. |
| **Config Storage** | `config.h`, `config.c` | ⚠️ 15% | Data structures defined (key maps, macro slots, display presets, device metadata). All persistence functions are no-ops. |
| **System Init** | `sys.c` | ⚠️ 10% | Tick counter + ISR prototype. No PLL, no peripheral clock enable, no real SysTick config. |
| **Boot/Startup** | `startup_ch32v30x.S` | ⚠️ 50% | `.data` copy ✅, `.bss` zero ✅, vector table ✅. No `SystemInit()` call, no real interrupt vectors beyond reset. |

### 2.2 Planned but Unimplemented Modules

| Module | Plan Reference | Status |
|--------|---------------|--------|
| **Macro VM / Bytecode Interpreter** | Plan §2 `src/macro/`, §3 P5 | ❌ No files exist. No `src/macro/` directory. |
| **Macro DSL Compiler** | Plan §2 `tools/macro_compiler`, §3 P5 | ❌ No files. |
| **USB Device Stack (USBHS)** | Plan §1.4 `vendor/ch32v307/USB/` | ❌ No vendor SDK integrated. No USB descriptors. |
| **Vendor HAL / Peripheral Drivers** | Plan §1.4 `vendor/ch32v307/Peripheral/` | ❌ No vendor files copied. |
| **LED/RGB Controller** | Plan §3 P8, Architecture §4 | ❌ No LED code. |
| **DFU / Firmware Update** | Plan §3 P8, commands.h `CMD_V3_BOOTLOADER` | ❌ Command ID defined but no handler. |
| **sayocli (Rust CLI)** | Architecture §6, Plan §3 P4 | ❌ No `cli/` directory. No Rust code. |
| **Web UI (SvelteKit + WebHID)** | Architecture §7, Plan §3 P7 | ❌ Empty directory structure only. |
| **fw_decrypt.py** | Architecture Appendix A `tools/` | ❌ `tools/` directory is empty. |
| **hid_sniff.py** | Architecture Appendix A `tools/` | ❌ `tools/` directory is empty. |
| **scripts/** | Plan §2 `scripts/flash.sh` etc. | ❌ `scripts/` directory is empty. |

---

## 3. Protocol Commands — Coverage

| Cmd ID | Name | Planned | Implemented |
|--------|------|---------|-------------|
| `0x80` | Ping | ✅ | ✅ Full — echo token back |
| `0x81` | Info | ✅ | ✅ Responds with version, model, battery stub |
| `0x82` | DisplayText | ✅ | ✅ Sets compositor text layer |
| `0x85` | DisplayClear | ✅ | ✅ Clears all layers |
| `0x83` | MacroDefine | ✅ | ❌ **Stub** — switch case exists, body is empty |
| `0x84` | MacroRun | ✅ | ❌ **Stub** — switch case exists, body is empty |
| `0x86` | DisplayBitmap | ✅ | ❌ Not in dispatcher |
| `0x00` | MetaInfo (v1) | ✅ | ❌ No v1 protocol support |
| `0x01` | MemoryRead (v1) | ✅ | ❌ |
| `0x02` | MemoryWrite (v1) | ✅ | ❌ |
| `0x04` | Save | ✅ | ❌ |
| `0x06–0x09` | SimpleKey, DeviceName, etc. | ✅ | ❌ |
| `0x0C` | Text (v1) | ✅ | ❌ |
| `0x10–0x11` | Light, Palette | ✅ | ❌ |
| `0x16` | Key (full mapping) | ✅ | ❌ |
| `0x31–0x33` | Screen Start/Main/Sleep | ✅ | ❌ |
| `0xFC` | Option | ✅ | ❌ |
| `0xFF` | Bootloader | ✅ | ❌ Command ID defined, no handler |

---

## 4. Deliverables Checklist (Feasibility Report §6) — Status

| # | Deliverable | Status | Gap |
|---|------------|--------|-----|
| D1 | Firmware binary flashable via wchisp | ❌ | Compiles for host tests only. No real target build verified. No vendor SDK. |
| D2 | 3 keys work as HID keyboard | ❌ | Scanner exists but no GPIO, no HID report output |
| D3 | OLED shows status on boot | ⚠️ | Compositor + driver code exists but needs real SPI HAL |
| D4 | Host can push text to OLED via HID | ⚠️ | Protocol handler works in tests; needs real USB stack |
| D5 | Macro record/playback | ❌ | Zero implementation |
| D6 | Config persists across power cycles | ❌ | `config_save/load` are no-ops |
| D7 | sayocli ping/status/display | ❌ | CLI tool doesn't exist |
| D8 | Web UI connects via WebHID | ❌ | Empty directory |
| D9 | Web UI key remapper | ❌ | Empty directory |
| D10 | Web UI macro editor | ❌ | Empty directory |
| D11 | Firmware OTA update via HID | ❌ | No bootloader command handler |
| D12 | LED/RGB configuration | ❌ | No LED code |
| D13 | Documentation (user guide) | ⚠️ | Technical docs excellent; no user guide |

---

## 5. Testing Coverage

| Test Suite | File | What's Tested | Passing |
|-----------|------|---------------|---------|
| Codec | `tests/protocol/test_codec.c` | Checksum, finalize, verify, iterator, response builder, ping builder, RX state machine | ✅ 7 tests |
| Compositor | `tests/display/test_compositor.c` | Init, text layer, fill layer, overlap, clear, dirty rect, out-of-bounds, bitmap | ✅ 8 tests |
| Display | `tests/display/test_display.c` | Init, flush dirty, flush clean skip, text-layer-to-SPI flow | ✅ 4 tests |
| Dispatch | `tests/flasher/test_dispatch_text.c` | DisplayText push, Ping echo, DisplayClear | ✅ 3 tests |

**Missing test coverage:**
- Key scanner (no test file)
- Config storage (no test file)
- Macro VM (nothing to test yet)
- Integration / end-to-end (no test file)
- USB stack (nothing to test yet)

---

## 6. Critical Gaps Preventing Real Hardware Operation

These must be resolved before the firmware can run on an actual O3C device:

| # | Gap | Severity | Blocking |
|---|-----|----------|----------|
| **C1** | **No vendor SDK integrated** — no `ch32v30x.h`, no peripheral register definitions, no startup `SystemInit()` | 🔴 CRITICAL | Cannot compile for target |
| **C2** | **No real USB device stack** — `usb.c` is a test mock. No USBHS descriptors, no endpoint config, no interrupt handlers | 🔴 CRITICAL | Device won't enumerate on USB bus |
| **C3** | **No clock/PLL initialization** — `sys_init()` is a no-op on target. MCU would run at default 8MHz HSI, peripherals unconfigured | 🔴 CRITICAL | Peripherals won't function |
| **C4** | **No real GPIO configuration** — `hal_key_read()` returns false. Keys won't work | 🟠 HIGH | No input |
| **C5** | **No real SPI initialization** — `hal_spi_send()` / `hal_gpio_write()` are weak stubs. OLED won't display | 🟠 HIGH | No display output |
| **C6** | **No HID keyboard report generation** — keys scan but never produce USB HID reports | 🟠 HIGH | Not a functional keyboard |
| **C7** | **Interrupt vector table is mostly empty** — only reset vector populated in startup.S | 🟡 MEDIUM | No SysTick, no USB interrupts |

---

## 7. Non-Critical Gaps (Feature Completeness)

| # | Gap | Priority | Effort Est. |
|---|-----|----------|-------------|
| N1 | Macro VM / bytecode interpreter | P5 feature | Large (2-3 weeks) |
| N2 | Macro DSL compiler (host-side) | P5 feature | Medium (1 week) |
| N3 | sayocli Rust CLI tool | P4 feature | Medium (1-2 weeks) |
| N4 | Web UI (SvelteKit + WebHID) | P7 feature | Large (3-4 weeks) |
| N5 | Config flash persistence (dual-bank) | P6 feature | Medium (1 week) |
| N6 | LED/RGB controller | P8 feature | Small (3-5 days) |
| N7 | DFU / firmware update handler | P8 feature | Medium (1 week) |
| N8 | v1 protocol backward compat | Nice-to-have | Medium |
| N9 | Display bitmap command handler | P3 extension | Small |
| N10 | Helper scripts (flash.sh, fw_decrypt.py, hid_sniff.py) | Tooling | Small |

---

## 8. What IS Working Well

Credit where due — these are solid, tested, production-quality implementations:

1. **Protocol v3 codec** — bit-perfect checksum, sub-command batching, streaming RX. Matches the RE'd wire format exactly.
2. **16-layer display compositor** — clean API, dirty-rect optimization, text/fill/bitmap support. Well-tested.
3. **SSD1306 driver logic** — correct init sequence, page-based write, auto-detect stub for SH1106.
4. **Build system** — Makefile supports both host-test (`make test`) and cross-compile targets. Clean separation.
5. **Architecture** — clean module boundaries, weak-symbol HAL abstraction enables host testing. Header-only dependencies between modules.
6. **Test infrastructure** — 22 passing host-native tests covering codec, compositor, display, and dispatch.

---

## 9. Recommended Priority Order

To reach a functional device on real hardware:

1. **Integrate vendor SDK** (C1) — copy CH32V307 peripheral headers + startup
2. **Implement `sys_init()`** (C3) — PLL, peripheral clocks, SysTick
3. **Implement real USB device stack** (C2) — USBHS HID descriptors + endpoints
4. **Wire GPIO for keys** (C4) — configure pins, implement `hal_key_read()`
5. **Wire SPI for OLED** (C5) — configure SPI peripheral, implement `hal_spi_send()`
6. **Add HID keyboard reports** (C6) — key press → USB HID report
7. **Flash config persistence** (N5) — make `config_save/load` real
8. **Then** proceed to CLI, Macro VM, Web UI (P4-P8)

---

*End of gap analysis.*
