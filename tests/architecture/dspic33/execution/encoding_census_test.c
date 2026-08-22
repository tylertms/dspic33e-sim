#include <stdint.h>

#include "architecture/dspic33/execution/internal.h"
#include "test.h"

typedef struct {
    uint64_t examined;
    uint64_t executed;
    uint64_t fingerprint;
} Census;

static uint64_t mix(uint64_t fingerprint, uint32_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static void prepare_cpu(Dspic33* cpu, uint8_t state) {
    static const uint16_t values[][4] = {
        {0u, 1u, 0x5000u, 0x7fffu},
        {UINT16_MAX, 0x8000u, 0x5100u, 0x5555u},
        {0xaaaau, 0x1234u, 0x5200u, 0x8001u},
        {0x00ffu, 0xff00u, 0x5300u, 0x3333u},
    };
    dspic33_reset(cpu, 0u);
    for (uint8_t index = 0u; index < 16u; index++) {
        cpu->w[index] = values[state][index & 3u];
    }
    cpu->w[15] = 0x6000u;
    cpu->sr = state == 1u ? 0x01ffu : 0u;
    cpu->corcon = state == 2u ? 0x00f3u : 0u;
    cpu->accumulator[0] = state == 3u ? INT64_C(0x7fffffffff) : 0;
    cpu->accumulator[1] = state == 3u ? -INT64_C(0x8000000000) : 0;
}

static Census census_encodings(Dspic33* cpu) {
    static const uint8_t low_bytes[] = {0x00u, 0x3cu, 0xa5u, 0xffu};
    Census census = {0u, 0u, UINT64_C(14695981039346656037)};
    for (uint8_t state = 0u; state < 4u; state++) {
        for (uint32_t high = 0u; high <= UINT16_MAX; high++) {
            const uint32_t opcode = (high << 8u) | low_bytes[state];
            prepare_cpu(cpu, state);
            const bool executed = dspic33_internal_execute(cpu, opcode);
            census.examined++;
            census.executed += executed;
            census.fingerprint = mix(census.fingerprint, opcode);
            census.fingerprint = mix(census.fingerprint, executed);
            census.fingerprint = mix(census.fingerprint, cpu->w[0]);
            census.fingerprint = mix(census.fingerprint, cpu->w[15]);
            census.fingerprint = mix(census.fingerprint, cpu->sr);
            census.fingerprint = mix(census.fingerprint, (uint32_t)cpu->accumulator[0]);
        }
    }
    return census;
}

int main(void) {
    TestState state = {0};
    Dspic33 cpu;
    const bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "cpu initialized");
    if (initialized) {
        const Census census = census_encodings(&cpu);
        expect(&state,
               census.examined == 262144u && census.executed == 243589u &&
                   census.fingerprint == UINT64_C(10547362093158882567),
               "instruction encoding census matches");
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
