# SayoDevice O3C — Feasibility Assessment & Project Roadmap

> **Version**: 1.0.0  
> **Date**: 2026-07-23  
> **Author**: Business Analyst (automated analysis)  
> **Status**: FINAL DRAFT  
> **Prerequisites**: [System Architecture](./sayobot-o3c-system-architecture.md) · [Firmware Dev Plan](./sayobot-o3c-firmware-dev-plan.md) · [Technical Reference](./sayobot-o3c-technical-reference.md)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Feasibility Assessment](#2-feasibility-assessment)
3. [Risk Matrix](#3-risk-matrix)
4. [Team & Agent Requirements](#4-team--agent-requirements)
5. [Project Roadmap](#5-project-roadmap)
6. [Deliverables Checklist](#6-deliverables-checklist)
7. [Decision Log](#7-decision-log)

---

## 1. Executive Summary

### Verdict: ✅ FEASIBLE — Medium-High Complexity

This project proposes replacing the stock firmware of the Sayobot O3C (a 3-key OLED macropad) with custom firmware and companion software (CLI + Web UI) to enable full key remapping, macro programming, and real-time display push from a host computer.

**The project is technically feasible.** Every critical dependency has an open-source or freely available solution. However, the complexity is non-trivial — it spans bare-metal RISC-V firmware, USB HID protocol implementation, a cross-platform CLI tool, and a WebHID-based browser application. The primary risks are hardware bricking during development and incomplete reverse-engineering data for the display subsystem.

| Dimension | Assessment | Confidence |
|-----------|-----------|------------|
| MCU Identification | ✅ Confirmed: WCH CH32V307 (RISC-V) | **High** |
| Toolchain Availability | ✅ Freely available (3+ options) | **High** |
| Bootloader/DFU Mechanism | ✅ Well-documented, multiple flash tools | **High** |
| Legal/Licensing | ⚠️ Mixed — GPLv3 on Web UI, MIT on CLI/Manual | **High** |
| OLED Display Protocol | ⚠️ Likely SSD1306-family but unconfirmed | **Medium** |
| WebHID Browser Support | ⚠️ Chromium-only (Chrome, Edge, Opera) | **High** |
| Overall Feasibility | ✅ Go — with risk mitigations | **Medium-High** |

---

## 2. Feasibility Assessment

### 2.1 MCU Identification & Toolchain

#### MCU: WCH CH32V307 — Confirmed ✅

The MCU is **positively identified** from multiple independent sources:

| Source | Evidence |
|--------|----------|
| khang06 RE gist | "CH32FV2x_V3x Reference Manual" cited; CH32V307 SDK linked |
| Sayo_CLI source | Device model codes mapping O3C variants to "WCH variant" |
| SysInfo response | `cpu_freq`, `hclk`, `pclk_1`, `pclk_2` fields match CH32V307 clock tree |
| Firmware analysis | Application base at `0x4000`, bootloader at `0x0` — matches WCH memory map |

**Key specifications:**

| Property | Value |
|----------|-------|
| Architecture | RISC-V 32-bit (RV32IMAFCX), QingKe V4F core |
| Flash / RAM | 256 KB / 64 KB |
| Max Clock | 144 MHz |
| USB | Built-in USB 2.0 HS (480 Mbps) + FS |
| SPI | 3× (used for OLED) |
| Debug | Serial 2-wire (SWD-like, via WCH-Link) |

#### Toolchain: Freely Available ✅

Three independent, free installation paths exist:

| Option | Source | Platform | License |
|--------|--------|----------|---------|
| **xPack RISC-V GCC** | [github.com/xpack-dev-tools](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack) | Win/Mac/Linux | MIT |
| **MounRiver Studio** | [mounriver.com](https://www.mounriver.com/) | Win/Linux | Freeware (IDE); GCC is open |
| **System packages** | `pacman -S riscv-none-elf-gcc` (Arch) | Linux | GPL (GCC) |

**Compiler flags**: `-march=rv32imafcx -mabi=ilp32f -Os`

**SDK**: Official WCH CH32V307 EVT SDK at [github.com/openwch/ch32v307](https://github.com/openwch/ch32v307) — includes startup code, peripheral drivers (GPIO, SPI, USB HS, DMA, FLASH, RCC), and USB device stack. Apache-2.0 licensed.

**Build system**: GNU Make (matches vendor conventions).

**Assessment**: No toolchain barrier exists. The xPack option enables CI/CD without vendor lock-in.

---

### 2.2 Bootloader & DFU Mechanism

#### Bootloader: Well-Documented ✅

The stock bootloader behavior is fully understood from reverse-engineering:

```
Boot Sequence:
1. Bootloader at 0x0 runs on power-on
2. Reads firmware size from 0x29F84, MD5 hash from 0x29FA0
3. Computes MD5 of firmware region at 0x4000
4. Match → jump to application
5. Mismatch → stay in bootloader mode (awaiting re-flash)
```

**Critical finding**: If a flashed firmware has an invalid MD5, the device **automatically enters bootloader mode** rather than bricking. This is a significant safety net.

#### Flash Tools: Multiple Options ✅

| Tool | Type | Tested on CH32V307 | Platform | License |
|------|------|-------------------|----------|---------|
| **wchisp** (Rust) | USB ISP | ✅ Confirmed (VCT6 + RCT6) | Win/Mac/Linux | MIT/Apache-2.0 |
| **wch-isp** (C) | USB ISP | ✅ Confirmed (CH32V307VCT6) | Linux | Open source |
| **WCHISPTool** | USB ISP (official) | ✅ Official vendor tool | Win/Linux | Proprietary freeware |
| **wlink** (Rust) | WCH-Link debug | ✅ SWD-equivalent | Win/Mac/Linux | MIT/Apache-2.0 |
| **HID cmd 0xFF** | In-firmware USB | ✅ Stock protocol | Cross-platform | N/A |

**Three distinct flash paths**:

1. **USB ISP (bootloader mode)**: Hold BOOT0 during reset → device appears as WCH ISP device → flash with `wchisp flash firmware.bin`
2. **WCH-Link (SWD debug)**: Connect WCH-Link adapter → flash + debug with `wlink` or MounRiver IDE
3. **In-app HID command**: Send command `0xFF` via USB HID → device enters bootloader → flash new image

**Assessment**: Flashing is well-supported with open-source tools. The MD5 safety mechanism prevents permanent bricking. The `wchisp` Rust tool is the recommended primary option.

---

### 2.3 Legal & Licensing Analysis

#### Sayobot Repository Licenses

| Repository | Stars | License | Implications |
|-----------|-------|---------|-------------|
| **SayoDevice_Web** (Vue 2 desktop UI) | 17 | **GPLv3** | ⚠️ Copyleft — derivative works must also be GPLv3 |
| **sayo-device-web-hid** (Angular WebHID UI) | 78 | **No license file** | ⚠️ All rights reserved by default |
| **Sayo_CLI** (C++ CLI tool) | 56 | **MIT** | ✅ Permissive — can use freely |
| **SayoDevice_manual** (documentation) | 169 | **MIT** | ✅ Permissive — can reference freely |

#### License Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|-----------|
| GPLv3 on SayoDevice_Web | **Medium** | We are writing new firmware and tools from scratch. As long as we don't copy GPLv3 code verbatim, no infection. The protocol is not copyrightable — only the expression is. |
| No license on sayo-device-web-hid | **Low** | We won't fork or copy this Angular app. Our Web UI is a clean-room SvelteKit implementation using only the publicly documented protocol. |
| MIT on Sayo_CLI | **None** | Protocol structure definitions (o2_protocol.h) can be studied freely. MIT allows derivative use with attribution. |
| AES key in public gist | **Low** | The decryption key is published by a third party (khang06). We use it only to understand the firmware structure — our custom firmware is unencrypted during development. |
| WCH SDK (Apache-2.0) | **None** | Permissive. We vendor SDK files and comply with attribution requirements. |

**Legal recommendation**: 
- License our custom firmware under **MIT** or **Apache-2.0** (permissive, avoids GPLv3 complexity).
- Do **not** copy any code from `SayoDevice_Web` (GPLv3) or `sayo-device-web-hid` (no license).
- Protocol knowledge is derived from clean-room RE (khang06 gist) and MIT-licensed Sayo_CLI sources. This is legally defensible.
- Include attribution for the WCH SDK per Apache-2.0 terms.

---

### 2.4 OLED Display Protocol

#### Display Controller: Likely SSD1306-family ⚠️

| Evidence | Points To |
|----------|-----------|
| SPI interface (3 SPI peripherals on CH32V307, OLED uses one) | SPI-mode OLED (SSD1306/SSD1315/SH1106) |
| Framebuffer dump via API v2 cmd `0x25` (Display) — returns `uint16_t` array | **16-bit color** or **16-bit-packed monochrome** |
| Screen layers support Color fill, Widget rendering, Image display | Composited on-MCU, standard OLED commands |
| `SysInfo` reports `width` and `height` — resolution is queryable at runtime | Standard controller behavior |
| CH32V307 + SSD1306 combination has [documented open-source drivers](https://github.com/Flogger69/ch32V307_ssd1306) | Known working combination |

**Uncertainty**: The gist does not explicitly name the OLED controller IC. The `uint16_t framebuffer` in the Display command response could indicate:
- **SSD1306** (monochrome, 128×64 or 128×32) with 16-bit packed data format
- **SSD1315** (SSD1306 successor, same protocol)
- **SH1106/SH1107** (SSD1306-compatible)
- A color OLED (SSD1331, SSD1351) if the device has a color screen

**Resolution strategy**: Query `SysInfo` on a real device to get `width` and `height`. Then compare framebuffer size from `Display` command response against known controller capabilities. All candidate controllers use standard SPI command sets — driver code is portable across the family.

**Assessment**: **Medium confidence** that the controller is SSD1306 or a close relative. The existing CH32V307 + SSD1306 open-source driver proves the combination works. Worst case, we need 2-3 days to identify and adapt to the actual controller.

---

### 2.5 WebHID Browser Compatibility

#### Support Matrix (as of July 2026)

| Browser | Desktop | Mobile | Status |
|---------|---------|--------|--------|
| **Chrome** | ✅ 89+ (Mar 2021) | ❌ Android: No | Stable, 5+ years |
| **Edge** | ✅ 89+ (Mar 2021) | ❌ | Stable (Chromium-based) |
| **Opera** | ✅ 76+ (Jun 2021) | ❌ | Stable |
| **Safari** | ❌ | ❌ iOS | No implementation planned |
| **Firefox** | ❌ | ❌ | [Positive position](https://github.com/nicman23/nicman23.github.io/issues/38) but no timeline |

**Market coverage**: Chrome (65%) + Edge (13%) + Opera (3%) = **~81% desktop browser share** supports WebHID.

#### WebHID with SayoDevice — Specific Considerations

| Factor | Assessment |
|--------|-----------|
| VID/PID filtering | ✅ WebHID supports vendor/product ID filtering (`0x8089`/`0x0009`) |
| Usage page access | ✅ Custom usage pages (0xFF20) accessible — WebHID can open any HID interface |
| Report size (1024B) | ✅ WebHID handles arbitrary report sizes via `sendFeatureReport()` / `sendReport()` |
| High-speed (8000Hz) | ⚠️ WebHID operates at browser event loop speed (~60Hz). Polling rate is device-side; host reads arrive asynchronously. For configuration (not gaming), this is fine. |
| Security model | ✅ Requires HTTPS or localhost. User must grant permission via picker. No silent access. |
| Concurrent access | ⚠️ Only one application can claim an HID interface at a time. CLI and Web UI cannot run simultaneously on the same device. |

**Assessment**: WebHID is **production-ready** for this use case. The 81% desktop coverage is acceptable for a niche keyboard enthusiast tool. Safari/Firefox users can fall back to the CLI tool.

---

## 3. Risk Matrix

### Risk Scoring Framework

- **Likelihood**: 1 (Rare) → 5 (Almost Certain)
- **Impact**: 1 (Negligible) → 5 (Catastrophic)
- **Risk Score**: Likelihood × Impact
- **Threshold**: ≤6 Accept, 7-12 Mitigate, ≥13 Avoid/Redesign

### 3.1 Risk Register

| ID | Risk | Likelihood | Impact | Score | Category | Status |
|----|------|-----------|--------|-------|----------|--------|
| R1 | **Hardware lockout / bricking** — Custom firmware crashes before USB init, device appears dead | 2 | 5 | **10** | 🟡 Mitigate | Open |
| R2 | **Incomplete RE data** — Critical protocol commands still marked "TODO" in gist | 3 | 4 | **12** | 🟡 Mitigate | Open |
| R3 | **Display controller unknown** — OLED IC is not SSD1306, requires custom driver | 2 | 3 | **6** | 🟢 Accept | Open |
| R4 | **WebHID browser support** — Safari/Firefox never implement WebHID, limiting user base | 4 | 2 | **8** | 🟡 Mitigate | Open |
| R5 | **Macro timing precision** — USB polling jitter + MCU timer resolution makes sub-ms macros unreliable | 3 | 3 | **9** | 🟡 Mitigate | Open |
| R6 | **GPLv3 license contamination** — Accidental code copying from SayoDevice_Web infects our codebase | 1 | 5 | **5** | 🟢 Accept | Open |
| R7 | **WCH SDK quality** — Vendor peripheral drivers have bugs (known issue in community) | 3 | 3 | **9** | 🟡 Mitigate | Open |
| R8 | **Flash endurance** — Frequent config saves wear out 256KB flash (typical: 10K-100K cycles) | 2 | 3 | **6** | 🟢 Accept | Open |
| R9 | **Firmware size budget** — 256KB flash minus 16KB bootloader = 240KB. Complex macro VM + display engine may exceed | 2 | 4 | **8** | 🟡 Mitigate | Open |
| R10 | **Single developer / bus factor** — Firmware expertise is rare; loss of key contributor stalls project | 3 | 4 | **12** | 🟡 Mitigate | Open |

### 3.2 Mitigation Strategies

#### R1: Hardware Lockout (Score: 10)

| Mitigation | Effectiveness |
|-----------|--------------|
| **MD5 safety net**: Stock bootloader auto-enters DFU on hash mismatch — device is recoverable | High |
| **WCH-Link backup**: Even if USB ISP fails, SWD debug interface can force-flash | High |
| **Develop on second device**: Keep one device with stock firmware as reference | Medium |
| **Incremental flashing**: Start with minimal "blink LED" firmware; validate flash/recovery cycle before adding USB stack | High |

**Residual risk after mitigation**: **Low** (Score: 4). The MD5 bootloader + SWD backup makes permanent bricking nearly impossible without physical damage.

#### R2: Incomplete RE Data (Score: 12)

| Mitigation | Effectiveness |
|-----------|--------------|
| **USB packet sniffing**: Use Wireshark + USBPcap to capture stock firmware ↔ official tool communication for all "TODO" commands | High |
| **Ghidra analysis**: Decrypted firmware binary is loadable; command handlers can be traced | High |
| **Sayo_CLI source code**: MIT-licensed C++ implementation contains protocol structs for most commands | High |
| **Incremental approach**: Implement known commands first (Info, Key, Display); add others as reverse-engineered | Medium |

**Residual risk after mitigation**: **Low** (Score: 6). Between packet sniffing, Ghidra, and the CLI source, all commands are discoverable.

#### R3: Display Controller Unknown (Score: 6)

| Mitigation | Effectiveness |
|-----------|--------------|
| **Runtime detection**: Query SysInfo for resolution, then try SSD1306 init sequence — if it works, confirmed | High |
| **Visual inspection**: Opening the device reveals the OLED module part number | High |
| **Portable driver design**: Abstract display API with swappable backends for SSD1306/SH1106/SSD1315 | High |

**Accepted**: Low risk. SSD1306-family covers >90% of small OLED modules. Driver swap is a 1-day effort.

#### R4: WebHID Browser Support (Score: 8)

| Mitigation | Effectiveness |
|-----------|--------------|
| **CLI tool as fallback**: `sayocli` provides 100% functionality without browser dependency | High |
| **Electron/Tauri wrapper**: If browser support is critical, wrap the Web UI in a desktop app with native HID access | Medium |
| **PWA with degraded experience**: Detect WebHID absence, show download link for CLI tool | Medium |

**Residual risk after mitigation**: **Low** (Score: 4). CLI ensures all platforms are covered.

#### R5: Macro Timing Precision (Score: 9)

| Mitigation | Effectiveness |
|-----------|--------------|
| **On-device execution**: Macros run entirely on the MCU using hardware timers (144MHz, sub-μs precision) | High |
| **Decouple from USB polling**: Macro VM ticks independently of USB frame timing | High |
| **Configurable delay granularity**: Support both millisecond and microsecond delay opcodes | Medium |
| **Timing test suite**: Automated tests with logic analyzer to validate macro timing | Medium |

**Residual risk after mitigation**: **Low** (Score: 3). On-device execution eliminates host-side jitter entirely.

#### R7: WCH SDK Quality (Score: 9)

| Mitigation | Effectiveness |
|-----------|--------------|
| **Vendor SDK files in-tree**: Copy minimal required files, patch bugs locally | High |
| **Community patches**: ch32-rs community actively maintains fixes | Medium |
| **Register-level fallback**: For critical peripherals (USB, SPI), bypass HAL and use direct register access | High |

#### R9: Firmware Size Budget (Score: 8)

| Mitigation | Effectiveness |
|-----------|--------------|
| **`-Os` optimization**: Size-optimized compilation is baseline | High |
| **Modular design**: Macro VM and display engine can be compile-time optional | Medium |
| **LTO (Link-Time Optimization)**: `-flto` eliminates dead code across translation units | High |
| **Size tracking in CI**: Fail build if binary exceeds 200KB threshold | High |

#### R10: Bus Factor (Score: 12)

| Mitigation | Effectiveness |
|-----------|--------------|
| **Comprehensive documentation**: Architecture doc, firmware dev plan, and this feasibility report | High |
| **Modular codebase**: Each component (USB, display, macro, key input) is independently testable | Medium |
| **Agent specialization**: Multiple specialist agents reduce single-point-of-failure | High |
| **Protocol spec as contract**: Once protocol v3 is specified, firmware and host tools can develop independently | High |



---

## 4. Team & Agent Requirements

### 4.1 Recommended Team Composition

Based on the project's three-component architecture (firmware + CLI + Web UI) and the risk profile, the following specialist agents are required:

| Role | Persona | Primary Responsibility | Deliverables |
|------|---------|----------------------|-------------|
| **Embedded Systems Engineer** | `embedded-engineer` | CH32V307 firmware: USB HID stack, key scanning, macro VM, OLED driver, flash storage, bootloader integration | `sayofw_o3c.bin`, linker scripts, hardware abstraction layer |
| **Protocol Engineer** | `protocol-engineer` | USB HID protocol v3 spec, packet framing, checksum logic, command serialization/deserialization libraries (shared between firmware + host tools) | Protocol spec document, C + Rust + TypeScript codec libraries |
| **Systems Programmer (CLI)** | `systems-programmer` | Rust CLI tool (`sayocli`): device discovery, display push, macro upload, config import/export, firmware update trigger | `sayocli` binary (cross-platform) |
| **Frontend Engineer** | `frontend-engineer` | SvelteKit Web UI: WebHID device connection, key mapper, macro editor, display preview, real-time device status | Web application (static SPA) |
| **QA / Test Engineer** | `qa-engineer` | Integration testing: USB packet validation, firmware ↔ CLI ↔ Web UI round-trip tests, macro timing verification | Test suites, CI pipeline, test fixtures |
| **Technical Writer** | `tech-writer` | User-facing docs: getting started guide, flashing instructions, macro DSL reference, API reference | Documentation site (mdBook or Docusaurus) |

### 4.2 Role Definitions

#### Embedded Systems Engineer (Critical Path)
- **Skills**: C, RISC-V assembly, bare-metal firmware, USB device stacks, SPI/I2C peripherals, real-time systems
- **Why needed**: The firmware is the project's foundation. Incorrect USB HID implementation or display driver bugs block all downstream work. This role requires domain expertise in register-level MCU programming that general software engineers lack.
- **Effort**: ~60% of total project effort
- **Risk owned**: R1 (bricking), R7 (SDK bugs), R9 (size budget)

#### Protocol Engineer (Shared Dependency)
- **Skills**: Binary protocol design, USB HID specification, serialization formats, cross-language library development
- **Why needed**: The protocol is the contract between firmware and all host tools. A dedicated role ensures consistency, backward compatibility, and proper documentation. Protocol bugs cause cascading failures across all components.
- **Effort**: ~10% of total project effort
- **Risk owned**: R2 (incomplete RE data), R5 (macro timing)

#### Systems Programmer — CLI (Parallel Track)
- **Skills**: Rust, async I/O, `hidapi`/`nusb` crates, cross-platform builds, CLI UX (clap)
- **Why needed**: The CLI is the primary interface for power users and the fallback for non-Chromium browsers. It also serves as the reference implementation for the protocol — bugs found here feed back to the protocol spec.
- **Effort**: ~10% of total project effort
- **Risk owned**: R4 (WebHID fallback)

#### Frontend Engineer (Parallel Track)
- **Skills**: SvelteKit/TypeScript, WebHID API, responsive UI design, real-time data visualization
- **Why needed**: The Web UI is the primary interface for most users. WebHID's security model (HTTPS, user permission) and async event-driven API require specific frontend expertise.
- **Effort**: ~15% of total project effort
- **Risk owned**: R4 (WebHID browser support)

#### QA Engineer + Technical Writer (~5% combined)
- Can be handled by existing agents or the primary engineers during the polish phase.

### 4.3 Dependency Graph

```
Protocol Engineer ──────────────────────────┐
  (Protocol v3 Spec)                        │
       │                                    │
       ▼                                    ▼
Embedded Engineer ──► Firmware ◄── Protocol Codec (C)
       │                                    │
       │              Protocol Codec (Rust) ─┤
       │                   │                │
       │                   ▼                │
       │           Systems Programmer       │
       │              (sayocli)             │
       │                                    │
       │              Protocol Codec (TS) ──┘
       │                   │
       │                   ▼
       │           Frontend Engineer
       │              (Web UI)
       │
       ▼
    QA Engineer ◄── All components
```

**Critical path**: Protocol Spec → Firmware USB HID → CLI MVP → Web UI

---

## 5. Project Roadmap

### 5.1 Overview

| Milestone | Duration | Cumulative | Gate Criteria |
|-----------|----------|-----------|---------------|
| **M0: Foundation** | Weeks 1–2 | 2 weeks | Toolchain builds, LED blinks on device, flash/recovery cycle validated |
| **M1: USB HID** | Weeks 3–4 | 4 weeks | Device enumerates as HID, responds to Ping, host reads device info |
| **M2: Key Input** | Week 5 | 5 weeks | 3 keys send HID keyboard reports to host OS |
| **M3: Display** | Weeks 5–6 | 6 weeks | OLED controller identified, static text renders, host can push text |
| **M4: CLI MVP** | Weeks 6–7 | 7 weeks | `sayocli status`, `sayocli display "Hello"`, `sayocli ping` work |
| **M5: Macro Engine** | Weeks 8–9 | 9 weeks | Bytecode VM runs macros, `sayocli macro set/run` works |
| **M6: Config Storage** | Week 10 | 10 weeks | Key maps + macros + display config persist across power cycles |
| **MVP RELEASE** | Week 10 | — | **Basic key remapping + display push via CLI** |
| **M7: Web UI** | Weeks 11–14 | 14 weeks | SvelteKit app: device connect, key mapper, display push, macro editor |
| **M8: Polish** | Weeks 15–16 | 16 weeks | Display designer, LED config, firmware OTA, full documentation |
| **FULL RELEASE** | Week 16 | — | **Complete Web UI + macros + full display control** |

### 5.2 Week-by-Week Detail

#### Phase 1: MVP (Weeks 1–10)

| Week | Focus | Tasks | Owner | Deliverable |
|------|-------|-------|-------|------------|
| **1** | Toolchain & Environment | Install xPack RISC-V GCC; clone WCH SDK; create project skeleton with Makefile; write linker script for app region (base 0x4000) | Embedded Eng | Build system compiles empty firmware |
| **1** | Protocol Spec | Document protocol v3 command table; define packet framing; write checksum routines in C + Rust + TS | Protocol Eng | `docs/protocol-v3-spec.md`, codec libs |
| **2** | Hello World Firmware | Minimal firmware: clock init, GPIO toggle (LED blink), validate flash via `wchisp` and WCH-Link; test recovery by flashing intentionally bad image | Embedded Eng | `sayofw_o3c.bin` (blink), flash recovery confirmed |
| **2** | CLI Skeleton | Rust project init (`cargo init sayocli`); add `nusb`/`hidapi` dep; implement device discovery by VID/PID | Systems Prog | `sayocli list` shows connected device |
| **3** | USB HID Device Stack | Implement USB HS device descriptor, HID report descriptor (usage page 0xFF20), endpoint configuration; handle SET_REPORT/GET_REPORT | Embedded Eng | Device enumerates in OS device manager |
| **4** | Protocol Handler | Implement v2 packet framing on firmware side; handle Info (0x00) and SysInfo (0x02) commands; echo responses | Embedded Eng + Protocol Eng | `sayocli status` returns device info |
| **5** | Key Scanning | Configure GPIO for 3 keys; debounce; generate HID keyboard reports (6KRO + NKRO); implement Key (0x10) command for remapping | Embedded Eng | Keys type characters; remap via `sayocli key set` |
| **5–6** | OLED Display Init | Identify controller via SysInfo resolution; implement SSD1306 SPI driver; render static text; implement DisplayTextLayer command | Embedded Eng | OLED shows text; `sayocli display "msg"` pushes text |
| **6–7** | CLI Display Push | Implement real-time display push loop: stdin pipe → HID report → device renders; add `sayocli display --watch` for continuous streaming | Systems Prog | `echo "CPU: 42%" \| sayocli display --push` works |
| **7** | CLI Config Commands | Implement `sayocli config export/import` (JSON); `sayocli key get/set`; `sayocli save` | Systems Prog | Full CLI config management |
| **8–9** | Macro VM | Register-based bytecode interpreter on firmware; opcodes: KEY_DOWN, KEY_UP, DELAY_MS, DELAY_US, LOOP, JUMP; implement ScriptStep (0x1A) command | Embedded Eng | Macros execute on-device with correct timing |
| **8–9** | Macro DSL Compiler | Human-readable DSL → bytecode compiler in Rust (runs in `sayocli macro compile`); syntax: `key(a) delay(50ms) key(b)` | Systems Prog | `sayocli macro set 1 "key(a) delay(50) key(b)"` |
| **10** | Config Persistence | Flash storage driver: wear-leveled config sectors; save/load key maps, macros, display layers; implement Save (0x0D) command | Embedded Eng | Config survives power cycle |
| **10** | **MVP Gate Review** | End-to-end test: remap key → type character → push display text → execute macro → power cycle → verify persistence | QA | **MVP Release: v0.1.0** |

#### Phase 2: Full Release (Weeks 11–16)

| Week | Focus | Tasks | Owner | Deliverable |
|------|-------|-------|-------|------------|
| **11** | Web UI Foundation | SvelteKit project init; WebHID connection manager; device picker (VID 0x8089, PID 0x0009); protocol codec (TypeScript) | Frontend Eng | Browser connects to device, shows device info |
| **12** | Key Mapper UI | Visual key layout (3 keys); drag-and-drop remap; layer support (5 layers per key); save to device | Frontend Eng | Visual key remapping in browser |
| **13** | Macro Editor UI | Visual macro builder: step list with key/delay/loop blocks; DSL text editor with syntax highlighting; compile + upload | Frontend Eng | Create and upload macros from browser |
| **13** | Display Designer UI | WYSIWYG display editor: text layers, widget placement, image upload; live preview via WebHID push | Frontend Eng | Design display layouts in browser |
| **14** | Real-time Dashboard | Live device status: CPU usage, key press indicators, uptime, ADC readings; WebSocket-like polling via HID reports | Frontend Eng | Real-time monitoring dashboard |
| **15** | LED & Advanced Config | LED lighting commands (Light 0x11, Palette 0x12); screen layer management (ScreenStart/Main/Sleep); device lock/unlock | Embedded Eng + Frontend Eng | Full feature parity with stock tools |
| **15** | Firmware OTA via UI | Web UI triggers firmware update: upload `.bin` → send via HID → device verifies + reboots; progress bar | Embedded Eng + Frontend Eng | Browser-based firmware update |
| **16** | Documentation & Polish | User guide (mdBook); API reference; flashing tutorial; macro DSL reference; architecture diagrams; README | Tech Writer | Documentation site |
| **16** | **Release Gate Review** | Full regression test; cross-browser testing (Chrome, Edge, Opera); cross-platform CLI testing (Win/Mac/Linux) | QA | **Full Release: v1.0.0** |

### 5.3 Gantt Summary

```
Week  1  2  3  4  5  6  7  8  9  10  11  12  13  14  15  16
      ├──┤                                                     Toolchain/Hello World
         ├─────┤                                               USB HID Stack
               ├──┤                                            Key Scanning
            ├─────┤                                            OLED Display
      ├──┤                                                     Protocol Spec
         ├──────────────────┤                                  CLI Tool
                        ├─────┤                                Macro VM
                              ├──┤                             Config Storage
                              ├──┤                             ★ MVP v0.1.0
                                  ├──────────────────┤         Web UI
                                                 ├─────┤      LED/Advanced
                                                       ├──┤   Docs/Polish
                                                       ├──┤   ★ Full v1.0.0
```

---

## 6. Deliverables Checklist

### 6.1 Firmware Artifacts

| # | Artifact | Format | Description | MVP | Full |
|---|----------|--------|-------------|-----|------|
| F1 | `sayofw_o3c.bin` | Binary | Flashable firmware image (base address 0x4000) | ✅ | ✅ |
| F2 | `sayofw_o3c.hex` | Intel HEX | Alternative flash format for WCH tools | ✅ | ✅ |
| F3 | `sayofw_o3c.elf` | ELF | Debug-symbol firmware for GDB/WCH-Link | ✅ | ✅ |
| F4 | `sayofw_o3c.map` | Linker map | Memory layout and symbol addresses | ✅ | ✅ |
| F5 | Linker script | `.ld` | Memory regions, section placement | ✅ | ✅ |
| F6 | Makefile | GNU Make | Build system with `all`, `clean`, `flash`, `size` targets | ✅ | ✅ |

### 6.2 CLI Tool Artifacts

| # | Artifact | Format | Description | MVP | Full |
|---|----------|--------|-------------|-----|------|
| C1 | `sayocli` | Rust binary | Cross-platform CLI (Linux/macOS/Windows) | ✅ | ✅ |
| C2 | Shell completions | bash/zsh/fish | Auto-generated via `clap` | — | ✅ |
| C3 | Man page | `sayocli.1` | Generated from CLI help | — | ✅ |

### 6.3 Web UI Artifacts

| # | Artifact | Format | Description | MVP | Full |
|---|----------|--------|-------------|-----|------|
| W1 | Web application | Static SPA (HTML/JS/CSS) | SvelteKit build output, deployable to any static host | — | ✅ |
| W2 | WebHID protocol codec | TypeScript library | Reusable `@sayofw/hid-codec` package | — | ✅ |

### 6.4 Documentation Artifacts

| # | Artifact | Format | Description | MVP | Full |
|---|----------|--------|-------------|-----|------|
| D1 | Protocol v3 Specification | Markdown | Complete command reference with packet diagrams | ✅ | ✅ |
| D2 | Getting Started Guide | Markdown/mdBook | Toolchain install → build → flash → first use | ✅ | ✅ |
| D3 | Flashing Guide | Markdown | Step-by-step: wchisp, WCH-Link, recovery procedures | ✅ | ✅ |
| D4 | Macro DSL Reference | Markdown | Syntax, opcodes, examples | — | ✅ |
| D5 | API Reference | Generated docs | Rust: `cargo doc`; TypeScript: typedoc | — | ✅ |
| D6 | Architecture Decision Records | Markdown | This document + system architecture + firmware dev plan | ✅ | ✅ |

### 6.5 Development Infrastructure

| # | Artifact | Format | Description | MVP | Full |
|---|----------|--------|-------------|-----|------|
| I1 | CI pipeline | GitHub Actions | Build firmware + CLI + Web UI on every push | ✅ | ✅ |
| I2 | Protocol test fixtures | JSON/binary | Known-good packets for regression testing | ✅ | ✅ |
| I3 | USB HID packet sniffer | Python script | `tools/hid_sniff.py` for debugging | ✅ | ✅ |
| I4 | Firmware decryption tool | Python script | `tools/fw_decrypt.py` for stock firmware analysis | ✅ | ✅ |

---

## 7. Decision Log

| ID | Date | Decision | Rationale | Alternatives Considered |
|----|------|---------|-----------|------------------------|
| D1 | 2026-07-22 | Use RISC-V GCC (not ARM) | MCU is CH32V307 (RISC-V), not STM32 (ARM) | None — architecture dictates toolchain |
| D2 | 2026-07-22 | GNU Make over CMake | Matches WCH SDK conventions; simpler for bare-metal | CMake (rejected: over-engineered for this project) |
| D3 | 2026-07-22 | Rust for CLI tool | Memory safety, excellent cross-platform support, `hidapi`/`nusb` ecosystem | Go (weaker HID libs), Python (distribution issues), C++ (memory safety risk) |
| D4 | 2026-07-22 | SvelteKit for Web UI | Lightweight, fast SSG, excellent DX, smaller bundle than React/Angular | React (larger bundle), Angular (too heavy for SPA) |
| D5 | 2026-07-22 | Protocol v3 extends v2 framing | Backward compatibility with stock firmware during incremental dev | Clean-break protocol (rejected: loses ability to test against stock FW) |
| D6 | 2026-07-23 | MIT license for all custom code | Permissive; avoids GPLv3 complications from SayoDevice_Web | Apache-2.0 (also acceptable), GPLv3 (rejected: copyleft unnecessary) |
| D7 | 2026-07-23 | wchisp as primary flash tool | Open-source (Rust), tested on CH32V307, cross-platform | WCHISPTool (vendor, Windows-primary), wch-isp (C, Linux-only) |
| D8 | 2026-07-23 | CLI as WebHID fallback | Ensures 100% platform coverage regardless of browser support | Electron wrapper (rejected: too heavy for a CLI-capable audience) |

---

*End of feasibility and planning document.*
