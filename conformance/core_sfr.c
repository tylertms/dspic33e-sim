#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dspic33.h"

enum {
    ACCAU = 0x0026u,
    ACCBU = 0x002cu,
    PCL = 0x002eu,
    PCH = 0x0030u,
    STATUS = 0x0042u,
    CORE_CONTROL = 0x0044u,
    DISI_COUNT = 0x0052u,
    INTCON1 = 0x08c0u,
    INTCON2 = 0x08c2u,
    OPCODE_MOV_PCL_W0 = 0xbf802eu,
    OPCODE_MOV_PCH_W0 = 0xbf8030u,
    OPCODE_MOV_W0_PCL = 0x880170u,
    OPCODE_MOV_W0_STATUS = 0x880210u,
    OPCODE_MOV_W0_DISICNT = 0x880290u,
    OPCODE_DISI_6 = 0xfc0006u
};

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} CoreSfrConformance;

static void expect(CoreSfrConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[core-sfr-failed] %s\n", name);
    }
}

static void accumulator_cases(CoreSfrConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_read_word(cpu, ACCAU) == 0u && dspic33_read_word(cpu, ACCBU) == 0u,
           "accumulator upper registers reset to zero");
    cpu->accumulator[0] = 0x7f12345678ll;
    expect(state, dspic33_read_word(cpu, ACCAU) == 0x007fu,
           "positive accumulator upper byte zero extends");
    dspic33_write_byte(cpu, ACCAU + 1u, 0xffu);
    expect(state, dspic33_read_word(cpu, ACCAU) == 0x007fu,
           "accumulator sign-extension byte ignores writes");
    dspic33_write_word(cpu, ACCAU, 0x12abu);
    expect(state, dspic33_read_word(cpu, ACCAU) == 0xffabu,
           "writable accumulator upper byte controls sign extension");
    expect(state, cpu->accumulator[0] == -0x54edcba988ll,
           "accumulator upper write preserves lower thirty-two bits");
    cpu->accumulator[1] = -1;
    expect(state, dspic33_read_word(cpu, ACCBU) == 0xffffu,
           "negative accumulator upper byte sign extends");
    dspic33_write_byte(cpu, ACCBU + 1u, 0u);
    expect(state, dspic33_read_word(cpu, ACCBU) == 0xffffu,
           "negative sign-extension byte remains read-only");
}

static void program_counter_cases(CoreSfrConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->pc = 0x123456u;
    expect(state,
           dspic33_read_word(cpu, PCL) == 0x3456u &&
               dspic33_read_word(cpu, PCH) == 0x0012u,
           "PCL and PCH expose live program counter");
    expect(state,
           dspic33_read_byte(cpu, PCL) == 0x56u &&
               dspic33_read_byte(cpu, PCL + 1u) == 0x34u &&
               dspic33_read_byte(cpu, PCH) == 0x12u &&
               dspic33_read_byte(cpu, PCH + 1u) == 0u,
           "program counter byte views expose implemented lanes");
    dspic33_write_word(cpu, PCL, 0xa55au);
    dspic33_write_word(cpu, PCH, 0xffffu);
    expect(state, cpu->pc == 0x123456u, "program counter SFR word writes are ignored");
    dspic33_write_byte(cpu, PCL, 0u);
    dspic33_write_byte(cpu, PCH, 0u);
    expect(state, cpu->pc == 0x123456u, "program counter SFR byte writes are ignored");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_PCL_W0);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 2u && cpu->pc == 2u,
           "instruction reads following PC through PCL");
    dspic33_reset(cpu, 0x012340u);
    dspic33_load_program_word(cpu, 0x012340u, OPCODE_MOV_PCH_W0);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 1u &&
               cpu->pc == 0x012342u,
           "instruction reads live high PC through PCH");
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W0_PCL);
    dspic33_set_working_register(cpu, 0u, 0xa55au);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u,
           "instruction write to PCL is ignored");
}

static void status_cases(CoreSfrConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, STATUS, 0xffffu);
    expect(state, cpu->sr == 0xfdefu,
           "SR data write sets writable and accumulator status fields");
    dspic33_write_word(cpu, STATUS, 0u);
    expect(state, cpu->sr == 0u, "SR data write clears writable status fields");
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W0_STATUS);
    dspic33_set_working_register(cpu, 0u, 0xffffu);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->sr == 0xfdefu,
           "instruction data write applies SR status semantics");

    cpu->sr = 0xfc00u;
    dspic33_write_byte(cpu, STATUS + 1u, 0xf4u);
    expect(state, cpu->sr == 0x3400u,
           "clearing OAB clears both accumulator overflow flags");
    cpu->sr = 0xfc00u;
    dspic33_write_byte(cpu, STATUS + 1u, 0xf8u);
    expect(state, cpu->sr == 0xc800u,
           "clearing SAB clears both accumulator saturation flags");
    cpu->sr = 0u;
    dspic33_write_byte(cpu, STATUS + 1u, 0x24u);
    expect(state, cpu->sr == 0x2400u,
           "software sets individual and combined saturation status");
    cpu->sr = 0u;
    dspic33_write_byte(cpu, STATUS + 1u, 0x88u);
    expect(state, cpu->sr == 0x8800u,
           "software sets individual and combined overflow status");

    cpu->sr = 0x0210u;
    dspic33_write_word(cpu, STATUS, 0u);
    expect(state, cpu->sr == 0x0210u, "data write preserves DA and RA status");
    dspic33_write_word(cpu, INTCON1, 0x8000u);
    cpu->sr = 0x00a5u;
    dspic33_write_byte(cpu, STATUS, 0u);
    expect(state, cpu->sr == 0x00a0u, "NSTDIS makes SR IPL fields read-only");
    dspic33_write_word(cpu, INTCON1, 0u);
    dspic33_write_byte(cpu, STATUS, 0x6fu);
    expect(state, cpu->sr == 0x006fu, "SR IPL fields are writable when NSTDIS clears");
}

static void core_control_cases(CoreSfrConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->corcon |= 0x0004u;
    dspic33_write_word(cpu, CORE_CONTROL, 0u);
    expect(state, dspic33_read_word(cpu, CORE_CONTROL) == 0x0004u,
           "CORCON SFA rejects word clear");
    dspic33_write_byte(cpu, CORE_CONTROL, 0xffu);
    expect(state, dspic33_read_word(cpu, CORE_CONTROL) == 0x00f7u,
           "CORCON low-byte write preserves SFA");
    dspic33_write_byte(cpu, CORE_CONTROL, 0u);
    expect(state, dspic33_read_word(cpu, CORE_CONTROL) == 0x0004u,
           "CORCON low-byte clear preserves SFA");
    dspic33_write_byte(cpu, CORE_CONTROL + 1u, 0xffu);
    expect(state, dspic33_read_word(cpu, CORE_CONTROL) == 0xb004u,
           "CORCON high-byte write applies target mask");
}

static void disicnt_cases(CoreSfrConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DISI_COUNT, 0x1234u);
    expect(state, cpu->disicnt == 0u,
           "DISICNT write cannot initiate an interrupt-disable interval");
    dspic33_write_byte(cpu, DISI_COUNT, 0x55u);
    dspic33_write_byte(cpu, DISI_COUNT + 1u, 0x2au);
    expect(state, cpu->disicnt == 0u,
           "DISICNT byte writes cannot initiate an interval");

    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W0_DISICNT);
    dspic33_set_working_register(cpu, 0u, 0x1234u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->disicnt == 0u,
           "instruction data write cannot initiate a DISICNT interval");
    dspic33_load_program_word(cpu, 2u, OPCODE_DISI_6);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->disicnt == 6u &&
               (dspic33_read_word(cpu, INTCON2) & 0x4000u) != 0u,
           "DISI instruction starts the disable interval");
    dspic33_write_word(cpu, DISI_COUNT, 0x1234u);
    expect(state, cpu->disicnt == 0x1234u,
           "active DISICNT interval accepts word extension");
    dspic33_write_byte(cpu, DISI_COUNT, 0x78u);
    expect(state, cpu->disicnt == 0x1278u,
           "active DISICNT interval accepts low-byte update");
    dspic33_write_byte(cpu, DISI_COUNT + 1u, 0xffu);
    expect(state, cpu->disicnt == 0x3f78u,
           "DISICNT high-byte update masks reserved bits");
    dspic33_write_word(cpu, DISI_COUNT, 0xffffu);
    expect(state, dspic33_read_word(cpu, DISI_COUNT) == 0x3fffu,
           "DISICNT word update masks reserved bits");
    dspic33_write_word(cpu, DISI_COUNT, 0u);
    expect(state,
           cpu->disicnt == 0u && (dspic33_read_word(cpu, INTCON2) & 0x4000u) == 0u,
           "DISICNT clear terminates the disable interval");
    dspic33_write_word(cpu, DISI_COUNT, 1u);
    expect(state, cpu->disicnt == 0u,
           "cleared DISICNT interval cannot restart by data write");
}

static void lifecycle_cases(CoreSfrConformance* state, Dspic33* source, Dspic33* copy) {
    dspic33_reset(source, 0x012340u);
    dspic33_reset(copy, 0u);
    source->accumulator[0] = -1;
    source->sr = 0x2405u;
    source->corcon |= 0x0004u;
    dspic33_load_program_word(source, 0x012340u, OPCODE_DISI_6);
    dspic33_step(source);
    expect(state, dspic33_copy(copy, source), "copy preserves core SFR state");
    expect(state,
           dspic33_read_word(copy, PCL) == 0x2342u &&
               dspic33_read_word(copy, PCH) == 1u &&
               dspic33_read_word(copy, ACCAU) == 0xffffu && copy->sr == 0x2405u &&
               copy->corcon == source->corcon && copy->disicnt == source->disicnt,
           "copied core SFR views match source state");
    dspic33_reset(source, 0u);
    expect(state,
           dspic33_read_word(source, PCL) == 0u &&
               dspic33_read_word(source, PCH) == 0u && source->sr == 0u &&
               source->disicnt == 0u,
           "POR restores core SFR state");
}

int main(void) {
    Dspic33 source;
    Dspic33 copy;
    CoreSfrConformance state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    accumulator_cases(&state, &source);
    program_counter_cases(&state, &source);
    status_cases(&state, &source);
    core_control_cases(&state, &source);
    disicnt_cases(&state, &source);
    lifecycle_cases(&state, &source, &copy);
    expect(&state, state.cases == 41u, "core SFR assertion accounting");
    printf("[core-sfr-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32
           "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&copy);
    dspic33_destroy(&source);
    return state.failed == 0u ? 0 : 1;
}
