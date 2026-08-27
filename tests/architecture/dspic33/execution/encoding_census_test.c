#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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

static Census census_encodings(Dspic33* cpu, uint8_t shard) {
    Census census = {0u, 0u, UINT64_C(14695981039346656037)};
    const uint32_t first = (uint32_t)shard << 20u;
    const uint32_t last = first | 0x000fffffu;
    for (uint32_t opcode = first; opcode <= last; opcode++) {
        const uint8_t state = (uint8_t)((opcode ^ (opcode >> 8u) ^ (opcode >> 16u)) & 3u);
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
    return census;
}

int main(int argc, char** argv) {
    char* end = NULL;
    const unsigned long parsed = argc == 2 ? strtoul(argv[1], &end, 10) : 0u;
    TestState state = {0};
    const bool valid_shard =
        argc == 1 || (argc == 2 && end != argv[1] && *end == '\0' && parsed < 16u);
    expect(&state, valid_shard, "encoding census shard is valid");
    if (!valid_shard) {
        return test_finish(&state);
    }
    const uint8_t shard = (uint8_t)parsed;
    static const uint64_t expected_executed[16] = {
        753678u,  1048576u, 1048576u, 1048576u, 1048576u, 1048576u, 1048576u, 1048576u,
        1048576u, 1048576u, 1048576u, 966656u,  809990u,  851968u,  917504u,  761856u,
    };
    static const uint64_t expected_fingerprints[16] = {
        UINT64_C(13435002434993978529), UINT64_C(764949113210372105),
        UINT64_C(17321065357767263013), UINT64_C(4091026983387374373),
        UINT64_C(3221232769131753183),  UINT64_C(2041206799426521133),
        UINT64_C(12291876438187873379), UINT64_C(4888425759874588143),
        UINT64_C(12513212965201142334), UINT64_C(17553851555242393381),
        UINT64_C(8459395887854694913),  UINT64_C(4748227562539279623),
        UINT64_C(12991544788716398853), UINT64_C(4064714396898001113),
        UINT64_C(14142881104235815568), UINT64_C(3251569668013103597),
    };
    Dspic33 cpu;
    const bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "cpu initialized");
    if (initialized) {
        const Census census = census_encodings(&cpu, shard);
        const bool matches = census.examined == 1048576u &&
                             census.executed == expected_executed[shard] &&
                             census.fingerprint == expected_fingerprints[shard];
        if (!matches) {
            printf("[census] shard=%u examined=%llu executed=%llu fingerprint=%llu\n", shard,
                   (unsigned long long)census.examined, (unsigned long long)census.executed,
                   (unsigned long long)census.fingerprint);
        }
        expect(&state, matches, "instruction encoding census matches");
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
