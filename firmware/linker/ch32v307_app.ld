/*
 * ch32v307_app.ld — Linker script for Sayobot O3C custom firmware
 *
 * Memory layout (matches stock firmware regions, see khang06 gist):
 *   0x00000000 - 0x00003FFF : Bootloader (16 KB) — left intact, owned by vendor
 *   0x08000000 - 0x08003FFF : Application slot 1 (16 KB)  ← we load here (0x00000 in bus addr; flash aliased)
 *   0x08004000 - 0x08019FFF : Application slot 2 (88 KB) — extended code+data
 *   0x0801A000 - 0x08029F7F : Free (54 KB scratch)
 *   0x08029F80 - 0x08029F83 : FW metadata (bootloader-managed)
 *   0x08029F84 - 0x08029F87 : FW size (uint32)
 *   0x08029FA0 - 0x08029FAF : FW MD5 hash (16 bytes)
 *
 * On CH32V307 the flash alias is 0x00000000 (BOOT0=0). We link against the
 * 0x08000000-base aliased address space. The startup code copies .data and
 * zeros .bss before entering main().
 */

OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08004000, LENGTH = 144K   /* app slot, 256K - 16K bootloader */
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

/* Top of stack — highest RAM address; grows down */
__stack_top = ORIGIN(RAM) + LENGTH(RAM);
_estack     = __stack_top;

/* Sections */
SECTIONS
{
    .text : ALIGN(4)
    {
        KEEP(*(.vectors))           /* Interrupt vector table */
        KEEP(*(.text.startup))      /* Startup code must come first */
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } >FLASH

    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } >FLASH

    .data : ALIGN(4)
    {
        _sdata = .;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT >FLASH
    _sidata = LOADADDR(.data);

    .bss (NOLOAD) : ALIGN(4)
    {
        _sbss = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM

    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
        *(.eh_frame)
    }
}
