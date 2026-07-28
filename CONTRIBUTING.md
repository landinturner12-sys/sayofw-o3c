# Contributing to SayoFW

Thank you for your interest in contributing. This guide covers the workflow, conventions, and — most importantly — how to add new firmware targets to the web flasher.

---

## Table of Contents

- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Adding a New Firmware Target](#adding-a-new-firmware-target)
  - [1. Build the Binary](#1-build-the-binary)
  - [2. Add the Binary to the Flasher](#2-add-the-binary-to-the-flasher)
  - [3. Update manifest.json](#3-update-manifestjson)
  - [4. Update firmware.json](#4-update-firmwarejson)
  - [5. Update wch-isp.js (if needed)](#5-update-wch-ispjs-if-needed)
  - [6. Test](#6-test)
- [Code Style](#code-style)
- [Commit Messages](#commit-messages)
- [Pull Request Process](#pull-request-process)
- [Reporting Issues](#reporting-issues)

---

## Getting Started

```bash
# Clone
git clone https://github.com/<your-username>/freya-control.git
cd freya-control

# Build firmware (requires RISC-V GCC toolchain)
cd firmware && make all

# Serve the web flasher locally for testing
cd docs/flasher && python3 -m http.server 8080
# Open http://localhost:8080 in Chrome/Edge
```

> **WebUSB works on `localhost`** without HTTPS. No certificate setup needed for local development.

---

## Development Workflow

1. **Fork** the repo and create a feature branch from `main`:
   ```bash
   git checkout -b feat/my-change
   ```
2. Make changes, test locally.
3. Commit with a [conventional message](#commit-messages).
4. Push and open a **Pull Request** against `main`.
5. CI deploys a preview — verify the flasher works end-to-end if you changed anything in `docs/flasher/`.

---

## Adding a New Firmware Target

This is the most common contribution. Follow all six steps.

### 1. Build the Binary

Your firmware must produce a **raw `.bin` file** (not ELF, not HEX). It must be linked for the correct flash address.

| Chip | Bootloader size | Application start address | Max binary size |
|---|---|---|---|
| CH32V307 | 16 KB | `0x08004000` | ~240 KB |

If you're targeting a different WCH chip, determine its bootloader size and application start address from the datasheet.

```bash
# Example: build and extract raw binary
make all
riscv-none-elf-objcopy -O binary build/my_firmware.elf build/my_firmware.bin
```

### 2. Add the Binary to the Flasher

Place the `.bin` file in the flasher's firmware directory:

```
docs/flasher/firmware/
├── firmware.json          # metadata for the default build
├── sayofw_o3c.bin         # existing default firmware
└── my_firmware.bin        # ← your new binary
```

### 3. Update manifest.json

Add a new entry to the `builds` array in `docs/flasher/manifest.json`:

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
    },
    {
      "chipFamily": "CH32V307",
      "parts": [
        {
          "path": "firmware/my_firmware.bin",
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
    "bootloader_vid": "0x4348",
    "bootloader_pid": "0x55e0"
  }
}
```

> **Note:** The `firmware.binary` field controls which binary is flashed by default. If you want your new target to be the default, update this field.

### 4. Update firmware.json

Create or update `docs/flasher/firmware/firmware.json` with the binary's metadata:

```json
{
  "name": "my_firmware",
  "version": "1.0.0",
  "target": "CH32V307",
  "flash_address": "0x08004000",
  "size_bytes": 32768,
  "sha256": "<run: sha256sum docs/flasher/firmware/my_firmware.bin>"
}
```

Always include the SHA-256 hash so users can verify integrity.

### 5. Update wch-isp.js (if needed)

The current `wch-isp.js` hardcodes constants for the CH32V307 bootloader:

```javascript
const WCH_VID = 0x4348;
const WCH_PID = 0x55e0;
const SECTOR_SIZE = 256;
const WRITE_CHUNK = 56;
```

**If your target uses the same WCH ISP bootloader** (all CH32V chips do), no changes are needed.

**If your target has different USB IDs or sector sizes**, you must either:
- Parameterize these constants (preferred — submit as a separate PR), or
- Create a new ISP driver file alongside `wch-isp.js`.

### 6. Test

Before submitting, verify on real hardware:

- [ ] Binary flashes successfully via the web flasher
- [ ] Verify step passes (no "Verify failed" errors)
- [ ] Device boots into the new firmware after reset
- [ ] "Use Custom .bin" also works with your binary
- [ ] `manifest.json` and `firmware.json` are valid JSON (`python3 -m json.tool < manifest.json`)

If you don't have the hardware, state this in the PR — a maintainer will test.

---

## Code Style

### Firmware (C)

- C11, `-Wall -Werror`
- 4-space indent, no tabs
- `snake_case` for functions and variables
- `UPPER_CASE` for constants and macros
- Every public function gets a doc comment in the header

### Web Flasher (JS/HTML/CSS)

- Vanilla JS — no frameworks, no build step
- `camelCase` for JS identifiers
- CSS custom properties for all colors (`:root` block in `style.css`)
- Semantic HTML, accessible labels on interactive elements

---

## Commit Messages

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
feat(flasher): add multi-firmware selector UI
fix(isp): handle timeout on slow erase
docs: update troubleshooting for Linux udev
chore: bump firmware to v0.2.0
```

Scopes: `firmware`, `flasher`, `isp`, `docs`, `ci`.

---

## Pull Request Process

1. **One concern per PR.** Don't mix firmware changes with flasher UI changes.
2. **Describe what and why** in the PR body. Link to issues if applicable.
3. **Include test evidence** — a screenshot of a successful flash, or terminal output.
4. **Keep the diff small.** PRs over 500 lines should be split.
5. A maintainer will review within a few days. Address feedback, then squash-merge.

---

## Reporting Issues

Use [GitHub Issues](https://github.com/ankur-sayofw/freya-control/issues). Include:

- **Browser & version** (e.g., Chrome 120 on Windows 11)
- **OS** (Windows / macOS / Linux distro)
- **Device state** (bootloader mode? normal mode?)
- **Console log** — copy the full log from the web flasher's log panel
- **Steps to reproduce**

For firmware bugs (not flasher bugs), also include:
- Firmware version / git commit
- Toolchain version (`riscv-none-elf-gcc --version`)
- Build output (`make all` log)
