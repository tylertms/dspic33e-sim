#include <stdint.h>

#include "architecture/dspic33/execution/internal.h"
#include "test.h"

typedef struct {
    uint64_t executed_count;
    uint64_t fingerprint;
} Census;

static uint32_t next_random_value(uint32_t* random_state) {
    *random_state = *random_state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *random_state;
}

static uint64_t mix(uint64_t fingerprint, uint32_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static void prepare_cpu(Dspic33* cpu, uint32_t* random_state, uint32_t scenario) {
    dspic33_reset(cpu, 0u);
    for (uint8_t register_index = 0u; register_index < 16u; register_index++) {
        cpu->w[register_index] = (uint16_t)next_random_value(random_state);
        cpu->initialized_working_registers |= (uint16_t)(1u << register_index);
    }
    cpu->w[15] = (uint16_t)(0x4000u + (next_random_value(random_state) & 0x1ffeu));
    cpu->sr = (uint16_t)next_random_value(random_state) & 0x7fffu;
    cpu->corcon = (uint16_t)next_random_value(random_state) & 0x33f3u;
    cpu->accumulator[0] = (int64_t)((uint64_t)next_random_value(random_state) << 16u |
                                    next_random_value(random_state));
    cpu->accumulator[1] = -(int64_t)((uint64_t)next_random_value(random_state) << 16u |
                                     next_random_value(random_state));
    cpu->rcount = (uint16_t)next_random_value(random_state);
    cpu->dcount = (uint16_t)next_random_value(random_state);
    cpu->tblpag = (uint16_t)next_random_value(random_state);
    cpu->dsrpag = (uint16_t)next_random_value(random_state);
    cpu->dswpag = (uint16_t)next_random_value(random_state);
    cpu->splim = (uint16_t)(0x4000u + (next_random_value(random_state) & 0x1ffeu));

    cpu->splim_enabled = (scenario & 1u) != 0u;
    cpu->repeat_active = (uint8_t)(scenario & 3u);
    cpu->do_depth = (uint8_t)(scenario & 3u);
    for (uint8_t loop_index = 0u; loop_index < 4u; loop_index++) {
        cpu->do_start[loop_index] = next_random_value(random_state) & 0x1fffeu;
        cpu->do_end[loop_index] = next_random_value(random_state) & 0x1fffeu;
        cpu->do_count[loop_index] = (uint16_t)next_random_value(random_state);
    }
    for (uint16_t data_address = 0x4000u; data_address < 0x4080u; data_address += 2u) {
        cpu->data[data_address] = (uint8_t)next_random_value(random_state);
        cpu->data[data_address + 1u] = (uint8_t)next_random_value(random_state);
    }
}

static Census census_states(Dspic33* cpu) {
    Census census = {0u, UINT64_C(14695981039346656037)};
    uint32_t random = UINT32_C(0x6d2b79f5);
    for (uint32_t scenario = 0u; scenario < 65536u; scenario++) {
        prepare_cpu(cpu, &random, scenario);
        const uint32_t opcode = next_random_value(&random) & UINT32_C(0xffffff);
        const bool instruction_executed = dspic33_internal_execute(cpu, opcode);
        census.executed_count += instruction_executed;
        census.fingerprint = mix(census.fingerprint, opcode);
        census.fingerprint = mix(census.fingerprint, instruction_executed);
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
               census.executed_count == 60593u &&
                   census.fingerprint == UINT64_C(13510892039604521079),
               "processor state census matches");
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
