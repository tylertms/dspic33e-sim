#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

enum {
    INTCON1 = 0x08c0u,
    INTCON2 = 0x08c2u,
    INTCON3 = 0x08c4u,
    INTCON4 = 0x08c6u,
    INTTREG = 0x08c8u,
    OPCODE_MOV_W0_INTCON2 = 0x884610u,
    OPCODE_MOV_W0_INTCON3 = 0x884620u,
    OPCODE_RETFIE = 0x064000u
};

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} InterruptControlConformance;

static void expect(InterruptControlConformance* state, bool condition,
                   const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[interrupt-control-failed] %s\n", name);
    }
}

static Dspic33PendingSoftTrap* pending_trap(Dspic33* cpu, uint16_t trap) {
    size_t index;
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active &&
            cpu->pending_soft_traps[index].trap == trap) {
            return &cpu->pending_soft_traps[index];
        }
    }
    return NULL;
}

static void access_cases(InterruptControlConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, INTCON1) == 0u, "INTCON1 POR state");
    expect(state, dspic33_read_word(cpu, INTCON2) == 0x8000u, "INTCON2 POR state");
    expect(state, dspic33_read_word(cpu, INTCON3) == 0u, "INTCON3 POR state");
    expect(state, dspic33_read_word(cpu, INTCON4) == 0u, "INTCON4 POR state");
    expect(state, dspic33_read_word(cpu, INTTREG) == 0u, "INTTREG POR state");

    dspic33_write_word(cpu, INTCON1, 0xffffu);
    expect(state, dspic33_read_word(cpu, INTCON1) == 0xfffeu,
           "INTCON1 implements every status bit except bit zero");
    expect(state, pending_trap(cpu, 4u) == NULL,
           "software MATHERR status does not create a trap source");
    dspic33_write_word(cpu, INTCON1, 0u);
    expect(state, dspic33_read_word(cpu, INTCON1) == 0u,
           "software clears writable INTCON1 status");

    dspic33_device_latch_math_error(cpu, 0x0040u);
    expect(state,
           dspic33_read_word(cpu, INTCON1) == 0x0050u && pending_trap(cpu, 4u) != NULL,
           "hardware math source latches status and schedules trap");
    dspic33_write_word(cpu, INTCON1, 0x0040u);
    expect(state,
           dspic33_read_word(cpu, INTCON1) == 0x0040u && pending_trap(cpu, 4u) == NULL,
           "software MATHERR clear cancels hardware trap source");

    cpu->disicnt = 0x1234u;
    dspic33_write_word(cpu, INTCON2, 0xffffu);
    expect(state, dspic33_read_word(cpu, INTCON2) == 0xe01fu,
           "INTCON2 preserves DISI and rejects reserved fields");
    dspic33_write_word(cpu, INTCON2, 0xc000u);
    expect(state, dspic33_read_word(cpu, INTCON2) == 0xc000u,
           "INTCON2 accepts GIE without software-changing DISI");
    dspic33_write_word(cpu, INTCON4, 0u);
    dspic33_write_word(cpu, INTCON3, 0xffffu);
    expect(state, dspic33_read_word(cpu, INTCON3) == 0x0070u,
           "INTCON3 rejects reserved fields");
    dspic33_write_word(cpu, INTCON3, 0u);
    expect(state, dspic33_read_word(cpu, INTCON3) == 0u,
           "INTCON3 software status clear is retained");
    dspic33_write_word(cpu, INTCON4, 0xffffu);
    expect(state, dspic33_read_word(cpu, INTCON4) == 0x0001u,
           "INTCON4 rejects reserved fields");
    expect(state, pending_trap(cpu, 2u) != NULL,
           "software SGHT creates a generic hard trap source");
    dspic33_write_word(cpu, INTCON4, 0u);
    expect(state, dspic33_read_word(cpu, INTCON4) == 0u,
           "INTCON4 software status clear is retained");
    expect(state, pending_trap(cpu, 2u) == NULL,
           "software SGHT clear cancels the hard trap source");

    dspic33_device_latch_interrupt(cpu, 6u, 9u);
    expect(state, dspic33_read_word(cpu, INTTREG) == 0x0906u,
           "INTTREG exposes dynamic vector and priority");
    dspic33_write_word(cpu, INTTREG, 0xffffu);
    expect(state, dspic33_read_word(cpu, INTTREG) == 0x0906u,
           "INTTREG word write is ignored");
    dspic33_write_byte(cpu, INTTREG, 0u);
    dspic33_write_byte(cpu, INTTREG + 1u, 0u);
    expect(state, dspic33_read_word(cpu, INTTREG) == 0x0906u,
           "INTTREG byte writes are ignored");
}

static void generic_hard_cases(InterruptControlConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W0_INTCON2);
    dspic33_load_program_word(cpu, 0x0008u, 0x000100u);
    dspic33_load_program_word(cpu, 0x0100u, OPCODE_RETFIE);
    dspic33_set_working_register(cpu, 0u, 0xa000u);
    dspic33_set_working_register(cpu, 15u, 0x2000u);
    cpu->sr = 0x0105u;
    cpu->stop_on_trap = true;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "SWTRAP instruction dispatches generic hard trap");
    expect(state,
           dspic33_read_word(cpu, INTCON2) == 0xa000u &&
               dspic33_read_word(cpu, INTCON4) == 0x0001u,
           "SWTRAP sets persistent hard-trap sources");
    expect(state,
           cpu->last_trap == 2u && cpu->last_trap_return == 2u && cpu->pc == 0x0100u &&
               cpu->trap_count == 1u,
           "generic hard trap records source and vector");
    expect(state,
           dspic33_read_word(cpu, INTTREG) == 0x0d02u &&
               (cpu->sr & 0x00e0u) == 0x00a0u && (cpu->corcon & 0x0008u) != 0u,
           "generic hard trap enters priority thirteen");
    expect(state,
           dspic33_read_word(cpu, 0x2000u) == 2u &&
               dspic33_read_word(cpu, 0x2002u) == 0x0500u && cpu->w[15] == 0x2004u,
           "generic hard trap stacks completed instruction state");

    dspic33_write_word(cpu, INTCON2, 0u);
    expect(state,
           dspic33_read_word(cpu, INTCON2) == 0u &&
               dspic33_read_word(cpu, INTCON4) == 1u && pending_trap(cpu, 2u) != NULL,
           "clearing SWTRAP leaves SGHT hard source active");
    cpu->stop_reason = DSPIC33_RUNNING;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->trap_count == 2u,
           "persistent SGHT reenters after RETFIE");

    dspic33_write_word(cpu, INTCON4, 0u);
    expect(state, pending_trap(cpu, 2u) == NULL,
           "clearing final hard source cancels reentry");
    cpu->stop_reason = DSPIC33_RUNNING;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
               cpu->interrupt_depth == 0u,
           "RETFIE returns after hard sources clear");
}

static void generic_soft_cases(InterruptControlConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W0_INTCON3);
    dspic33_load_program_word(cpu, 0x0010u, 0x000120u);
    dspic33_load_program_word(cpu, 0x0120u, OPCODE_RETFIE);
    dspic33_set_working_register(cpu, 0u, 0x0070u);
    dspic33_set_working_register(cpu, 15u, 0x3000u);
    cpu->sr = 0x0105u;
    cpu->stop_on_trap = true;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "INTCON3 instruction dispatches generic soft trap");
    expect(state, dspic33_read_word(cpu, INTCON3) == 0x0070u,
           "generic soft sources retain all requested status bits");
    expect(state,
           cpu->last_trap == 6u && cpu->last_trap_return == 2u && cpu->pc == 0x0120u &&
               cpu->trap_count == 1u,
           "generic soft trap coalesces sources at vector sixteen");
    expect(state,
           dspic33_read_word(cpu, INTTREG) == 0x0906u &&
               (cpu->sr & 0x00e0u) == 0x0020u && (cpu->corcon & 0x0008u) != 0u,
           "generic soft trap enters priority nine");
    expect(state,
           dspic33_read_word(cpu, 0x3000u) == 2u &&
               dspic33_read_word(cpu, 0x3002u) == 0x0500u && cpu->w[15] == 0x3004u,
           "generic soft trap stacks completed instruction state");

    dspic33_write_word(cpu, INTCON3, 0u);
    expect(state, pending_trap(cpu, 6u) == NULL,
           "clearing generic soft sources cancels reentry");
    cpu->stop_reason = DSPIC33_RUNNING;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
               cpu->interrupt_depth == 0u,
           "RETFIE returns after soft sources clear");
}

static void lifecycle_cases(InterruptControlConformance* state, Dspic33* source,
                            Dspic33* copy) {
    dspic33_reset(source, 0u);
    dspic33_reset(copy, 0u);
    dspic33_write_word(source, INTCON1, 0x7800u);
    dspic33_device_latch_interrupt(source, 2u, 13u);
    expect(state, dspic33_copy(copy, source), "copy preserves interrupt controls");
    expect(state,
           dspic33_read_word(copy, INTCON1) == 0x7800u &&
               dspic33_read_word(copy, INTTREG) == 0x0d02u,
           "copy preserves interrupt status and dynamic latch");
    dspic33_write_word(source, INTCON1, 0u);
    expect(state, dspic33_read_word(copy, INTCON1) == 0x7800u,
           "copied interrupt status is independent");
    dspic33_reset(source, 0u);
    expect(state,
           dspic33_read_word(source, INTCON1) == 0u &&
               dspic33_read_word(source, INTCON2) == 0x8000u &&
               dspic33_read_word(source, INTCON3) == 0u &&
               dspic33_read_word(source, INTCON4) == 0u &&
               dspic33_read_word(source, INTTREG) == 0u,
           "POR restores interrupt-control state");
}

int main(void) {
    Dspic33 source;
    Dspic33 copy;
    InterruptControlConformance state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    access_cases(&state, &source);
    generic_hard_cases(&state, &source);
    generic_soft_cases(&state, &source);
    lifecycle_cases(&state, &source, &copy);
    expect(&state, state.cases == 41u, "interrupt-control assertion accounting");
    printf("[interrupt-control-summary] cases=%" PRIu32 " passed=%" PRIu32
           " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&copy);
    dspic33_destroy(&source);
    return state.failed == 0u ? 0 : 1;
}
