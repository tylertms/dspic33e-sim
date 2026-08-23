#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "architecture/dspic33/internal.h"
#include "device/dspic33ep_mu/data.h"
#include "sfr_cases.h"

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

typedef struct {
    SfrAccessCensus access;
    uint32_t addresses;
    uint32_t unresolved_addresses;
    uint32_t selector_reset_addresses;
    uint32_t selector_reset_bits;
    uint32_t selector_switch_addresses;
    uint32_t selector_switch_bits;
    uint32_t absence_addresses;
    uint32_t isolation_addresses;
} SfrConditionalCensus;

typedef struct {
    uint32_t addresses;
    uint32_t normal_write_failures;
    uint32_t restricted_read_failures;
    uint32_t restricted_write_failures;
    uint32_t resurrection_failures;
    uint32_t failures;
} SfrDependentNormalCensus;

typedef struct {
    uint32_t addresses;
    uint32_t direct_set_failures;
    uint32_t direct_clear_failures;
    uint32_t failures;
} SfrProtectedCensus;

typedef struct {
    uint32_t implemented_words;
    uint32_t absent_words;
    uint32_t absent_ranges;
    uint32_t direct_byte_checks;
    uint32_t direct_word_checks;
    uint32_t internal_pad_byte_checks;
    uint32_t internal_pad_word_checks;
    uint32_t odd_crossing_checks;
    uint32_t lifecycle_checks;
    uint32_t failures;
} SfrMapCensus;

static bool map_word_is_implemented(uint32_t slot) {
    const uint8_t* bitmap = dspic33ep_mu_implementation_bitmap(DSPIC33EP_MU_DEVICE_512MU810);
    return (bitmap[slot >> 3u] & (uint8_t)(1u << (slot & 7u))) != 0u;
}

static void inspect_map_condition(SfrMapCensus* census, bool condition, uint16_t address,
                                  const char* operation) {
    if (!condition) {
        census->failures++;
        printf("[sfr-map-failed] address=0x%04x operation=%s\n", (unsigned)address, operation);
    }
}

static void inspect_absent_word(SfrMapCensus* census, Dspic33* cpu, uint16_t address) {
    uint32_t events = (uint32_t)cpu->events.count;
    cpu->data[address] = 0xa5u;
    cpu->data[address + 1u] = 0x5au;
    cpu->address_error = false;
    cpu->io.cpu_write_valid = false;
    inspect_map_condition(census, dspic33_read_byte(cpu, address) == 0u, address,
                          "direct-low-read");
    inspect_map_condition(census, dspic33_read_byte(cpu, address + 1u) == 0u, address,
                          "direct-high-read");
    dspic33_write_byte(cpu, address, 0x3cu);
    dspic33_write_byte(cpu, address + 1u, 0xc3u);
    inspect_map_condition(census, dspic33_read_word(cpu, address) == 0u, address,
                          "direct-word-read");
    dspic33_write_word(cpu, address, 0x3cc3u);
    inspect_map_condition(census,
                          cpu->data[address] == 0xa5u && cpu->data[address + 1u] == 0x5au &&
                              !cpu->address_error && !cpu->io.cpu_write_valid &&
                              cpu->events.count == events,
                          address, "direct-write-isolation");
    census->direct_byte_checks += 2u;
    census->direct_word_checks++;

    cpu->instruction_active = true;
    cpu->io.dma_transfer_active = true;
    inspect_map_condition(census, dspic33_read_byte(cpu, address) == 0u, address,
                          "internal-pad-low-read");
    inspect_map_condition(census, dspic33_read_byte(cpu, address + 1u) == 0u, address,
                          "internal-pad-high-read");
    dspic33_write_byte(cpu, address, 0x69u);
    dspic33_write_byte(cpu, address + 1u, 0x96u);
    inspect_map_condition(census, dspic33_read_word(cpu, address) == 0u, address,
                          "internal-pad-word-read");
    dspic33_write_word(cpu, address, 0x6996u);
    inspect_map_condition(census,
                          cpu->data[address] == 0xa5u && cpu->data[address + 1u] == 0x5au &&
                              !cpu->address_error && !cpu->io.cpu_write_valid &&
                              cpu->events.count == events,
                          address, "internal-pad-write-isolation");
    cpu->io.dma_transfer_active = false;
    cpu->instruction_active = false;
    census->internal_pad_byte_checks += 2u;
    census->internal_pad_word_checks++;
}

static SfrMapCensus inspect_sfr_map(Dspic33* cpu) {
    bool expected[DSPIC33_SFR_WORD_COUNT] = {false};
    SfrMapCensus census = {0u};
    Dspic33 copy;
    uint32_t slot;
    uint32_t index;
    bool copy_initialized;
    for (index = 0u; index < DSPIC33_SFR_ACCESS_ADDRESS_COUNT; index++) {
        expected[dspic33_sfr_access_expectations[index].address >> 1u] = true;
    }
    for (slot = 0u; slot < DSPIC33_SFR_WORD_COUNT; slot++) {
        bool implemented = map_word_is_implemented(slot);
        uint16_t address = (uint16_t)(slot << 1u);
        inspect_map_condition(&census, implemented == expected[slot], address, "dfp-membership");
        if (implemented) {
            census.implemented_words++;
            continue;
        }
        census.absent_words++;
        census.absent_ranges += slot == 0u || map_word_is_implemented(slot - 1u);
        inspect_absent_word(&census, cpu, address);
    }

    dspic33_reset(cpu, 0u);
    cpu->data[0x013eu] = 0xa5u;
    cpu->io.cpu_write_valid = false;
    dspic33_write_word(cpu, 0x013du, 0x69a0u);
    inspect_map_condition(&census, dspic33_read_word(cpu, 0x013cu) == 0xa000u, 0x013du,
                          "odd-present-low-write");
    inspect_map_condition(&census, dspic33_read_byte(cpu, 0x013eu) == 0u, 0x013du,
                          "odd-absent-high-read");
    inspect_map_condition(&census, cpu->data[0x013eu] == 0xa5u, 0x013du, "odd-absent-high-storage");
    inspect_map_condition(&census,
                          cpu->io.cpu_write_valid && cpu->io.cpu_write_address == 0x013du &&
                              cpu->io.cpu_write_width == 1u,
                          0x013du, "odd-present-low-transaction");
    inspect_map_condition(&census,
                          dspic33_read_word(cpu, 0x013du) == 0x00a0u && !cpu->address_error,
                          0x013du, "odd-present-low-result");

    dspic33_reset(cpu, 0u);
    cpu->data[0x00ffu] = 0xa5u;
    cpu->io.cpu_write_valid = false;
    dspic33_write_word(cpu, 0x00ffu, 0x5a69u);
    inspect_map_condition(&census, dspic33_read_byte(cpu, 0x00ffu) == 0u, 0x00ffu,
                          "odd-absent-low-read");
    inspect_map_condition(&census, cpu->data[0x00ffu] == 0xa5u, 0x00ffu, "odd-absent-low-storage");
    inspect_map_condition(&census, dspic33_read_word(cpu, 0x0100u) == 0x005au, 0x00ffu,
                          "odd-present-high-write");
    inspect_map_condition(&census,
                          cpu->io.cpu_write_valid && cpu->io.cpu_write_address == 0x0100u &&
                              cpu->io.cpu_write_width == 1u,
                          0x00ffu, "odd-present-high-transaction");
    inspect_map_condition(&census,
                          dspic33_read_word(cpu, 0x00ffu) == 0x5a00u && !cpu->address_error,
                          0x00ffu, "odd-present-high-result");
    census.odd_crossing_checks += 10u;

    cpu->data[0x0a9cu] = 0xa5u;
    cpu->data[0x0a9du] = 0x5au;
    copy_initialized = dspic33_initialize(&copy);
    inspect_map_condition(&census, copy_initialized, 0x0a9cu, "copy-initialize");
    census.lifecycle_checks++;
    if (copy_initialized) {
        inspect_map_condition(&census, dspic33_copy(&copy, cpu), 0x0a9cu, "copy-state");
        inspect_map_condition(&census, dspic33_read_word(&copy, 0x0a9cu) == 0u, 0x0a9cu,
                              "copy-read");
        census.lifecycle_checks += 2u;
        dspic33_release(&copy);
    }
    dspic33_reset(cpu, 0u);
    inspect_map_condition(&census, dspic33_read_word(cpu, 0x0a9cu) == 0u, 0x0a9cu, "reset-read");
    dspic33_write_word(cpu, 0x0a9cu, 0xffffu);
    inspect_map_condition(&census, dspic33_read_word(cpu, 0x0a9cu) == 0u, 0x0a9cu, "reset-write");
    census.lifecycle_checks += 2u;
    return census;
}

static uint32_t bit_count(uint16_t value) {
    uint32_t count = 0u;
    while (value != 0u) {
        count += value & 1u;
        value >>= 1u;
    }
    return count;
}

static bool access_is_unresolved(const SfrAccessDifference* difference) {
    return difference->normal != 0u || difference->read_only != 0u || difference->reserved != 0u ||
           difference->write_only != 0u;
}

static void select_register(Dspic33* cpu, uint16_t selector_address, uint16_t selector_mask,
                            uint16_t selector_value) {
    uint16_t selector = dspic33_read_word(cpu, selector_address);
    selector = (uint16_t)((selector & ~selector_mask) | (selector_value & selector_mask));
    dspic33_write_word(cpu, selector_address, selector);
}

static void prepare_register(Dspic33* cpu, uint16_t selector_address, uint16_t selector_mask,
                             uint16_t selector_value) {
    dspic33_reset(cpu, 0u);
    if (selector_mask != 0u) {
        select_register(cpu, selector_address, selector_mask, selector_value);
    }
}

static SfrAccessDifference
inspect_access(Dspic33* cpu, uint16_t address, uint16_t normal_mask, uint16_t read_only_mask,
               uint16_t reserved_mask, uint16_t write_only_mask, uint16_t dependent_read_only_mask,
               uint16_t selector_address, uint16_t selector_mask, uint16_t selector_value) {
    SfrAccessDifference difference;
    prepare_register(cpu, selector_address, selector_mask, selector_value);
    difference.initial = dspic33_read_word(cpu, address);
    dspic33_write_word(cpu, address, 0xffffu);
    difference.ones = dspic33_read_word(cpu, address);
    prepare_register(cpu, selector_address, selector_mask, selector_value);
    dspic33_write_word(cpu, address, 0u);
    difference.zeroes = dspic33_read_word(cpu, address);
    prepare_register(cpu, selector_address, selector_mask, selector_value);
    dspic33_write_word(cpu, address, (uint16_t)(UINT16_MAX & ~dependent_read_only_mask));
    difference.ones_without_dependent = dspic33_read_word(cpu, address);
    prepare_register(cpu, selector_address, selector_mask, selector_value);
    dspic33_write_word(cpu, address, dependent_read_only_mask);
    difference.zeroes_with_dependent = dspic33_read_word(cpu, address);
    difference.normal = (uint16_t)(((uint16_t)~difference.ones | difference.zeroes) & normal_mask);
    difference.read_only = (uint16_t)((((difference.ones ^ difference.initial) |
                                        (difference.zeroes ^ difference.initial)) &
                                       read_only_mask & ~dependent_read_only_mask) |
                                      (((difference.ones ^ difference.ones_without_dependent) |
                                        (difference.zeroes ^ difference.zeroes_with_dependent)) &
                                       dependent_read_only_mask));
    difference.reserved =
        (uint16_t)((difference.initial | difference.ones | difference.zeroes) & reserved_mask);
    difference.write_only =
        (uint16_t)((difference.initial | difference.ones | difference.zeroes) & write_only_mask);
    return difference;
}

static void record_access(SfrAccessCensus* census, const SfrAccessDifference* difference) {
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
    census->aliases += expectation->aliases;
    census->mux_defaults += (expectation->flags & DSPIC33_SFR_ACCESS_MUX_DEFAULT) != 0u;
    census->dependent_read_only_bits += bit_count(expectation->dependent_read_only);
    if ((expectation->flags & DSPIC33_SFR_ACCESS_CONDITIONAL) != 0u) {
        return;
    }
    SfrAccessDifference difference =
        inspect_access(cpu, expectation->address, expectation->normal, expectation->read_only,
                       expectation->reserved, expectation->write_only,
                       expectation->dependent_read_only, 0u, 0u, 0u);
    record_access(census, &difference);
    if (access_is_unresolved(&difference)) {
        printf("[sfr-access-unresolved] address=0x%04x initial=0x%04x "
               "ones=0x%04x zeroes=0x%04x normal=0x%04x "
               "read-only=0x%04x reserved=0x%04x write-only=0x%04x "
               "flags=0x%02x aliases=%u\n",
               (unsigned)expectation->address, (unsigned)difference.initial,
               (unsigned)difference.ones, (unsigned)difference.zeroes, (unsigned)difference.normal,
               (unsigned)difference.read_only, (unsigned)difference.reserved,
               (unsigned)difference.write_only, (unsigned)expectation->flags,
               (unsigned)expectation->aliases);
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
    select_register(cpu, expectation->selector_address, expectation->selector_mask,
                    expectation->selector_value);
    selector_switch = (uint16_t)((dspic33_read_word(cpu, expectation->selector_address) ^
                                  expectation->selector_value) &
                                 expectation->selector_mask);
    difference = inspect_access(
        cpu, expectation->address, expectation->normal, expectation->read_only,
        expectation->reserved, expectation->write_only, expectation->dependent_read_only,
        expectation->selector_address, expectation->selector_mask, expectation->selector_value);
    unresolved = selector_reset != 0u || selector_switch != 0u || access_is_unresolved(&difference);
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
               (unsigned)selector_reset, (unsigned)selector_switch, (unsigned)difference.initial,
               (unsigned)difference.ones, (unsigned)difference.zeroes, (unsigned)difference.normal,
               (unsigned)difference.read_only, (unsigned)difference.reserved,
               (unsigned)difference.write_only);
    }
}

static void
inspect_conditional_register(SfrConditionalCensus* census, Dspic33* cpu,
                             const Dspic33SfrConditionalAccessExpectation* expectation) {
    SfrAccessDifference difference = inspect_access(
        cpu, expectation->address, expectation->normal, expectation->read_only,
        expectation->reserved, expectation->write_only, expectation->dependent_read_only,
        expectation->selector_address, expectation->selector_mask, expectation->selector_value);
    uint16_t selected_value = (uint16_t)(0xa55au & expectation->normal);
    uint16_t selector_reset;
    uint16_t selector_switch;
    bool absence;
    bool isolation;
    bool unresolved;
    record_access(&census->access, &difference);
    dspic33_reset(cpu, 0u);
    selector_reset = (uint16_t)((dspic33_read_word(cpu, expectation->selector_address) ^
                                 expectation->selector_reset) &
                                expectation->selector_mask);
    select_register(cpu, expectation->selector_address, expectation->selector_mask,
                    expectation->selector_value);
    selector_switch = (uint16_t)((dspic33_read_word(cpu, expectation->selector_address) ^
                                  expectation->selector_value) &
                                 expectation->selector_mask);
    dspic33_write_word(cpu, expectation->address, selected_value);
    isolation = dspic33_read_word(cpu, expectation->address) != selected_value;
    select_register(cpu, expectation->selector_address, expectation->selector_mask,
                    expectation->selector_reset);
    absence = dspic33_read_word(cpu, expectation->address) != 0u;
    dspic33_write_word(cpu, expectation->address, 0xffffu);
    absence = absence || dspic33_read_word(cpu, expectation->address) != 0u;
    select_register(cpu, expectation->selector_address, expectation->selector_mask,
                    expectation->selector_value);
    isolation = isolation || dspic33_read_word(cpu, expectation->address) != selected_value;
    unresolved = selector_reset != 0u || selector_switch != 0u || absence || isolation ||
                 access_is_unresolved(&difference);
    census->addresses++;
    census->unresolved_addresses += unresolved;
    census->selector_reset_addresses += selector_reset != 0u;
    census->selector_reset_bits += bit_count(selector_reset);
    census->selector_switch_addresses += selector_switch != 0u;
    census->selector_switch_bits += bit_count(selector_switch);
    census->absence_addresses += absence;
    census->isolation_addresses += isolation;
    if (unresolved) {
        printf("[sfr-conditional-unresolved] address=0x%04x selector=0x%04x "
               "selector-reset=0x%04x selector-switch=0x%04x absence=%u "
               "isolation=%u normal=0x%04x read-only=0x%04x reserved=0x%04x "
               "write-only=0x%04x\n",
               (unsigned)expectation->address, (unsigned)expectation->selector_address,
               (unsigned)selector_reset, (unsigned)selector_switch, absence ? 1u : 0u,
               isolation ? 1u : 0u, (unsigned)difference.normal, (unsigned)difference.read_only,
               (unsigned)difference.reserved, (unsigned)difference.write_only);
    }
}

static uint16_t dependent_normal_request(Dspic33* cpu,
                                         const Dspic33SfrDependentNormalExpectation* expectation,
                                         uint16_t selector_value, uint16_t value) {
    if (expectation->address != expectation->selector_address) {
        return value;
    }
    return (uint16_t)((dspic33_read_word(cpu, expectation->address) &
                       ~(expectation->selector_mask | expectation->mask)) |
                      selector_value | value);
}

static void
inspect_dependent_normal_register(SfrDependentNormalCensus* census, Dspic33* cpu,
                                  const Dspic33SfrDependentNormalExpectation* expectation) {
    uint16_t normal;
    uint16_t restricted_read;
    uint16_t restricted_write;
    uint16_t resurrected;
    prepare_register(cpu, expectation->selector_address, expectation->selector_mask,
                     expectation->normal_value);
    dspic33_write_word(
        cpu, expectation->address,
        dependent_normal_request(cpu, expectation, expectation->normal_value, expectation->mask));
    normal = (uint16_t)(dspic33_read_word(cpu, expectation->address) & expectation->mask);
    select_register(cpu, expectation->selector_address, expectation->selector_mask,
                    expectation->restricted_value);
    restricted_read = (uint16_t)(dspic33_read_word(cpu, expectation->address) & expectation->mask);
    dspic33_write_word(cpu, expectation->address,
                       dependent_normal_request(cpu, expectation, expectation->restricted_value,
                                                expectation->mask));
    restricted_write = (uint16_t)(dspic33_read_word(cpu, expectation->address) & expectation->mask);
    select_register(cpu, expectation->selector_address, expectation->selector_mask,
                    expectation->normal_value);
    resurrected = (uint16_t)(dspic33_read_word(cpu, expectation->address) & expectation->mask);
    census->addresses++;
    census->normal_write_failures += normal != expectation->mask;
    census->restricted_read_failures += restricted_read != 0u;
    census->restricted_write_failures += restricted_write != 0u;
    census->resurrection_failures += resurrected != 0u;
    census->failures += normal != expectation->mask || restricted_read != 0u ||
                        restricted_write != 0u || resurrected != 0u;
    if (normal != expectation->mask || restricted_read != 0u || restricted_write != 0u ||
        resurrected != 0u) {
        printf("[sfr-dependent-normal-failed] address=0x%04x normal=0x%04x "
               "restricted-read=0x%04x restricted-write=0x%04x "
               "resurrected=0x%04x\n",
               (unsigned)expectation->address, (unsigned)normal, (unsigned)restricted_read,
               (unsigned)restricted_write, (unsigned)resurrected);
    }
}

static void inspect_protected_register(SfrProtectedCensus* census, Dspic33* cpu,
                                       const Dspic33SfrProtectedAccessExpectation* expectation) {
    uint16_t initial;
    uint16_t mask = (uint16_t)(expectation->normal | expectation->set_only);
    uint16_t direct_set;
    uint16_t direct_clear;
    dspic33_reset(cpu, 0u);
    initial = (uint16_t)(dspic33_read_word(cpu, expectation->address) & mask);
    dspic33_write_word(cpu, expectation->address, (uint16_t)(initial | mask));
    direct_set = (uint16_t)(dspic33_read_word(cpu, expectation->address) & mask);
    dspic33_write_word(cpu, expectation->address, (uint16_t)(initial & ~mask));
    direct_clear = (uint16_t)(dspic33_read_word(cpu, expectation->address) & mask);
    census->addresses++;
    census->direct_set_failures += direct_set != initial;
    census->direct_clear_failures += direct_clear != initial;
    census->failures += direct_set != initial || direct_clear != initial;
    if (direct_set != initial || direct_clear != initial) {
        printf("[sfr-protected-failed] address=0x%04x initial=0x%04x "
               "direct-set=0x%04x direct-clear=0x%04x\n",
               (unsigned)expectation->address, (unsigned)initial, (unsigned)direct_set,
               (unsigned)direct_clear);
    }
}

static void print_inventory(const SfrAccessCensus* census) {
    printf(
        "[sfr-access-inventory] definitions=%u addresses=%u aliases=%" PRIu32
        " mux-defaults=%" PRIu32 " mux-alternates=%u normal-bits=%u "
        "read-only-bits=%u reserved-bits=%u write-only-bits=%u "
        "side-effect-bits=%u split-access-addresses=%u split-access-bits=%u "
        "dependent-normal-addresses=%u dependent-normal-bits=%u "
        "protected-addresses=%u protected-normal-bits=%u "
        "protected-set-only-bits=%u "
        "alternate-normal-bits=%u "
        "alternate-read-only-bits=%u alternate-reserved-bits=%u "
        "alternate-write-only-bits=%u alternate-side-effect-bits=%u\n",
        DSPIC33_SFR_ACCESS_DEFINITION_COUNT, DSPIC33_SFR_ACCESS_ADDRESS_COUNT, census->aliases,
        census->mux_defaults, DSPIC33_SFR_ACCESS_MUX_ALTERNATE_COUNT,
        DSPIC33_SFR_ACCESS_NORMAL_BIT_COUNT, DSPIC33_SFR_ACCESS_READ_ONLY_BIT_COUNT,
        DSPIC33_SFR_ACCESS_RESERVED_BIT_COUNT, DSPIC33_SFR_ACCESS_WRITE_ONLY_BIT_COUNT,
        DSPIC33_SFR_ACCESS_SIDE_EFFECT_BIT_COUNT, DSPIC33_SFR_ACCESS_SPLIT_ACCESS_ADDRESS_COUNT,
        DSPIC33_SFR_ACCESS_SPLIT_ACCESS_BIT_COUNT,
        DSPIC33_SFR_ACCESS_DEPENDENT_NORMAL_ADDRESS_COUNT,
        DSPIC33_SFR_ACCESS_DEPENDENT_NORMAL_BIT_COUNT, DSPIC33_SFR_ACCESS_PROTECTED_ADDRESS_COUNT,
        DSPIC33_SFR_ACCESS_PROTECTED_NORMAL_BIT_COUNT,
        DSPIC33_SFR_ACCESS_PROTECTED_SET_ONLY_BIT_COUNT, DSPIC33_SFR_MUX_ACCESS_NORMAL_BIT_COUNT,
        DSPIC33_SFR_MUX_ACCESS_READ_ONLY_BIT_COUNT, DSPIC33_SFR_MUX_ACCESS_RESERVED_BIT_COUNT,
        DSPIC33_SFR_MUX_ACCESS_WRITE_ONLY_BIT_COUNT, DSPIC33_SFR_MUX_ACCESS_SIDE_EFFECT_BIT_COUNT);
}

static void print_side_effect_expectations(void) {
    uint32_t index;
    for (index = 0u; index < DSPIC33_SFR_ACCESS_ADDRESS_COUNT; index++) {
        const Dspic33SfrAccessExpectation* expectation = &dspic33_sfr_access_expectations[index];
        if (expectation->side_effect != 0u) {
            printf("[sfr-side-effect-expected] address=0x%04x mask=0x%04x\n",
                   (unsigned)expectation->address, (unsigned)expectation->side_effect);
        }
    }
}

static void print_access_summary(const SfrAccessCensus* census) {
    printf("[sfr-access-summary] unresolved-addresses=%" PRIu32 " normal-addresses=%" PRIu32
           " normal-bits=%" PRIu32 " read-only-addresses=%" PRIu32 " read-only-bits=%" PRIu32
           " reserved-addresses=%" PRIu32 " reserved-bits=%" PRIu32 " write-only-addresses=%" PRIu32
           " write-only-bits=%" PRIu32 "\n",
           census->unresolved_addresses, census->normal_addresses, census->normal_bits,
           census->read_only_addresses, census->read_only_bits, census->reserved_addresses,
           census->reserved_bits, census->write_only_addresses, census->write_only_bits);
}

static void print_mux_summary(const SfrMuxCensus* census) {
    printf("[sfr-mux-summary] addresses=%" PRIu32 " unresolved-addresses=%" PRIu32
           " selector-reset-addresses=%" PRIu32 " selector-reset-bits=%" PRIu32
           " selector-switch-addresses=%" PRIu32 " selector-switch-bits=%" PRIu32
           " alternate-unresolved-addresses=%" PRIu32 " alternate-normal-addresses=%" PRIu32
           " alternate-normal-bits=%" PRIu32 " alternate-read-only-addresses=%" PRIu32
           " alternate-read-only-bits=%" PRIu32 " alternate-reserved-addresses=%" PRIu32
           " alternate-reserved-bits=%" PRIu32 " alternate-write-only-addresses=%" PRIu32
           " alternate-write-only-bits=%" PRIu32 "\n",
           census->addresses, census->unresolved_addresses, census->selector_reset_addresses,
           census->selector_reset_bits, census->selector_switch_addresses,
           census->selector_switch_bits, census->access.unresolved_addresses,
           census->access.normal_addresses, census->access.normal_bits,
           census->access.read_only_addresses, census->access.read_only_bits,
           census->access.reserved_addresses, census->access.reserved_bits,
           census->access.write_only_addresses, census->access.write_only_bits);
}

static void print_conditional_summary(const SfrConditionalCensus* census) {
    printf("[sfr-conditional-summary] addresses=%" PRIu32
           " normal-bits=%u read-only-bits=%u reserved-bits=%u write-only-bits=%u "
           "side-effect-bits=%u unresolved-addresses=%" PRIu32 " selector-reset-addresses=%" PRIu32
           " selector-reset-bits=%" PRIu32 " selector-switch-addresses=%" PRIu32
           " selector-switch-bits=%" PRIu32 " absence-addresses=%" PRIu32
           " isolation-addresses=%" PRIu32 " access-unresolved-addresses=%" PRIu32
           " access-normal-bits=%" PRIu32 " access-read-only-bits=%" PRIu32
           " access-reserved-bits=%" PRIu32 " access-write-only-bits=%" PRIu32 "\n",
           census->addresses, DSPIC33_SFR_CONDITIONAL_ACCESS_NORMAL_BIT_COUNT,
           DSPIC33_SFR_CONDITIONAL_ACCESS_READ_ONLY_BIT_COUNT,
           DSPIC33_SFR_CONDITIONAL_ACCESS_RESERVED_BIT_COUNT,
           DSPIC33_SFR_CONDITIONAL_ACCESS_WRITE_ONLY_BIT_COUNT,
           DSPIC33_SFR_CONDITIONAL_ACCESS_SIDE_EFFECT_BIT_COUNT, census->unresolved_addresses,
           census->selector_reset_addresses, census->selector_reset_bits,
           census->selector_switch_addresses, census->selector_switch_bits,
           census->absence_addresses, census->isolation_addresses,
           census->access.unresolved_addresses, census->access.normal_bits,
           census->access.read_only_bits, census->access.reserved_bits,
           census->access.write_only_bits);
}

static void print_dependent_normal_summary(const SfrDependentNormalCensus* census) {
    printf("[sfr-dependent-normal-summary] addresses=%" PRIu32 " normal-write-failures=%" PRIu32
           " restricted-read-failures=%" PRIu32 " restricted-write-failures=%" PRIu32
           " resurrection-failures=%" PRIu32 " failures=%" PRIu32 "\n",
           census->addresses, census->normal_write_failures, census->restricted_read_failures,
           census->restricted_write_failures, census->resurrection_failures, census->failures);
}

static void print_protected_summary(const SfrProtectedCensus* census) {
    printf("[sfr-protected-summary] addresses=%" PRIu32 " direct-set-failures=%" PRIu32
           " direct-clear-failures=%" PRIu32 " failures=%" PRIu32 "\n",
           census->addresses, census->direct_set_failures, census->direct_clear_failures,
           census->failures);
}

static void print_map_summary(const SfrMapCensus* census) {
    printf("[sfr-map-summary] words=%u implemented=%" PRIu32 " absent=%" PRIu32
           " absent-ranges=%" PRIu32 " direct-byte-checks=%" PRIu32 " direct-word-checks=%" PRIu32
           " internal-pad-byte-checks=%" PRIu32 " internal-pad-word-checks=%" PRIu32
           " odd-crossing-checks=%" PRIu32 " lifecycle-checks=%" PRIu32 " failures=%" PRIu32 "\n",
           DSPIC33_SFR_WORD_COUNT, census->implemented_words, census->absent_words,
           census->absent_ranges, census->direct_byte_checks, census->direct_word_checks,
           census->internal_pad_byte_checks, census->internal_pad_word_checks,
           census->odd_crossing_checks, census->lifecycle_checks, census->failures);
}

int main(void) {
    Dspic33 cpu;
    SfrAccessCensus census = {0u};
    SfrMuxCensus mux_census = {0u};
    SfrConditionalCensus conditional_census = {0u};
    SfrDependentNormalCensus dependent_normal_census = {0u};
    SfrProtectedCensus protected_census = {0u};
    SfrMapCensus map_census;
    uint32_t index;
    _Static_assert(sizeof(dspic33_sfr_access_expectations) /
                           sizeof(dspic33_sfr_access_expectations[0]) ==
                       DSPIC33_SFR_ACCESS_ADDRESS_COUNT,
                   "SFR access expectation count");
    _Static_assert(sizeof(dspic33_sfr_mux_access_expectations) /
                           sizeof(dspic33_sfr_mux_access_expectations[0]) ==
                       DSPIC33_SFR_ACCESS_MUX_ALTERNATE_COUNT,
                   "SFR mux access expectation count");
    _Static_assert(sizeof(dspic33_sfr_split_access_expectations) /
                           sizeof(dspic33_sfr_split_access_expectations[0]) ==
                       DSPIC33_SFR_ACCESS_SPLIT_ACCESS_ADDRESS_COUNT,
                   "SFR split access expectation count");
    _Static_assert(sizeof(dspic33_sfr_dependent_normal_expectations) /
                           sizeof(dspic33_sfr_dependent_normal_expectations[0]) ==
                       DSPIC33_SFR_ACCESS_DEPENDENT_NORMAL_ADDRESS_COUNT,
                   "SFR dependent normal expectation count");
    _Static_assert(sizeof(dspic33_sfr_protected_access_expectations) /
                           sizeof(dspic33_sfr_protected_access_expectations[0]) ==
                       DSPIC33_SFR_ACCESS_PROTECTED_ADDRESS_COUNT,
                   "SFR protected access expectation count");
    _Static_assert(sizeof(dspic33_sfr_conditional_access_expectations) /
                           sizeof(dspic33_sfr_conditional_access_expectations[0]) ==
                       DSPIC33_SFR_ACCESS_CONDITIONAL_COUNT,
                   "SFR conditional access expectation count");
    if (!dspic33_initialize(&cpu)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    for (index = 0u; index < DSPIC33_SFR_ACCESS_ADDRESS_COUNT; index++) {
        inspect_register(&census, &cpu, &dspic33_sfr_access_expectations[index]);
    }
    for (index = 0u; index < DSPIC33_SFR_ACCESS_MUX_ALTERNATE_COUNT; index++) {
        inspect_mux_register(&mux_census, &cpu, &dspic33_sfr_mux_access_expectations[index]);
    }
    for (index = 0u; index < DSPIC33_SFR_ACCESS_CONDITIONAL_COUNT; index++) {
        inspect_conditional_register(&conditional_census, &cpu,
                                     &dspic33_sfr_conditional_access_expectations[index]);
    }
    for (index = 0u; index < DSPIC33_SFR_ACCESS_DEPENDENT_NORMAL_ADDRESS_COUNT; index++) {
        inspect_dependent_normal_register(&dependent_normal_census, &cpu,
                                          &dspic33_sfr_dependent_normal_expectations[index]);
    }
    for (index = 0u; index < DSPIC33_SFR_ACCESS_PROTECTED_ADDRESS_COUNT; index++) {
        inspect_protected_register(&protected_census, &cpu,
                                   &dspic33_sfr_protected_access_expectations[index]);
    }
    map_census = inspect_sfr_map(&cpu);
    dspic33_release(&cpu);
    print_inventory(&census);
    print_side_effect_expectations();
    print_access_summary(&census);
    print_mux_summary(&mux_census);
    print_conditional_summary(&conditional_census);
    print_dependent_normal_summary(&dependent_normal_census);
    print_protected_summary(&protected_census);
    print_map_summary(&map_census);
    if (census.aliases != DSPIC33_SFR_ACCESS_ALIAS_COUNT ||
        census.mux_defaults != DSPIC33_SFR_ACCESS_MUX_DEFAULT_COUNT ||
        census.dependent_read_only_bits != DSPIC33_SFR_ACCESS_DEPENDENT_READ_ONLY_BIT_COUNT ||
        mux_census.addresses != DSPIC33_SFR_ACCESS_MUX_ALTERNATE_COUNT ||
        conditional_census.addresses != DSPIC33_SFR_ACCESS_CONDITIONAL_COUNT ||
        dependent_normal_census.addresses != DSPIC33_SFR_ACCESS_DEPENDENT_NORMAL_ADDRESS_COUNT ||
        protected_census.addresses != DSPIC33_SFR_ACCESS_PROTECTED_ADDRESS_COUNT) {
        return 2;
    }
    return census.unresolved_addresses == 0u && mux_census.unresolved_addresses == 0u &&
                   conditional_census.unresolved_addresses == 0u &&
                   dependent_normal_census.failures == 0u && map_census.failures == 0u &&
                   protected_census.failures == 0u
               ? 0
               : 1;
}
