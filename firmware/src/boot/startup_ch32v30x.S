/*
 * src/boot/startup_ch32v30x.S — Minimal CH32V307 startup.
 *
 *   - Boot from 0x08004000 (our application entry)
 *   - Set up stack pointer to top of RAM (__stack_top from linker)
 *   - Copy .data from flash to RAM
 *   - Zero .bss
 *   - Call SystemInit (clock tree)
 *   - Jump to main()
 *
 * No interrupts routed — that's handled by NVIC setup in main.c.
 */
    .section .vectors, "a", %progbits
    .align 4
    .globl   _vectors
_vectors:
    .word    __stack_top        /* 0x00  initial SP */
    .word    _start             /* 0x04  reset vector */
    .rept    62
    .word    0                  /* 0x08..0xFF: reserved / unused */
    .endr

    .section .text.startup, "ax", %progbits
    .globl   _start
    .type    _start, %function

_start:
    /* .data copy: src = _sidata, dst = _sdata, end = _edata */
    la       a0, _sdata
    la       a1, _edata
    la       a2, _sidata
    /* If no .data, skip (a0 == a1). */
1:  beq      a0, a1, 2f
    lw       a3, 0(a2)
    sw       a3, 0(a0)
    addi     a0, a0, 4
    addi     a2, a2, 4
    j        1b
2:
    /* .bss zero: from = _sbss, to = _ebss */
    la       a0, _sbss
    la       a1, _ebss
    bgeu     a1, a0, 3f
4:  sw       zero, 0(a0)
    addi     a0, a0, 4
    bltu     a0, a1, 4b
3:
    /* Set global pointer (gp) to start of .data for fast addressing. */
    la       gp, _sdata
    /* Jump to C runtime init, then main. */
    /* On the actual target we'd call SystemInit() here to configure the
     * PLL. Host tests link this code so it must not perform clock setup
     * before main() — main() does it. */
    call     main
5:  j        5b                /* hang if main returns */

    .size _start, .-_start
