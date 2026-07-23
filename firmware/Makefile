# =============================================================================
# Sayobot O3C Custom Firmware — Build System
# =============================================================================
# Target: WCH CH32V307 (RISC-V RV32IMAFCX, 144 MHz)
# Layout: Bootloader @ 0x00000000, Application @ 0x08004000, Metadata @ 0x08029F80
# =============================================================================

PROJECT      := sayofw_o3c
VERSION      := 0.1.0
BUILD_DIR    := build
SRC_DIR      := src
INC_DIR      := include
TEST_DIR     := tests

# ---------- Toolchain ----------
CROSS        := riscv-none-elf-
CC           := $(CROSS)gcc
AS           := $(CROSS)gcc -x assembler-with-cpp
LD           := $(CROSS)gcc
OBJCOPY      := $(CROSS)objcopy
OBJDUMP      := $(CROSS)objdump
SIZE         := $(CROSS)size

# ---------- Architecture flags ----------
CPU          := -march=rv32imafcx -mabi=ilp32f
LDSCRIPT     := linker/ch32v307_app.ld

# ---------- Compiler flags ----------
CFLAGS       := $(CPU) -Os -std=gnu11 -Wall -Wextra -Werror \
                -ffunction-sections -fdata-sections -fno-common \
                -fno-builtin -nostdlib -nostartfiles \
                -DVERSION='"$(VERSION)"' \
                -I $(INC_DIR)
ASFLAGS      := $(CPU)
LDFLAGS      := $(CPU) -T $(LDSCRIPT) -Wl,--gc-sections \
                -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map \
                -nostdlib -nostartfiles

# ---------- Sources ----------
C_SRCS       := $(shell find $(SRC_DIR) -name '*.c' 2>/dev/null)
S_SRCS       := $(shell find $(SRC_DIR) -name '*.S' 2>/dev/null)
SRCS         := $(C_SRCS) $(S_SRCS)
OBJS         := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
                $(patsubst %.S,$(BUILD_DIR)/%.o,$(S_SRCS))
DEPS         := $(OBJS:.o=.d)

# ---------- Native (host) test toolchain ----------
TEST_CC      := gcc
TEST_CFLAGS  := -std=gnu11 -Wall -Wextra -Werror -I $(INC_DIR) -O2 -g
TEST_LDFLAGS :=

# ---------- Targets ----------
.PHONY: all clean flash size distclean test test-clean help

all: $(BUILD_DIR)/$(PROJECT).elf $(BUILD_DIR)/$(PROJECT).bin $(BUILD_DIR)/$(PROJECT).hex
	@echo ""
	@echo "Build OK. Sizes:"
	@$(SIZE) $(BUILD_DIR)/$(PROJECT).elf
	@echo ""
	@echo "Output files:"
	@ls -la $(BUILD_DIR)/$(PROJECT).{elf,bin,hex,map} 2>/dev/null

$(BUILD_DIR)/$(PROJECT).elf: $(OBJS) $(LDSCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	@echo "  LD      $@"

$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@
	@echo "  BIN     $@"

$(BUILD_DIR)/$(PROJECT).hex: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@
	@echo "  HEX     $@"

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -c $< -o $@
	@echo "  CC      $<"

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@
	@echo "  AS      $<"

# ---------- Flash (manual, build-only by default) ----------
# Flashing requires external consent. Default is to print the command.
flash: $(BUILD_DIR)/$(PROJECT).bin
	@echo ""
	@echo "============================================================"
	@echo "  FIRMWARE BUILD COMPLETE — MANUAL FLASH ONLY"
	@echo "============================================================"
	@echo "  Binary: $(BUILD_DIR)/$(PROJECT).bin"
	@echo "  Load addr (app): 0x08004000"
	@echo "  Size: $$($(SIZE) --format=berkeley $(BUILD_DIR)/$(PROJECT).elf)"
	@echo ""
	@echo "  To flash, run ONE of the following explicitly:"
	@echo "    wchisp flash --address 0x08004000 $(BUILD_DIR)/$(PROJECT).bin"
	@echo "    wlink flash $(BUILD_DIR)/$(PROJECT).bin"
	@echo "============================================================"

# ---------- Size reporting ----------
size: $(BUILD_DIR)/$(PROJECT).elf
	@$(SIZE) --format=berkeley $(BUILD_DIR)/$(PROJECT).elf

# ---------- Cleanup ----------
clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned build artifacts"

distclean: clean
	rm -rf $(BUILD_DIR)/.* 2>/dev/null || true
	@echo "Full clean done"

# ---------- Native (host) unit tests ----------
# Tests run on the build host (not the target). They cover pure-logic modules
# (protocol codec, compositor, display grid math). Hardware-bound code (USB
# ISR, SPI driver) is stubbed out for tests via header-only hooks.

TEST_SRCS    := $(shell find $(TEST_DIR) -name '*.c' 2>/dev/null)
TEST_BINS    := $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/tests/%,$(TEST_SRCS))

# Common firmware sources needed by every test (protocol codec, display,
# driver stubs). Tests that need USB HAL (hid_send) include their own
# implementation rather than linking usb.c, to avoid duplicate symbols.
COMMON_FW_SRCS := \
    src/protocol/codec.c \
    src/display/display.c \
    src/display/compositor.c \
    src/display/driver.c \
    src/display/driver_ssd1306.c \
    src/display/font_5x7.c

# dispatch test additionally needs dispatch.c (which uses hid_send)
DISPATCH_FW_SRCS := \
    src/macro/vm.c \
    src/protocol/codec.c \
    src/protocol/dispatch.c \
    src/display/display.c \
    src/display/compositor.c \
    src/display/driver.c \
    src/display/driver_ssd1306.c \
    src/display/font_5x7.c \
    src/display/display_main.c \
    src/storage/config.c \
    src/input/hid_report.c

test: $(TEST_BINS)
	@echo ""
	@echo "============================================================"
	@echo "  Running native unit tests"
	@echo "============================================================"
	@fail=0; \
	for t in $(TEST_BINS); do \
	  echo ""; \
	  echo "--- $$t ---"; \
	  ./$$t || { echo "  [FAIL] $$t"; fail=1; }; \
	done; \
	if [ $$fail -eq 0 ]; then \
	  echo ""; \
	  echo "All tests PASSED"; \
	else \
	  echo ""; \
	  echo "Some tests FAILED"; \
	  exit 1; \
	fi

# Pattern rule: tests in flasher/ link with DISPATCH_FW_SRCS (includes
# dispatch.c). Must come BEFORE the generic rule below, or Make will pick
# the wrong pattern.
$(BUILD_DIR)/tests/flasher/%: $(TEST_DIR)/flasher/%.c $(DISPATCH_FW_SRCS)
	@mkdir -p $(dir $@)
	$(TEST_CC) $(TEST_CFLAGS) -o $@ $< $(DISPATCH_FW_SRCS) $(TEST_LDFLAGS)
	@echo "  TEST    $<"

# Default: any other test uses the common firmware sources.
$(BUILD_DIR)/tests/%: $(TEST_DIR)/%.c $(COMMON_FW_SRCS)
	@mkdir -p $(dir $@)
	$(TEST_CC) $(TEST_CFLAGS) -o $@ $< $(COMMON_FW_SRCS) $(TEST_LDFLAGS)
	@echo "  TEST    $<"

test-clean:
	rm -rf $(BUILD_DIR)/tests
	@echo "Cleaned test artifacts"

# ---------- Help ----------
help:
	@echo "Targets:"
	@echo "  all              Build firmware (elf + bin + hex)"
	@echo "  clean            Remove build artifacts"
	@echo "  flash            Show manual flash instructions (does NOT flash)"
	@echo "  size             Show ELF size sections"
	@echo "  test             Run native unit tests"
	@echo "  test-clean       Remove test artifacts"
	@echo ""
	@echo "Toolchain:"
	@$(CC) --version 2>/dev/null | head -1 || echo "  (riscv-none-elf-gcc not found)"

-include $(DEPS)
