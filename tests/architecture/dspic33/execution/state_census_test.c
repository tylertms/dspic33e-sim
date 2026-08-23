#include <stdint.h>

#include "architecture/dspic33/execution/internal.h"
#include "test.h"

typedef struct {
    uint64_t executed;
    uint64_t fingerprint;
} Census;

static uint32_t next_value(uint32_t* state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static uint64_t mix(uint64_t fingerprint, uint32_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static void prepare_cpu(Dspic33* cpu, uint32_t* random, uint32_t scenario) {
    dspic33_reset(cpu, 0u);
    for (uint8_t index = 0u; index < 16u; index++) {
        cpu->w[index] = (uint16_t)next_value(random);
        cpu->initialized_working_registers |= (uint16_t)(1u << index);
    }
    cpu->w[15] = (uint16_t)(0x4000u + (next_value(random) & 0x1ffeu));
    cpu->sr = (uint16_t)next_value(random) & 0x7fffu;
    cpu->corcon = (uint16_t)next_value(random) & 0x33f3u;
    cpu->accumulator[0] = (int64_t)((uint64_t)next_value(random) << 16u | next_value(random));
    cpu->accumulator[1] = -(int64_t)((uint64_t)next_value(random) << 16u | next_value(random));
    cpu->rcount = (uint16_t)next_value(random);
    cpu->dcount = (uint16_t)next_value(random);
    cpu->tblpag = (uint16_t)next_value(random);
    cpu->dsrpag = (uint16_t)next_value(random);
    cpu->dswpag = (uint16_t)next_value(random);
    cpu->splim = (uint16_t)(0x4000u + (next_value(random) & 0x1ffeu));
    cpu->splim_enabled = (scenario & 1u) != 0u;
    cpu->repeat_active = (uint8_t)(scenario & 3u);
    cpu->do_depth = (uint8_t)(scenario & 3u);
    for (uint8_t index = 0u; index < 4u; index++) {
        cpu->do_start[index] = next_value(random) & 0x1fffeu;
        cpu->do_end[index] = next_value(random) & 0x1fffeu;
        cpu->do_count[index] = (uint16_t)next_value(random);
    }
    for (uint16_t address = 0x4000u; address < 0x4080u; address += 2u) {
        cpu->data[address] = (uint8_t)next_value(random);
        cpu->data[address + 1u] = (uint8_t)next_value(random);
    }
}

static Census census_states(Dspic33* cpu) {
    Census census = {0u, UINT64_C(14695981039346656037)};
    uint32_t random = UINT32_C(0x6d2b79f5);
    for (uint32_t scenario = 0u; scenario < 65536u; scenario++) {
        prepare_cpu(cpu, &random, scenario);
        const uint32_t opcode = next_value(&random) & UINT32_C(0xffffff);
        const bool executed = dspic33_internal_execute(cpu, opcode);
        census.executed += executed;
        census.fingerprint = mix(census.fingerprint, opcode);
        census.fingerprint = mix(census.fingerprint, executed);
        census.fingerprint = mix(census.fingerprint, cpu->w[0]);
        census.fingerprint = mix(census.fingerprint, cpu->w[15]);
        census.fingerprint = mix(census.fingerprint, cpu->sr);
        census.fingerprint = mix(census.fingerprint, (uint32_t)cpu->accumulator[0]);
        census.fingerprint = mix(census.fingerprint, cpu->illegal_reset);
        census.fingerprint = mix(census.fingerprint, cpu->address_error);
    }
    return census;
}

int main(void) {
    TestState state = {0};
    Dspic33 cpu;
    const bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "cpu initialized");
    if (initialized) {
        const Census census = census_states(&cpu);
        expect(&state,
               census.executed == 60593u && census.fingerprint == UINT64_C(9164222310185084880),
               "processor state census matches");
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
