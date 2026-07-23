# SayoDevice O3C — Comprehensive Technical Reference

## 1. Hardware Platform

- **MCU**: WCH CH32V307 — RISC-V 32-bit, NOT STM32/ARM
  - Reference manual: CH32FV2x_V3x
  - SDK: [github.com/openwch/ch32v307](https://github.com/openwch/ch32v307)
  - Toolchain: MounRiver Studio (Eclipse-based, RISC-V GCC)
  - Built-in USB 2.0 HS (480Mbps) — enables 8000Hz polling
  - Clock info exposed via SysInfo: cpu_freq, hclk, pclk_1, pclk_2
- **OLED Display**: Integrated small screen (resolution reported via SysInfo `width`/`height`)
- **USB IDs**: VID `0x8089`, PID `0x0009`
- **Device model codes** (from Sayo_CLI):
  - `0x0002` O2 Standard (STM32 variant)
  - `0x0003` O2C Standard (WCH variant — the O3C line)
  - `0x0004` O2C(S), `0x0005` O2C(T_ES), `0x0006` O2C(T_QS), `0x0007` O2C(MINI), `0x0008` M1T4K

## 2. Firmware Structure

### 2.1 Memory Layout
| Region | Address | Content |
|--------|---------|---------|
| Bootloader | `0x00000` | Loaded at reset, integrity-checks app |
| Application | `0x04000` | Decrypted firmware image base |
| FW Metadata | `0x29F80` | Metadata block |
| FW Size | `0x29F84` | 4-byte image size |
| FW MD5 Hash | `0x29FA0` | 16-byte MD5 of firmware image |

### 2.2 Firmware Encryption
- **Algorithm**: AES-256-CBC
- **Key**: `C4053DDF225E89F74868C1E1F4C00D514F02A8A8692F997869ABEB155250150C`
- **IV**: Zero (16 null bytes)
- **Update URL**: `https://a.sayobot.cn/firmware/update/9/firmware/app_O3C.bin`

### 2.3 Boot Sequence
1. Bootloader at `0x0` runs on power-on
2. Reads firmware size from `0x29F84`, MD5 from `0x29FA0`
3. Computes MD5 of firmware region, compares
4. **Match** → jump to app at `0x4000`
5. **Mismatch** → stay in bootloader mode (awaiting re-flash)
6. Same integrity check runs *before* rebooting after an OTA update

### 2.4 Firmware Analysis
Decrypt the `.bin`, load into IDA/Ghidra at base address `0x4000`. Bootloader dump available separately (loads at `0x0`).

## 3. USB HID Protocol

### 3.1 API v1 (Legacy — Usage Page `0xFF00`)

#### Packet Format
```c
struct hid_packet_v1_t {
    uint8_t report_id;      // 0x00, always 2
    uint8_t id;             // 0x01 — command ID
    uint8_t length;         // 0x02 — data length
    uint8_t data[length];   // 0x03 — variable payload
    uint8_t checksum;       // offset length+3, sum of all preceding bytes
    // zero-padded to 64 bytes total
};
```

#### Response Status Codes (from test.c)
| `cmd` byte | Meaning |
|------------|---------|
| 0 | Success |
| 1 | (data follows) |
| 2 | Tip/info message (ASCII in data) |
| 3 | Data Error |
| 4 | Checksum failed |

#### Command Table (v1)
| ID | Name | Purpose |
|----|------|---------|
| `0x00` | MetaInfo | Query device info (version, model code, support list) |
| `0x01` | MemoryRead | Raw memory read at addr_h:addr_l |
| `0x02` | MemoryWrite | Raw memory write at addr_h:addr_l |
| `0x04` | Save | Persist config to flash |
| `0x06` | SimpleKey | Simple key mapping |
| `0x08` | DeviceName | Get/set device name |
| `0x0B` | Password | Device password |
| `0x0C` | Text | Text/string to display |
| `0x10` | Light | LED lighting control |
| `0x11` | Palette | Color palette config |
| `0x16` | Key | Full key mapping with layers |
| `0x31` | ScreenStart | OLED startup screen |
| `0x32` | ScreenMain | OLED main screen content |
| `0x33` | ScreenSleep | OLED sleep screen |
| `0xFC` | Option | Device options |
| `0xFF` | Bootloader | Enter bootloader / firmware flash |

### 3.2 API v2 (Current — Usage Page `0xFF11`/`0xFF12`)

- `0xFF12` = high-speed mode (8000Hz polling), report_id `0x22`, 1024-byte packets
- `0xFF11` = normal polling rates, report_id `0x21`, 64-byte packets

#### Packet Format
```c
struct hid_cmd_v2_t {
    uint16_t length;            // 0x00 — total cmd length including header
    uint8_t  id;                // 0x02 — command ID
    uint8_t  index;             // 0x03 — response correlation index
    uint8_t  data[length - 4];  // 0x04 — payload
    // padded to 4-byte alignment
};

struct hid_packet_v2_t {
    uint8_t  report_id;     // 0x00 — 0x22 (HS) or 0x21 (normal)
    uint8_t  echo;          // 0x01 — always 3 in web UI
    uint16_t checksum;      // 0x02 — sum of packet as 16-bit words
    hid_cmd_v2_t cmds[];    // 0x04 — one or more commands
    // zero-padded to 1024 (HS) or 64 bytes
};
```
**Key difference**: v2 supports **multiple commands per packet** (batch) and uses 16-bit checksums.

#### Command Table (v2)
| ID | Name | Request/Response Struct |
|----|------|----------------------|
| `0x00` | Info | → model_code(u16), firmware_version(u16), battery(u8), fn(u8), cpu_s(u8), cpu_ms(u8) |
| `0x01` | DeviceName | ↔ name[12] as uint32 (Unicode, 48 bytes) |
| `0x02` | SysInfo | → width(u16), height(u16), refresh_rate(u8), sys_ms(u16), sys_s(u32), vid(u16), pid(u16), cpu_1m(u8), cpu_5m(u8), cpu_freq(u32), hclk(u32), pclk_1/2(u32), adc_0/1(u32) |
| `0x03` | Setting | Device settings |
| `0x04` | BLE | Bluetooth (not on O3C) |
| `0x05` | DeviceLock | Lock device |
| `0x06` | DeviceUnlock | Unlock device |
| `0x07` | IO | GPIO (not on O3C) |
| `0x08` | MonkeyIO | (not on O3C) |
| `0x09` | MonkeyKey | (not on O3C) |


## 4. Key Mapping & Macro Storage (from Sayo_CLI)

### 4.1 HID Data Structure (v1 — o2_protocol.h)
```c
#pragma pack(1)
struct o2_hid_data {  // 64 bytes total
    uint8_t id;        // report ID (always 2)
    uint8_t cmd;       // command byte
    uint8_t data_len;  // payload length
    union {
        struct { uint8_t versionH, versionL, modelH, modelL; } info;
        struct { uint8_t addr_h, addr_l; char data[58]; } config;  // memory R/W
        struct { uint8_t addr_h, addr_l; char data[58]; } script;  // macro script R/W
        struct { uint8_t pattern, number; char name[32]; } script_sw;  // script switch
        struct {  // key mapping
            uint8_t pattern, number, type, retain;
            union { struct { uint8_t plain[4]; } keyboard; uint8_t data[4]; } data;
        } key;
        struct {  // button/touch config
            uint8_t pattern, number, mode, type;
            uint16_t site_x, site_y, retain, shape_x, shape_y, shape_r;
            struct { uint8_t mode, retain, plain[4]; } key_lay[5];  // 5 layers per button
        } bottom;
        struct {  // LED/lamp control
            uint8_t pattern, number, type, event, lamp_cmd_len;
            union {
                struct { uint8_t r, g, b, interval_time; } col;
                struct { uint8_t number, interval_time; } sb;
            } data;
        } lamp;
        struct { uint8_t pattern; char data[59]; } name_of_device;
        struct { uint8_t pattern, number; char pwd[58]; } ok_pwd;
    } data;
    uint8_t check_sum;  // sum of bytes [0..data_len+2]
};
```

### 4.2 Macro/Script System (script.h)
The device has a full bytecode VM for macros — NOT just keystroke recording:

**Control Flow**: NOP, JMP, SJMP, AJMP, CALL, RET, conditional jumps (JC/JNC/JZ/JNZ/DJNZ/CJNE)
**Key Operations**: SK (set key), GK (gamepad key), MK (mouse key), MU (multimedia), with UP variants (USK/UGK/UMK)
**Mouse/Gamepad**: MO_XYZ (mouse move), GA_XYZ (gamepad axis), TB_XY (tablet)
**ALU**: ADD, SUB, MUL, DIV, ANL, ORL, XRL, RL, RR, CLR, CPL, XCH, MOV, PUSH, POP
**Timing**: SLEEP (ms), SLEEP_X256, SLEEP_RAND (random delay)
**LED**: LED_CTRL (select/on/off/reload), LED_COL (set color)
**Registers**: R0-R3, A, B, DPTR, RET, V0-V3, plus indirect @R0-@R3, @DPTR
**System**: SYS_TIME_MS, SYS_TIME_S, SYS_KBLED

Scripts stored as bytecode, transferred in 58-byte chunks via memory read/write at addr_h:addr_l. Up to 6144 script steps, 256 script slots.

## 5. OLED Display Protocol

### 5.1 Display Commands (v1 API)
| Command | ID | Purpose |
|---------|-----|---------|
| ScreenStart | `0x31` | Boot/splash screen content |
| ScreenMain | `0x32` | Active display content |
| ScreenSleep | `0x33` | Sleep/idle screen |
| Text | `0x0C` | Push text string to display |

### 5.2 Display Parameters (from v2 SysInfo)
- Resolution reported as `width` × `height` (u16 each)
- Refresh rate reported as `refresh_rate` (u8)
- The SayoDeviceStreamingAssistant (C# .NET) captures screen regions and streams framebuffer data to the OLED over USB HID

### 5.3 Real-time Display Push
The streaming assistant demonstrates that arbitrary image/text data can be pushed to the display in real-time over USB. The display accepts framebuffer data through the screen commands, making CLI-driven real-time display updates feasible.

## 6. Existing Software Ecosystem

### 6.1 SayoDevice_Web (v1 — Vue.js)
- **Stack**: Vue 2 + Bootstrap-Vue + vcolorpicker
- **License**: GPLv3
- **Architecture**: Single-page app (`Connect.vue` = 46KB, entire UI in one component)
- **Communication**: HTTP XHR to local Sayo_CLI server (NOT direct USB)
- **API endpoints**: `/API/DEVICES/*` for device operations
- Supports: key mapping, lighting, scripts, device naming, password, firmware update
- i18n via JSON lang files

### 6.2 sayo-device-web-hid (v2 — Angular)
- **Stack**: Angular 13 + Angular Material + RxJS
- **Communication**: WebHID API (direct browser-to-device, no middleware)
- **Status**: Scaffolded but app.component.html is still Angular starter template — **incomplete/WIP**
- Uses v2 protocol with usage pages `0xFF11`/`0xFF12`

### 6.3 Sayo_CLI (C++ middleware)
- **Stack**: C++ with hidapi, jsoncpp, custom HTTP server
- **Architecture**: Embedded HTTP server (port 7296) serving the Vue web UI + JSON API
- **Platform**: Windows (WinSock) and Linux/macOS (POSIX sockets)
- **Protocol**: Implements full v1 HID protocol — key mapping, lighting, scripts, bootloader, firmware write
- **API**: REST-style JSON over HTTP, device operations at `/API/DEVICES/`
- **udev rules**: `98-saybot.rules` provided for Linux

### 6.4 SayoDeviceStreamingAssistant (C# .NET)
- **Stack**: C# .NET, Windows Forms
- **Purpose**: Capture screen regions → stream to device OLED display
- **Demonstrates**: Real-time framebuffer push over USB HID is working and supported

## 7. Bootloader & DFU Flashing

### 7.1 Entering Bootloader
- Send command `0xFF` (v1) via HID
- Device response with `cmd==2` indicates bootloader mode
- Bootloader reports model code for device identification

### 7.2 Firmware Write Process
1. Download encrypted `.bin` from `https://a.sayobot.cn/firmware/update/9/firmware/app_O3C.bin`
2. Decrypt with AES-256-CBC (key above, zero IV)
3. Transfer via `firmware_write` command (referenced in manual format JSONs)
4. Update URL pattern: `https://cmcc.sayobot.cn:25225/devices/update/firmware_0x0809_0x0002.json`
5. Bootloader validates MD5 at `0x29FA0` against image before accepting reboot

### 7.3 CH32V307 Native Flashing
- WCH-Link debugger (JTAG/SWD equivalent for RISC-V)
- WCHISPTool for USB-based ISP programming
- MounRiver Studio integrates build + flash

## 8. Protocol Checksum Details

### v1 Checksum
```python
checksum = sum(packet[0:data_len+3]) & 0xFF
packet[data_len+3] = checksum
```

### v2 Checksum
```python
# Reinterpret packet as uint16 array, sum all words
checksum = sum(struct.unpack(f'<{len(packet)//2}H', packet)) & 0xFFFF
# Written to packet[2:4] as little-endian uint16
```

## 9. Memory Read/Write Protocol

Commands `0x01`/`0x02` (v1) provide raw memory access:
- `addr_h:addr_l` = 16-bit address (big-endian in packet)
- `data[58]` = up to 58 bytes per transfer
- Used for reading/writing key configs, scripts, device settings
- `Save` command (`0x04`) persists RAM changes to flash

## 10. Summary of Key Findings

| Aspect | Detail |
|--------|--------|
| MCU | CH32V307 (RISC-V), NOT ARM/STM32 |
| Toolchain | MounRiver Studio / RISC-V GCC |
| USB | HID with vendor usage pages, 64B or 1024B packets |
| Protocol versions | v1 (0xFF00, 64B) and v2 (0xFF11/12, batched cmds) |
| Display | OLED with real-time framebuffer push capability |
| Macros | Full bytecode VM with ALU, control flow, registers |
| Existing Web UI | Vue 2 (working, via CLI proxy) + Angular (incomplete, WebHID) |
| Firmware | AES-256-CBC encrypted, MD5-validated, OTA-updatable |
| Flashing | HID bootloader cmd 0xFF + WCH native tools |
