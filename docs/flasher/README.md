# SayoFW Web Flasher — Documentation

> Flash custom firmware to the Sayobot O3C macropad directly from your browser — no tools, no drivers (on most platforms), no CLI.

**Live site:** `https://<your-username>.github.io/freya-control/`

---

## Table of Contents

- [Project Overview](#project-overview)
- [How It Works](#how-it-works)
- [Prerequisites](#prerequisites)
  - [Supported Browsers](#supported-browsers)
  - [Operating System Notes](#operating-system-notes)
- [How to Use the Web Flasher](#how-to-use-the-web-flasher)
  - [Step 1 — Enter Bootloader Mode](#step-1--enter-bootloader-mode)
  - [Step 2 — Open the Web Flasher](#step-2--open-the-web-flasher)
  - [Step 3 — Connect the Device](#step-3--connect-the-device)
  - [Step 4 — Flash the Firmware](#step-4--flash-the-firmware)
  - [Step 5 — Verify and Reboot](#step-5--verify-and-reboot)
  - [Flashing a Custom .bin File](#flashing-a-custom-bin-file)
- [How to Customize](#how-to-customize)
  - [Replacing the Firmware Binary](#replacing-the-firmware-binary)
  - [Updating manifest.json](#updating-manifestjson)
  - [Updating firmware.json](#updating-firmwarejson)
  - [Changing Branding and Appearance](#changing-branding-and-appearance)
- [How to Deploy (GitHub Pages)](#how-to-deploy-github-pages)
  - [Fork and Enable Pages](#fork-and-enable-pages)
  - [Custom Domain (Optional)](#custom-domain-optional)
  - [Verify the Deployment](#verify-the-deployment)
- [Architecture](#architecture)
  - [Directory Layout](#directory-layout)
  - [WebUSB / WCH ISP Protocol](#webusb--wch-isp-protocol)
  - [Flash Process Sequence](#flash-process-sequence)
- [Troubleshooting](#troubleshooting)
- [FAQ](#faq)

---

## Project Overview

The **SayoFW Web Flasher** is a static, single-page web application that flashes firmware onto the **Sayobot O3C** macropad (WCH CH32V307, RISC-V) through the browser using the **WebUSB API**.

### What it does

| Capability | Detail |
|---|---|
| **Target device** | Sayobot O3C (3-key OLED macropad) |
| **Target MCU** | WCH CH32V307 — RISC-V RV32IMAFCX, 144 MHz |
| **Protocol** | WCH ISP bootloader over USB bulk transfers |
| **Bootloader USB IDs** | VID `0x4348`, PID `0x55e0` |
| **Application USB IDs** | VID `0x8089`, PID `0x0009` |
| **Flash address** | `0x08004000` (after 16 KB vendor bootloader) |
| **Default firmware** | SayoFW v0.1.0 — open-source replacement with macro VM, display compositor, HID protocol |

### What it does NOT do

- Does **not** overwrite the vendor bootloader (first 16 KB at `0x00000000`).
- Does **not** require any native software installation.
- Does **not** support ARM-based devices — only WCH CH32V RISC-V chips.

### Why a web flasher?

End users shouldn't need to install Rust toolchains, `wchisp`, or understand flash addresses. A web flasher lets anyone update their O3C firmware in under 60 seconds with just a browser and a USB cable.

---

## How It Works

```
┌──────────────────────────────────────────────────────────┐
│  Browser (Chrome / Edge / Opera)                         │
│                                                          │
│  index.html ─── main.js ─── wch-isp.js                  │
│       │              │            │                      │
│       │         loads manifest    implements WCH ISP     │
│       │         & firmware.bin    over WebUSB bulk I/O   │
│       │              │            │                      │
│       ▼              ▼            ▼                      │
│  ┌──────────────────────────────────────────────┐        │
│  │  navigator.usb.requestDevice()               │        │
│  │  → filter: VID 0x4348, PID 0x55e0            │        │
│  └──────────────────────────────────────────────┘        │
│       │                                                  │
└───────┼──────────────────────────────────────────────────┘
        │  USB bulk transfers
        ▼
┌──────────────────────────────────────────────────────────┐
│  CH32V307 — WCH Bootloader (ISP mode)                    │
│                                                          │
│  Commands: Identify → ReadConfig → SetKey → Erase        │
│            → Program (56-byte chunks) → Verify → Reset   │
└──────────────────────────────────────────────────────────┘
```

1. The page loads `manifest.json` to display firmware metadata.
2. User clicks **Connect & Flash** → browser shows a USB device picker.
3. `wch-isp.js` sends ISP commands: identify → read config → set XOR key → erase → program → verify → reset.
4. Firmware is written in 56-byte chunks to `0x08004000` with XOR encryption.
5. After verify, the device resets into the new firmware.

---

## Prerequisites

### Supported Browsers

WebUSB is required. Only Chromium-based browsers implement it:

| Browser | Minimum Version | Status |
|---|---|---|
| **Google Chrome** | 89+ | ✅ Fully supported |
| **Microsoft Edge** | 89+ | ✅ Fully supported |
| **Opera** | 75+ | ✅ Fully supported |
| **Brave** | 89+ | ⚠️ Works (may need `brave://flags/#enable-experimental-web-platform-features`) |
| Firefox | — | ❌ Not supported (no WebUSB) |
| Safari | — | ❌ Not supported (no WebUSB) |
| Mobile browsers | — | ❌ Not supported (WebUSB is desktop-only) |

> **How to check your version:** navigate to `chrome://version` (or `edge://version`).

### Operating System Notes

#### Windows

- **Driver required.** The WCH bootloader does not use the standard HID class — Windows needs a WinUSB driver bound to the bootloader's USB interface.
- **Install via [Zadig](https://zadig.akeo.ie/):**
  1. Put the O3C into bootloader mode (see [Step 1](#step-1--enter-bootloader-mode)).
  2. Download and run [Zadig](https://zadig.akeo.ie/).
  3. In the dropdown, select the device with USB ID **4348:55E0**.
     - If you don't see it, go to **Options → List All Devices**.
  4. Set the target driver to **WinUSB**.
  5. Click **Replace Driver** (or **Install Driver** if no driver is assigned).
  6. Done. The web flasher can now access the device.

> **Note:** You only need to do this once per machine. The WinUSB driver persists across reboots.

> **Common Windows chip drivers:** If your O3C uses a CP2102 or CH340 USB-to-serial bridge for normal operation (not bootloader mode), the standard drivers from [Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) or [WCH CH340](http://www.wch-ic.com/downloads/CH341SER_EXE.html) may also be needed. These are separate from the Zadig/WinUSB driver required for flashing.

#### macOS

- **No driver needed.** WebUSB works out of the box on macOS.
- Tested on macOS 12 (Monterey) and later.

#### Linux

- **udev rule required.** By default, non-root users cannot access raw USB devices.
- **Create the rule:**

```bash
# Create the udev rule file
sudo tee /etc/udev/rules.d/99-wch-isp.rules << 'EOF'
# WCH CH32V bootloader (ISP mode) — allow unprivileged WebUSB access
SUBSYSTEM=="usb", ATTR{idVendor}=="4348", ATTR{idProduct}=="55e0", MODE="0666", TAG+="uaccess"

# SayoFW application mode (optional, for future WebHID tools)
SUBSYSTEM=="usb", ATTR{idVendor}=="8089", ATTR{idProduct}=="0009", MODE="0666", TAG+="uaccess"
EOF

# Reload rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

- After creating the rule, **unplug and re-plug** the device.
- Verified on Ubuntu 22.04, Fedora 38, and Arch Linux.

#### ChromeOS

- Works if the Chrome version is 89+. No additional setup needed.

---

## How to Use the Web Flasher

### Step 1 — Enter Bootloader Mode

The CH32V307's vendor bootloader is always present in the first 16 KB of flash. To activate it:

1. **Unplug** the O3C from USB.
2. **Hold the BOOT0 button** — a small tactile button on the PCB (you may need to open the case).
3. **While holding BOOT0**, plug the USB cable into your computer.
4. **Release BOOT0.**

The device is now in ISP/bootloader mode. The OLED will stay blank — this is expected.

> **Tip:** If the device has working SayoFW firmware, you can also enter bootloader mode via a USB HID command (planned for future releases).

<!-- Screenshot placeholder: photo of O3C PCB showing BOOT0 button location -->
<!-- ![Entering bootloader mode](static/img/bootloader-mode.png) -->

### Step 2 — Open the Web Flasher

Navigate to the live site:

```
https://<your-username>.github.io/freya-control/
```

The page will check browser compatibility on load. If WebUSB is available, you'll see a green "Browser supports WebUSB ✓" message in the log console.

<!-- Screenshot placeholder: web flasher initial state -->
<!-- ![Web flasher loaded](static/img/flasher-loaded.png) -->

### Step 3 — Connect the Device

1. Click the **🔌 Connect & Flash** button.
2. A browser dialog appears listing available USB devices.
3. Select the device labeled **4348:55E0** (WCH ISP device).
4. Click **Connect**.

The flasher will:
- Open the USB interface
- Send an `Identify` command to read the chip ID
- Read the chip configuration (UID, bootloader version)
- Set the ISP encryption key

You'll see device details (chip ID, flash size, UID, bootloader version) appear on screen.

<!-- Screenshot placeholder: device connected, showing chip info -->
<!-- ![Device connected](static/img/device-connected.png) -->

### Step 4 — Flash the Firmware

Once connected, the firmware card appears showing the bundled firmware:

| Field | Value |
|---|---|
| Name | SayoFW O3C |
| Version | v0.1.0 |
| Target | CH32V307 |
| Flash address | 0x08004000 |

Click **⚡ Flash Firmware** to begin. The process:

1. **Erase** — clears the required flash sectors
2. **Program** — writes firmware in 56-byte chunks with XOR encryption
3. **Verify** — reads back and compares every byte
4. **Reset** — reboots the device into the new firmware

A progress bar shows real-time progress and write speed (typically ~2–5 KB/s over USB bulk).

<!-- Screenshot placeholder: flashing in progress with progress bar -->
<!-- ![Flashing in progress](static/img/flashing-progress.png) -->

### Step 5 — Verify and Reboot

On success, you'll see:

```
✅ Flash Complete!
Firmware written and verified. Unplug and replug your O3C to start the new firmware.
```

Unplug the USB cable, then plug it back in **without** holding BOOT0. The O3C should boot into SayoFW.

<!-- Screenshot placeholder: flash complete success -->
<!-- ![Flash complete](static/img/flash-complete.png) -->

### Flashing a Custom .bin File

Instead of the bundled firmware, you can flash any `.bin` file:

1. Complete Steps 1–3 (connect the device).
2. Click **📁 Use Custom .bin** instead of "Flash Firmware".
3. Select your `.bin` file from the file picker.
4. The firmware card updates to show the custom file name and size.
5. Click **⚡ Flash Firmware**.

> **Warning:** The flasher writes to `0x08004000` regardless of the file you provide. Make sure your binary is linked for that address. An incorrect binary won't brick the device (the bootloader is never overwritten), but the firmware won't work until you re-flash a correct one.

---

## How to Customize

### Replacing the Firmware Binary

To ship a different firmware build:

1. Build your firmware (output must be a raw `.bin`, not ELF):
   ```bash
   cd firmware && make all
   # Output: build/sayofw_o3c.bin
   ```

2. Copy the binary into the flasher's firmware directory:
   ```bash
   cp firmware/build/sayofw_o3c.bin docs/flasher/firmware/sayofw_o3c.bin
   ```

3. Update `docs/flasher/firmware/firmware.json` with the new metadata:
   ```json
   {
     "name": "sayofw_o3c",
     "version": "0.2.0",
     "target": "CH32V307",
     "flash_address": "0x08004000",
     "size_bytes": 32768,
     "sha256": "<sha256sum of the .bin file>",
     "note": "Production build"
   }
   ```

4. Update the version in `docs/flasher/manifest.json` (see below).

5. Commit and push — GitHub Actions deploys automatically.

### Updating manifest.json

`docs/flasher/manifest.json` is the top-level metadata file loaded by the flasher UI on startup. It controls what the user sees:

```json
{
  "name": "SayoFW O3C",
  "version": "v0.2.0",
  "description": "Custom open-source firmware for Sayobot O3C",
  "builds": [
    {
      "chipFamily": "CH32V307",
      "parts": [
        {
          "path": "firmware/sayofw_o3c.bin",
          "offset": 16384
        }
      ]
    }
  ],
  "firmware": {
    "binary": "firmware/sayofw_o3c.bin",
    "target": "CH32V307",
    "flash_address": "0x08004000",
    "mcu": "WCH CH32V307 (RISC-V RV32IMAFCX)",
    "usb_vid": "0x8089",
    "usb_pid": "0x0009",
    "bootloader_vid": "0x4348",
    "bootloader_pid": "0x55e0"
  }
}
```

| Field | Purpose |
|---|---|
| `name` | Display name in the header and firmware card |
| `version` | Shown as a badge; update on each release |
| `description` | Subtitle text below the firmware name |
| `builds[].chipFamily` | Informational — the target chip |
| `builds[].parts[].path` | Relative path to the `.bin` file (from `docs/flasher/`) |
| `builds[].parts[].offset` | Byte offset = flash address − base (16384 = 0x4000) |
| `firmware.binary` | Same path as above (used by `main.js` `FW_URL`) |
| `firmware.bootloader_vid/pid` | USB filter for the WebUSB device picker |

### Updating firmware.json

`docs/flasher/firmware/firmware.json` provides per-binary metadata (checksum, size). This is separate from `manifest.json` to allow multiple firmware variants in the future:

```json
{
  "name": "sayofw_o3c",
  "version": "0.2.0",
  "target": "CH32V307",
  "flash_address": "0x08004000",
  "size_bytes": 32768,
  "sha256": "e3b0c44298fc1c149afbf4c8996fb924..."
}
```

Generate the SHA-256:
```bash
sha256sum docs/flasher/firmware/sayofw_o3c.bin
```

### Changing Branding and Appearance

All visual elements are in three files:

| File | What to change |
|---|---|
| `docs/flasher/index.html` | Page title, header text, footer links, step instructions |
| `docs/flasher/static/css/style.css` | Colors (CSS custom properties in `:root`), fonts, layout |
| `docs/flasher/manifest.json` | Firmware name, version badge, description text |

#### Changing colors

Edit the CSS custom properties at the top of `style.css`:

```css
:root {
  --bg: #0f1117;        /* page background */
  --surface: #1a1d27;   /* card background */
  --accent: #6c5ce7;    /* primary accent (buttons, active step) */
  --accent2: #a29bfe;   /* secondary accent (links) */
  --green: #00b894;     /* success states */
  --red: #e17055;       /* error states */
  --orange: #f39c12;    /* warning states */
  --blue: #74b9ff;      /* info badges */
}
```

#### Changing the page title and header

In `index.html`, edit:
```html
<title>SayoFW Web Flasher — Flash your O3C from the browser</title>
<!-- ... -->
<h1 class="header__title">SayoFW Web Flasher</h1>
<p class="header__subtitle">Custom firmware for the Sayobot O3C macropad</p>
```

#### Adding a logo

Place an image in `docs/flasher/static/img/` and update the header icon in `index.html`:
```html
<!-- Replace the emoji icon -->
<span class="header__icon">⌨️</span>
<!-- With an image -->
<img class="header__icon" src="static/img/logo.png" alt="Logo" width="40">
```

---

## How to Deploy (GitHub Pages)

The flasher is a fully static site — no build step, no server, no framework. GitHub Pages serves it directly.

### Fork and Enable Pages

1. **Fork** the repository on GitHub (or push your own copy).

2. Go to **Settings → Pages** in your repository.

3. Under **Build and deployment → Source**, select **GitHub Actions**.

4. The workflow (`.github/workflows/deploy.yml`) triggers automatically on push to `main`. It:
   - Checks out the repo
   - Uploads `docs/flasher/` as a Pages artifact
   - Deploys to GitHub Pages

5. Wait 1–2 minutes for the first deployment.

> **Note:** The `.nojekyll` file in `docs/flasher/` is already committed — this tells GitHub Pages to serve files as-is without Jekyll processing.

### Custom Domain (Optional)

1. Edit `docs/flasher/CNAME` — uncomment and set your domain:
   ```
   flasher.yourdomain.com
   ```

2. In your DNS provider, create a **CNAME record**:
   ```
   flasher.yourdomain.com → <your-username>.github.io
   ```

3. In GitHub **Settings → Pages → Custom domain**, enter `flasher.yourdomain.com`.

4. Enable **Enforce HTTPS** (required for WebUSB — it only works on HTTPS or `localhost`).

### Verify the Deployment

1. Navigate to `https://<your-username>.github.io/freya-control/`.
2. Open the browser DevTools console (F12).
3. Confirm no 404 errors for `manifest.json`, `style.css`, `main.js`, or `wch-isp.js`.
4. Confirm the log shows "Browser supports WebUSB ✓".
5. (Optional) Connect a device to test the full flash flow.

> **HTTPS is mandatory.** WebUSB is blocked on plain HTTP (except `localhost`). GitHub Pages serves HTTPS by default.

---

## Architecture

### Directory Layout

```
docs/flasher/
├── .nojekyll              # Tells GitHub Pages to skip Jekyll
├── CNAME                  # Custom domain config (commented out by default)
├── index.html             # Single-page application — all UI
├── manifest.json          # Firmware metadata (name, version, paths)
├── README.md              # This file
├── firmware/
│   ├── firmware.json      # Per-binary metadata (sha256, size)
│   └── sayofw_o3c.bin     # Firmware binary (raw .bin, not ELF)
└── static/
    ├── css/
    │   └── style.css      # All styles — dark theme, responsive
    ├── img/               # Screenshots and logos (currently empty)
    └── js/
        ├── main.js        # UI controller — state machine, progress, file handling
        └── wch-isp.js     # WCH ISP protocol — WebUSB bulk transfers
```

### WebUSB / WCH ISP Protocol

`wch-isp.js` implements the WCH CH32V bootloader ISP protocol over USB bulk endpoints. Key constants:

| Constant | Value | Purpose |
|---|---|---|
| `WCH_VID` | `0x4348` | Vendor ID for WCH bootloader mode |
| `WCH_PID` | `0x55e0` | Product ID for WCH bootloader mode |
| `SECTOR_SIZE` | 256 bytes | Flash erase granularity |
| `WRITE_CHUNK` | 56 bytes | Max payload per program/verify command |

ISP command bytes:

| Command | Byte | Description |
|---|---|---|
| `CMD_IDENTIFY` | `0xA1` | Read chip ID and type |
| `CMD_ISP_END` | `0xA2` | Reset device (end ISP session) |
| `CMD_ISP_KEY` | `0xA3` | Set XOR encryption key (30-byte random seed) |
| `CMD_ERASE` | `0xA4` | Erase N sectors |
| `CMD_PROGRAM` | `0xA5` | Write chunk at address (XOR-encrypted) |
| `CMD_VERIFY` | `0xA6` | Verify chunk at address (XOR-encrypted) |
| `CMD_READ_CFG` | `0xA7` | Read chip config (UID, bootloader version) |
| `CMD_WRITE_CFG` | `0xA8` | Write chip config (not used by the flasher) |

### Flash Process Sequence

```
Browser                           CH32V307 Bootloader
   │                                     │
   │  CMD_IDENTIFY (0xA1)                │
   │────────────────────────────────────▶│
   │◀────────────────────────────────────│  chip_id, chip_type
   │                                     │
   │  CMD_READ_CFG (0xA7)               │
   │────────────────────────────────────▶│
   │◀────────────────────────────────────│  UID, bootloader version
   │                                     │
   │  CMD_ISP_KEY (0xA3) + 30 random     │
   │────────────────────────────────────▶│
   │◀────────────────────────────────────│  ACK
   │                                     │
   │  CMD_ERASE (0xA4) + sector count    │
   │────────────────────────────────────▶│
   │◀────────────────────────────────────│  ACK
   │                                     │
   │  CMD_PROGRAM (0xA5) + addr + data   │  ← repeated for each
   │────────────────────────────────────▶│    56-byte chunk
   │◀────────────────────────────────────│  ACK
   │          ... (loop) ...             │
   │                                     │
   │  CMD_VERIFY (0xA6) + addr + data    │  ← repeated for each
   │────────────────────────────────────▶│    56-byte chunk
   │◀────────────────────────────────────│  ACK
   │          ... (loop) ...             │
   │                                     │
   │  CMD_ISP_END (0xA2)                 │
   │────────────────────────────────────▶│
   │                                     │  Device resets
   │           ✅ Done                    │
```

---

## Troubleshooting

### "No device found" / Device picker is empty

| Cause | Fix |
|---|---|
| Device not in bootloader mode | Re-do [Step 1](#step-1--enter-bootloader-mode): unplug → hold BOOT0 → plug in → release |
| USB cable is charge-only | Use a data-capable USB cable |
| **Windows:** WinUSB driver not installed | Run [Zadig](https://zadig.akeo.ie/) and install WinUSB for VID 4348 (see [Windows notes](#windows)) |
| **Linux:** No udev rule | Create `/etc/udev/rules.d/99-wch-isp.rules` (see [Linux notes](#linux)) |
| Device claimed by another app | Close `wchisp`, WCH ISP Studio, or any other tool that may hold the USB interface |
| USB hub issues | Try connecting directly to a port on your computer (no hub) |

### "Unsupported browser" / WebUSB not available

- Use **Chrome 89+**, **Edge 89+**, or **Opera 75+**.
- Firefox and Safari will **never** work — they do not implement WebUSB.
- Make sure you're on **HTTPS** or **localhost**. WebUSB is blocked on plain HTTP.
- If using Brave, enable `brave://flags/#enable-experimental-web-platform-features`.

### "Connection failed" / USB transfer error

- **Unplug and replug** the device in bootloader mode.
- **Try a different USB port** (prefer rear ports on desktops).
- **Close other USB tools** — only one application can claim the USB interface at a time.
- **Windows:** Confirm Zadig shows "WinUSB" as the current driver for device 4348:55E0.

### "Erase failed" / "Program failed at 0x..." / "Verify failed at 0x..."

- The bootloader may have timed out. **Unplug, re-enter bootloader mode, and retry.**
- If "Verify failed" occurs consistently at the same address, the flash may have a bad sector. Try flashing a few more times — flash memory can sometimes recover.
- Ensure the firmware binary is not corrupted: re-download or rebuild it.
- Check the binary size — CH32V307 has 256 KB flash, and the application region starts at 0x4000, so the max binary size is ~240 KB.

### "Flash Complete" but device doesn't boot

- The firmware binary may not be linked for the correct address (`0x08004000`). Re-check your linker script.
- If the firmware's interrupt vector table doesn't have a valid entry at offset 0, the bootloader may not jump to it.
- **Recovery is always possible:** re-enter bootloader mode (BOOT0 button) and flash again. The vendor bootloader is never overwritten.

### Device disconnects during flashing

- Use a short, high-quality USB cable.
- Avoid USB hubs and extension cables.
- On laptops, ensure the USB port provides enough power (try a powered hub if needed).
- Retry — the bootloader will still be intact.

### Console shows "Using default firmware config"

- `manifest.json` failed to load. Check that the file exists at `docs/flasher/manifest.json` and is valid JSON.
- This is a warning, not an error — the flasher will still work with hardcoded defaults.

---

## FAQ

**Q: Can I brick my O3C with the web flasher?**
A: No. The vendor bootloader occupies the first 16 KB of flash and is never overwritten. If the application firmware has an invalid MD5 or fails to boot, the bootloader stays in ISP mode automatically. You can always re-flash.

**Q: How long does flashing take?**
A: A typical ~32 KB firmware takes 15–30 seconds (program + verify at ~2–5 KB/s over USB bulk).

**Q: Can I flash firmware built for a different chip?**
A: Don't. The flasher targets CH32V307 specifically. Flashing a binary built for a different MCU will result in non-functional firmware (but won't brick the device).

**Q: Does the web flasher work offline?**
A: Yes, once the page is loaded. All assets (HTML, CSS, JS, firmware binary) are served as static files. There are no server-side dependencies.

**Q: Can I add support for other WCH chips (CH32V203, CH32V003, etc.)?**
A: The ISP protocol is similar across WCH chips, but USB IDs, flash layout, and sector sizes differ. You would need to modify `wch-isp.js` constants, add new entries to `manifest.json`, and test on the target hardware. See [CONTRIBUTING.md](../../CONTRIBUTING.md) for guidelines.

**Q: Why 56-byte chunks instead of larger transfers?**
A: The WCH ISP protocol has a maximum payload size per command. 56 bytes is the safe limit discovered through testing with the CH32V307 bootloader. Larger payloads cause transfer failures.

---

## Related Documentation

- [Main project README](../../README.md) — firmware build instructions, architecture, roadmap
- [CONTRIBUTING.md](../../CONTRIBUTING.md) — how to contribute, add firmware targets
- [WCH CH32V307 datasheet](http://www.wch-ic.com/products/CH32V307.html)
- [WebUSB API spec](https://wicg.github.io/webusb/)
- [Zadig USB driver tool](https://zadig.akeo.ie/)
