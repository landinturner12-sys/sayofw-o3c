/*
 * macro/vm.h — Macro bytecode VM for SayoBot O3C.
 *
 * Executes macro slots containing up to MACRO_STEPS_PER_SLOT instructions.
 * Opcodes mirror the stock firmware RE (khang06 gist) with extensions.
 *
 * Instruction format (4 bytes each):
 *   [0] opcode   [1] arg0   [2..3] arg1 (u16 LE)
 *
 * The VM is non-preemptive: macro_vm_tick() runs one instruction per call,
 * returning true while the macro is still executing. The main loop calls
 * it once per scan cycle (~1 ms).
 */
#ifndef SAYOFW_MACRO_VM_H
#define SAYOFW_MACRO_VM_H

#include <stdint.h>
#include <stdbool.h>
#include "sayofw_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Opcodes ===== */
#define OP_NOP          0x00U  /* No operation                              */
#define OP_KEY_DOWN     0x01U  /* arg0=modifier, arg1=HID keycode           */
#define OP_KEY_UP       0x02U  /* arg0=modifier, arg1=HID keycode           */
#define OP_KEY_TAP      0x03U  /* arg0=modifier, arg1=HID keycode (down+up) */
#define OP_DELAY        0x04U  /* arg1=milliseconds to wait                 */
#define OP_CONSUMER     0x05U  /* arg1=HID consumer usage (media keys)      */
#define OP_MOUSE_MOVE   0x06U  /* arg0=dx (i8), arg1_lo=dy (i8)            */
#define OP_MOUSE_BTN    0x07U  /* arg0=button mask, arg1=0 release / 1 press*/
#define OP_DISPLAY_TEXT 0x08U  /* arg0=layer, arg1=text_offset in slot data */
#define OP_LOOP_START   0x09U  /* arg1=iteration count (0=infinite)         */
#define OP_LOOP_END     0x0AU  /* Jump back to matching LOOP_START          */
#define OP_HALT         0xFFU  /* Stop execution                            */

/* Single instruction (packed 4 bytes). */
typedef struct {
    uint8_t  opcode;
    uint8_t  arg0;
    uint16_t arg1;       /* little-endian on wire */
} __attribute__((packed)) macro_insn_t;

/* A macro slot: fixed-size array of instructions. */
typedef struct {
    macro_insn_t steps[MACRO_STEPS_PER_SLOT];
    uint8_t      step_count;   /* valid instructions (0 = empty slot) */
    uint8_t      _pad[3];
} macro_slot_t;

/* VM execution state. */
typedef enum {
    VM_IDLE = 0,
    VM_RUNNING,
    VM_WAITING,      /* inside OP_DELAY */
} vm_state_t;

#define VM_LOOP_STACK_DEPTH 4U

typedef struct {
    macro_slot_t  slots[MACRO_SLOTS];

    /* Runtime state */
    vm_state_t    state;
    uint16_t      active_slot;     /* which slot is executing */
    uint8_t       pc;              /* program counter (step index) */

    /* Delay timer */
    uint32_t      delay_until_ms;  /* sys_tick_ms target for OP_DELAY */

    /* Loop stack */
    struct {
        uint8_t  pc_start;         /* PC of the OP_LOOP_START */
        uint16_t remaining;        /* iterations left; 0 = infinite */
    } loop_stack[VM_LOOP_STACK_DEPTH];
    uint8_t       loop_sp;         /* stack pointer (0 = empty) */
} macro_vm_t;

/* ===== API ===== */

/* Initialize VM: clear all slots, set state to IDLE. */
void macro_vm_init(macro_vm_t *vm);

/* Define (upload) a macro slot. Returns false if slot_idx out of range or
 * step_count exceeds MACRO_STEPS_PER_SLOT. */
bool macro_vm_define(macro_vm_t *vm, uint16_t slot_idx,
                     const macro_insn_t *steps, uint8_t step_count);

/* Start executing a macro slot. Returns false if slot empty or already running. */
bool macro_vm_run(macro_vm_t *vm, uint16_t slot_idx);

/* Stop the currently running macro immediately. */
void macro_vm_stop(macro_vm_t *vm);

/* Tick the VM: execute one instruction (or continue waiting on DELAY).
 * Returns true if the VM is still active, false if IDLE.
 * Call from main loop at ~1 kHz. */
bool macro_vm_tick(macro_vm_t *vm, uint32_t now_ms);

/* Query state. */
vm_state_t macro_vm_get_state(const macro_vm_t *vm);

/* ===== Callbacks (weak, target provides real implementations) ===== */

/* Called by VM to inject a HID keyboard report. */
extern void macro_emit_key(uint8_t modifier, uint16_t keycode, bool press);

/* Called by VM to inject a HID consumer report. */
extern void macro_emit_consumer(uint16_t usage);

/* Called by VM to inject a mouse movement/button report. */
extern void macro_emit_mouse(int8_t dx, int8_t dy, uint8_t buttons);

#ifdef __cplusplus
}
#endif

#endif /* SAYOFW_MACRO_VM_H */
