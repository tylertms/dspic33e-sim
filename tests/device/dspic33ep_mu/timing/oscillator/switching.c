#include "device/dspic33ep_mu/timing/oscillator/internal.h"

void dspic33_oscillator_test_reference_clock_cases(TestState* state, Dspic33* source,
                                                   Dspic33* copy) {
    dspic33_reset(source, 0u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0u,
           "REFOCON resets disabled");

    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0x0500u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x0500u,
           "disabled reference clock accepts divisor");
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0x8500u);
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0x8a00u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x8500u,
           "enabled reference clock preserves divisor on word write");

    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL,
                       REFERENCE_CLOCK_SLEEP | REFERENCE_CLOCK_SOURCE | 0x0300u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x3500u,
           "disable write preserves divisor while applying controls");
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0x0300u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x0300u,
           "subsequent disabled write changes divisor");
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0x8700u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x8700u,
           "disabled reference clock enables with new divisor");

    dspic33_write_byte(source, REFERENCE_CLOCK_CONTROL + 1u, 0xbau);
    expect(state,
           dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) ==
               (REFERENCE_CLOCK_ENABLE | REFERENCE_CLOCK_SLEEP | REFERENCE_CLOCK_SOURCE | 0x0700u),
           "enabled high-byte write preserves divisor and applies controls");

    dspic33_load_program_word(source, 0u, OPCODE_MOV_W0_W1);
    source->pc = 0u;
    dspic33_set_working_register(source, 0u, 0x8900u);
    dspic33_set_working_register(source, 1u, REFERENCE_CLOCK_CONTROL);
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x8700u,
           "stepped word write preserves enabled divisor");

    dspic33_set_working_register(source, 0u, 0x0900u);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x0700u,
           "stepped disable preserves enabled divisor");
    dspic33_set_working_register(source, 0u, 0x0900u);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x0900u,
           "stepped disabled write changes divisor");

    expect(state, dspic33_copy(copy, source), "copy preserves reference clock state");
    expect(state, dspic33_read_word(copy, REFERENCE_CLOCK_CONTROL) == 0x0900u,
           "copy retains reference clock divisor");
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0u,
           "warm reset clears reference clock control");
    dspic33_reset(copy, 0u);
    expect(state, dspic33_read_word(copy, REFERENCE_CLOCK_CONTROL) == 0u,
           "cold reset clears copied reference clock control");
    expect(state,
           source->events.count == 0u && !source->oscillator.active && copy->events.count == 0u &&
               !copy->oscillator.active,
           "reference clock writes create no oscillator lifecycle");
}

void dspic33_oscillator_test_reference_clock_pin_cases(TestState* state, Dspic33* source,
                                                       Dspic33* copy) {
    uint64_t edges = 0u;
    dspic33_reset(source, 0u);
    dspic33_write_word(source, 0x0680u, 49u);
    expect(state, !dspic33_reference_clock_pin(source, 64u, 0u, &edges),
           "disabled reference clock releases its PPS output");
    expect(state, dspic33_device_advance(source, 65536u), "reference clock edge domain advances");
    for (uint8_t divisor = 0u; divisor < 16u; divisor++) {
        dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, (uint16_t)(divisor << 8u));
        dspic33_write_word(source, REFERENCE_CLOCK_CONTROL,
                           (uint16_t)(REFERENCE_CLOCK_ENABLE | divisor << 8u));
        expect(state,
               dspic33_reference_clock_pin(source, 64u, 0u, &edges) &&
                   edges == (source->device_cycles * 2u >> divisor),
               "REFCLKO divides the system clock by RODIV");
        dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0u);
    }

    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL,
                       REFERENCE_CLOCK_ENABLE | REFERENCE_CLOCK_SOURCE);
    expect(state, dspic33_reference_clock_pin(source, 64u, 0x123456u, &edges) && edges == 0x123456u,
           "REFCLKO selects supplied primary oscillator edges");

    source->power_state = DSPIC33_POWER_SLEEP;
    expect(state, !dspic33_reference_clock_pin(source, 64u, 0x123456u, &edges),
           "REFCLKO stops in Sleep when ROSSLP is clear");
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL,
                       REFERENCE_CLOCK_ENABLE | REFERENCE_CLOCK_SLEEP | REFERENCE_CLOCK_SOURCE);
    expect(state, dspic33_reference_clock_pin(source, 64u, 0x123456u, &edges) && edges == 0x123456u,
           "REFCLKO continues in Sleep when ROSSLP is set");
    source->power_state = DSPIC33_POWER_ACTIVE;

    dspic33_write_word(source, 0x0680u, (uint16_t)(49u << 8u));
    expect(state,
           !dspic33_reference_clock_pin(source, 64u, 0x123456u, &edges) &&
               dspic33_reference_clock_pin(source, 65u, 0x123456u, &edges),
           "REFCLKO follows PPS remapping");
    expect(state,
           dspic33_copy(copy, source) &&
               dspic33_reference_clock_pin(copy, 65u, 0x654321u, &edges) && edges == 0x654321u,
           "copy preserves REFCLKO source, divisor and mapping");
    expect(state,
           !dspic33_reference_clock_pin(source, 63u, 0u, &edges) &&
               !dspic33_reference_clock_pin(source, 65u, 0u, NULL),
           "REFCLKO observation rejects invalid outputs");

    dspic33_write_word(source, 0x0766u, 0x0008u);
    expect(state, dspic33_reference_clock_pin(source, 65u, 0x123456u, &edges),
           "REFOMD disable retains one-cycle output window");
    expect(state, dspic33_device_advance(source, 1u), "advance REFOMD disable boundary");
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0u);
    expect(state,
           !dspic33_reference_clock_pin(source, 65u, 0x123456u, &edges) &&
               dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0u,
           "REFOMD disables output and register access");
    dspic33_write_word(source, 0x0766u, 0u);
    expect(state, !dspic33_reference_clock_pin(source, 65u, 0x123456u, &edges),
           "REFOMD enable retains one-cycle disabled window");
    expect(state, dspic33_device_advance(source, 1u), "advance REFOMD enable boundary");
    expect(state,
           dspic33_reference_clock_pin(source, 65u, 0x123456u, &edges) &&
               dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) ==
                   (REFERENCE_CLOCK_ENABLE | REFERENCE_CLOCK_SLEEP | REFERENCE_CLOCK_SOURCE),
           "REFOMD enable restores preserved reference clock state");
}

void dspic33_oscillator_test_main_pll_configuration_cases(TestState* state, Dspic33* source,
                                                          Dspic33* copy) {
    uint32_t generation;
    uint64_t deadline;

    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005eu);
    dspic33_reset(source, 0u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_CLOCK_LOCK);
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0xffffu);
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0xffffu);
    dspic33_write_word(source, MAIN_OSCILLATOR_TUNING, 0xffffu);
    expect(state,
           dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x3040u &&
               dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x0030u &&
               dspic33_read_word(source, MAIN_OSCILLATOR_TUNING) == 0u,
           "effective CLKLOCK rejects main PLL word writes");
    dspic33_write_byte(source, MAIN_CLOCK_DIVISOR, 0x1fu);
    dspic33_write_byte(source, MAIN_CLOCK_DIVISOR + 1u, 0x07u);
    dspic33_write_byte(source, MAIN_PLL_FEEDBACK, 0xffu);
    dspic33_write_byte(source, MAIN_PLL_FEEDBACK + 1u, 0x01u);
    dspic33_write_byte(source, MAIN_OSCILLATOR_TUNING, 0x3fu);
    expect(state,
           dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x3040u &&
               dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x0030u &&
               dspic33_read_word(source, MAIN_OSCILLATOR_TUNING) == 0u,
           "effective CLKLOCK rejects main PLL byte writes");
    expect(state, source->oscillator.generation == generation && source->events.count == 0u,
           "rejected main PLL writes preserve oscillator lifecycle");

    expect(state, dspic33_oscillator_test_program_fosc(source, 0x001eu),
           "FCKSM zero-zero immediately unlocks clock configuration");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x071fu);
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x01ffu);
    dspic33_write_word(source, MAIN_OSCILLATOR_TUNING, 0x003fu);
    expect(state,
           dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x071fu &&
               dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x01ffu &&
               dspic33_read_word(source, MAIN_OSCILLATOR_TUNING) == 0x003fu,
           "FCKSM zero-zero permits clock configuration with CLKLOCK set");
    expect(state,
           source->oscillator.generation == generation && !source->oscillator.active &&
               !source->oscillator.lock_pending && source->events.count == 0u,
           "non-PLL configuration writes create no relock lifecycle");
    expect(state, dspic33_oscillator_test_program_fosc(source, 0x00deu),
           "FCKSM one-one immediately locks clock configuration");
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0030u);
    expect(state, dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x01ffu,
           "FCKSM one-one enforces CLKLOCK");
    expect(state, dspic33_oscillator_test_program_fosc(source, 0x009eu),
           "FCKSM one-zero immediately unlocks clock configuration");
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0030u);
    expect(state, dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x0030u,
           "FCKSM one-zero permits clock configuration with CLKLOCK set");

    expect(state, dspic33_oscillator_test_select_locked_main_pll(source, 1u),
           "select stable FRCPLL for relock cases");
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x1100u && source->oscillator.lock_pending &&
               !source->oscillator.active && source->oscillator.generation == generation + 1u &&
               source->events.count == 1u && source->events.items[0].source == 2u,
           "FRCPLL feedback change restarts lock");
    expect(state,
           dspic33_device_advance(source, 31u) &&
               dspic33_oscillator_test_control(source) == 0x1100u,
           "FRCPLL feedback remains unlocked before new deadline");
    expect(state,
           dspic33_device_advance(source, 1u) && dspic33_oscillator_test_control(source) == 0x1120u,
           "FRCPLL feedback relocks at new deadline");

    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3041u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x1100u && source->oscillator.lock_pending,
           "FRCPLL prescaler change restarts lock");
    expect(state,
           dspic33_device_advance(source, 32u) &&
               dspic33_oscillator_test_control(source) == 0x1120u,
           "FRCPLL prescaler relocks");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3141u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x1100u && source->oscillator.lock_pending,
           "FRCPLL input divider change restarts lock");
    expect(state,
           dspic33_device_advance(source, 32u) &&
               dspic33_oscillator_test_control(source) == 0x1120u,
           "FRCPLL input divider relocks");
    dspic33_write_word(source, MAIN_OSCILLATOR_TUNING, 0x0001u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x1100u && source->oscillator.lock_pending,
           "FRCPLL tuning change restarts lock");
    expect(state,
           dspic33_device_advance(source, 32u) &&
               dspic33_oscillator_test_control(source) == 0x1120u,
           "FRCPLL tuning relocks");

    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3101u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x1120u &&
               source->oscillator.generation == generation && source->events.count == 0u,
           "FRCPLL postscaler change preserves lock");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x0101u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x1120u &&
               source->oscillator.generation == generation && source->events.count == 0u,
           "FRCPLL DOZE field change preserves PLL lock");

    expect(state, dspic33_oscillator_test_select_locked_main_pll(source, 3u),
           "select stable POSCPLL for relock cases");
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x3300u && source->oscillator.lock_pending,
           "POSCPLL feedback change restarts lock");
    expect(state,
           dspic33_device_advance(source, 32u) &&
               dspic33_oscillator_test_control(source) == 0x3320u,
           "POSCPLL feedback relocks");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3041u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x3300u && source->oscillator.lock_pending,
           "POSCPLL prescaler change restarts lock");
    expect(state,
           dspic33_device_advance(source, 32u) &&
               dspic33_oscillator_test_control(source) == 0x3320u,
           "POSCPLL prescaler relocks");
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3141u);
    dspic33_write_word(source, MAIN_OSCILLATOR_TUNING, 0x0001u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3101u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x3320u &&
               source->oscillator.generation == generation && source->events.count == 0u,
           "POSCPLL irrelevant fields preserve lock");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x00fbu);
    dspic33_reset(source, 0u);
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x0300u && source->oscillator.active &&
               source->oscillator.automatic && !source->oscillator.source_ready &&
               source->oscillator.generation == generation + 1u,
           "pre-source PLL change restarts switch generation");
    expect(state,
           dspic33_device_advance(source, 1u) && source->oscillator.source_ready &&
               source->oscillator.lock_pending,
           "restarted PLL switch reaches source-ready phase");
    expect(state,
           dspic33_device_advance(source, 31u) &&
               dspic33_oscillator_test_control(source) == 0x3320u,
           "restarted PLL switch locks and transfers");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_reset(source, 0u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 3u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL,
                                                 OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 1u);
    generation = source->oscillator.generation;
    deadline = source->events.items[0].cycle;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           source->oscillator.active && source->oscillator.source_ready &&
               source->oscillator.lock_pending &&
               source->oscillator.generation == generation + 1u &&
               source->events.items[source->events.count - 1u].cycle ==
                   source->device_cycles + OSCILLATOR_SWITCH_DELAY &&
               source->events.items[0].cycle == deadline,
           "source-ready PLL change replaces lock deadline and leaves stale event");
    expect(state,
           dspic33_device_advance(source, 31u) &&
               dspic33_oscillator_test_control(source) == 0x0301u,
           "stale source-ready lock event cannot transfer");
    expect(state,
           dspic33_device_advance(source, 1u) && dspic33_oscillator_test_control(source) == 0x3320u,
           "replacement source-ready lock event transfers");

    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00dfu);
    dspic33_reset(source, 0u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 1u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL,
                                                 OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 1u);
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_OSCILLATOR_TUNING, 0x0001u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x1100u && !source->oscillator.active &&
               source->oscillator.lock_pending && source->oscillator.generation == generation + 1u,
           "PLLK zero post-transfer change replaces lock deadline");
    expect(state,
           dspic33_device_advance(source, 31u) &&
               dspic33_oscillator_test_control(source) == 0x1100u,
           "stale PLLK zero lock event cannot lock");
    expect(state,
           dspic33_device_advance(source, 1u) && dspic33_oscillator_test_control(source) == 0x1120u,
           "replacement PLLK zero event locks");

    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00ffu);
    expect(state, dspic33_oscillator_test_select_locked_main_pll(source, 1u),
           "select FRCPLL for prohibited direct transition");
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 3u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL,
                                                 OSCILLATOR_SWITCH_ENABLE);
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x1321u && source->oscillator.active &&
               source->oscillator.generation == generation && source->events.count == 0u,
           "PLL register write cannot complete prohibited direct transition");
    expect(state,
           dspic33_device_advance(source, 32u) &&
               dspic33_oscillator_test_control(source) == 0x1321u,
           "prohibited direct transition remains pending after PLL write");

    expect(state, dspic33_oscillator_test_select_locked_main_pll(source, 1u),
           "select FRCPLL for switch-away configuration change");
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0030u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x1120u &&
               source->oscillator.generation == generation && source->events.count == 0u,
           "unchanged PLL configuration preserves lock lifecycle");
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 2u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL,
                                                 OSCILLATOR_SWITCH_ENABLE);
    generation = source->oscillator.generation;
    deadline = source->events.items[0].cycle;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0x1201u && source->oscillator.active &&
               source->oscillator.generation == generation && source->events.count == 1u &&
               source->events.items[0].cycle == deadline,
           "PLL change while switching away preserves destination readiness");
    expect(state,
           dspic33_device_advance(source, 31u) &&
               dspic33_oscillator_test_control(source) == 0x2200u,
           "switch away from PLL completes after configuration change");

    expect(state, dspic33_oscillator_test_select_locked_main_pll(source, 1u),
           "select FRCPLL for relock lifecycle");
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    dspic33_device_advance(source, 10u);
    expect(state, dspic33_copy(copy, source), "copy preserves main PLL relock");
    expect(state,
           copy->oscillator.lock_pending && copy->events.count == 1u &&
               dspic33_device_advance(source, 22u) &&
               dspic33_oscillator_test_control(source) == 0x1120u &&
               dspic33_device_advance(copy, 22u) &&
               dspic33_oscillator_test_control(copy) == 0x1120u,
           "copied main PLL relocks complete independently");

    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0032u);
    dspic33_device_advance(source, 10u);
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->oscillator.lock_pending &&
               source->events.count == 1u && dspic33_oscillator_test_control(source) == 0x1100u,
           "warm reset reconstructs main PLL relock");
    expect(state,
           dspic33_device_advance(source, 21u) &&
               dspic33_oscillator_test_control(source) == 0x1120u,
           "warm-reset main PLL relock keeps remaining deadline");

    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0033u);
    dspic33_reset(source, 0u);
    expect(state,
           dspic33_oscillator_test_control(source) == 0u && !source->oscillator.lock_pending &&
               source->events.count == 0u &&
               dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x0030u,
           "cold reset cancels relock and restores PLL configuration");

    expect(state, dspic33_oscillator_test_select_locked_main_pll(source, 1u),
           "select FRCPLL for relock schedule failure");
    source->device_cycles = UINT64_MAX;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           source->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               dspic33_oscillator_test_control(source) == 0x1100u &&
               source->oscillator.lock_pending && source->events.count == 0u,
           "main PLL relock schedule failure is explicit and deterministic");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005eu);
    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00ffu);
}

static void configure_doze_interrupt(Dspic33* cpu, uint8_t priority) {
    dspic33_write_word(cpu, 0x0820u, 1u);
    dspic33_write_word(cpu, 0x0840u, priority);
    dspic33_load_program_word(cpu, 0x0014u, 0x0300u);
    dspic33_load_program_word(cpu, 0x0300u, OPCODE_NOP);
    dspic33_set_working_register(cpu, 15u, 0x1800u);
    dspic33_raise_interrupt(cpu, 0u);
}

void dspic33_oscillator_test_doze_cases(TestState* state, Dspic33* source, Dspic33* copy) {
    uint64_t cpu_cycles;
    uint64_t device_cycles;
    uint8_t divisor;
    Dspic33StopReason trap_result;

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x7800u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x7800u,
           "disabled DOZE field and DOZEN set together");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x1800u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x7800u,
           "enabled DOZE field ignores writes");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x1000u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x7000u,
           "combined disable and divisor write preserves enabled divisor");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x1000u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x1000u,
           "disabled DOZE field accepts a subsequent write");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_DOZE_ENABLE);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0u,
           "DOZEN cannot set at the one-to-one ratio");
    dspic33_write_byte(source, MAIN_CLOCK_DIVISOR + 1u, 0x38u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x3800u,
           "high-byte write sets a disabled DOZE ratio");
    dspic33_write_byte(source, MAIN_CLOCK_DIVISOR + 1u, 0x18u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x3800u,
           "high-byte write preserves an enabled DOZE field");

    for (divisor = 1u; divisor <= 7u; divisor++) {
        uint64_t ratio = UINT64_C(1) << divisor;
        dspic33_reset(source, 0u);
        dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                           (uint16_t)((uint16_t)divisor << 12u) | MAIN_CLOCK_DOZE_ENABLE);
        dspic33_load_program_word(source, 0u, OPCODE_NOP);
        source->pc = 0u;
        cpu_cycles = source->cycles;
        device_cycles = source->device_cycles;
        expect(state,
               dspic33_step(source) == DSPIC33_RUNNING && source->cycles - cpu_cycles == 1u &&
                   source->device_cycles - device_cycles == ratio,
               "DOZE ratio scales stepped peripheral cycles");
    }

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x2800u);
    dspic33_load_program_word(source, 0u, OPCODE_GOTO_W0);
    dspic33_load_program_word(source, 0x0100u, OPCODE_NOP);
    dspic33_set_working_register(source, 0u, 0x0100u);
    dspic33_schedule(source, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 15u);
    source->pc = 0u;
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->pc == 0x0100u &&
               source->cycles - cpu_cycles == 4u && source->device_cycles - device_cycles == 16u &&
               (dspic33_read_word(source, 0x0800u) & 1u) != 0u && source->events.count == 0u,
           "DOZE scales multi-cycle instructions across device events");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x2800u);
    dspic33_load_program_word(source, 0u, OPCODE_MOV_IFS0_W2);
    dspic33_write_word(source, 0x0800u, 0x1234u);
    source->pc = 0u;
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->w[2] == 0x1234u &&
               source->cycles - cpu_cycles == 2u && source->device_cycles - device_cycles == 8u,
           "DOZE scales the separated non-CPU SFR wait cycle");

    dspic33_reset(source, 0x200u);
    dspic33_load_program_word(source, 0x200u, OPCODE_MOV_W0_W1);
    dspic33_load_program_word(source, 0x202u, OPCODE_NOP);
    dspic33_set_working_register(source, 0u, 0x1800u);
    dspic33_set_working_register(source, 1u, MAIN_CLOCK_DIVISOR);
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x1800u &&
               source->cycles - cpu_cycles == 1u && source->device_cycles - device_cycles == 1u,
           "DOZEN setting instruction uses the previous ratio");
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->device_cycles - device_cycles == 2u,
           "instruction after DOZEN setting uses the new ratio");
    source->pc = 0x200u;
    dspic33_set_working_register(source, 0u, 0x1000u);
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x1000u &&
               source->device_cycles - device_cycles == 2u,
           "DOZEN clearing instruction uses the previous ratio");
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->device_cycles - device_cycles == 1u,
           "instruction after DOZEN clearing returns to one-to-one");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3800u);
    dspic33_write_word(source, TIMER1_COUNTER, 0u);
    dspic33_write_word(source, TIMER1_PERIOD, 0xffffu);
    dspic33_write_word(source, TIMER1_CONTROL, 0x8000u);
    dspic33_load_program_word(source, 0u, OPCODE_NOP);
    source->pc = 0u;
    source->disicnt = 5u;
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->cycles - cpu_cycles == 1u &&
               source->device_cycles - device_cycles == 8u && source->disicnt == 4u &&
               dspic33_read_word(source, TIMER1_COUNTER) == 8u,
           "DOZE keeps CPU state slow while peripherals run at full speed");
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_device_advance(source, 1u) && source->cycles - cpu_cycles == 1u &&
               source->device_cycles - device_cycles == 1u &&
               dspic33_read_word(source, TIMER1_COUNTER) == 9u,
           "public device advance remains an equal-domain stimulus");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3800u);
    dspic33_write_word(source, CRC_PMD_ADDRESS, CRC_PMD);
    expect(state,
           source->events.count == 1u &&
               source->events.items[0].cycle - source->device_cycles == 8u &&
               !source->io.crc.pmd_disabled,
           "DOZE scales one-instruction PMD transitions");
    expect(state, dspic33_device_advance(source, 7u) && !source->io.crc.pmd_disabled,
           "scaled PMD transition remains pending before its deadline");
    expect(state, dspic33_device_advance(source, 1u) && source->io.crc.pmd_disabled,
           "scaled PMD transition completes at one divided instruction");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3800u);
    dspic33_load_program_word(source, 0u, OPCODE_MOV_W0_W1);
    dspic33_set_working_register(source, 0u, CRC_PMD);
    dspic33_set_working_register(source, 1u, CRC_PMD_ADDRESS);
    source->pc = 0u;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->io.crc.pmd_disabled &&
               source->device_cycles - device_cycles == 8u && source->events.count == 0u,
           "stepped PMD write completes after one divided instruction");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->pc = 0u;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->last_interrupt == 0u &&
               source->pc == 0x0302u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) & MAIN_CLOCK_DOZE_ENABLE) != 0u &&
               source->device_cycles - device_cycles == 20u,
           "interrupt preserves DOZE when ROI is clear");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->pc = 0u;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->last_interrupt == 0u &&
               source->pc == 0x0302u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) & MAIN_CLOCK_DOZE_ENABLE) == 0u &&
               source->device_cycles - device_cycles == 10u,
           "ROI clears DOZEN before the first handler instruction");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->sr = 0x0020u;
    source->power_state = DSPIC33_POWER_IDLE;
    expect(state,
           dspic33_device_wake(source) && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) & MAIN_CLOCK_DOZE_ENABLE) == 0u &&
               (dspic33_read_word(source, 0x0800u) & 1u) != 0u,
           "ROI clears DOZEN on a wake-only interrupt");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->sr = 0x0020u;
    source->power_state = DSPIC33_POWER_IDLE;
    source->pc = 0u;
    dspic33_load_program_word(source, 0u, OPCODE_NOP);
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->power_state == DSPIC33_POWER_ACTIVE &&
               source->interrupt_count == 0u && source->pc == 2u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) & MAIN_CLOCK_DOZE_ENABLE) == 0u &&
               source->device_cycles - device_cycles == 1u,
           "wake-only ROI resumes stepped execution at one-to-one");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    dspic33_load_program_word(source, 0u, OPCODE_NOP);
    source->pc = 0u;
    dspic33_raise_interrupt(source, 0u);
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) & MAIN_CLOCK_DOZE_ENABLE) != 0u &&
               source->device_cycles - device_cycles == 2u,
           "masked interrupt does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 0u);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) & MAIN_CLOCK_DOZE_ENABLE) != 0u,
           "priority-zero interrupt does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->pc = 0u;
    source->sr = 0x0020u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) & MAIN_CLOCK_DOZE_ENABLE) != 0u,
           "IPL-blocked interrupt does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->pc = 0u;
    source->interrupt_deferred[0] = 1u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) & MAIN_CLOCK_DOZE_ENABLE) != 0u,
           "deferred interrupt does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    dspic33_write_word(source, 0x08c2u, 0u);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) & MAIN_CLOCK_DOZE_ENABLE) != 0u,
           "GIE-disabled interrupt does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    dspic33_load_program_word(source, 0x0008u, 0x0300u);
    dspic33_load_program_word(source, 0x0300u, OPCODE_NOP);
    dspic33_set_working_register(source, 15u, 0x1800u);
    dspic33_write_word(source, 0x08c6u, 1u);
    source->pc = 0u;
    device_cycles = source->device_cycles;
    trap_result = dspic33_step(source);
    expect(state,
           trap_result == DSPIC33_TRAPPED && source->last_trap == 2u && source->pc == 0x0302u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) & MAIN_CLOCK_DOZE_ENABLE) != 0u &&
               source->device_cycles - device_cycles == 2u,
           "trap entry does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3800u);
    expect(state, dspic33_copy(copy, source), "copy preserves DOZE state");
    expect(state, dspic33_read_word(copy, MAIN_CLOCK_DIVISOR) == 0x3800u,
           "copy retains the selected DOZE ratio");
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x3800u &&
               source->device_cycles - device_cycles == 8u,
           "warm reset preserves DOZE and finishes at the old ratio");
    dspic33_reset(copy, 0u);
    expect(state, dspic33_read_word(copy, MAIN_CLOCK_DIVISOR) == 0x3040u,
           "POR restores the configured clock divisor");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x7800u);
    dspic33_load_program_word(source, 0u, OPCODE_NOP);
    source->pc = 0u;
    source->device_cycles = UINT64_MAX - 100u;
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_EVENT_QUEUE_ERROR && source->cycles == cpu_cycles &&
               source->device_cycles == device_cycles,
           "DOZE device-cycle overflow reports an event error without clock advance");
}
