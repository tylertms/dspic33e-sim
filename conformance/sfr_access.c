#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dspic33.h"
#include "dspic33ep512mu810_sfr_access.h"

typedef struct {
    uint32_t unresolved_addresses;
    uint32_t normal_addresses;
    uint32_t normal_bits;
    uint32_t read_only_addresses;
    uint32_t read_only_bits;
    uint32_t reserved_addresses;
    uint32_t reserved_bits;
    uint32_t write_only_addresses;
    uint32_t write_only_bits;
    uint32_t aliases;
    uint32_t mux_defaults;
} SfrAccessCensus;

static uint32_t bit_count(uint16_t value) {
    uint32_t count = 0u;
    while (value != 0u) {
        count += value & 1u;
        value >>= 1u;
    }
    return count;
}

static void inspect_register(SfrAccessCensus* census, Dspic33* cpu,
                             const Dspic33SfrAccessExpectation* expectation) {
    uint16_t initial;
    uint16_t ones;
    uint16_t zeroes;
    uint16_t normal;
    uint16_t read_only;
    uint16_t reserved;
    uint16_t write_only;
    bool unresolved;

    dspic33_reset(cpu, 0u);
    initial = dspic33_read_word(cpu, expectation->address);
    dspic33_write_word(cpu, expectation->address, 0xffffu);
    ones = dspic33_read_word(cpu, expectation->address);
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, expectation->address, 0u);
    zeroes = dspic33_read_word(cpu, expectation->address);
    normal = (uint16_t)(((uint16_t)~ones | zeroes) & expectation->normal);
    read_only =
        (uint16_t)(((ones ^ initial) | (zeroes ^ initial)) & expectation->read_only);
    reserved = (uint16_t)((initial | ones | zeroes) & expectation->reserved);
    write_only = (uint16_t)((initial | ones | zeroes) & expectation->write_only);
    unresolved = normal != 0u || read_only != 0u || reserved != 0u || write_only != 0u;
    census->normal_addresses += normal != 0u;
    census->normal_bits += bit_count(normal);
    census->read_only_addresses += read_only != 0u;
    census->read_only_bits += bit_count(read_only);
    census->reserved_addresses += reserved != 0u;
    census->reserved_bits += bit_count(reserved);
    census->write_only_addresses += write_only != 0u;
    census->write_only_bits += bit_count(write_only);
    census->unresolved_addresses += unresolved;
    census->aliases += expectation->aliases;
    census->mux_defaults += (expectation->flags & DSPIC33_SFR_ACCESS_MUX_DEFAULT) != 0u;
    if (unresolved) {
        printf("[sfr-access-unresolved] address=0x%04x initial=0x%04x "
               "ones=0x%04x zeroes=0x%04x normal=0x%04x "
               "read-only=0x%04x reserved=0x%04x write-only=0x%04x "
               "flags=0x%02x aliases=%u\n",
               (unsigned)expectation->address, (unsigned)initial, (unsigned)ones,
               (unsigned)zeroes, (unsigned)normal, (unsigned)read_only,
               (unsigned)reserved, (unsigned)write_only, (unsigned)expectation->flags,
               (unsigned)expectation->aliases);
    }
}

int main(void) {
    Dspic33 cpu;
    SfrAccessCensus census = {0u};
    uint32_t index;

    _Static_assert(sizeof(dspic33_sfr_access_expectations) /
                           sizeof(dspic33_sfr_access_expectations[0]) ==
                       DSPIC33_SFR_ACCESS_ADDRESS_COUNT,
                   "SFR access expectation count");
    if (!dspic33_initialize(&cpu)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    for (index = 0u; index < DSPIC33_SFR_ACCESS_ADDRESS_COUNT; index++) {
        inspect_register(&census, &cpu, &dspic33_sfr_access_expectations[index]);
    }
    dspic33_destroy(&cpu);
    printf("[sfr-access-inventory] definitions=%u addresses=%u aliases=%" PRIu32
           " mux-defaults=%" PRIu32 " normal-bits=%u read-only-bits=%u "
           "reserved-bits=%u write-only-bits=%u side-effect-bits=%u\n",
           DSPIC33_SFR_ACCESS_DEFINITION_COUNT, DSPIC33_SFR_ACCESS_ADDRESS_COUNT,
           census.aliases, census.mux_defaults, DSPIC33_SFR_ACCESS_NORMAL_BIT_COUNT,
           DSPIC33_SFR_ACCESS_READ_ONLY_BIT_COUNT,
           DSPIC33_SFR_ACCESS_RESERVED_BIT_COUNT,
           DSPIC33_SFR_ACCESS_WRITE_ONLY_BIT_COUNT,
           DSPIC33_SFR_ACCESS_SIDE_EFFECT_BIT_COUNT);
    printf("[sfr-access-summary] unresolved-addresses=%" PRIu32
           " normal-addresses=%" PRIu32 " normal-bits=%" PRIu32
           " read-only-addresses=%" PRIu32 " read-only-bits=%" PRIu32
           " reserved-addresses=%" PRIu32 " reserved-bits=%" PRIu32
           " write-only-addresses=%" PRIu32 " write-only-bits=%" PRIu32 "\n",
           census.unresolved_addresses, census.normal_addresses, census.normal_bits,
           census.read_only_addresses, census.read_only_bits, census.reserved_addresses,
           census.reserved_bits, census.write_only_addresses, census.write_only_bits);
    if (census.aliases != DSPIC33_SFR_ACCESS_ALIAS_COUNT ||
        census.mux_defaults != DSPIC33_SFR_ACCESS_MUX_DEFAULT_COUNT) {
        return 2;
    }
    return census.unresolved_addresses == 0u ? 0 : 1;
}
