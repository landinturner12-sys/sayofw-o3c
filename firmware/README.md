# Sayobot O3C Custom Firmware

Custom RISC-V firmware for the Sayobot O3C OLED macropad. Replaces the stock
firmware with a clean-room implementation that exposes:

- Full HID keyboard reporting (NKRO optional)
- Programmable macro bytecode VM (16 opcodes, 256 slots, 6144 steps)
- Layer-composited OLED display with **host-push text/graphics via USB**
- Dual-bank flash config storage with wear-leveling
- DFU bridge to the stock bootloader for safe firmware recovery

## Target

- **MCU**: WCH CH32V307 (RISC-V RV32IMAFCX, 144 MHz, 256 KB flash, 64 KB SRAM)
- **Boot**: Loads at `0x08004000` (after vendor bootloader at `0x0`)
- **USB**: HID device on vendor usage page `0xFF20`
- **Display**: SSD1306/SH1106-compatible OLED (auto-detected at init)

## Project Layout

```
firmware/
├── Makefile                      # GNU Make build (tested with xPack RISC-V GCC)
├── linker/
│   └── ch32v307_app.ld          # Memory map (app @ 0x08004000)
├── include/                      # Public headers (host-testable)
│   ├── sayofw.h                 # Master include
│   ├── protocol/                # HID v3 codec
│   ├── display/                 # Compositor + driver
│   └── ...
├── src/
│   ├── main.c                   # Bare-metal main loop
│   ├── system/                  # System init, clock tree
│   ├── protocol/                # HID packet parser
│   ├── display/                 # OLED driver + compositor
│   ├── usb/                     # HID stack integration
│   ├── input/                   # Key scanner + debounce
│   ├── storage/                 # Flash config (dual-bank)
│   └── boot/                    # Startup code + bootloader bridge
├── tests/                       # Native host tests (run on build host)
│   ├── protocol/                # Codec tests (checksum, framing)
│   └── display/                 # Compositor + driver integration tests
└── tools/
    └── gen_test_vectors.py      # Generate protocol test vectors
```

## Building

Requires `riscv-none-elf-gcc` (xPack, MounRiver, or system package — see
[firmware dev plan](../docs/sayobot-o3c-firmware-dev-plan.md)).

```bash
# Cross-compile firmware
make all

# Run native unit tests (runs on host, not MCU)
make test
```

Outputs:
- `build/sayofw_o3c.elf` — Debug symbols
- `build/sayofw_o3c.bin` — Raw binary, load at `0x08004000`
- `build/sayofw_o3c.hex` — Intel HEX

## Flashing (Manual)

```bash
# Default: prints instructions, does NOT flash
make flash

# Actually flash (requires explicit consent)
wchisp flash --address 0x08004000 build/sayofw_o3c.bin
# or with WCH-Link:
wlink flash build/sayofw_o3c.bin
```

The bootloader at `0x0` is left untouched. The MD5 check at `0x08029FA0`
ensures an invalid image puts the device back into bootloader mode rather
than bricking it.

## Testing

```bash
make test
```

Runs host-native tests against:
- Protocol codec (checksum, framing, batch parsing)
- Display compositor (layer blend, dirty-rect, text rendering)
- Driver abstraction (mock SPI)

Target-specific code (USB ISR, real SPI) is stubbed at the header boundary.

## Status

See [todo list](../TODO) for current progress. Initial release implements:

- ✅ HID v3 protocol dispatcher with Ping/Info/DisplayText/MacroDefine commands
- ✅ SSD1306/SH1106 OLED driver (SPI, 128×32 default)
- ✅ 16-layer compositor (color fills, text, host-push)
- ✅ Real-time host display push (text) via HID command `0x80`
