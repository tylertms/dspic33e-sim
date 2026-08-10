#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dspic33.h"
#include "dspic33ep512mu810_sfr_access.h"

typedef struct {
    uint16_t initial;
    uint16_t ones;
    uint16_t zeroes;
    uint16_t ones_without_dependent;
    uint16_t zeroes_with_dependent;
    uint16_t normal;
    uint16_t read_only;
    uint16_t reserved;
    uint16_t write_only;
} SfrAccessDifference;

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
    uint32_t dependent_read_only_bits;
} SfrAccessCensus;

typedef struct {
    SfrAccessCensus access;
    uint32_t addresses;
    uint32_t unresolved_addresses;
    uint32_t selector_reset_addresses;
    uint32_t selector_reset_bits;
    uint32_t selector_switch_addresses;
    uint32_t selector_switch_bits;
} SfrMuxCensus;

static uint32_t bit_count(uint16_t value) {
    uint32_t count = 0u;
    while (value != 0u) {
        count += value & 1u;
        value >>= 1u;
    }
    return count;
}

static bool access_is_unresolved(const SfrAccessDifference* difference) {
    return difference->normal != 0u || difference->read_only != 0u ||
           difference->reserved != 0u || difference->write_only != 0u;
}

static void select_mux(Dspic33* cpu,
                       const Dspic33SfrMuxAccessExpectation* expectation) {
    uint16_t selector = dspic33_read_word(cpu, expectation->selector_address);
    selector = (uint16_t)((selector & ~expectation->selector_mask) |
                          expectation->selector_value);
    dspic33_write_word(cpu, expectation->selector_address, selector);
}

static void prepare_register(Dspic33* cpu,
                             const Dspic33SfrMuxAccessExpectation* mux_expectation) {
    dspic33_reset(cpu, 0u);
    if (mux_expectation != NULL) {
        select_mux(cpu, mux_expectation);
    }
}

static SfrAccessDifference
inspect_access(Dspic33* cpu, uint16_t address, uint16_t normal_mask,
               uint16_t read_only_mask, uint16_t reserved_mask,
               uint16_t write_only_mask, uint16_t dependent_read_only_mask,
               const Dspic33SfrMuxAccessExpectation* mux_expectation) {
    SfrAccessDifference difference;
    prepare_register(cpu, mux_expectation);
    difference.initial = dspic33_read_word(cpu, address);
    dspic33_write_word(cpu, address, 0xffffu);
    difference.ones = dspic33_read_word(cpu, address);
    prepare_register(cpu, mux_expectation);
    dspic33_write_word(cpu, address, 0u);
    difference.zeroes = dspic33_read_word(cpu, address);
    prepare_register(cpu, mux_expectation);
    dspic33_write_word(cpu, address,
                       (uint16_t)(UINT16_MAX & ~dependent_read_only_mask));
    difference.ones_without_dependent = dspic33_read_word(cpu, address);
    prepare_register(cpu, mux_expectation);
    dspic33_write_word(cpu, address, dependent_read_only_mask);
    difference.zeroes_with_dependent = dspic33_read_word(cpu, address);
    difference.normal =
        (uint16_t)(((uint16_t)~difference.ones | difference.zeroes) & normal_mask);
    difference.read_only =
        (uint16_t)((((difference.ones ^ difference.initial) |
                     (difference.zeroes ^ difference.initial)) &
                    read_only_mask & ~dependent_read_only_mask) |
                   (((difference.ones ^ difference.ones_without_dependent) |
                     (difference.zeroes ^ difference.zeroes_with_dependent)) &
                    dependent_read_only_mask));
    difference.reserved =
        (uint16_t)((difference.initial | difference.ones | difference.zeroes) &
                   reserved_mask);
    difference.write_only =
        (uint16_t)((difference.initial | difference.ones | difference.zeroes) &
                   write_only_mask);
    return difference;
}

static void record_access(SfrAccessCensus* census,
                          const SfrAccessDifference* difference) {
    census->normal_addresses += difference->normal != 0u;
    census->normal_bits += bit_count(difference->normal);
    census->read_only_addresses += difference->read_only != 0u;
    census->read_only_bits += bit_count(difference->read_only);
    census->reserved_addresses += difference->reserved != 0u;
    census->reserved_bits += bit_count(difference->reserved);
    census->write_only_addresses += difference->write_only != 0u;
    census->write_only_bits += bit_count(difference->write_only);
    census->unresolved_addresses += access_is_unresolved(difference);
}

static void inspect_register(SfrAccessCensus* census, Dspic33* cpu,
                             const Dspic33SfrAccessExpectation* expectation) {
    SfrAccessDifference difference =
        inspect_access(cpu, expectation->address, expectation->normal,
                       expectation->read_only, expectation->reserved,
                       expectation->write_only, expectation->dependent_read_only, NULL);
    record_access(census, &difference);
    census->aliases += expectation->aliases;
    census->mux_defaults += (expectation->flags & DSPIC33_SFR_ACCESS_MUX_DEFAULT) != 0u;
    census->dependent_read_only_bits += bit_count(expectation->dependent_read_only);
    if (access_is_unresolved(&difference)) {
        printf("[sfr-access-unresolved] address=0x%04x initial=0x%04x "
               "ones=0x%04x zeroes=0x%04x normal=0x%04x "
               "read-only=0x%04x reserved=0x%04x write-only=0x%04x "
               "flags=0x%02x aliases=%u\n",
               (unsigned)expectation->address, (unsigned)difference.initial,
               (unsigned)difference.ones, (unsigned)difference.zeroes,
               (unsigned)difference.normal, (unsigned)difference.read_only,
               (unsigned)difference.reserved, (unsigned)difference.write_only,
               (unsigned)expectation->flags, (unsigned)expectation->aliases);
    }
}

static void inspect_mux_register(SfrMuxCensus* census, Dspic33* cpu,
                                 const Dspic33SfrMuxAccessExpectation* expectation) {
    uint16_t selector_reset;
    uint16_t selector_switch;
    SfrAccessDifference difference;
    bool unresolved;
    dspic33_reset(cpu, 0u);
    selector_reset = (uint16_t)((dspic33_read_word(cpu, expectation->selector_address) ^
                                 expectation->selector_reset) &
                                expectation->selector_mask);
    select_mux(cpu, expectation);
    selector_switch =
        (uint16_t)((dspic33_read_word(cpu, expectation->selector_address) ^
                    expectation->selector_value) &
                   expectation->selector_mask);
    difference = inspect_access(cpu, expectation->address, expectation->normal,
                                expectation->read_only, expectation->reserved,
                                expectation->write_only,
                                expectation->dependent_read_only, expectation);
    unresolved = selector_reset != 0u || selector_switch != 0u ||
                 access_is_unresolved(&difference);
    census->addresses++;
    census->unresolved_addresses += unresolved;
    census->selector_reset_addresses += selector_reset != 0u;
    census->selector_reset_bits += bit_count(selector_reset);
    census->selector_switch_addresses += selector_switch != 0u;
    census->selector_switch_bits += bit_count(selector_switch);
    record_access(&census->access, &difference);
    if (unresolved) {
        printf("[sfr-mux-unresolved] address=0x%04x selector=0x%04x "
               "selector-reset=0x%04x selector-switch=0x%04x "
               "initial=0x%04x ones=0x%04x zeroes=0x%04x normal=0x%04x "
               "read-only=0x%04x reserved=0x%04x write-only=0x%04x\n",
               (unsigned)expectation->address, (unsigned)expectation->selector_address,
               (unsigned)selector_reset, (unsigned)selector_switch,
               (unsigned)difference.initial, (unsigned)difference.ones,
               (unsigned)difference.zeroes, (unsigned)difference.normal,
               (unsigned)difference.read_only, (unsigned)difference.reserved,
               (unsigned)difference.write_only);
    }
}

static void print_inventory(const SfrAccessCensus* census) {
    printf(
        "[sfr-access-inventory] definitions=%u addresses=%u aliases=%" PRIu32
        " mux-defaults=%" PRIu32 " mux-alternates=%u normal-bits=%u "
        "read-only-bits=%u reserved-bits=%u write-only-bits=%u "
        "side-effect-bits=%u alternate-normal-bits=%u "
        "alternate-read-only-bits=%u alternate-reserved-bits=%u "
        "alternate-write-only-bits=%u alternate-side-effect-bits=%u\n",
        DSPIC33_SFR_ACCESS_DEFINITION_COUNT, DSPIC33_SFR_ACCESS_ADDRESS_COUNT,
        census->aliases, census->mux_defaults, DSPIC33_SFR_ACCESS_MUX_ALTERNATE_COUNT,
        DSPIC33_SFR_ACCESS_NORMAL_BIT_COUNT, DSPIC33_SFR_ACCESS_READ_ONLY_BIT_COUNT,
        DSPIC33_SFR_ACCESS_RESERVED_BIT_COUNT, DSPIC33_SFR_ACCESS_WRITE_ONLY_BIT_COUNT,
        DSPIC33_SFR_ACCESS_SIDE_EFFECT_BIT_COUNT,
        DSPIC33_SFR_MUX_ACCESS_NORMAL_BIT_COUNT,
        DSPIC33_SFR_MUX_ACCESS_READ_ONLY_BIT_COUNT,
        DSPIC33_SFR_MUX_ACCESS_RESERVED_BIT_COUNT,
        DSPIC33_SFR_MUX_ACCESS_WRITE_ONLY_BIT_COUNT,
        DSPIC33_SFR_MUX_ACCESS_SIDE_EFFECT_BIT_COUNT);
}

static void print_access_summary(const SfrAccessCensus* census) {
    printf("[sfr-access-summary] unresolved-addresses=%" PRIu32
           " normal-addresses=%" PRIu32 " normal-bits=%" PRIu32
           " read-only-addresses=%" PRIu32 " read-only-bits=%" PRIu32
           " reserved-addresses=%" PRIu32 " reserved-bits=%" PRIu32
           " write-only-addresses=%" PRIu32 " write-only-bits=%" PRIu32 "\n",
           census->unresolved_addresses, census->normal_addresses, census->normal_bits,
           census->read_only_addresses, census->read_only_bits,
           census->reserved_addresses, census->reserved_bits,
           census->write_only_addresses, census->write_only_bits);
}

static void print_mux_summary(const SfrMuxCensus* census) {
    printf("[sfr-mux-summary] addresses=%" PRIu32 " unresolved-addresses=%" PRIu32
           " selector-reset-addresses=%" PRIu32 " selector-reset-bits=%" PRIu32
           " selector-switch-addresses=%" PRIu32 " selector-switch-bits=%" PRIu32
           " alternate-unresolved-addresses=%" PRIu32
           " alternate-normal-addresses=%" PRIu32 " alternate-normal-bits=%" PRIu32
           " alternate-read-only-addresses=%" PRIu32
           " alternate-read-only-bits=%" PRIu32 " alternate-reserved-addresses=%" PRIu32
           " alternate-reserved-bits=%" PRIu32
           " alternate-write-only-addresses=%" PRIu32
           " alternate-write-only-bits=%" PRIu32 "\n",
           census->addresses, census->unresolved_addresses,
           census->selector_reset_addresses, census->selector_reset_bits,
           census->selector_switch_addresses, census->selector_switch_bits,
           census->access.unresolved_addresses, census->access.normal_addresses,
           census->access.normal_bits, census->access.read_only_addresses,
           census->access.read_only_bits, census->access.reserved_addresses,
           census->access.reserved_bits, census->access.write_only_addresses,
           census->access.write_only_bits);
}

int main(void) {
    Dspic33 cpu;
    SfrAccessCensus census = {0u};
    SfrMuxCensus mux_census = {0u};
    uint32_t index;
    _Static_assert(sizeof(dspic33_sfr_access_expectations) /
                           sizeof(dspic33_sfr_access_expectations[0]) ==
                       DSPIC33_SFR_ACCESS_ADDRESS_COUNT,
                   "SFR access expectation count");
    _Static_assert(sizeof(dspic33_sfr_mux_access_expectations) /
                           sizeof(dspic33_sfr_mux_access_expectations[0]) ==
                       DSPIC33_SFR_ACCESS_MUX_ALTERNATE_COUNT,
                   "SFR mux access expectation count");
    if (!dspic33_initialize(&cpu)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    for (index = 0u; index < DSPIC33_SFR_ACCESS_ADDRESS_COUNT; index++) {
        inspect_register(&census, &cpu, &dspic33_sfr_access_expectations[index]);
    }
    for (index = 0u; index < DSPIC33_SFR_ACCESS_MUX_ALTERNATE_COUNT; index++) {
        inspect_mux_register(&mux_census, &cpu,
                             &dspic33_sfr_mux_access_expectations[index]);
    }
    dspic33_destroy(&cpu);
    print_inventory(&census);
    print_access_summary(&census);
    print_mux_summary(&mux_census);
    if (census.aliases != DSPIC33_SFR_ACCESS_ALIAS_COUNT ||
        census.mux_defaults != DSPIC33_SFR_ACCESS_MUX_DEFAULT_COUNT ||
        census.dependent_read_only_bits !=
            DSPIC33_SFR_ACCESS_DEPENDENT_READ_ONLY_BIT_COUNT ||
        mux_census.addresses != DSPIC33_SFR_ACCESS_MUX_ALTERNATE_COUNT) {
        return 2;
    }
    return census.unresolved_addresses == 0u && mux_census.unresolved_addresses == 0u
               ? 0
               : 1;
}
