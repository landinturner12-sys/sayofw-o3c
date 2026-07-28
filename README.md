<p align="center">
  <h1 align="center">⌨️ SayoFW — Custom Firmware for the Sayobot O3C Macropad</h1>
  <p align="center">
    Clean-room RISC-V firmware &amp; companion tools for the SayoDevice O3C 3-key OLED macropad
  </p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/MCU-CH32V307_(RISC--V)-blue?style=flat-square" alt="MCU">
  <img src="https://img.shields.io/badge/arch-RV32IMAFCX-informational?style=flat-square" alt="Arch">
  <img src="https://img.shields.io/badge/version-0.1.0-orange?style=flat-square" alt="Version">
  <img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="License">
  <img src="https://img.shields.io/badge/status-active_development-yellow?style=flat-square" alt="Status">
</p>

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware](#hardware)
- [Prerequisites](#prerequisites)
- [Building the Firmware](#building-the-firmware)
- [Flashing / Installation](#flashing--installation)
  - [Method 1: USB ISP (Bootloader Mode)](#method-1-usb-isp-bootloader-mode)
  - [Method 2: WCH-Link (SWD Debug Adapter)](#method-2-wch-link-swd-debug-adapter)
  - [Method 3: In-App HID Command (OTA)](#method-3-in-app-hid-command-ota)
- [Recovery — Unbricking](#recovery--unbricking)
- [Running Tests](#running-tests)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Architecture Overview](#architecture-overview)
- [Configuration](#configuration)
- [USB Protocol](#usb-protocol)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgments](#acknowledgments)

---

## Overview

**SayoFW** is a from-scratch replacement firmware for the [Sayobot O3C](https://www.sayobot.cn/) — a compact 3-key programmable macropad with a built-in SSD1306 OLED display. The stock firmware is closed-source and has limited customization. This project replaces it with a fully open, extensible firmware that adds:

- **Host-push display** — render text and graphics on the OLED from your computer in real time via USB
- **Programmable macro VM** — a bytecode virtual machine with 256 macro slots and 6,144 total steps
- **Layer-composited OLED** — 16-layer display compositor with dirty-rect tracking for minimal SPI traffic
- **Full HID keyboard support** — NKRO-capable key reporting with debounce
- **Dual-bank flash config** — wear-leveled persistent storage with factory reset support

The project also plans companion host tools: a **Rust CLI** (`sayocli`) and a **SvelteKit Web UI** with WebHID support (see [Roadmap](#roadmap)).

---

## Features

| Feature | Status | Description |
|---------|--------|-------------|
| HID v3 Protocol | ✅ Done | Binary protocol with checksum, batch commands, streaming RX |
| Protocol Dispatcher | ✅ Done | Handles Ping, Info, DisplayText, DisplayClear, DisplayRect |
| Display Compositor | ✅ Done | 16-layer system (fill, text, bitmap), dirty-rect optimization |
| SSD1306/SH1106 Driver | ✅ Done | Auto-detect, SPI page-write, init sequences |
| 5×7 Bitmap Font | ✅ Done | Full printable ASCII (0x20–0x7E) |
| Key Scanner | ⚠️ Partial | Debounce logic + HID usage map done; GPIO HAL stub |
| Macro VM | ⚠️ Header | 12 opcodes defined, VM API declared, implementation in progress |
| Config Storage | ⚠️ Stub | Dual-bank data structures defined; flash persistence pending |
| Real USB Stack | 🔜 Planned | USBHS device stack integration pending |
| CLI Tool (Rust) | 🔜 Planned | `sayocli` for host-side control |
| Web UI (SvelteKit) | 🔜 Planned | Browser-based configurator with WebHID |

---

## Hardware

| Property | Value |
|----------|-------|
| **Device** | Sayobot O3C (3-key OLED macropad) |
| **MCU** | WCH CH32V307 — **RISC-V** (RV32IMAFCX), QingKe V4F core |
| **Clock** | 144 MHz |
| **Flash / RAM** | 256 KB / 64 KB SRAM |
| **USB** | Built-in USB 2.0 High-Speed (480 Mbps) |
| **Display** | SSD1306/SH1106 OLED, 128×32px, SPI interface |
| **USB IDs** | VID `0x8089`, PID `0x0009` |

> ⚠️ **This is a RISC-V chip, not ARM.** Do not use `arm-none-eabi-*` tools.

---

## Prerequisites

### Toolchain

You need a **RISC-V bare-metal GCC** toolchain. Pick **one**:

```bash
# Option A — xPack (recommended, CI-friendly)
npm install --global @xpack-dev-tools/riscv-none-elf-gcc@latest

# Option B — System package (Arch Linux)
sudo pacman -S riscv-none-elf-gcc riscv-none-elf-binutils riscv-none-elf-newlib

# Option C — MounRiver Studio (vendor IDE, includes bundled GCC)
# Download from https://www.mounriver.com/download
```

Verify installation:

```bash
riscv-none-elf-gcc --version
# Should output: riscv-none-elf-gcc (...) 12.x or 13.x
```

### Flash Tools (for installing firmware on device)

```bash
# wchisp — recommended (Rust, cross-platform)
cargo install wchisp

# or wlink (for WCH-Link debug adapter)
cargo install wlink
```

### Build Tools

- **GNU Make** (any recent version)
- **GCC** (host compiler, for running native unit tests)

---

## Building the Firmware

```bash
cd firmware

# Build firmware binary (ELF + BIN + HEX)
make all
```

Build outputs:

| File | Description |
|------|-------------|
| `build/sayofw_o3c.elf` | ELF with debug symbols |
| `build/sayofw_o3c.bin` | Raw binary — **this is what you flash** |
| `build/sayofw_o3c.hex` | Intel HEX format |
| `build/sayofw_o3c.map` | Linker map (for debugging) |

Other targets:

```bash
make size       # Show firmware size breakdown (text/data/bss)
make clean      # Remove all build artifacts
make help       # List all available targets
```

---

## Flashing / Installation

> ⚠️ **Read the [Recovery](#recovery--unbricking) section first.** The stock bootloader has a safety mechanism that prevents permanent bricking, but you should understand it before flashing.

The firmware binary loads at address **`0x08004000`** (after the 16 KB vendor bootloader at `0x00000000`). The bootloader is **never overwritten** by this process.

### Method 1: USB ISP (Bootloader Mode)

The simplest method — no extra hardware needed.

**Step 1 — Enter bootloader mode:**

1. Unplug the O3C from USB
2. Hold the **BOOT0** button (small button on the PCB)
3. While holding BOOT0, plug in the USB cable
4. Release BOOT0 — the device is now in ISP/bootloader mode

**Step 2 — Flash the firmware:**

```bash
cd firmware

# Build if you haven't already
make all

# Flash to the application region (0x08004000)
wchisp flash --address 0x08004000 build/sayofw_o3c.bin
```

**Step 3 — Verify and reboot:**

```bash
# Unplug and replug the device (without holding BOOT0)
# The bootloader will verify the MD5 hash and boot the new firmware
```

### Method 2: WCH-Link (SWD Debug Adapter)

For development — allows flashing + live debugging.

**Requirements:** A [WCH-Link](http://www.wch-ic.com/products/WCH-Link.html) adapter (~$3 USD).

**Step 1 — Connect WCH-Link to O3C:**

| WCH-Link Pin | O3C Pad |
|-------------|---------|
| SWDIO | DIO |
| SWCLK | CLK |
| GND | GND |

**Step 2 — Flash:**

```bash
cd firmware
make all
wlink flash build/sayofw_o3c.bin
```

**Step 3 — (Optional) Debug:**

```bash
# Start GDB server
wlink server

# In another terminal
riscv-none-elf-gdb build/sayofw_o3c.elf -ex "target remote :3333"
```

### Method 3: In-App HID Command (OTA)

If the device is running stock firmware (or a previous version of SayoFW), you can trigger a reboot into bootloader mode via USB HID command `0xFF`, then flash as in Method 1.

```bash
# Send bootloader command (requires a HID tool or sayocli when available)
# The device will reboot into bootloader mode
# Then flash with wchisp as above
```

---

## Recovery — Unbricking

**The O3C is designed to be hard to brick.** The stock bootloader at `0x00000000` is never overwritten during flashing. On every boot it:

1. Reads the firmware size from `0x08029F84`
2. Reads the MD5 hash from `0x08029FA0`
3. Computes the MD5 of the firmware at `0x08004000`
4. **Match** → boots the firmware
5. **Mismatch** → stays in bootloader mode, waiting for a valid firmware

This means: **if you flash a broken firmware, the device automatically enters bootloader mode** on the next power cycle. Just re-flash with a known-good binary.

If the device is completely unresponsive:

1. Hold **BOOT0** while plugging in USB — forces bootloader mode
2. Flash a known-good binary with `wchisp`
3. To restore stock firmware, download from: `https://a.sayobot.cn/firmware/update/9/firmware/app_O3C.bin` (AES-256-CBC encrypted; decrypt with the key in [`docs/api/TECHNICAL_REFERENCE.md`](docs/api/TECHNICAL_REFERENCE.md))

---

## Running Tests

Tests run **natively on your host machine** (not on the MCU). They cover pure-logic modules (protocol codec, compositor, display math) with hardware calls stubbed out.

```bash
cd firmware

# Run all tests
make test

# Clean test artifacts
make test-clean
```

Test suites:

| Test | Covers |
|------|--------|
| `tests/protocol/test_codec.c` | Checksum, framing, batch parsing, streaming RX |
| `tests/display/test_compositor.c` | Layer blend, dirty-rect tracking, text rendering |
| `tests/display/test_display.c` | Display init, flush, startup screen |
| `tests/flasher/test_dispatch_text.c` | Protocol → compositor integration (DisplayText command) |

---

## Usage

### Current Capabilities

The firmware currently operates in a bare-metal main loop:

```
sys_init() → display_init() → protocol_init() → keys_init()
            ↓
        ┌───────────────────────────┐
        │  protocol_pump()          │  ← drain incoming USB HID packets
        │  keys_scan()              │  ← debounce + read key states
        │  display_flush()          │  ← push dirty framebuffer to OLED
        │  (repeat)                 │
        └───────────────────────────┘
```

### HID Commands (via USB)

Once a host tool is available, you can interact with the device over USB HID (usage page `0xFF20`):

| Command | ID | Description |
|---------|----|-------------|
| Ping | `0x80` | Echo test — verifies communication |
| Info | `0x81` | Query firmware version and capabilities |
| Display Text | `0x82` | Push text to a display layer |
| Display Rect | `0x83` | Draw a filled rectangle |
| Display Bitmap | `0x84` | Push 1bpp bitmap data |
| Display Clear | `0x85` | Clear a display layer |
| Macro Define | `0x88` | Upload bytecode to a macro slot |
| Macro Run | `0x89` | Execute a macro by slot index |
| Macro Stop | `0x8A` | Stop running macro |

---

## Project Structure

```
freya-control/
├── README.md                           ← you are here
├── firmware/                           ← bare-metal RISC-V firmware (C)
│   ├── Makefile                        # GNU Make build system
│   ├── linker/
│   │   └── ch32v307_app.ld            # Memory layout (app @ 0x08004000)
│   ├── include/                        # Public headers
│   │   ├── sayofw.h                   # Master umbrella include
│   │   ├── sayofw_config.h            # Compile-time configuration
│   │   ├── hal/hal.h                  # Hardware abstraction layer
│   │   ├── protocol/                  # HID v3 codec, commands, dispatcher
│   │   │   ├── codec.h
│   │   │   ├── commands.h
│   │   │   └── hid.h
│   │   ├── display/                   # OLED compositor + driver
│   │   │   ├── display.h
│   │   │   ├── compositor.h
│   │   │   ├── driver.h
│   │   │   └── font.h
│   │   ├── input/                     # Key scanner
│   │   │   ├── keys.h
│   │   │   └── hid_report.h
│   │   ├── macro/vm.h                 # Macro bytecode VM
│   │   └── storage/config.h           # Flash config persistence
│   ├── src/                            # Implementation
│   │   ├── main.c                     # Bare-metal main loop
│   │   ├── boot/startup_ch32v30x.S   # Reset vector, .data/.bss init
│   │   ├── system/sys.c              # Clock + tick counter
│   │   ├── protocol/                  # Codec, dispatcher, pump
│   │   ├── display/                   # SSD1306 driver, compositor, font
│   │   ├── input/                     # Key scanner + debounce
│   │   ├── usb/usb.c                 # USB HID (stub)
│   │   ├── macro/vm.c                # Macro VM implementation
│   │   └── storage/config.c          # Config persistence (stub)
│   ├── tests/                          # Native host tests
│   │   ├── protocol/test_codec.c
│   │   ├── display/test_compositor.c
│   │   ├── display/test_display.c
│   │   └── flasher/test_dispatch_text.c
│   └── tools/                          # Dev utilities (planned)
├── docs/                               ← design documents
│   ├── api/TECHNICAL_REFERENCE.md     # Reverse-engineered protocol spec
│   ├── design/SYSTEM_ARCHITECTURE.md  # Full system architecture
│   ├── plans/FIRMWARE_DEV_PLAN.md     # Development plan + phases
│   ├── plans/FEASIBILITY.md           # Feasibility assessment
│   └── plans/GAP_ANALYSIS.md          # Current implementation status
└── webui/                              ← web flasher/configurator (planned)
    ├── flasher/                        # WebHID firmware flasher
    ├── src/                            # SvelteKit app source
    └── static/                         # Static assets
```

---

## Architecture Overview

```
┌───────────────────────────── HOST COMPUTER ─────────────────────────────┐
│                                                                         │
│   sayocli (Rust)          Web UI (SvelteKit)         Other HID apps     │
│        │                       │                          │             │
│        └───── USB HID ─────────┴──── WebHID API ──────────┘             │
│                         │                                               │
└─────────────────────────┼───────────────────────────────────────────────┘
                          │  USB Cable
┌─────────────────────────┼───────────────────────────────────────────────┐
│                   SAYOBOT O3C DEVICE                                     │
│                         │                                               │
│   ┌─────────────────────┴─────────────────────────────┐                 │
│   │           USB HID Interface (0xFF20)               │                 │
│   └─────────────────────┬─────────────────────────────┘                 │
│                         │                                               │
│   ┌─────────────────────┴─────────────────────────────┐                 │
│   │              Protocol Dispatcher                   │                 │
│   │   Codec → Pump → Command routing → Response        │                 │
│   └──┬──────────┬────────────┬────────────┬───────────┘                 │
│      │          │            │            │                              │
│   ┌──┴───┐  ┌──┴────┐  ┌───┴─────┐  ┌──┴──────┐                       │
│   │ Keys │  │Display │  │Macro VM │  │ Config  │                       │
│   │Scan +│  │Compos. │  │Bytecode │  │Dual-bank│                       │
│   │Bounce│  │16-layer│  │interp.  │  │flash    │                       │
│   └──────┘  └───┬────┘  └─────────┘  └─────────┘                       │
│                 │                                                        │
│          ┌──────┴──────┐                                                │
│          │ SSD1306/    │                                                 │
│          │ SH1106 OLED │                                                 │
│          │ 128×32 SPI  │                                                 │
│          └─────────────┘                                                │
└─────────────────────────────────────────────────────────────────────────┘
```

**Design principles:**
- **Single USB interface**, multiplexed HID commands — no custom drivers needed
- **Layer-composited display** — host sends text/graphic layer updates, not raw framebuffer
- **Firmware works standalone** — keys, macros, and saved configs work without host software
- **Host-testable** — pure logic modules (codec, compositor) have no MCU dependencies; hardware calls go through weak-symbol HAL stubs

For the full architecture, see [`docs/design/SYSTEM_ARCHITECTURE.md`](docs/design/SYSTEM_ARCHITECTURE.md).

---

## Configuration

Compile-time tunables are centralized in [`firmware/include/sayofw_config.h`](firmware/include/sayofw_config.h). Override at build time with `-D` flags:

```bash
# Example: build for a 128x64 OLED instead of 128x32
make all CFLAGS+="-DDISPLAY_WIDTH_PX=128 -DDISPLAY_HEIGHT_PX=64"
```

| Parameter | Default | Description |
|-----------|---------|-------------|
| `MCU_FREQ_HZ` | `144000000` | CPU clock frequency |
| `DISPLAY_WIDTH_PX` | `128` | OLED width in pixels |
| `DISPLAY_HEIGHT_PX` | `32` | OLED height in pixels |
| `DISPLAY_MAX_LAYERS` | `16` | Max compositor layers |
| `DISPLAY_TEXT_CHARS` | `32` | Max characters per text layer |
| `PROTOCOL_PACKET_SIZE` | `64` | HID packet size (full-speed) |
| `PROTOCOL_HS_PACKET_SIZE` | `1024` | HID packet size (high-speed) |
| `PROTOCOL_RX_RING_SIZE` | `16` | RX ring buffer depth |
| `MACRO_SLOTS` | `256` | Number of macro slots |
| `MACRO_STEPS_PER_SLOT` | `24` | Instructions per macro |
| `CONFIG_NUM_BANKS` | `2` | Dual-bank wear-leveling |
| `CONFIG_BASE_ADDR` | `0x0801A000` | Flash config base address |

---

## USB Protocol

Communication uses **USB HID** on vendor usage page `0xFF20`. Packet format (v3):

```
┌────────────┬──────┬───────────┬──────────────────────┐
│ report_id  │ echo │ checksum  │ commands[]            │
│  (1 byte)  │(1 B) │ (2 B LE)  │ (variable, batched)  │
└────────────┴──────┴───────────┴──────────────────────┘
```

Each command within a packet:

```
┌────────┬────┬───────┬──────────────────┐
│ length │ id │ index │ data[length - 4] │
│ (2B LE)│(1B)│ (1B)  │ (variable)       │
└────────┴────┴───────┴──────────────────┘
```

- **Checksum**: Sum of all packet bytes as 16-bit words (LE)
- **Batch**: Multiple commands can be packed into a single USB transfer
- **Backward compatible**: Commands `0x00–0x3F` follow stock v2 semantics

Full protocol spec: [`docs/api/TECHNICAL_REFERENCE.md`](docs/api/TECHNICAL_REFERENCE.md)

---

## Troubleshooting

### Build Issues

| Problem | Solution |
|---------|----------|
| `riscv-none-elf-gcc: command not found` | Install the RISC-V toolchain (see [Prerequisites](#prerequisites)) |
| `arm-none-eabi-gcc` errors | Wrong toolchain — this is RISC-V, not ARM |
| Linker errors about missing `_start` | Ensure `src/boot/startup_ch32v30x.S` is being compiled |
| `region FLASH overflowed` | Firmware too large; enable `-Os` or reduce features |

### Flashing Issues

| Problem | Solution |
|---------|----------|
| `wchisp` can't find device | Ensure device is in bootloader mode (hold BOOT0 during plug-in) |
| `wchisp` permission denied | Add udev rules: `SUBSYSTEM=="usb", ATTR{idVendor}=="4348", MODE="0666"` |
| Device doesn't boot after flash | Check `make flash` output for the correct load address (`0x08004000`) |
| Device stuck in bootloader | MD5 mismatch — the firmware binary is invalid. Re-flash a working build. |

### Runtime Issues

| Problem | Solution |
|---------|----------|
| OLED display blank | Check SPI wiring; verify SSD1306 vs SH1106 auto-detect in driver |
| Keys not responding | HAL GPIO stubs — real GPIO init required (see [Roadmap](#roadmap)) |
| USB device not recognized | Real USB stack not yet integrated — currently runs with test stubs |

### Test Issues

| Problem | Solution |
|---------|----------|
| Tests won't compile | Need host `gcc` installed (tests run natively, not on MCU) |
| Test failures | Run `make test` and check output; file an issue with the failing test name |

---

## Roadmap

Development follows the phased plan in [`docs/plans/FIRMWARE_DEV_PLAN.md`](docs/plans/FIRMWARE_DEV_PLAN.md):

- [x] **P0 — Toolchain**: Build system, linker script, startup assembly
- [x] **P1 — Protocol**: HID v3 codec, packet parser, dispatcher
- [x] **P3 — Display**: SSD1306 driver, 16-layer compositor, font rendering
- [ ] **P1.5 — USB Stack**: Integrate WCH USBHS device stack for real enumeration
- [ ] **P2 — Key Input**: Wire GPIO, generate HID keyboard reports
- [ ] **P4 — CLI Tool**: `sayocli` (Rust) — ping, display push, status, macro upload
- [ ] **P5 — Macro VM**: Bytecode interpreter, DSL compiler
- [ ] **P6 — Config**: Flash persistence, dual-bank wear-leveling, export/import
- [ ] **P7 — Web UI**: SvelteKit + WebHID configurator
- [ ] **P8 — Polish**: LED control, display designer, OTA update, docs

See [`docs/plans/GAP_ANALYSIS.md`](docs/plans/GAP_ANALYSIS.md) for detailed current status.

---

## Contributing

Contributions are welcome! This project is in early development — there's plenty to do.

### Getting Started

1. Fork the repository
2. Create a feature branch: `git checkout -b feat/my-feature`
3. Make your changes
4. Run the tests: `cd firmware && make test`
5. Commit with a descriptive message
6. Push and open a Pull Request

### Development Notes

- **Host-testable code**: Keep MCU-dependent code behind the HAL (`include/hal/hal.h`). Pure logic should compile and run on the host.
- **Coding style**: C11, `snake_case`, `-Wall -Wextra -Werror`. No warnings allowed.
- **Tests required**: New protocol commands or display features must include host-native tests.
- **Commit messages**: Use conventional commits (`feat:`, `fix:`, `docs:`, `test:`, `refactor:`).

### Areas That Need Help

- 🔌 **USB stack integration** — porting the WCH USBHS device stack
- 🎮 **Macro VM** — bytecode interpreter implementation
- 💾 **Flash storage** — real dual-bank wear-leveled persistence
- 🖥️ **CLI tool** — Rust + hidapi for cross-platform device control
- 🌐 **Web UI** — SvelteKit + WebHID browser configurator

---

## License

This project is released under the **MIT License**. See [LICENSE](LICENSE) for details.

> **Note**: This is an independent, clean-room implementation. It is not affiliated with or endorsed by Sayobot/SayoDevice. The stock firmware encryption key and protocol details were obtained through publicly available reverse-engineering efforts.

---

## Acknowledgments

- **[khang06](https://gist.github.com/khang06)** — Original reverse engineering of the O3C firmware, protocol, and memory layout
- **[WCH / Nanjing Qinheng](http://www.wch-ic.com/)** — CH32V307 MCU documentation and SDK
- **[wchisp](https://github.com/nicecoolwinter/wchisp)** — Open-source WCH ISP flash tool (Rust)
- **[wlink](https://github.com/nicecoolwinter/wlink)** — Open-source WCH-Link debug tool (Rust)
- **Sayobot community** — Protocol documentation and testing

---

## 🌐 Web Flasher (GitHub Pages)

A browser-based firmware flasher is hosted at **GitHub Pages** — no installs needed.

**Live URL:** `https://<your-username>.github.io/freya-control/`

### Enable GitHub Pages

1. Go to **Settings → Pages** in your repository
2. Under **Source**, select **GitHub Actions**
3. Push to `main` — the workflow deploys `docs/flasher/` automatically
4. (Optional) Set a custom domain in `docs/flasher/CNAME`

### Requirements

- **Chrome 61+** or **Edge 79+** (WebUSB required)
- **Windows:** Install WinUSB driver via [Zadig](https://zadig.akeo.ie/)
- **Linux:** Add udev rule for VID `4348`
- **macOS:** Works out of the box
