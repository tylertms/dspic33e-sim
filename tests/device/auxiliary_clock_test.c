#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"
#include "test.h"

enum {
    AUXILIARY_CLOCK_CONTROL = 0x0758u,
    AUXILIARY_CLOCK_DIVISOR = 0x075au,
    OSCILLATOR_CONTROL = 0x0742u,
    OSCILLATOR_TUNING = 0x0748u,
    OSCILLATOR_CONFIGURATION = 0xf80008u,
    OSCILLATOR_CLOCK_LOCK = 0x0080u,
    OSCILLATOR_CONFIGURATION_CLOCK_LOCK = 0x0040u,
    AUXILIARY_PLL_ENABLE = 0x8000u,
    AUXILIARY_PLL_LOCK = 0x4000u,
    AUXILIARY_CLOCK_SOURCE_PRIMARY = 0x0400u,
    AUXILIARY_CLOCK_SOURCE_FRC = 0x0200u,
    AUXILIARY_CLOCK_OSCILLATOR_MODE = 0x1800u,
    AUXILIARY_PLL_PRESCALER = 0x0007u,
    AUXILIARY_PLL_FRC_CONFIGURATION = 0x8200u,
    AUXILIARY_CLOCK_WRITABLE = 0xbee7u,
    AUXILIARY_CLOCK_DIVISOR_WRITABLE = 0x0007u,
    AUXILIARY_CLOCK_NONWRITABLE = 0x4118u
};

static uint16_t control(Dspic33* cpu) {
    return dspic33_read_word(cpu, AUXILIARY_CLOCK_CONTROL);
}

static uint16_t configuration_from_ordinal(uint16_t ordinal) {
    uint16_t configuration = 0u;
    uint8_t bit;
    for (bit = 0u; bit < 16u; bit++) {
        uint16_t mask = (uint16_t)(1u << bit);
        if ((AUXILIARY_CLOCK_WRITABLE & mask) == 0u) {
            continue;
        }
        if ((ordinal & 1u) != 0u) {
            configuration |= mask;
        }
        ordinal >>= 1u;
    }
    return configuration;
}

static uint8_t auxiliary_pll_input(uint16_t configuration) {
    if ((configuration & AUXILIARY_CLOCK_SOURCE_FRC) != 0u) {
        return 1u;
    }
    if ((configuration & AUXILIARY_CLOCK_SOURCE_PRIMARY) != 0u) {
        return 2u;
    }
    return (uint8_t)(4u | ((configuration & AUXILIARY_CLOCK_OSCILLATOR_MODE) >> 11u));
}

static bool auxiliary_pll_input_available(const Dspic33* cpu, uint16_t configuration) {
    uint8_t input = auxiliary_pll_input(configuration);
    if (input == 1u) {
        return true;
    }
    if (input == 2u) {
        return (cpu->configuration[8u] & 0x03u) != 0x03u;
    }
    return input != 4u;
}

static void program_fosc(Dspic33* cpu, uint8_t configuration) {
    cpu->nvm.control = 0u;
    cpu->nvm.address = OSCILLATOR_CONFIGURATION;
    cpu->nvm.latches[0] = configuration;
    dspic33_complete_nvm(cpu);
}

static void access_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, control(cpu) == 0u, "POR clears ACLKCON3");
    expect(state, cpu->io.auxiliary_pll_generation == 0u,
           "POR clears auxiliary PLL generation");
    expect(state, cpu->events.count == 0u, "POR has no auxiliary PLL event");
    expect(state, dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) == 0u,
           "POR clears ACLKDIV3");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0xffffu);
    expect(state, control(cpu) == AUXILIARY_CLOCK_WRITABLE,
           "ACLKCON3 word write applies target mask");
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "APLLCK rejects software set");
    expect(state, (control(cpu) & 0x0118u) == 0u, "ACLKCON3 reserved bits read zero");
    expect(state, dspic33_read_byte(cpu, AUXILIARY_CLOCK_CONTROL + 1u) == 0xbeu,
           "ACLKCON3 high-byte read exposes implemented fields");
    expect(state, dspic33_read_byte(cpu, AUXILIARY_CLOCK_CONTROL) == 0xe7u,
           "ACLKCON3 low-byte read exposes implemented fields");

    dspic33_write_byte(cpu, AUXILIARY_CLOCK_CONTROL, 0u);
    expect(state, control(cpu) == 0xbe00u,
           "ACLKCON3 low-byte write preserves high fields");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_CONTROL, 0xffu);
    expect(state, control(cpu) == AUXILIARY_CLOCK_WRITABLE,
           "ACLKCON3 low-byte write applies low mask");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_CONTROL + 1u, 0u);
    expect(state, control(cpu) == 0x00e7u,
           "ACLKCON3 high-byte write preserves low fields");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_CONTROL + 1u, 0xffu);
    expect(state, control(cpu) == AUXILIARY_CLOCK_WRITABLE,
           "ACLKCON3 high-byte write applies high mask");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_DIVISOR, 0xffffu);
    expect(state,
           dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) ==
               AUXILIARY_CLOCK_DIVISOR_WRITABLE,
           "ACLKDIV3 word write applies target mask");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_DIVISOR + 1u, 0xffu);
    expect(state,
           dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) ==
               AUXILIARY_CLOCK_DIVISOR_WRITABLE,
           "ACLKDIV3 high-byte write rejects reserved bits");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_DIVISOR, 0u);
    expect(state, dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) == 0u,
           "ACLKDIV3 low-byte write clears multiplier");
}

static void lock_cases(TestState* state, Dspic33* cpu) {
    uint32_t generation;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_FRC_CONFIGURATION);
    expect(state, control(cpu) == AUXILIARY_PLL_FRC_CONFIGURATION,
           "enabling auxiliary PLL starts unlocked");
    expect(state, cpu->events.count == 1u, "enable schedules one lock event");
    expect(state, cpu->io.auxiliary_pll_generation == 1u,
           "enable advances lock generation");
    expect(state, dspic33_device_advance(cpu, 31u), "advance before lock deadline");
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "auxiliary PLL stays unlocked before deadline");
    expect(state, cpu->events.count == 1u, "lock event remains before deadline");
    expect(state, dspic33_device_advance(cpu, 1u), "advance to lock deadline");
    expect(state,
           control(cpu) == (AUXILIARY_PLL_FRC_CONFIGURATION | AUXILIARY_PLL_LOCK),
           "auxiliary PLL locks at modeled deadline");
    expect(state, cpu->events.count == 0u, "lock event is consumed");
    expect(state, cpu->io.auxiliary_pll_generation == 1u,
           "lock completion preserves generation");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_FRC_CONFIGURATION);
    expect(state, cpu->events.count == 0u, "same configuration does not relock");
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "same configuration preserves lock");
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL,
                       AUXILIARY_PLL_FRC_CONFIGURATION | AUXILIARY_PLL_LOCK);
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "software lock write cannot clear hardware lock");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0u);
    expect(state, control(cpu) == 0u, "disable clears auxiliary PLL lock");
    expect(state, cpu->events.count == 0u, "disable leaves no new lock event");
    expect(state, cpu->io.auxiliary_pll_generation == 2u,
           "disable invalidates lock generation");
    expect(state, dspic33_device_advance(cpu, 64u) && control(cpu) == 0u,
           "disabled auxiliary PLL cannot relock");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_FRC_CONFIGURATION);
    expect(state, control(cpu) == AUXILIARY_PLL_FRC_CONFIGURATION,
           "reenable starts a fresh unlocked interval");
    expect(state, cpu->events.count == 1u, "reenable schedules fresh lock event");
    expect(state,
           dspic33_device_advance(cpu, 32u) &&
               control(cpu) == (AUXILIARY_PLL_FRC_CONFIGURATION | AUXILIARY_PLL_LOCK),
           "reenabled auxiliary PLL reaches lock");

    generation = cpu->io.auxiliary_pll_generation;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL,
                       AUXILIARY_PLL_ENABLE | AUXILIARY_CLOCK_SOURCE_PRIMARY);
    expect(state,
           control(cpu) == (AUXILIARY_PLL_ENABLE | AUXILIARY_CLOCK_SOURCE_PRIMARY),
           "live source reconfiguration clears lock");
    expect(state, cpu->events.count == 1u, "live reconfiguration schedules relock");
    expect(state, cpu->io.auxiliary_pll_generation == generation + 1u,
           "live reconfiguration advances generation");
    expect(state,
           dspic33_device_advance(cpu, 31u) &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "reconfigured PLL remains unlocked before deadline");
    expect(state,
           dspic33_device_advance(cpu, 1u) && (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "reconfigured PLL locks at fresh deadline");
}

static void stale_event_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_FRC_CONFIGURATION);
    dspic33_device_advance(cpu, 1u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL,
                       AUXILIARY_PLL_FRC_CONFIGURATION | 0x0001u);
    expect(state, cpu->events.count == 2u,
           "reconfiguration retains stale event for generation check");
    expect(state, cpu->io.auxiliary_pll_generation == 2u,
           "reconfiguration creates second generation");
    expect(state,
           dspic33_device_advance(cpu, 31u) &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "stale completion cannot lock current generation");
    expect(state, cpu->events.count == 1u, "stale completion consumes only old event");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance to current generation event");
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "current generation completion sets lock");
    expect(state, cpu->events.count == 0u, "current completion drains lock events");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_FRC_CONFIGURATION);
    dspic33_device_advance(cpu, 1u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0u);
    expect(state, cpu->events.count == 1u, "disable retains stale queued completion");
    expect(state, dspic33_device_advance(cpu, 31u) && control(cpu) == 0u,
           "disabled generation rejects stale completion");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance after disabled stale deadline");
    expect(state, cpu->events.count == 0u, "disabled stale event is consumed");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0x0001u);
    expect(state, cpu->events.count == 0u,
           "disabled configuration change schedules no lock");
    expect(state, cpu->io.auxiliary_pll_generation == 3u,
           "disabled configuration change advances generation");
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "disabled configuration remains unlocked");
}

static void divisor_cases(TestState* state, Dspic33* cpu) {
    uint32_t generation;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0x24c2u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_DIVISOR, 0x0007u);
    expect(state,
           control(cpu) == 0x24c2u &&
               dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) == 0x0007u,
           "firmware auxiliary PLL configuration is retained");
    expect(state, cpu->events.count == 0u,
           "disabled firmware configuration schedules no lock");
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0xa4c2u);
    expect(state,
           control(cpu) == 0xa4c2u && cpu->events.count == 1u &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "firmware auxiliary PLL enable starts unlocked interval");
    expect(state, dspic33_device_advance(cpu, 32u) && control(cpu) == 0xe4c2u,
           "firmware auxiliary PLL configuration reaches lock");

    generation = cpu->io.auxiliary_pll_generation;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_DIVISOR, 0x0006u);
    expect(state,
           dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) == 0x0006u &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "live multiplier change clears auxiliary PLL lock");
    expect(state,
           cpu->io.auxiliary_pll_generation == generation + 1u &&
               cpu->events.count == 1u,
           "live multiplier change schedules fresh lock generation");
    dspic33_write_word(cpu, AUXILIARY_CLOCK_DIVISOR, 0x0006u);
    expect(state, cpu->events.count == 1u,
           "same multiplier write schedules no duplicate lock");
    expect(state,
           dspic33_device_advance(cpu, 32u) &&
               control(cpu) == (uint16_t)(0xa4c2u | AUXILIARY_PLL_LOCK),
           "changed multiplier reaches fresh lock");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0x24c2u);
    generation = cpu->io.auxiliary_pll_generation;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_DIVISOR, 0x0005u);
    expect(state,
           cpu->io.auxiliary_pll_generation == generation + 1u &&
               cpu->events.count == 0u && (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "disabled multiplier change invalidates without scheduling");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_FRC_CONFIGURATION);
    expect(state, dspic33_device_advance(cpu, 32u),
           "establish lock before divisor scheduling failure");
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_DIVISOR, 0x0001u);
    expect(state,
           dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) == 0x0001u &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u && cpu->events.count == 0u &&
               cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "multiplier lock scheduling failure leaves unlocked configuration");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_FRC_CONFIGURATION);
    dspic33_device_advance(cpu, 1u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_DIVISOR, 0x0001u);
    expect(state, cpu->events.count == 2u,
           "multiplier change retains stale lock event");
    expect(state,
           dspic33_device_advance(cpu, 31u) &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u && cpu->events.count == 1u,
           "stale multiplier generation cannot lock");
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               (control(cpu) & AUXILIARY_PLL_LOCK) != 0u && cpu->events.count == 0u,
           "current multiplier generation reaches lock");
}

static void reconfiguration_cases(TestState* state, Dspic33* cpu) {
    uint16_t configuration = AUXILIARY_PLL_FRC_CONFIGURATION;
    uint32_t generation;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
    dspic33_device_advance(cpu, 32u);
    generation = cpu->io.auxiliary_pll_generation;

    configuration |= 0x2000u;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
    expect(state,
           (control(cpu) & AUXILIARY_PLL_LOCK) != 0u &&
               cpu->io.auxiliary_pll_generation == generation &&
               cpu->events.count == 0u,
           "auxiliary divider source mux preserves PLL lock");
    configuration |= 0x0020u;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
    expect(state,
           (control(cpu) & AUXILIARY_PLL_LOCK) != 0u &&
               cpu->io.auxiliary_pll_generation == generation &&
               cpu->events.count == 0u,
           "PLL output postscaler preserves lock");
    configuration |= AUXILIARY_CLOCK_SOURCE_PRIMARY;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
    expect(state,
           (control(cpu) & AUXILIARY_PLL_LOCK) != 0u &&
               cpu->io.auxiliary_pll_generation == generation &&
               cpu->events.count == 0u,
           "primary selection beneath FRC override preserves lock");
    configuration |= 0x0800u;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
    expect(state,
           (control(cpu) & AUXILIARY_PLL_LOCK) != 0u &&
               cpu->io.auxiliary_pll_generation == generation &&
               cpu->events.count == 0u,
           "auxiliary mode beneath FRC override preserves lock");

    configuration &= (uint16_t)~AUXILIARY_CLOCK_SOURCE_FRC;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
    expect(state,
           (control(cpu) & AUXILIARY_PLL_LOCK) == 0u &&
               cpu->io.auxiliary_pll_generation == generation + 1u &&
               cpu->events.count == 1u,
           "effective FRC to primary source change relocks");
    expect(state, dspic33_device_advance(cpu, 32u),
           "primary source change reaches lock");
    generation = cpu->io.auxiliary_pll_generation;
    dspic33_write_word(cpu, OSCILLATOR_TUNING, 0x0001u);
    expect(state,
           (control(cpu) & AUXILIARY_PLL_LOCK) != 0u &&
               cpu->io.auxiliary_pll_generation == generation &&
               cpu->events.count == 0u,
           "FRC tuning beneath primary source preserves lock");
    configuration =
        (uint16_t)((configuration & ~AUXILIARY_CLOCK_OSCILLATOR_MODE) | 0x1000u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
    expect(state,
           (control(cpu) & AUXILIARY_PLL_LOCK) != 0u &&
               cpu->io.auxiliary_pll_generation == generation &&
               cpu->events.count == 0u,
           "auxiliary mode beneath primary source preserves lock");

    configuration &= (uint16_t)~AUXILIARY_CLOCK_SOURCE_PRIMARY;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) == 0u && cpu->events.count == 1u,
           "effective primary to auxiliary source change relocks");
    expect(state, dspic33_device_advance(cpu, 32u),
           "auxiliary source change reaches lock");
    generation = cpu->io.auxiliary_pll_generation;
    configuration =
        (uint16_t)((configuration & ~AUXILIARY_CLOCK_OSCILLATOR_MODE) | 0x0800u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
    expect(state,
           (control(cpu) & AUXILIARY_PLL_LOCK) == 0u &&
               cpu->io.auxiliary_pll_generation == generation + 1u &&
               cpu->events.count == 1u,
           "live auxiliary oscillator mode change relocks");
    expect(state, dspic33_device_advance(cpu, 32u),
           "changed auxiliary oscillator mode reaches lock");
    configuration &= (uint16_t)~AUXILIARY_CLOCK_OSCILLATOR_MODE;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) == 0u && cpu->events.count == 0u,
           "disabled effective auxiliary source cannot lock");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_FRC_CONFIGURATION);
    dspic33_device_advance(cpu, 32u);
    generation = cpu->io.auxiliary_pll_generation;
    dspic33_write_word(cpu, OSCILLATOR_TUNING, 0x0001u);
    expect(state,
           (control(cpu) & AUXILIARY_PLL_LOCK) == 0u &&
               cpu->io.auxiliary_pll_generation == generation + 1u &&
               cpu->events.count == 1u,
           "FRC tuning change starts fresh auxiliary PLL lock interval");
    dspic33_write_word(cpu, OSCILLATOR_TUNING, 0x0001u);
    expect(state, cpu->events.count == 1u,
           "same FRC tuning write schedules no duplicate lock");
}

static void source_admission_cases(TestState* state, Dspic33* cpu) {
    uint8_t mode;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE);
    expect(state, cpu->events.count == 0u && (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "disabled auxiliary oscillator cannot start PLL lock");
    expect(state,
           dspic33_device_advance(cpu, 32u) &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "unavailable auxiliary input remains unlocked");
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE | 0x0800u);
    expect(state, cpu->events.count == 1u,
           "configured auxiliary oscillator starts PLL lock");
    expect(state,
           dspic33_device_advance(cpu, 32u) &&
               (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "configured auxiliary oscillator reaches lock");

    dspic33_load_configuration_word(cpu, OSCILLATOR_CONFIGURATION, 0x005fu);
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL,
                       AUXILIARY_PLL_ENABLE | AUXILIARY_CLOCK_SOURCE_PRIMARY);
    expect(state, cpu->events.count == 0u && (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "disabled primary oscillator cannot start PLL lock");
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_FRC_CONFIGURATION);
    expect(state, cpu->events.count == 1u,
           "FRC override starts lock with primary oscillator disabled");
    expect(state,
           dspic33_device_advance(cpu, 32u) &&
               (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "FRC override reaches lock with primary oscillator disabled");

    for (mode = 0u; mode < 4u; mode++) {
        bool available = mode != 3u;
        dspic33_load_configuration_word(cpu, OSCILLATOR_CONFIGURATION,
                                        (uint16_t)(0x005cu | mode));
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL,
                           AUXILIARY_PLL_ENABLE | AUXILIARY_CLOCK_SOURCE_PRIMARY);
        expect(state, cpu->events.count == (available ? 1u : 0u),
               "primary oscillator mode controls PLL lock admission");
        expect(state,
               dspic33_device_advance(cpu, 32u) &&
                   ((control(cpu) & AUXILIARY_PLL_LOCK) != 0u) == available,
               "primary oscillator mode controls PLL lock completion");
    }

    dspic33_load_configuration_word(cpu, OSCILLATOR_CONFIGURATION, 0x005eu);
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL,
                       AUXILIARY_PLL_ENABLE | AUXILIARY_CLOCK_SOURCE_PRIMARY);
    dspic33_device_advance(cpu, 32u);
    program_fosc(cpu, 0x005fu);
    expect(state,
           dspic33_read_configuration_byte(cpu, OSCILLATOR_CONFIGURATION) == 0x5fu &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u && cpu->events.count == 0u,
           "RTSP primary disable immediately clears auxiliary PLL lock");
    program_fosc(cpu, 0x005eu);
    expect(state,
           dspic33_read_configuration_byte(cpu, OSCILLATOR_CONFIGURATION) == 0x5eu &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u && cpu->events.count == 1u,
           "RTSP primary enable immediately starts auxiliary PLL relock");
    expect(state,
           dspic33_device_advance(cpu, 32u) &&
               (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "RTSP-enabled primary source reaches lock");
}

static void configuration_matrix_cases(TestState* state, Dspic33* cpu) {
    uint16_t ordinal;
    for (ordinal = 0u; ordinal < 4096u; ordinal++) {
        uint16_t configuration = configuration_from_ordinal(ordinal);
        uint8_t multiplier;
        for (multiplier = 0u; multiplier < 8u; multiplier++) {
            uint16_t expected = configuration;
            dspic33_reset(cpu, 0u);
            dspic33_write_word(cpu, AUXILIARY_CLOCK_DIVISOR,
                               (uint16_t)(0xfff8u | multiplier));
            dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL,
                               (uint16_t)(configuration | AUXILIARY_CLOCK_NONWRITABLE));
            expect(state,
                   control(cpu) == configuration &&
                       dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) == multiplier,
                   "auxiliary clock configuration matrix applies exact masks");
            expect(state,
                   cpu->events.count ==
                       (((configuration & AUXILIARY_PLL_ENABLE) != 0u &&
                         auxiliary_pll_input_available(cpu, configuration))
                            ? 1u
                            : 0u),
                   "auxiliary clock matrix schedules by enable and source");
            if ((configuration & AUXILIARY_PLL_ENABLE) != 0u &&
                auxiliary_pll_input_available(cpu, configuration)) {
                expected |= AUXILIARY_PLL_LOCK;
            }
            expect(state,
                   dspic33_device_advance(cpu, 32u) && control(cpu) == expected &&
                       cpu->events.count == 0u,
                   "auxiliary clock configuration matrix completes modeled lock");
        }
    }
}

static void clock_lock_cases(TestState* state, Dspic33* cpu) {
    uint16_t ordinal;
    dspic33_load_configuration_word(cpu, OSCILLATOR_CONFIGURATION, 0x005eu);
    for (ordinal = 0u; ordinal < 4096u; ordinal++) {
        uint16_t configuration = configuration_from_ordinal(ordinal);
        uint8_t multiplier;
        for (multiplier = 0u; multiplier < 8u; multiplier++) {
            dspic33_reset(cpu, 0u);
            cpu->data[OSCILLATOR_CONTROL] = OSCILLATOR_CLOCK_LOCK;
            dspic33_write_word(cpu, AUXILIARY_CLOCK_DIVISOR, multiplier);
            dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, configuration);
            expect(state,
                   control(cpu) == 0u &&
                       dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) == 0u &&
                       cpu->io.auxiliary_pll_generation == 0u &&
                       cpu->events.count == 0u,
                   "clock lock rejects auxiliary PLL configuration matrix");
        }
    }

    dspic33_write_byte(cpu, AUXILIARY_CLOCK_CONTROL, 0xffu);
    expect(state,
           control(cpu) == 0u && cpu->io.auxiliary_pll_generation == 0u &&
               cpu->events.count == 0u,
           "clock lock rejects ACLKCON3 low-byte write");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_CONTROL + 1u, 0xffu);
    expect(state,
           control(cpu) == 0u && cpu->io.auxiliary_pll_generation == 0u &&
               cpu->events.count == 0u,
           "clock lock rejects ACLKCON3 high-byte write");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_DIVISOR, 0xffu);
    expect(state,
           dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) == 0u &&
               cpu->io.auxiliary_pll_generation == 0u && cpu->events.count == 0u,
           "clock lock rejects ACLKDIV3 low-byte write");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_DIVISOR + 1u, 0xffu);
    expect(state,
           dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) == 0u &&
               cpu->io.auxiliary_pll_generation == 0u && cpu->events.count == 0u,
           "clock lock rejects ACLKDIV3 high-byte write");

    dspic33_load_configuration_word(
        cpu, OSCILLATOR_CONFIGURATION,
        (uint16_t)(0x005eu & ~OSCILLATOR_CONFIGURATION_CLOCK_LOCK));
    dspic33_reset(cpu, 0u);
    cpu->data[OSCILLATOR_CONTROL] = OSCILLATOR_CLOCK_LOCK;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_DIVISOR, 0x0007u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0xa4c2u);
    expect(state,
           control(cpu) == 0xa4c2u &&
               dspic33_read_word(cpu, AUXILIARY_CLOCK_DIVISOR) == 0x0007u &&
               cpu->events.count == 1u,
           "disabled configuration lock permits auxiliary PLL writes");
    expect(state, dspic33_device_advance(cpu, 32u) && control(cpu) == 0xe4c2u,
           "unlocked auxiliary PLL configuration reaches lock");
    dspic33_load_configuration_word(cpu, OSCILLATOR_CONFIGURATION, 0x005eu);
}

static void lifecycle_cases(TestState* state, Dspic33* source, Dspic33* copy) {
    uint32_t generation;
    dspic33_reset(source, 0u);
    dspic33_reset(copy, 0u);
    dspic33_write_word(source, AUXILIARY_CLOCK_DIVISOR, 0x0007u);
    dspic33_write_word(source, AUXILIARY_CLOCK_CONTROL,
                       AUXILIARY_PLL_FRC_CONFIGURATION);
    dspic33_device_advance(source, 10u);
    expect(state, dspic33_copy(copy, source), "copy preserves auxiliary PLL state");
    expect(state,
           copy->io.auxiliary_pll_generation == source->io.auxiliary_pll_generation,
           "copy preserves auxiliary PLL generation");
    expect(state, copy->events.count == source->events.count,
           "copy preserves pending lock event");
    expect(state, dspic33_read_word(copy, AUXILIARY_CLOCK_DIVISOR) == 0x0007u,
           "copy preserves auxiliary PLL divisor");
    expect(state,
           dspic33_device_advance(source, 22u) &&
               (control(source) & AUXILIARY_PLL_LOCK) != 0u,
           "source completes copied lock interval");
    expect(state,
           dspic33_device_advance(copy, 22u) &&
               (control(copy) & AUXILIARY_PLL_LOCK) != 0u,
           "copy independently completes lock interval");
    dspic33_write_word(source, AUXILIARY_CLOCK_CONTROL, 0u);
    dspic33_write_word(source, AUXILIARY_CLOCK_DIVISOR, 0u);
    expect(state,
           (control(copy) & AUXILIARY_PLL_LOCK) != 0u &&
               dspic33_read_word(copy, AUXILIARY_CLOCK_DIVISOR) == 0x0007u,
           "source mutation does not alter copied auxiliary PLL state");

    dspic33_write_word(source, AUXILIARY_CLOCK_DIVISOR, 0x0006u);
    dspic33_write_word(source, AUXILIARY_CLOCK_CONTROL,
                       AUXILIARY_PLL_FRC_CONFIGURATION);
    dspic33_device_advance(source, 10u);
    expect(state, source->events.count == 1u,
           "cold reset starts with pending lock event");
    dspic33_reset(source, 0u);
    expect(state, control(source) == 0u, "cold reset clears ACLKCON3");
    expect(state, dspic33_read_word(source, AUXILIARY_CLOCK_DIVISOR) == 0u,
           "cold reset clears ACLKDIV3");
    expect(state, source->io.auxiliary_pll_generation == 0u,
           "cold reset clears lock generation");
    expect(state, source->events.count == 0u, "cold reset cancels lock events");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0u,
           "cold reset prevents stale relock");

    dspic33_write_word(source, AUXILIARY_CLOCK_DIVISOR, 0x0005u);
    dspic33_write_word(source, AUXILIARY_CLOCK_CONTROL,
                       AUXILIARY_PLL_FRC_CONFIGURATION);
    dspic33_device_advance(source, 10u);
    generation = source->io.auxiliary_pll_generation;
    dspic33_load_program_word(source, 0u, 0xfe0000u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "warm reset executes with pending auxiliary PLL lock");
    expect(state,
           control(source) == AUXILIARY_PLL_FRC_CONFIGURATION &&
               dspic33_read_word(source, AUXILIARY_CLOCK_DIVISOR) == 0x0005u &&
               source->io.auxiliary_pll_generation == generation,
           "warm reset preserves pending auxiliary PLL configuration");
    expect(state, source->events.count == 1u,
           "warm reset preserves pending auxiliary PLL lock event");
    expect(state, source->events.items[0].cycle - source->device_cycles == 21u,
           "warm reset preserves remaining auxiliary PLL lock interval");
    expect(state,
           dspic33_device_advance(source, 20u) &&
               (control(source) & AUXILIARY_PLL_LOCK) == 0u,
           "warm-reset auxiliary PLL remains unlocked before preserved deadline");
    expect(state,
           dspic33_device_advance(source, 1u) &&
               control(source) ==
                   (AUXILIARY_PLL_FRC_CONFIGURATION | AUXILIARY_PLL_LOCK),
           "warm-reset auxiliary PLL locks at preserved deadline");
    dspic33_load_program_word(source, 0u, 0xfe0000u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "warm reset executes with locked auxiliary PLL");
    expect(state,
           control(source) == (AUXILIARY_PLL_FRC_CONFIGURATION | AUXILIARY_PLL_LOCK) &&
               dspic33_read_word(source, AUXILIARY_CLOCK_DIVISOR) == 0x0005u &&
               source->io.auxiliary_pll_generation == generation &&
               source->events.count == 0u,
           "warm reset preserves locked auxiliary PLL configuration");

    dspic33_reset(source, 0u);
    source->device_cycles = UINT64_MAX;
    dspic33_write_word(source, AUXILIARY_CLOCK_CONTROL,
                       AUXILIARY_PLL_FRC_CONFIGURATION);
    expect(state, source->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "lock schedule overflow reports event error");
    expect(state, control(source) == AUXILIARY_PLL_FRC_CONFIGURATION,
           "failed schedule leaves enabled PLL unlocked");
    expect(state, source->events.count == 0u,
           "failed lock schedule leaves no pending event");
}

int main(void) {
    Dspic33 source;
    Dspic33 copy;
    TestState state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    dspic33_load_configuration_word(&source, OSCILLATOR_CONFIGURATION, 0x005eu);
    access_cases(&state, &source);
    lock_cases(&state, &source);
    stale_event_cases(&state, &source);
    divisor_cases(&state, &source);
    reconfiguration_cases(&state, &source);
    source_admission_cases(&state, &source);
    configuration_matrix_cases(&state, &source);
    clock_lock_cases(&state, &source);
    lifecycle_cases(&state, &source, &copy);
    expect(&state, state.cases == 131204u, "auxiliary clock assertion arithmetic");
    printf("[auxiliary-clock-summary] cases=%" PRIu32 " passed=%" PRIu32
           " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&copy);
    dspic33_destroy(&source);
    return state.failed == 0u ? 0 : 1;
}
