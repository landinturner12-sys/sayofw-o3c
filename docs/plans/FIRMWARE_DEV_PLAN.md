# SayoDevice O3C — Firmware Development Plan

> **Version**: 1.0.0  
> **Date**: 2025-07-22  
> **Status**: DRAFT  
> **Prerequisites**: [System Architecture](./sayobot-o3c-system-architecture.md) · [Technical Reference](./sayobot-o3c-technical-reference.md)

---

## Table of Contents

1. [Toolchain Setup](#1-toolchain-setup)
2. [Module Breakdown & File Structure](#2-module-breakdown--file-structure)
3. [Development Phases & Effort Estimates](#3-development-phases--effort-estimates)
4. [Testing Strategy](#4-testing-strategy)
5. [Risks & Unknowns](#5-risks--unknowns)
6. [Appendices](#6-appendices)

---

## 1. Toolchain Setup

### 1.1 Target MCU

| Property | Value |
|----------|-------|
| MCU | **WCH CH32V307** |
| Architecture | **RISC-V 32-bit** (RV32IMAFCX) — NOT ARM Cortex-M |
| Core | QingKe V4F (custom WCH RISC-V core with FPU) |
| Flash | 256 KB internal |
| RAM | 64 KB SRAM |
| Clock | 144 MHz max (HSE 8 MHz × PLL) |
| USB | Built-in USB 2.0 HS (480 Mbps OTG) + USB 2.0 FS |
| SPI | 3× SPI (used for OLED) |
| Peripherals | UART, I²C, ADC, DMA, timers, GPIO |

> **Critical**: This is RISC-V, not ARM. All `arm-none-eabi-*` references in the original request are inapplicable. The correct toolchain is RISC-V GCC.

### 1.2 Compiler

| Component | Specification |
|-----------|---------------|
| **Compiler** | `riscv-none-elf-gcc` (RISC-V bare-metal GCC) |
| **Version** | ≥ 12.2 (via [xPack RISC-V GCC](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack) or MounRiver bundled) |
| **ABI** | `ilp32` (32-bit int, long, pointer) or `ilp32f` (with hardware float) |
| **Architecture flags** | `-march=rv32imafcx -mabi=ilp32f` |
| **Optimization** | `-Os` for release (size-optimized, fits 256KB flash), `-Og -g3` for debug |
| **Warnings** | `-Wall -Wextra -Werror -Wno-unused-parameter` |
| **Standards** | `-std=gnu11` (C11 with GNU extensions for inline asm) |

**Installation options** (pick one):

```bash
# Option A: xPack (recommended for CI — scriptable, no IDE)
npm install --global @xpack-dev-tools/riscv-none-elf-gcc@latest

# Option B: MounRiver Studio (vendor IDE — Eclipse + bundled GCC)
# Download from https://www.mounriver.com/download
# Toolchain at: MounRiver/MRS_Toolchain_Linux/RISC-V/bin/riscv-none-elf-gcc

# Option C: System package (Arch Linux)
pacman -S riscv-none-elf-gcc riscv-none-elf-binutils riscv-none-elf-newlib

# Verify:
riscv-none-elf-gcc --version
# Expected: riscv-none-elf-gcc (xPack ...) 12.x or 13.x
```

### 1.3 Build System

**GNU Make** (not CMake — matches WCH SDK conventions and keeps minimal overhead).

```makefile
# Top-level Makefile — key variables
TARGET    = sayofw_o3c
BUILD_DIR = build

# Toolchain
CROSS     = riscv-none-elf-
CC        = $(CROSS)gcc
AS        = $(CROSS)gcc -x assembler-with-cpp
LD        = $(CROSS)gcc
OBJCOPY   = $(CROSS)objcopy
OBJDUMP   = $(CROSS)objdump
SIZE      = $(CROSS)size

# Architecture
CPU       = -march=rv32imafcx -mabi=ilp32f
LDSCRIPT  = linker/ch32v307_app.ld

# Flags
CFLAGS    = $(CPU) -Os -std=gnu11 -Wall -Wextra -Werror \
            -ffunction-sections -fdata-sections -fno-common \
            -nostdlib -nostartfiles \
            -I include -I include/ch32v307
LDFLAGS   = $(CPU) -T $(LDSCRIPT) -Wl,--gc-sections \
            -Wl,-Map=$(BUILD_DIR)/$(TARGET).map -nostdlib
ASFLAGS   = $(CPU) -Wall

# Sources (auto-discovered)
C_SRCS    = $(shell find src -name '*.c')
S_SRCS    = $(shell find src -name '*.S')
OBJS      = $(C_SRCS:%.c=$(BUILD_DIR)/%.o) $(S_SRCS:%.S=$(BUILD_DIR)/%.o)

# Targets
all: $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex
	@$(SIZE) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).bin
	@echo "⚠ Manual flash only — use wch-isp or WCH-Link"
	@echo "Binary: $(BUILD_DIR)/$(TARGET).bin (load at 0x08004000)"

-include $(OBJS:.o=.d)

.PHONY: all clean flash
```

### 1.4 SDK / HAL Library

**WCH CH32V307 EVT SDK** — the vendor's official peripheral library (NOT STM32 HAL).

| Component | Source | Purpose |
|-----------|--------|---------|
| **Startup** | `Startup/startup_ch32v30x.S` | Reset vector, stack init, interrupt vectors |
| **System** | `Core/core_riscv.h`, `system_ch32v30x.c` | Clock tree init, system tick, CSR access |
| **Peripheral drivers** | `Peripheral/inc/*.h`, `Peripheral/src/*.c` | GPIO, SPI, USART, USB, DMA, FLASH, RCC |
| **USB library** | `USB-Driver/USBHS/` | USB HS device stack (HID class) |

```bash
# Clone vendor SDK
git clone https://github.com/openwch/ch32v307.git vendor/ch32v307-sdk

# Relevant paths to vendor into our project:
# vendor/ch32v307-sdk/EVT/EXAM/SRC/Startup/startup_ch32v30x.S
# vendor/ch32v307-sdk/EVT/EXAM/SRC/Core/core_riscv.h
# vendor/ch32v307-sdk/EVT/EXAM/SRC/Peripheral/
# vendor/ch32v307-sdk/EVT/EXAM/USB/USBHS/DEVICE/
```

**Integration strategy**: Copy the minimal required SDK files into `vendor/ch32v307/` within our repo tree. This avoids submodule fragility and lets us patch vendor bugs in-tree.

Vendor files to copy:
```
vendor/ch32v307/
├── Core/
│   ├── core_riscv.h          # CSR macros, interrupt enable/disable
│   └── core_riscv.c
├── Startup/
│   └── startup_ch32v30x.S    # Boot entry, vector table
├── System/
│   ├── system_ch32v30x.h
│   ├── system_ch32v30x.c     # SystemInit(), clock config
│   └── ch32v30x.h            # Master MCU header (register defs)
├── Peripheral/
│   ├── inc/
│   │   ├── ch32v30x_gpio.h
│   │   ├── ch32v30x_spi.h
│   │   ├── ch32v30x_usart.h
│   │   ├── ch32v30x_dma.h
│   │   ├── ch32v30x_flash.h
│   │   ├── ch32v30x_rcc.h
│   │   ├── ch32v30x_tim.h
│   │   └── ch32v30x_misc.h   # NVIC helpers
│   └── src/
│       ├── ch32v30x_gpio.c
│       ├── ch32v30x_spi.c
│       ├── ch32v30x_dma.c
│       ├── ch32v30x_flash.c
│       ├── ch32v30x_rcc.c
│       ├── ch32v30x_tim.c
│       └── ch32v30x_misc.c
└── USB/
    └── usbhs_device.c        # USB HS device-mode driver
    └── usbhs_device.h
```

### 1.5 Linker Script

Custom linker script for application loaded at `0x08004000` (above bootloader):

```ld
/* ch32v307_app.ld — Application firmware, bootloader at 0x0-0x3FFF */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08004000, LENGTH = 256K - 16K  /* 240KB app space */
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

ENTRY(_start)

SECTIONS
{
    .text : {
        . = ALIGN(4);
        KEEP(*(.init))
        KEEP(*(.vector_table))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data : AT(_etext) {
        . = ALIGN(4);
        _sdata = .;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = .;
    } > RAM

    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* Config storage region — last 8KB of flash (dual-bank, 4KB each) */
    .config_a (NOLOAD) : {
        . = ALIGN(4096);
        _config_a_start = .;
        . += 4096;
    } > FLASH

    .config_b (NOLOAD) : {
        . = ALIGN(4096);
        _config_b_start = .;
        . += 4096;
    } > FLASH

    /* Macro bytecode storage — 24KB region after config */
    .macro_storage (NOLOAD) : {
        . = ALIGN(4096);
        _macro_storage_start = .;
        . += 24576;
    } > FLASH

    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
```

### 1.6 Flashing Tools

| Tool | Method | Use Case |
|------|--------|----------|
| **WCH-Link** + `wch-openocd` | SWD/JTAG over custom debug probe | **Primary development path**. Full debug: breakpoints, memory inspect, single-step. Required for initial bring-up |
| **WCHISPTool** | USB ISP (boot pins) | Fallback — program via USB when device boots to ISP mode (hold BOOT0 high during reset) |
| **wchisp** (open-source) | USB ISP CLI | [ch32-rs/wchisp](https://github.com/ch32-rs/wchisp) — cross-platform Rust CLI. `cargo install wchisp` |
| **HID Bootloader** (stock) | USB HID command 0xFF | Flash encrypted firmware via stock bootloader. Requires AES-256-CBC encryption of our binary |
| **OpenOCD** (patched for WCH) | SWD via WCH-Link | `openocd -f wch-riscv.cfg -c "program build/sayofw_o3c.bin 0x08004000 verify reset"` |

**Recommended dev workflow**:
```bash
# 1. Build
make -j$(nproc)

# 2. Flash via wchisp (USB ISP — no JTAG probe needed)
wchisp flash build/sayofw_o3c.bin --address 0x08004000

# 3. Or flash via WCH-Link (JTAG — allows debugging)
openocd -f interface/wch-riscv.cfg -f target/ch32v307.cfg \
  -c "program build/sayofw_o3c.bin 0x08004000 verify reset exit"

# 4. Or prepare for stock bootloader (encrypted)
python3 tools/fw_encrypt.py build/sayofw_o3c.bin build/sayofw_o3c_enc.bin
# Then flash via HID bootloader protocol
```

### 1.7 Debug Setup

| Component | Tool |
|-----------|------|
| **Debug probe** | WCH-Link (≈$5, USB-C) |
| **GDB** | `riscv-none-elf-gdb` (bundled with toolchain) |
| **GDB server** | `wch-openocd` (WCH fork of OpenOCD) |
| **Serial console** | USB-UART via WCH-Link (SWO pin) or device USART1 if exposed |
| **Logic analyzer** | Sigrok/PulseView for SPI (OLED), GPIO (keys) debug |

```bash
# Terminal 1: Start OpenOCD
openocd -f interface/wch-riscv.cfg -f target/ch32v307.cfg

# Terminal 2: Connect GDB
riscv-none-elf-gdb build/sayofw_o3c.elf \
  -ex "target extended-remote :3333" \
  -ex "monitor reset halt" \
  -ex "load" \
  -ex "break main" \
  -ex "continue"
```

### 1.8 Firmware Encryption (for stock bootloader compatibility)

To flash custom firmware through the stock HID bootloader without a JTAG probe:

```python
# tools/fw_encrypt.py
from Crypto.Cipher import AES
import hashlib, struct, sys

KEY = bytes.fromhex('C4053DDF225E89F74868C1E1F4C00D514F02A8A8692F997869ABEB155250150C')
IV  = b'\x00' * 16

def encrypt_firmware(plaintext_path, output_path):
    with open(plaintext_path, 'rb') as f:
        firmware = f.read()

    # Pad to AES block size (16 bytes)
    pad_len = (16 - len(firmware) % 16) % 16
    firmware_padded = firmware + b'\x00' * pad_len

    # Compute MD5 of original firmware
    md5 = hashlib.md5(firmware).digest()
    fw_size = struct.pack('<I', len(firmware))

    # Encrypt
    cipher = AES.new(KEY, AES.MODE_CBC, IV)
    encrypted = cipher.encrypt(firmware_padded)

    # Write: encrypted_fw + metadata area
    # Metadata at offset 0x29F80 (relative to 0x04000 base = 0x25F80 into file)
    with open(output_path, 'wb') as f:
        f.write(encrypted)
        # Pad to metadata offset if needed
        meta_offset = 0x29F80 - 0x04000
        if len(encrypted) < meta_offset:
            f.write(b'\xFF' * (meta_offset - len(encrypted)))
        f.seek(meta_offset)
        f.write(b'\x00' * 4)     # metadata magic
        f.write(fw_size)          # firmware size at 0x29F84
        f.write(b'\x00' * 24)    # padding to 0x29FA0
        f.seek(meta_offset + 0x20)
        f.write(md5)              # MD5 at 0x29FA0

    print(f"Encrypted {len(firmware)} bytes → {output_path}")
    print(f"MD5: {md5.hex()}")

if __name__ == '__main__':
    encrypt_firmware(sys.argv[1], sys.argv[2])
```

### 1.9 Toolchain Validation Checklist

Before any firmware development begins, verify:

- [ ] `riscv-none-elf-gcc --version` outputs ≥ 12.2
- [ ] `make` builds a minimal `main.c` → `.bin` with zero errors
- [ ] `.bin` size < 240KB (fits app region)
- [ ] `riscv-none-elf-objdump -d build/sayofw_o3c.elf` shows correct `.text` at `0x08004000`
- [ ] WCH-Link probe detected: `lsusb | grep 4348` shows WCH device
- [ ] OpenOCD connects: `openocd -f wch-riscv.cfg` prints "Info : CH32V307 detected"
- [ ] GDB can halt, read memory, single-step on target
- [ ] `wchisp` can detect device in ISP mode (fallback flash path)



---

## 2. Module Breakdown & File Structure

### 2.1 Repository Layout

```
firmware/
├── Makefile                        # GNU Make build (see §1.3)
├── README.md
├── vendor/
│   └── ch32v307/                   # Vendored SDK subset (see §1.4)
│       ├── Core/
│       ├── Startup/
│       ├── System/
│       ├── Peripheral/{inc,src}/
│       └── USB/
├── linker/
│   └── ch32v307_app.ld             # App at 0x08004000 (see §1.5)
├── include/
│   ├── board.h                     # Pin assignments, clock defines, OLED resolution
│   ├── version.h                   # FW version, build timestamp (auto-generated)
│   └── hal_config.h                # Feature flags, buffer sizes, tick rate
├── src/
│   ├── main.c                      # Init + cooperative main loop
│   ├── system_init.c               # Clock tree, GPIO AF, peripheral enables
│   │
│   ├── usb/
│   │   ├── usb_desc.c              # HID report descriptors (usage page 0xFF20)
│   │   ├── usb_desc.h              # Descriptor byte arrays, sizes
│   │   ├── usb_hid.c              # USB HS peripheral init, EP callbacks, report I/O
│   │   └── usb_hid.h              # send_report(), recv_report(), usb_is_configured()
│   │
│   ├── protocol/
│   │   ├── protocol.c              # v3 packet parser, checksum, command dispatcher
│   │   ├── protocol.h              # hid_packet_v3_t, hid_cmd_v3_t, cmd_id_t enum
│   │   ├── proto_v2_compat.c       # v2 command handlers (0x00–0x2F) for stock compat
│   │   └── proto_v3_cmds.c         # v3-only commands (0x80–0xBF): display push, macro, etc.
│   │
│   ├── input/
│   │   ├── key_scan.c              # GPIO scan 3 keys, debounce FSM (4ms window)
│   │   ├── key_scan.h              # key_event_t, key_scan_init(), key_scan_poll()
│   │   ├── key_map.c               # Action lookup: [key][layer] → HID/macro/media/layer
│   │   └── key_map.h               # key_action_t, 5 layers × 3 keys
│   │
│   ├── macro/
│   │   ├── macro_vm.c              # Bytecode interpreter: fetch-decode-execute
│   │   ├── macro_vm.h              # macro_vm_state_t, macro_vm_tick(), opcodes
│   │   ├── macro_ops.h             # Opcode enum (KEY_DOWN..HALT), encoding helpers
│   │   └── macro_storage.c         # Load/store bytecode slots from flash macro region
│   │
│   ├── display/
│   │   ├── display_engine.c        # 16-layer compositor → framebuffer
│   │   ├── display_engine.h        # display_layer_t, layer_type_t, display_composite()
│   │   ├── display_text.c          # Glyph renderer: ASCII→bitmap via font table
│   │   ├── display_widget.c        # Built-in widgets (CPU gauge, key pressure bar)
│   │   ├── display_image.c         # 1-bit RLE image decode + blit
│   │   └── font_8x8.h             # 96-char ASCII bitmap font (768 bytes)
│   │
│   ├── driver/
│   │   ├── oled_spi.c              # SSD1306/SH1106 SPI driver (see §2.6)
│   │   ├── oled_spi.h              # oled_init(), oled_flush(), oled_set_brightness()
│   │   ├── led_rgb.c               # WS2812 or PWM RGB LED control
│   │   └── flash_storage.c         # Raw flash read/write/erase (sector-level)
│   │
│   ├── config/
│   │   ├── config.c                # Serialize/deserialize device_config_t, dual-bank swap
│   │   └── config.h                # device_config_t struct, config_load(), config_save()
│   │
│   └── boot/
│       ├── bootloader.c            # DFU entry: write magic → reset; expose firmware_info_t
│       └── bootloader.h
│
├── test/                           # Host-side unit tests (compiled with native gcc)
│   ├── test_protocol.c             # Packet build/parse, checksum
│   ├── test_macro_vm.c             # VM instruction execution, edge cases
│   ├── test_display.c              # Layer composition logic
│   ├── test_config.c               # Serialization round-trip
│   ├── test_main.c                 # Test runner (Unity or MinUnit)
│   └── mock/
│       ├── mock_usb.h              # Stub USB send/recv for protocol tests
│       ├── mock_flash.h            # RAM-backed flash for config tests
│       └── mock_oled.h             # Null OLED driver for display tests
│
└── tools/
    ├── fw_encrypt.py               # Encrypt binary for stock bootloader (see §1.8)
    ├── fw_decrypt.py               # Decrypt stock firmware for analysis
    └── hid_sniff.py                # USB HID packet capture/decode
```

### 2.2 Module: USB HID (usb/)

**Purpose**: Enumerate as USB HID device with vendor usage page; send/receive 64B or 1024B reports.

| File | Responsibility |
|------|---------------|
| `usb_desc.c/.h` | HID report descriptor with usage page `0xFF20`, device descriptor (VID `0x8089`, PID `0x0009`), config/interface/endpoint descriptors. Two endpoints: EP1 IN (device→host reports), EP1 OUT (host→device reports). Report sizes: 64B at normal polling, 1024B at HS 8kHz |
| `usb_hid.c/.h` | Initialize CH32V307 USBHS peripheral. Register EP callbacks. `usb_hid_poll()` drains RX FIFO into a ring buffer. `usb_hid_send()` queues a report on EP1 IN. Handles SET_REPORT/GET_REPORT control transfers. Exposes `usb_is_configured()` for main loop gating |

**HID Report Descriptor** (key structure):
```c
// Usage Page 0xFF20 = Custom firmware
// Usage 0x01 = Device Control
// Report ID 0x21 = 64-byte normal-speed
// Report ID 0x22 = 1024-byte high-speed
// Input/Output: Feature reports for vendor commands
// Plus standard keyboard report (boot protocol compatible) on Usage Page 0x01
```

**Dependencies**: `vendor/ch32v307/USB/usbhs_device.c` (HAL-level USB HS driver)

### 2.3 Module: Protocol Handler (protocol/)

**Purpose**: Parse incoming HID packets, validate checksums, dispatch commands, build responses.

| File | Responsibility |
|------|---------------|
| `protocol.c/.h` | `protocol_dispatch(uint8_t *pkt, uint16_t len)` — validates checksum (16-bit word sum), iterates batched `hid_cmd_v3_t` entries, dispatches by command ID to handler table. Builds response packets with status codes. Handles chunked transfers (multi-packet sequences for large payloads) |
| `proto_v2_compat.c` | Handlers for stock v2 commands `0x00–0x2F` (Info, DeviceName, SysInfo, Setting, Key, Light, Palette, Text, Screen, Save). Enables gradual migration — device works with stock tools during development |
| `proto_v3_cmds.c` | Handlers for new v3 commands `0x80–0xBF`: DisplayTextLayer (`0x80`), DisplayClearLayer (`0x81`), DisplayGraphicLayer (`0x82`), DisplayRefresh (`0x83`), MacroDefine (`0x84`), MacroExecute (`0x85`), MacroStop (`0x86`), MacroStatus (`0x87`), ConfigExport (`0x88`), ConfigImport (`0x89`), FirmwareInfo (`0x8A`), Ping (`0x8B`), DisplayBrightness (`0x8C`) |

**Command dispatch table** (function pointer array):
```c
typedef void (*cmd_handler_t)(uint8_t *data, uint16_t len, uint8_t index);
static const cmd_handler_t cmd_table[256] = {
    [0x00] = cmd_info,
    [0x01] = cmd_device_name,
    [0x02] = cmd_sysinfo,
    // ... v2 commands ...
    [0x80] = cmd_display_text_layer,
    [0x81] = cmd_display_clear_layer,
    // ... v3 commands ...
    [0x8B] = cmd_ping,
};
```

### 2.4 Module: Key Scanning & Debounce (input/)

**Purpose**: Scan 3 physical keys via GPIO, debounce, emit key events, map to actions per layer.

| File | Responsibility |
|------|---------------|
| `key_scan.c/.h` | Configure 3 GPIO pins as inputs with pull-up. Poll at 1kHz (every main loop tick). Per-key 4-state FSM: `IDLE → PRESSING(4ms) → HELD → RELEASING(4ms)`. Emits `key_event_t {key_id, type=PRESS/RELEASE, timestamp}` |
| `key_map.c/.h` | Lookup table `key_action_t key_map[3][5]` (3 keys × 5 layers). Action types: `HID_KEY` (send keyboard report), `MACRO` (trigger VM slot), `MEDIA` (consumer control), `LAYER_SWITCH` (change active layer). `key_map_handle(key_event_t)` resolves action and executes |

**Debounce FSM** (per key):
```
IDLE ──[pin LOW]──→ PRESSING ──[4ms elapsed, still LOW]──→ HELD
  ↑                    │                                      │
  │              [pin HIGH before 4ms]                  [pin HIGH]
  │                    │                                      │
  └────────────────────┘              RELEASING ──[4ms]──→ IDLE
                                         ↑                    │
                                         └──[pin LOW before 4ms]
```

### 2.5 Module: Macro Engine (macro/)

**Purpose**: Execute bytecode programs triggered by key events or host commands.

| File | Responsibility |
|------|---------------|
| `macro_vm.c/.h` | Register-based VM with 8 general registers, 4-deep loop stack. `macro_vm_tick()` executes one instruction per main-loop iteration (non-blocking). Handles delays via `delay_until` timestamp comparison. State machine: `IDLE → RUNNING → WAITING → RUNNING → ... → IDLE` |
| `macro_ops.h` | Opcode enum and encoding. 16 opcodes: `KEY_DOWN(0x01)` through `HALT(0xFF)`. Each instruction is 1-4 bytes (opcode + operands). Max 6144 steps per slot, 256 slots |
| `macro_storage.c` | Read/write bytecode to flash macro region (`_macro_storage_start`, 24KB). Slot index table at start of region: `{offset, length}` per slot. Linear allocation with compaction on full |

**VM execution model** (cooperative, non-blocking):
```c
void macro_vm_tick(void) {
    if (vm.state == VM_IDLE) return;
    if (vm.state == VM_WAITING) {
        if (systick_ms() < vm.delay_until) return;  // still waiting
        vm.state = VM_RUNNING;
    }
    // Execute ONE instruction
    uint8_t opcode = script[vm.pc++];
    switch (opcode) {
        case OP_KEY_DOWN:   usb_hid_key_press(script[vm.pc], script[vm.pc+1]); vm.pc += 2; break;
        case OP_DELAY:      vm.delay_until = systick_ms() + read_u16(&script[vm.pc]); vm.pc += 2; vm.state = VM_WAITING; break;
        case OP_LOOP_START: vm.loop_stack[vm.loop_sp++] = vm.pc + 1; vm.repeat_count = script[vm.pc++]; break;
        case OP_LOOP_END:   if (--vm.repeat_count > 0) vm.pc = vm.loop_stack[vm.loop_sp - 1]; else vm.loop_sp--; break;
        case OP_HALT:       vm.state = VM_IDLE; break;
        // ... remaining opcodes ...
    }
}
```

### 2.6 Module: OLED Display Driver (driver/ + display/)

**Purpose**: Drive SSD1306 or SH1106 OLED via SPI; composite 16 layers into 1-bit framebuffer.

#### OLED Controller Identification

The exact controller (SSD1306 vs SH1106) is **not yet confirmed** from RE data. Both are 1-bit monochrome, SPI-driven, and near-identical. Key difference:

| Feature | SSD1306 | SH1106 |
|---------|---------|--------|
| Column addressing | Continuous (0–127) | Page-based, needs column offset +2 |
| Display RAM | 128×64 mapped 1:1 | 132×64 (4 extra columns, display starts at col 2) |
| Init sequence | Identical except addressing mode |

**Strategy**: Implement both init sequences behind a compile-time `#define OLED_CONTROLLER_SSD1306` / `OLED_CONTROLLER_SH1106`. Auto-detect at runtime by querying SysInfo `width` field (128 → SSD1306, 132 → SH1106) if possible, or hardcode after first hardware test.

| File | Responsibility |
|------|---------------|
| `oled_spi.c/.h` | SPI peripheral init (CH32V307 SPI1). `oled_init()` — reset pulse, send init command sequence. `oled_flush(uint8_t *framebuf)` — DMA transfer 1024 bytes (128×64/8). `oled_set_brightness(uint8_t val)` — contrast command `0x81, val`. `oled_sleep()` / `oled_wake()` |
| `display_engine.c/.h` | 16-layer stack. `display_composite()` iterates layers bottom→top, blits enabled layers into 1024-byte framebuffer. Marks `dirty=0` after composite. Called from main loop only when `display_is_dirty()` returns true |
| `display_text.c` | `display_render_text(layer, x, y, text, font_size)` — rasterize ASCII string into layer's pixel region using `font_8x8.h` bitmap data. Supports 8×8 (small) and 16×16 (large, pixel-doubled) sizes |
| `display_widget.c` | Pre-built widgets: `WIDGET_KEY_PRESSURE` (bar graph), `WIDGET_CPU_GAUGE` (percentage arc), `WIDGET_CLOCK` (HH:MM from systick). Each renders into a fixed rectangular region |
| `display_image.c` | Decode 1-bit RLE-compressed images from flash/host and blit into layer region. Max image size: 128×64 = 1024 bytes uncompressed |
| `font_8x8.h` | 96-glyph ASCII bitmap font (space through tilde). 8 bytes per glyph, 768 bytes total. Compile-time constant array |

**Display update flow**:
```
Host sends DisplayTextLayer(layer=13, x=0, y=16, "CPU: 72°C")
  → proto_v3_cmds.c: cmd_display_text_layer()
    → display_engine: layers[13] = { type=HOST_TEXT, text="CPU: 72°C", x=0, y=16, dirty=1 }
  → main loop detects dirty
    → display_composite(): for each enabled layer, blit to framebuf[]
    → oled_flush(framebuf): SPI DMA 1024 bytes to OLED
```

### 2.7 Module: Configuration Manager (config/)

**Purpose**: Persist device settings, key maps, macro slot index to internal flash with wear leveling.

| File | Responsibility |
|------|---------------|
| `config.c/.h` | `config_load()` — read both flash banks, pick the one with higher generation counter and valid CRC-16. `config_save()` — serialize `device_config_t`, write to inactive bank, increment generation, update CRC, swap active pointer. `config_reset()` — write defaults to both banks |
| `flash_storage.c` | Low-level flash HAL wrapper: `flash_erase_sector(addr)`, `flash_write(addr, data, len)`, `flash_read(addr, buf, len)`. Uses CH32V307 flash controller registers. Sector size: 4KB (to verify from datasheet) |

**Dual-bank wear leveling**:
```
Config Bank A (4KB @ _config_a_start):
  [generation: u32] [crc16: u16] [device_config_t ...]

Config Bank B (4KB @ _config_b_start):
  [generation: u32] [crc16: u16] [device_config_t ...]

On save:
  1. Determine inactive bank (lower generation)
  2. Erase inactive bank
  3. Write new config with generation = active.generation + 1
  4. New bank becomes active (highest generation wins on next boot)

On load:
  1. Read both banks
  2. CRC-check each
  3. Pick valid bank with highest generation
  4. If both invalid → load defaults (first boot or corruption)
```

**Power-loss safety**: If power is lost during erase/write of inactive bank, the active bank is untouched. On next boot, the corrupted bank fails CRC and the valid bank is used.

### 2.8 Module: Bootloader Integration (boot/)

**Purpose**: Enter DFU mode on command, expose firmware metadata, validate firmware integrity.

| File | Responsibility |
|------|---------------|
| `bootloader.c/.h` | `bootloader_enter_dfu()` — write magic word `0xDEADBEEF` to a fixed RAM address (`0x20000000`), trigger software reset via `PFIC_SystemReset()`. Stock bootloader checks this address on boot and enters DFU if magic is present. `bootloader_get_info()` — return `firmware_info_t` with version, build timestamp, git hash, image MD5 |

**Two DFU entry paths**:
1. **Software**: Host sends Ping → FirmwareInfo → Bootloader command. Firmware writes magic, resets. Bootloader stays in DFU.
2. **Hardware**: User holds key 0 during power-on. Bootloader reads GPIO → DFU mode.

> **Note**: The stock bootloader at `0x00000–0x03FFF` is NOT replaced by this project. Our firmware lives at `0x04000+` and cooperates with the existing bootloader.



---

## 3. Development Phases & Effort Estimates

> Effort is in **engineer-days** assuming one embedded developer familiar with RISC-V.  
> Calendar time ≈ effort × 1.3 (context switching, hardware debug surprises).

### Phase 1 — Minimal USB HID Enumeration + Ping (Foundation)

| Task | Effort | Output |
|------|--------|--------|
| Vendor SDK integration, Makefile, linker script | 2d | Compiling `main.c` → `.bin` |
| Clock tree init (HSE→PLL→144MHz), GPIO bringup | 1d | LED blink on known GPIO |
| USB HS peripheral init + HID descriptor (usage page `0xFF20`) | 3d | Device enumerates on host, `lsusb` shows VID/PID |
| Protocol v3 packet parser + checksum + Ping handler | 2d | `sayocli ping` → round-trip latency response |
| **Phase 1 total** | **8d** | **Device on USB, responds to Ping** |

**Exit criteria**: `hidapitool --list` shows device; sending Ping packet returns echo with correct checksum.

### Phase 2 — Key Input with Basic Mapping

| Task | Effort | Output |
|------|--------|--------|
| GPIO input scan for 3 keys (identify pins from stock FW RE) | 1d | Raw key state readable in debugger |
| Debounce FSM (4ms, per-key state machine) | 1d | Clean press/release events |
| HID keyboard report descriptor (boot + NKRO) | 1d | OS recognizes as keyboard |
| Key map lookup (3 keys × 5 layers, hardcoded defaults) | 1d | Press key → character appears on host |
| v2-compat key config commands (0x10 Key, 0x06 SimpleKey) | 2d | Stock web UI can read/write key maps |
| **Phase 2 total** | **6d** | **Functional 3-key keyboard** |

**Exit criteria**: All 3 keys send correct keycodes; layer switching works; stock Sayo web UI reads key config.

### Phase 3 — Macro Engine

| Task | Effort | Output |
|------|--------|--------|
| Bytecode VM core (fetch-decode-execute, 16 opcodes) | 3d | VM runs hardcoded test script |
| Delay handling (non-blocking via systick comparison) | 0.5d | `DELAY 500` pauses macro without blocking main loop |
| Loop/conditional support (loop stack, CMP_JMP) | 1d | Nested loops execute correctly |
| Macro storage (flash region read/write, slot index) | 1.5d | Scripts persist across power cycles |
| MacroDefine/Execute/Stop/Status protocol commands | 1.5d | Host can upload + trigger macros via HID |
| Key→macro trigger wiring | 0.5d | Key press starts assigned macro slot |
| **Phase 3 total** | **8d** | **Full macro VM with host upload** |

**Exit criteria**: Upload "ctrl+c, delay 200, ctrl+v" macro via HID → assign to key 1 → press key → clipboard paste occurs with 200ms delay.

### Phase 4 — Display: Real-Time Host Push

| Task | Effort | Output |
|------|--------|--------|
| SPI peripheral init for OLED | 1d | SPI clock + data signals on logic analyzer |
| SSD1306/SH1106 driver (init sequence, flush framebuffer) | 2d | Static test pattern on OLED |
| Font 8×8 renderer (ASCII→bitmap) | 1d | "Hello" rendered on screen |
| 16-layer compositor (bottom→top blit) | 2d | Multiple text layers composited correctly |
| DisplayTextLayer/ClearLayer/Brightness v3 commands | 1.5d | Host pushes text → appears on OLED |
| DisplayGraphicLayer (raw 1-bit pixel push, chunked) | 1.5d | Host sends bitmap region → renders |
| v2-compat screen commands (ScreenStart/Main/Sleep) | 1d | Stock UI screen config works |
| **Phase 4 total** | **10d** | **Host-driven OLED with layer composition** |

**Exit criteria**: `sayocli display "CPU: 45°C" --layer 13 --y 16` shows text on OLED within 5ms.

### Phase 5 — Configuration Persistence

| Task | Effort | Output |
|------|--------|--------|
| Flash HAL (erase/write/read sector, CH32V307 flash controller) | 1.5d | Raw flash R/W verified in debugger |
| Dual-bank config manager (CRC-16, generation counter) | 2d | Config survives power cycle |
| Config save/load on boot + explicit Save command | 1d | Settings persist automatically |
| ConfigExport/ConfigImport protocol commands (chunked) | 1.5d | Full config backup/restore via HID |
| LED control (WS2812 or PWM, basic modes) | 2d | Static/breathe LED modes |
| FirmwareInfo + Bootloader entry commands | 1d | Host queries version, triggers DFU |
| **Phase 5 total** | **9d** | **Complete firmware with persistence** |

**Exit criteria**: Change key mapping → power cycle → mapping retained. Export config → factory reset → import → all settings restored.

### Summary

| Phase | Description | Effort | Cumulative |
|-------|-------------|--------|------------|
| P1 | USB HID + Ping | 8d | 8d |
| P2 | Key input + mapping | 6d | 14d |
| P3 | Macro VM | 8d | 22d |
| P4 | OLED display + host push | 10d | 32d |
| P5 | Config persistence + polish | 9d | 41d |
| **Total** | | **41 engineer-days** | **~8–9 calendar weeks** |

---

## 4. Testing Strategy

### 4.1 Unit Tests (Host-Side, Native GCC)

All pure-logic modules compile on x86 with `gcc` (not RISC-V) using mock hardware stubs.

| Test File | Module Under Test | Key Assertions |
|-----------|-------------------|----------------|
| `test_protocol.c` | `protocol.c` | Checksum calculation correct for known vectors; malformed packets return ERR; batched commands all dispatched; response indices match request |
| `test_macro_vm.c` | `macro_vm.c` | KEY_DOWN/UP emits correct HID report; DELAY sets correct wait; nested LOOPs (4 deep) execute N×M times; CMP_JMP branches correctly; HALT stops VM; invalid opcodes don't crash; PC stays in bounds |
| `test_display.c` | `display_engine.c`, `display_text.c` | Layer 0 alone → correct framebuffer; layer 15 over layer 0 → correct overlay; disabled layer not rendered; text "A" at (0,0) matches font_8x8 bitmap; clear layer zeros region |
| `test_config.c` | `config.c` | Serialize→deserialize round-trip matches; CRC validates for known data; corrupted bank detected; highest-generation bank selected; dual-bank swap logic correct |
| `test_key_scan.c` | `key_scan.c` | 4ms debounce filters bounce; clean press emits PRESS once; release after hold emits RELEASE once; rapid toggle within 4ms filtered |

**Test framework**: [MinUnit](https://jera.com/techinfo/jtns/jtn002) (3-line macro, zero deps) or [Unity](https://github.com/ThrowTheSwitch/Unity).

```bash
# Build and run tests on host
make test   # Compiles test/*.c + src/{protocol,macro_vm,display_engine,config}.c with -DUNIT_TEST
./build/test_runner
# Expected: "42 tests, 42 passed, 0 failed"
```

### 4.2 Protocol Conformance (Shared Test Vectors)

```json
// test/vectors/protocol_v3.json — consumed by C, Rust, and TypeScript tests
{
  "checksum_vectors": [
    { "packet_hex": "2103000080000D00...", "expected_checksum": "0xA3F1" },
    ...
  ],
  "command_vectors": [
    { "name": "Ping", "request_hex": "...", "expected_response_status": 0 },
    { "name": "DisplayTextLayer", "request_hex": "...", "expected_layer_state": {...} },
    ...
  ]
}
```

All three codebases (firmware C, CLI Rust, WebUI TypeScript) run these same vectors → guarantees protocol interop.

### 4.3 Integration Tests (USB HID Mock)

For the Rust CLI (`sayocli`):
```rust
// Mock HID transport that replays firmware responses
#[test]
fn test_ping_roundtrip() {
    let mock = MockTransport::new()
        .expect_send(ping_request_bytes())
        .respond_with(ping_response_bytes());
    let result = commands::ping(&mock).unwrap();
    assert!(result.latency_us < 10_000);
}

#[test]
fn test_display_push() {
    let mock = MockTransport::new()
        .expect_send(display_text_layer_bytes("CPU: 72°C", 13, 0, 16))
        .respond_with(ok_response_bytes());
    commands::display(&mock, "CPU: 72°C", 13, 0, 16).unwrap();
}
```

### 4.4 Macro Parser/Compiler Tests

```rust
#[test]
fn test_dsl_compile_key_tap() {
    let bytecode = compile_dsl("key_tap ctrl+c").unwrap();
    assert_eq!(bytecode, vec![0x01, 0x06, 0x01,  // KEY_DOWN c, modifier=ctrl
                               0x02, 0x06, 0x01,  // KEY_UP c, modifier=ctrl
                               0xFF]);             // HALT
}

#[test]
fn test_dsl_compile_loop() {
    let bytecode = compile_dsl("repeat 3 { key_tap a, delay 100 }").unwrap();
    assert_eq!(bytecode[0], 0x05); // LOOP_START
    assert_eq!(bytecode[1], 3);    // count=3
    // ... KEY_TAP a, DELAY 100 ...
    assert!(bytecode.contains(&0x06)); // LOOP_END
}

#[test]
fn test_dsl_decompile_roundtrip() {
    let source = "key_tap ctrl+c\ndelay 200\nkey_tap ctrl+v";
    let bytecode = compile_dsl(source).unwrap();
    let decompiled = decompile_bytecode(&bytecode).unwrap();
    assert_eq!(compile_dsl(&decompiled).unwrap(), bytecode); // semantic round-trip
}
```

### 4.5 Hardware-in-the-Loop (Simulation Only — No Flashing)

| Test | Method | What It Validates |
|------|--------|-------------------|
| **QEMU RISC-V** | Run firmware ELF in `qemu-system-riscv32` with virtual UART | Main loop doesn't crash; protocol parser handles packets; macro VM executes to completion. No USB/SPI (stubbed) |
| **Renode** | Full system simulation with CH32V307 model (if available) or generic RV32 | GPIO pin toggle, SPI transactions, timer interrupts |
| **USB Gadget mock** (Linux) | Python script using `uhid` kernel module to emulate device-side HID | End-to-end: CLI sends packet → uhid delivers → firmware logic (running as Linux process with mock HAL) → response → CLI validates |
| **SPI logic capture playback** | Record SPI transactions from stock firmware via logic analyzer → replay against our OLED driver | Init sequence matches; framebuffer flush has correct byte count and timing |

**No physical flashing** is performed in CI. All hardware-specific tests run against simulation or recorded captures.

### 4.6 Fuzz Testing

```bash
# AFL fuzzing of macro VM (finds crash-inducing bytecode)
cd test/fuzz
afl-gcc -o fuzz_macro_vm fuzz_macro_vm.c ../../src/macro/macro_vm.c -DUNIT_TEST
afl-fuzz -i seeds/ -o findings/ ./fuzz_macro_vm

# Seeds: valid bytecode for basic macros (key_tap, delay, loop)
# Goal: no crashes, no infinite loops (VM has step limit of 6144)
```

---

## 5. Risks & Unknowns

### 5.1 Critical Risks

| # | Risk | Severity | Likelihood | Mitigation |
|---|------|----------|------------|------------|
| R1 | **Proprietary bootloader may reject unsigned firmware** | CRITICAL | Medium | Stock bootloader validates MD5, not a signature. We can write the correct MD5 to `0x29FA0`. If it checks more, bypass entirely via SWD/JTAG flashing (writes directly to flash, bootloader never runs our validation). Worst case: replace bootloader via SWD |
| R2 | **Flash read-protection (RDP) may be enabled** | CRITICAL | Low | CH32V307 supports flash protection bits. If stock firmware locked flash, SWD read may fail. Mitigation: full-chip erase clears RDP (destroys stock FW). We have the decrypted stock binary to restore if needed |
| R3 | **Exact GPIO pin assignments unknown** | HIGH | High | Stock firmware RE hasn't mapped all GPIOs to physical keys/SPI/LEDs. Must be determined empirically: probe with multimeter/logic analyzer, or disassemble stock FW init routines in Ghidra. Budget 2 extra days for P1 |
| R4 | **OLED controller model unconfirmed** | MEDIUM | High | SSD1306 vs SH1106 — slight init differences. Implement both behind compile flag. One hardware test confirms which. Low-risk: both are well-documented |
| R5 | **CH32V307 USB HS quirks** | HIGH | Medium | WCH's USB HS implementation has known errata (DMA alignment, endpoint buffer sizing). Must test thoroughly. Vendor SDK examples cover basic HID but not vendor usage pages with 1024B reports. Budget 1 extra day for P1 |

### 5.2 Moderate Risks

| # | Risk | Severity | Likelihood | Mitigation |
|---|------|----------|------------|------------|
| R6 | **Stock bootloader magic word address unknown** | MEDIUM | Medium | We assume `0x20000000` but the actual RAM address where bootloader checks for DFU magic must be RE'd from bootloader binary. Use Ghidra on decrypted bootloader dump. Fallback: hardware DFU entry (hold key during power-on) |
| R7 | **Flash sector size assumption (4KB) may be wrong** | MEDIUM | Low | CH32V307 datasheet lists variable sector sizes (some WCH chips use 256B pages). Wrong assumption breaks wear-leveling math. Verify from datasheet before P5 |
| R8 | **Macro bytecode format partially documented** | MEDIUM | Medium | Stock opcodes from RE may be incomplete. Unknown opcodes → `NOP` fallback in our VM. We control the DSL compiler, so we only generate known opcodes for new macros. Stock macro import may have gaps |
| R9 | **8kHz polling mode (1024B packets) may need tuning** | LOW | Medium | High-speed mode requires precise USB HS endpoint configuration. Start with 1kHz/64B (simpler), add HS in P5 as optimization |
| R10 | **WCH-Link probe availability** | MEDIUM | Low | WCH-Link is cheap (~$5) but may have lead time. Alternative: any SWD-compatible probe + `wch-openocd`. Absolute fallback: USB ISP via `wchisp` (no debug, but can flash) |

### 5.3 Unknowns Requiring Hardware Access

These can only be resolved with physical device in hand:

| Unknown | Resolution Method | Blocks |
|---------|-------------------|--------|
| GPIO pin map (keys, SPI OLED, LEDs) | Continuity test + Ghidra analysis of `SystemInit()` in stock FW | P1, P2, P4 |
| OLED resolution (128×64 or 128×32) | Query stock FW via SysInfo command, or read OLED markings | P4 |
| OLED controller (SSD1306 vs SH1106) | SPI logic capture of stock FW init sequence | P4 |
| Flash sector geometry | CH32V307 reference manual + `flash_read` test | P5 |
| Bootloader DFU magic RAM address | Ghidra on bootloader binary at `0x0` | P5 |
| LED type (WS2812 vs direct PWM) | Visual inspection + oscilloscope on LED data pin | P5 |

### 5.4 Contingency: Cannot Flash Custom Firmware

If the stock bootloader cannot be used (additional validation beyond MD5) AND the device has flash read-protection AND no exposed SWD pins:

1. **Desolder approach**: WCH-Link can connect via test pads if SWD pins exist on PCB (likely, for factory programming)
2. **Glitch attack**: Voltage glitching to bypass RDP (documented for similar WCH chips)
3. **Fallback to host-only**: Abandon custom firmware; build only the CLI + WebUI using stock protocol v2. Loss: no display layer composition, no v3 commands. Gain: zero firmware risk

**Estimated probability of this contingency**: <10%. WCH chips rarely have aggressive read protection, and we have the AES key for encrypted flashing.

---

## 6. Appendices

### 6.1 Reference Documents

| Document | Location |
|----------|----------|
| System Architecture | [`docs/sayobot-o3c-system-architecture.md`](./sayobot-o3c-system-architecture.md) |
| Technical Reference | [`docs/sayobot-o3c-technical-reference.md`](./sayobot-o3c-technical-reference.md) |
| CH32V307 Datasheet | [wch-ic.com/products/CH32V307.html](http://www.wch-ic.com/products/CH32V307.html) |
| CH32V307 Reference Manual | CH32FV2x_V3xRM.pdf (from WCH) |
| WCH SDK | [github.com/openwch/ch32v307](https://github.com/openwch/ch32v307) |
| Reverse Engineering Gist | [gist.github.com/khang06/6186543b](https://gist.github.com/khang06/6186543b560548370ce7cc08cad7f710) |
| Stock Firmware RE (khang06) | Decrypted `.bin`, Ghidra project, protocol analysis |

### 6.2 Glossary

| Term | Definition |
|------|-----------|
| DFU | Device Firmware Update — mode where bootloader accepts new firmware over USB |
| SWD | Serial Wire Debug — 2-pin debug interface (SWDIO + SWCLK), used by WCH-Link |
| ISP | In-System Programming — flash programming via USB boot mode |
| HS | High-Speed USB (480 Mbps) |
| NKRO | N-Key Rollover — all simultaneous key presses reported |
| RDP | Read-out Protection — flash protection preventing debug readback |
| DSL | Domain-Specific Language — human-readable macro syntax compiled to bytecode |

---

*End of firmware development plan.*
