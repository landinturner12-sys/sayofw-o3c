/*
 * src/macro/vm.c — Macro bytecode VM interpreter.
 *
 * Non-preemptive: macro_vm_tick() executes one instruction per call.
 * The main loop calls it at ~1 kHz (once per scan cycle).
 */
#include "macro/vm.h"
#include <string.h>

/* Weak stubs for HID emission — target overrides with real USB reports. */
__attribute__((weak)) void macro_emit_key(uint8_t mod, uint16_t kc, bool press)
    { (void)mod; (void)kc; (void)press; }
__attribute__((weak)) void macro_emit_consumer(uint16_t usage)
    { (void)usage; }
__attribute__((weak)) void macro_emit_mouse(int8_t dx, int8_t dy, uint8_t btns)
    { (void)dx; (void)dy; (void)btns; }

void macro_vm_init(macro_vm_t *vm)
{
    if (!vm) return;
    memset(vm, 0, sizeof(*vm));
    vm->state = VM_IDLE;
}

bool macro_vm_define(macro_vm_t *vm, uint16_t slot_idx,
                     const macro_insn_t *steps, uint8_t step_count)
{
    if (!vm || slot_idx >= MACRO_SLOTS || !steps) return false;
    if (step_count > MACRO_STEPS_PER_SLOT) return false;
    memcpy(vm->slots[slot_idx].steps, steps,
           (size_t)step_count * sizeof(macro_insn_t));
    vm->slots[slot_idx].step_count = step_count;
    /* Zero remaining steps */
    for (uint8_t i = step_count; i < MACRO_STEPS_PER_SLOT; i++) {
        memset(&vm->slots[slot_idx].steps[i], 0, sizeof(macro_insn_t));
    }
    return true;
}

bool macro_vm_run(macro_vm_t *vm, uint16_t slot_idx)
{
    if (!vm || slot_idx >= MACRO_SLOTS) return false;
    if (vm->slots[slot_idx].step_count == 0) return false;
    if (vm->state == VM_RUNNING || vm->state == VM_WAITING) return false;
    vm->active_slot = slot_idx;
    vm->pc = 0;
    vm->loop_sp = 0;
    vm->state = VM_RUNNING;
    return true;
}

void macro_vm_stop(macro_vm_t *vm)
{
    if (!vm) return;
    vm->state = VM_IDLE;
    vm->pc = 0;
    vm->loop_sp = 0;
}

bool macro_vm_tick(macro_vm_t *vm, uint32_t now_ms)
{
    if (!vm || vm->state == VM_IDLE) return false;

    /* Handle delay wait */
    if (vm->state == VM_WAITING) {
        if (now_ms < vm->delay_until_ms) return true; /* still waiting */
        vm->state = VM_RUNNING;
        vm->pc++;
    }

    macro_slot_t *slot = &vm->slots[vm->active_slot];
    if (vm->pc >= slot->step_count) {
        vm->state = VM_IDLE;
        return false;
    }

    const macro_insn_t *insn = &slot->steps[vm->pc];
    switch (insn->opcode) {
    case OP_NOP:
        vm->pc++;
        break;
    case OP_KEY_DOWN:
        macro_emit_key(insn->arg0, insn->arg1, true);
        vm->pc++;
        break;
    case OP_KEY_UP:
        macro_emit_key(insn->arg0, insn->arg1, false);
        vm->pc++;
        break;
    case OP_KEY_TAP:
        macro_emit_key(insn->arg0, insn->arg1, true);
        macro_emit_key(insn->arg0, insn->arg1, false);
        vm->pc++;
        break;
    case OP_DELAY:
        vm->delay_until_ms = now_ms + (uint32_t)insn->arg1;
        vm->state = VM_WAITING;
        /* pc incremented when delay expires */
        break;
    case OP_CONSUMER:
        macro_emit_consumer(insn->arg1);
        vm->pc++;
        break;
    case OP_MOUSE_MOVE:
        macro_emit_mouse((int8_t)insn->arg0, (int8_t)(insn->arg1 & 0xFF), 0);
        vm->pc++;
        break;
    case OP_MOUSE_BTN:
        macro_emit_mouse(0, 0, insn->arg0);
        vm->pc++;
        break;
    case OP_LOOP_START:
        if (vm->loop_sp < VM_LOOP_STACK_DEPTH) {
            vm->loop_stack[vm->loop_sp].pc_start = vm->pc;
            vm->loop_stack[vm->loop_sp].remaining = insn->arg1;
            vm->loop_sp++;
        }
        vm->pc++;
        break;
    case OP_LOOP_END:
        if (vm->loop_sp > 0) {
            uint8_t si = (uint8_t)(vm->loop_sp - 1U);
            if (vm->loop_stack[si].remaining == 0) {
                /* Infinite loop */
                vm->pc = (uint8_t)(vm->loop_stack[si].pc_start + 1U);
            } else {
                vm->loop_stack[si].remaining--;
                if (vm->loop_stack[si].remaining > 0) {
                    vm->pc = (uint8_t)(vm->loop_stack[si].pc_start + 1U);
                } else {
                    vm->loop_sp--;
                    vm->pc++;
                }
            }
        } else {
            vm->pc++; /* unmatched LOOP_END = skip */
        }
        break;
    case OP_HALT:
    default:
        vm->state = VM_IDLE;
        return false;
    }

    /* Check if we've run past the end */
    if (vm->state == VM_RUNNING && vm->pc >= slot->step_count) {
        vm->state = VM_IDLE;
        return false;
    }
    return (vm->state != VM_IDLE);
}

vm_state_t macro_vm_get_state(const macro_vm_t *vm)
{
    return vm ? vm->state : VM_IDLE;
}
