#include "output_compare_test_support.h"

static void access_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        expect(state, dspic33_read_word(cpu, base) == 0u, "OCCON1 reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0x000cu,
               "OCCON2 reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u,
               "OCRS deterministic reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0u,
               "OCR deterministic reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "OCTMR deterministic reset");
        dspic33_write_word(cpu, base, UINT16_MAX);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), UINT16_MAX);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), UINT16_MAX);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), UINT16_MAX);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), UINT16_MAX);
        expect(state, dspic33_read_word(cpu, base) == 0x3fffu, "OCCON1 access mask");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0xf1ffu,
               "OCCON2 access mask");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == UINT16_MAX,
               "OCRS writable");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == UINT16_MAX,
               "OCR writable");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "OCTMR read only");
    }
}

static void waveform_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        dspic33_reset(cpu, 0u);
        configure_compare(cpu, channel, 4u, 2u);
        expect(state, output_is(cpu, channel, true), "PWM starts high at timer zero");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "PWM timer starts at zero");
        expect(state, dspic33_device_advance(cpu, 1u), "advance before duty match");
        expect(state,
               output_is(cpu, channel, true) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 1u,
               "PWM remains high before duty match");
        expect(state, dspic33_device_advance(cpu, 1u), "advance duty match");
        expect(state,
               output_is(cpu, channel, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                   !interrupt_flag(cpu, channel),
               "duty match lowers output without interrupt");
        expect(state, dspic33_device_advance(cpu, 2u), "advance through period value");
        expect(state,
               output_is(cpu, channel, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 4u &&
                   !interrupt_flag(cpu, channel),
               "period value is the final low timer cycle");
        expect(state, dspic33_device_advance(cpu, 1u), "advance self synchronization");
        expect(state,
               output_is(cpu, channel, true) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   interrupt_flag(cpu, channel),
               "RS plus one resets timer raises output and interrupt");
    }
}

static void constant_output_case(TestState* state, Dspic33* cpu, uint16_t period,
                                 uint16_t duty, bool high, const char* name) {
    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, period, duty);
    expect(state, output_is(cpu, 0u, high), name);
    expect(state, dspic33_device_advance(cpu, period),
           "advance constant PWM period value");
    expect(state,
           output_is(cpu, 0u, high) && dspic33_read_word(cpu, 0x0908u) == period &&
               !interrupt_flag(cpu, 0u),
           "constant PWM holds through period value");
    expect(state, dspic33_device_advance(cpu, 1u), "advance constant PWM rollover");
    expect(state,
           output_is(cpu, 0u, high) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               interrupt_flag(cpu, 0u),
           "constant PWM rolls over at RS plus one");
}

static void boundary_cases(TestState* state, Dspic33* cpu) {
    constant_output_case(state, cpu, 4u, 0u, false, "zero duty starts low");
    constant_output_case(state, cpu, 4u, 4u, true, "equal duty stays high");
    constant_output_case(state, cpu, 4u, 5u, true, "greater duty stays high");
    constant_output_case(state, cpu, 0u, 0u, false, "zero period zero duty starts low");
    constant_output_case(state, cpu, 0u, 1u, true,
                         "zero period nonzero duty starts high");
}

static void buffering_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance before buffered writes");
    dspic33_write_word(cpu, 0x0904u, 6u);
    dspic33_write_word(cpu, 0x0906u, 3u);
    expect(state,
           cpu->io.output_compare.active_rs[0] == 4u &&
               cpu->io.output_compare.active_r[0] == 2u &&
               dspic33_read_word(cpu, 0x0904u) == 6u &&
               dspic33_read_word(cpu, 0x0906u) == 3u,
           "R and RS writes remain buffered");
    expect(state, dspic33_device_advance(cpu, 1u), "advance old buffered duty");
    expect(state, output_is(cpu, 0u, false) && dspic33_read_word(cpu, 0x0908u) == 2u,
           "old duty controls current period");
    expect(state, dspic33_device_advance(cpu, 3u), "advance old buffered period");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               cpu->io.output_compare.active_rs[0] == 6u &&
               cpu->io.output_compare.active_r[0] == 3u,
           "period boundary loads both buffers");
    clear_interrupt(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 3u), "advance newly buffered duty");
    expect(state,
           output_is(cpu, 0u, false) && dspic33_read_word(cpu, 0x0908u) == 3u &&
               !interrupt_flag(cpu, 0u),
           "new duty controls next period");
    expect(state, dspic33_device_advance(cpu, 4u), "advance newly buffered period");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               interrupt_flag(cpu, 0u),
           "new period rolls over at new RS plus one");
}

static void free_running_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        dspic33_reset(cpu, 0u);
        configure_compare_source(cpu, channel, 4u, 2u, COMPARE_NO_SYNC);
        expect(state,
               output_is(cpu, channel, true) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "free-running PWM starts at timer zero");
        expect(state, dspic33_device_advance(cpu, 5u),
               "advance free-running PWM beyond RS");
        expect(state,
               output_is(cpu, channel, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 5u &&
                   !interrupt_flag(cpu, channel),
               "free-running PWM ignores RS after duty match");
        expect(state, dspic33_device_advance(cpu, UINT16_MAX - 5u),
               "advance free-running PWM to timer maximum");
        expect(state,
               output_is(cpu, channel, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == UINT16_MAX &&
                   !interrupt_flag(cpu, channel),
               "free-running PWM holds low through timer maximum");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance free-running PWM rollover");
        expect(state,
               output_is(cpu, channel, true) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   interrupt_flag(cpu, channel),
               "free-running rollover starts a new PWM cycle");
    }
}

static void instruction_transition_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t program[] = {
        0xef2900u, 0xef2902u, 0x200040u, 0x884820u, 0x200020u, 0x884830u,
        0x21c060u, 0x884800u, 0x000000u, 0x2001f0u, 0x884810u, 0x000000u,
        0x000000u, 0x200000u, 0x884810u, 0x000000u, 0x000000u, 0x000000u,
    };
    bool loaded = true;
    bool ran = true;
    size_t index;
    dspic33_reset(cpu, 0x200u);
    for (index = 0u; index < sizeof(program) / sizeof(program[0]); index++) {
        loaded = loaded && dspic33_load_program_word(
                               cpu, 0x200u + (uint32_t)(index * 2u), program[index]);
    }
    expect(state, loaded, "load exact OC synchronization transition sequence");
    for (index = 0u; index < 7u; index++) {
        ran = ran && dspic33_step(cpu) == DSPIC33_RUNNING;
    }
    expect(state, ran, "execute OC synchronization setup sequence");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 0u && output_is(cpu, 0u, true),
           "OC enable takes effect after its instruction cycle");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 1u && output_is(cpu, 0u, true),
           "SYNCSEL zero advances on the next instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 2u && output_is(cpu, 0u, false),
           "SYNCSEL literal instruction preserves free-running phase");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0902u) == COMPARE_SELF_SYNC &&
               dspic33_read_word(cpu, 0x0908u) == 3u,
           "zero to self synchronization preserves timer phase");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 4u && !interrupt_flag(cpu, 0u),
           "self synchronization waits through the RS timer value");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 0u && output_is(cpu, 0u, true) &&
               interrupt_flag(cpu, 0u),
           "self synchronization resets on the next increment");
    clear_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 1u,
           "no-sync literal instruction advances self-running timer");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0902u) == COMPARE_NO_SYNC &&
               dspic33_read_word(cpu, 0x0908u) == 2u && output_is(cpu, 0u, false),
           "self to zero synchronization preserves timer phase");
    ran = true;
    for (index = 0u; index < 3u; index++) {
        ran = ran && dspic33_step(cpu) == DSPIC33_RUNNING;
    }
    expect(state,
           ran && dspic33_read_word(cpu, 0x0908u) == 5u && output_is(cpu, 0u, false) &&
               !interrupt_flag(cpu, 0u),
           "removed self synchronization advances beyond RS");
}

static void free_running_buffer_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_source(cpu, 0u, 4u, 2u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, 5u),
           "advance before free-running buffered writes");
    dspic33_write_word(cpu, 0x0904u, 6u);
    dspic33_write_word(cpu, 0x0906u, 3u);
    expect(state,
           cpu->io.output_compare.active_rs[0] == 4u &&
               cpu->io.output_compare.active_r[0] == 2u,
           "free-running compare writes remain buffered");
    expect(state, dspic33_device_advance(cpu, UINT16_MAX - 4u),
           "advance free-running buffered rollover");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 0u && output_is(cpu, 0u, true) &&
               cpu->io.output_compare.active_rs[0] == 6u &&
               cpu->io.output_compare.active_r[0] == 3u && interrupt_flag(cpu, 0u),
           "free-running rollover loads both compare buffers");
    clear_interrupt(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 3u), "advance new free-running duty");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 3u && output_is(cpu, 0u, false) &&
               !interrupt_flag(cpu, 0u),
           "new free-running duty controls the next cycle");
}

static void pps_case(TestState* state, Dspic33* cpu, uint8_t channel, uint8_t pin,
                     uint16_t address, uint8_t shift, uint8_t function) {
    uint16_t mapping = (uint16_t)(function << shift);
    dspic33_reset(cpu, 0u);
    configure_compare(cpu, channel, 3u, 1u);
    dspic33_write_word(cpu, address, mapping);
    expect(state, pin_is(cpu, pin, true), "mapped PPS output starts high");
    expect(state, !dspic33_output_compare_pin(cpu, (uint8_t)(pin + 1u), NULL),
           "PPS output rejects null destination");
    {
        bool high;
        expect(state, !dspic33_output_compare_pin(cpu, (uint8_t)(pin + 1u), &high),
               "unmapped PPS pin is rejected");
    }
    expect(state, dspic33_device_advance(cpu, 1u), "advance mapped PPS duty");
    expect(state, pin_is(cpu, pin, false), "mapped PPS output follows duty transition");
    expect(state, dspic33_device_advance(cpu, 3u), "advance mapped PPS rollover");
    expect(state, pin_is(cpu, pin, true), "mapped PPS output follows rollover");
    dspic33_write_word(cpu, (uint16_t)(compare_base(channel) + 2u), 0x003fu);
    {
        bool high;
        expect(state, !dspic33_output_compare_pin(cpu, pin, &high),
               "OCTRIS disconnects PPS output");
    }
}

static void pps_cases(TestState* state, Dspic33* cpu) {
    pps_case(state, cpu, 0u, 109u, 0x0698u, 0u, 0x10u);
    pps_case(state, cpu, 1u, 65u, 0x0680u, 8u, 0x11u);
    pps_case(state, cpu, 4u, 108u, 0x0696u, 8u, 0x14u);
    pps_case(state, cpu, 15u, 64u, 0x0680u, 0u, 0x2cu);
}

static void interrupt_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        dspic33_reset(cpu, 0u);
        configure_interrupt(cpu, channel);
        configure_compare(cpu, channel, 0u, 0u);
        expect(state, !dspic33_device_interrupt_pending(cpu),
               "OC interrupt is not pending before rollover");
        expect(state, dspic33_device_advance(cpu, 1u), "advance OC interrupt rollover");
        expect(state,
               interrupt_flag(cpu, channel) && dspic33_device_interrupt_pending(cpu),
               "OC interrupt becomes pending at rollover");
        expect(state,
               dspic33_device_service_interrupt(cpu) &&
                   cpu->last_interrupt == compare_irqs[channel] &&
                   cpu->pc == COMPARE_VECTOR,
               "OC channel uses documented interrupt vector");
    }
}

static void dma_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < 4u; channel++) {
        uint16_t base = compare_base(channel);
        uint16_t value = (uint16_t)(0x5100u + channel);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, COMPARE_DMA_MEMORY, value);
        configure_compare_dma(cpu, compare_irqs[channel], COMPARE_DMA_MEMORY,
                              (uint16_t)(base + 4u));
        configure_compare(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 4u)) == value &&
                   (cpu->io.dma_active & 1u) != 0u,
               "OC1 through OC4 compare interrupts request DMA");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   (dspic33_read_word(cpu, 0x0800u) & 0x0010u) != 0u,
               "output compare DMA transfer completes through DMA0IF");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, COMPARE_DMA_MEMORY, value);
        configure_compare_dma(cpu, compare_irqs[channel], COMPARE_DMA_MEMORY,
                              (uint16_t)(base + 4u));
        configure_fault_compare(cpu, channel, 6u, 0u, 0u);
        expect(state,
               drive_compare_fault(cpu, 0u, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 4u)) == value &&
                   (cpu->io.dma_active & 1u) != 0u,
               "OC1 through OC4 fault interrupts request DMA");
    }

    for (channel = 4u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, COMPARE_DMA_MEMORY, (uint16_t)(0x5200u + channel));
        dspic33_write_word(cpu, COMPARE_DMA_PAD, 0x1111u);
        configure_compare_dma(cpu, compare_irqs[channel], COMPARE_DMA_MEMORY,
                              COMPARE_DMA_PAD);
        configure_compare(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_read_word(cpu, COMPARE_DMA_PAD) == 0x1111u &&
                   cpu->io.dma_active == 0u && cpu->io.dma_peripheral_pending == 0u,
               "OC5 through OC16 do not expose DMA request sources");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, COMPARE_DMA_MEMORY, 0x5301u);
    configure_compare_dma(cpu, compare_irqs[1], COMPARE_DMA_MEMORY, 0x090eu);
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                      COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, drive_compare_fault(cpu, 0u, true),
           "release direct cascade DMA fault source");
    dspic33_write_word(
        cpu, 0x090au,
        (uint16_t)(dspic33_read_word(cpu, 0x090au) | COMPARE_FAULT_ENABLE_A));
    expect(state,
           drive_compare_fault(cpu, 0u, false) &&
               dspic33_read_word(cpu, 0x090eu) == 0x5301u &&
               (cpu->io.dma_active & 1u) != 0u,
           "cascaded OC pair requests DMA through its even output owner");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, COMPARE_DMA_MEMORY, 0x5302u);
    configure_compare_dma(cpu, compare_irqs[0], COMPARE_DMA_MEMORY, COMPARE_DMA_PAD);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    dspic33_write_word(cpu, compare_pmd_address(0u), compare_pmd_mask(0u));
    expect(state, dspic33_device_advance(cpu, 1u),
           "apply fault DMA PMD disable boundary");
    expect(state,
           drive_compare_fault(cpu, 0u, false) && cpu->io.dma_active == 0u &&
               cpu->io.dma_peripheral_pending == 0u,
           "PMD-disabled output compare suppresses fault DMA requests");
    dspic33_write_word(cpu, compare_pmd_address(0u), 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_read_word(cpu, COMPARE_DMA_PAD) == 0x5302u &&
               (cpu->io.dma_active & 1u) != 0u,
           "PMD re-enable requests DMA for a retained active OC fault");

    dspic33_reset(cpu, 0u);
    cpu->configuration[10u] = 0x80u;
    dspic33_write_word(cpu, COMPARE_DMA_MEMORY, 0x5303u);
    configure_compare_dma(cpu, compare_irqs[0], COMPARE_DMA_MEMORY, COMPARE_DMA_PAD);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           drive_compare_fault(cpu, 0u, false) && cpu->io.dma_active == 0u &&
               cpu->io.dma_peripheral_pending == 0u,
           "Sleep queues OC fault DMA with its interrupt");
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state,
           dspic33_device_advance(cpu, 0u) &&
               dspic33_read_word(cpu, COMPARE_DMA_PAD) == 0x5303u &&
               (cpu->io.dma_active & 1u) != 0u,
           "independent wake publishes queued OC fault DMA request");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, COMPARE_DMA_MEMORY, 0x5304u);
    configure_compare_dma(cpu, compare_irqs[0], COMPARE_DMA_MEMORY, COMPARE_DMA_PAD);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    dspic33_write_word(cpu, 0x0b02u, (uint16_t)(0x8000u | compare_irqs[0]));
    expect(state,
           drive_compare_fault(cpu, 0u, false) &&
               (dspic33_read_word(cpu, 0x0bf2u) & 1u) != 0u &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x0020u) != 0u,
           "OC DMA request colliding with FORCE raises DMACERR");
}

static void single_compare_cases(TestState* state, Dspic33* cpu) {
    static const bool initial[3] = {false, true, false};
    static const bool matched[3] = {true, false, true};
    uint8_t mode;
    for (mode = 1u; mode <= 3u; mode++) {
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, mode, 4u, 2u, COMPARE_SELF_SYNC);
        expect(state, output_is(cpu, 0u, initial[mode - 1u]),
               "single compare initializes documented level");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance single compare primary match");
        expect(state,
               output_is(cpu, 0u, initial[mode - 1u]) &&
                   dspic33_read_word(cpu, 0x0908u) == 2u && !interrupt_flag(cpu, 0u),
               "single compare match precedes output transition");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance single compare output pipeline");
        expect(state,
               output_is(cpu, 0u, matched[mode - 1u]) &&
                   dspic33_read_word(cpu, 0x0908u) == 3u && !interrupt_flag(cpu, 0u),
               "single compare output changes one clock after match");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance single compare interrupt pipeline");
        expect(state,
               output_is(cpu, 0u, matched[mode - 1u]) &&
                   dspic33_read_word(cpu, 0x0908u) == 0u && interrupt_flag(cpu, 0u),
               "single compare interrupt follows output by two clocks");
        clear_interrupt(cpu, 0u);
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance single compare next-cycle match");
        expect(state,
               output_is(cpu, 0u, matched[mode - 1u]) && !interrupt_flag(cpu, 0u),
               "single compare next match does not change output immediately");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance single compare repeated output pipeline");
        expect(state,
               output_is(cpu, 0u, mode == 3u ? false : matched[mode - 1u]) &&
                   !interrupt_flag(cpu, 0u),
               "single-shot holds while toggle changes on the next clock");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance repeated single compare interrupt pipeline");
        expect(state, interrupt_flag(cpu, 0u) == (mode == 3u),
               "only toggle mode repeats its interrupt");
    }

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 3u, 4u, 4u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance equal single compare and period");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "equal single compare records match before output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance equal single compare output and boundary");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               !interrupt_flag(cpu, 0u),
           "equal single compare changes output on next clock");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance equal single compare interrupt");
    expect(state, interrupt_flag(cpu, 0u),
           "equal single compare raises delayed interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 3u, 0u, COMPARE_SELF_SYNC);
    expect(state, output_is(cpu, 0u, false), "zero single compare starts unchanged");
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance zero single compare through first boundary");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "zero single compare remains initial at synchronization");
    expect(state, dspic33_device_advance(cpu, 8u),
           "advance zero single compare across later synchronizations");
    expect(state,
           output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.phase[0] == 0u,
           "zero single compare never generates an event while held in reset");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 3u, 0u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, UINT16_MAX + 1u),
           "advance zero free-running single compare to rollover");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "zero free-running single compare detects at rollover");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance zero free-running single compare output");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "zero free-running single compare changes after rollover");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance zero free-running single compare interrupt");
    expect(state, interrupt_flag(cpu, 0u),
           "zero free-running single compare raises delayed interrupt");

    for (mode = 1u; mode <= 3u; mode++) {
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, mode, 2u, 4u, COMPARE_SELF_SYNC);
        expect(state, dspic33_device_advance(cpu, 9u),
               "advance single compare beyond synchronization period");
        expect(state,
               output_is(cpu, 0u, mode == 2u) && !interrupt_flag(cpu, 0u) &&
                   cpu->io.output_compare.phase[0] == 0u,
               "single compare beyond period remains at initial level");
    }
}

static void dual_compare_cases(TestState* state, Dspic33* cpu) {
    uint8_t mode;
    for (mode = 4u; mode <= 5u; mode++) {
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, mode, 4u, 2u, COMPARE_SELF_SYNC);
        expect(state, output_is(cpu, 0u, false), "dual compare initializes low");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance dual compare rising edge");
        expect(state,
               output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
                   dspic33_read_word(cpu, 0x0908u) == 2u,
               "dual primary match precedes rising edge");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance dual compare rising pipeline");
        expect(state,
               output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u) &&
                   dspic33_read_word(cpu, 0x0908u) == 3u,
               "dual primary output changes one clock after match");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance dual compare secondary match");
        expect(state,
               output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u) &&
                   dspic33_read_word(cpu, 0x0908u) == 4u,
               "dual secondary match precedes falling edge");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance dual compare falling pipeline");
        expect(state,
               output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
                   dspic33_read_word(cpu, 0x0908u) == 0u,
               "dual secondary output changes one clock after match");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance dual compare interrupt pipeline");
        expect(state, output_is(cpu, 0u, false) && interrupt_flag(cpu, 0u),
               "dual interrupt follows falling edge by two clocks");
        clear_interrupt(cpu, 0u);
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance continuous dual repeated rising pipeline");
        expect(state, output_is(cpu, 0u, mode == 5u) && !interrupt_flag(cpu, 0u),
               "dual single-shot stops while continuous mode repeats");
    }

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 2u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u), "advance equal dual primary match");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "equal dual values detect primary before output");
    expect(state, dspic33_device_advance(cpu, 1u), "advance equal dual primary output");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "equal dual values raise output in first cycle");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance equal dual secondary match in next cycle");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "equal dual secondary match precedes output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance equal dual secondary output");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "equal dual values lower output in following cycle");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance equal dual interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u), "equal dual values raise delayed interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 3u, 0u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance zero-primary dual first cycle");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "zero-primary dual records match at first synchronization");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance zero-primary dual rising pipeline");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "zero-primary dual rises after first synchronization");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance zero-primary dual secondary match");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "zero-primary dual secondary match precedes output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance zero-primary dual falling pipeline");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "zero-primary dual falls after secondary match");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance zero-primary dual interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u),
           "zero-primary dual raises delayed interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 0u, 0u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance zero dual values across boundaries");
    expect(state,
           output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "zero dual values remain low without interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 2u, 4u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance reversed dual primary match");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "reversed dual primary match precedes output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance reversed dual rising pipeline");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "reversed dual values raise output before rollover");
    expect(state, dspic33_device_advance(cpu, UINT16_MAX - 5u),
           "advance reversed dual to timer maximum");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == UINT16_MAX,
           "reversed dual holds output through timer maximum");
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance reversed dual across rollover to secondary match");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 2u &&
               !interrupt_flag(cpu, 0u),
           "reversed dual secondary match precedes output after rollover");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance reversed dual falling pipeline");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "reversed dual falls one clock after secondary match");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance reversed dual interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u), "reversed dual raises delayed interrupt");

    for (mode = 4u; mode <= 5u; mode++) {
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, mode, 2u, 4u, COMPARE_SELF_SYNC);
        expect(state, dspic33_device_advance(cpu, 9u),
               "advance self-synchronized reversed dual values");
        expect(state,
               output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
                   cpu->io.output_compare.phase[0] == 0u,
               "synchronization before primary compare suppresses dual pulse");
    }

    for (mode = 4u; mode <= 7u; mode++) {
        if (mode == 6u) {
            continue;
        }
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, mode, 0u, 0u, COMPARE_SELF_SYNC);
        expect(state, dspic33_device_advance(cpu, 4u),
               "advance zero dual values across mode");
        expect(state,
               output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
                   cpu->io.output_compare.phase[0] == 0u,
               "zero dual values suppress every pulse mode");
    }
}

static void center_aligned_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 7u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, output_is(cpu, 0u, false), "center PWM initializes low");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance center PWM before buffered writes");
    dspic33_write_word(cpu, 0x0904u, 6u);
    dspic33_write_word(cpu, 0x0906u, 3u);
    expect(state,
           cpu->io.output_compare.active_rs[0] == 4u &&
               cpu->io.output_compare.active_r[0] == 2u,
           "center PWM compare writes remain buffered");
    expect(state, dspic33_device_advance(cpu, 1u), "advance center PWM primary match");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "center PWM primary match precedes output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance center PWM rising pipeline");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "center PWM primary output changes one clock later");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance center PWM secondary match");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "center PWM secondary match precedes output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance center PWM falling pipeline and boundary");
    expect(state,
           output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.active_rs[0] == 6u &&
               cpu->io.output_compare.active_r[0] == 3u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "center PWM falling edge and boundary load compare buffers");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance center PWM interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u),
           "center PWM interrupt follows falling edge by two clocks");
}

static void immediate_compare_write_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 3u, 6u, 4u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance before immediate primary write");
    dspic33_write_word(cpu, 0x0906u, 2u);
    expect(state,
           cpu->io.output_compare.active_r[0] == 2u &&
               dspic33_device_advance(cpu, 1u) && output_is(cpu, 0u, false) &&
               !interrupt_flag(cpu, 0u),
           "non-PWM primary write changes current compare cycle");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance immediate primary output pipeline");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "immediate primary write changes output after one clock");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance immediate primary interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u),
           "immediate primary write raises delayed interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 6u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance before immediate secondary write");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance primary output before secondary write");
    dspic33_write_word(cpu, 0x0904u, 4u);
    expect(state,
           cpu->io.output_compare.active_rs[0] == 4u &&
               dspic33_device_advance(cpu, 1u) && output_is(cpu, 0u, true) &&
               !interrupt_flag(cpu, 0u),
           "non-PWM secondary write changes current compare cycle");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance immediate secondary output pipeline");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "immediate secondary write changes output after one clock");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance immediate secondary interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u),
           "immediate secondary write raises delayed interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance primary match before compare rewrite");
    dspic33_write_word(cpu, 0x0906u, 3u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance latched primary output after compare rewrite");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "primary compare rewrite preserves latched output event");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance latched primary interrupt after compare rewrite");
    expect(state, interrupt_flag(cpu, 0u),
           "primary compare rewrite preserves latched interrupt event");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance secondary match before compare rewrite");
    dspic33_write_word(cpu, 0x0904u, 6u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance latched secondary output after compare rewrite");
    expect(state,
           output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.active_rs[0] == 6u,
           "secondary compare rewrite preserves latched output event");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance latched secondary interrupt after compare rewrite");
    expect(state, interrupt_flag(cpu, 0u),
           "secondary compare rewrite preserves latched interrupt event");
}

static void output_control_cases(TestState* state, Dspic33* cpu) {
    bool high;
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u,
                           (uint16_t)(COMPARE_SELF_SYNC | 0x1000u));
    expect(state, output_is(cpu, 0u, true), "OCINV inverts initialized output");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance inverted single compare match");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "OCINV remains unchanged at compare match");
    expect(state, dspic33_device_advance(cpu, 1u), "advance inverted output pipeline");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "OCINV inverts delayed matched output");
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 3u,
           "live OCINV clear preserves timer and mode phase");

    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    dspic33_write_word(cpu, 0x0698u, 0x0010u);
    expect(state, pin_is(cpu, 109u, true), "driven OC pin is observable");
    dspic33_write_word(cpu, 0x0902u, (uint16_t)(COMPARE_SELF_SYNC | 0x0020u));
    expect(state,
           dspic33_output_compare_output(cpu, 0u, &high) && high &&
               !dspic33_output_compare_pin(cpu, 109u, &high),
           "OCTRIS disconnects pin without stopping module output");
    expect(state, dspic33_device_advance(cpu, 2u), "advance tri-stated OC module");
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, pin_is(cpu, 109u, false),
           "clearing OCTRIS reconnects preserved module phase");
}

static void channel_mode_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t mode;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        for (mode = 1u; mode <= 7u; mode++) {
            bool initial = mode == 2u || mode == 6u;
            bool primary_detection = mode == 2u;
            bool primary_output = mode != 2u && mode != 6u;
            bool boundary_output = mode == 1u || mode == 3u || mode == 6u;
            bool delayed_secondary_interrupt = mode == 4u || mode == 5u || mode == 7u;
            bool next_output = mode == 1u || mode == 5u || mode == 7u;
            uint16_t base = compare_base(channel);
            dspic33_reset(cpu, 0u);
            configure_compare_mode(cpu, channel, mode, 4u, 2u, COMPARE_SELF_SYNC);
            expect(state,
                   output_is(cpu, channel, initial) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
                   "channel mode matrix initial state");
            expect(state, dspic33_device_advance(cpu, 2u),
                   "channel mode matrix primary advance");
            expect(state,
                   output_is(cpu, channel, primary_detection) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                       !interrupt_flag(cpu, channel),
                   "channel mode matrix primary detection");
            expect(state, dspic33_device_advance(cpu, 1u),
                   "channel mode matrix primary output advance");
            expect(state,
                   output_is(cpu, channel, primary_output) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 3u &&
                       !interrupt_flag(cpu, channel),
                   "channel mode matrix primary output pipeline");
            expect(state, dspic33_device_advance(cpu, 1u),
                   "channel mode matrix secondary advance");
            expect(state,
                   output_is(cpu, channel, primary_output) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 4u &&
                       !interrupt_flag(cpu, channel),
                   "channel mode matrix secondary detection");
            expect(state, dspic33_device_advance(cpu, 1u),
                   "channel mode matrix boundary advance");
            expect(state,
                   output_is(cpu, channel, boundary_output) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                       interrupt_flag(cpu, channel) == (mode <= 3u || mode == 6u),
                   "channel mode matrix output and first interrupt pipeline");
            clear_interrupt(cpu, channel);
            expect(state, dspic33_device_advance(cpu, 2u),
                   "channel mode matrix delayed interrupt advance");
            expect(state,
                   output_is(cpu, channel, mode == 1u || mode == 3u) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                       interrupt_flag(cpu, channel) == delayed_secondary_interrupt,
                   "channel mode matrix delayed secondary interrupt");
            clear_interrupt(cpu, channel);
            expect(state, dspic33_device_advance(cpu, 1u),
                   "channel mode matrix next output advance");
            expect(state,
                   output_is(cpu, channel, next_output) &&
                       !interrupt_flag(cpu, channel),
                   "channel mode matrix next-cycle output pipeline");
        }
    }
}

static void one_shot_restart_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 4u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 7u), "advance first one-shot pulse");
    expect(state,
           output_is(cpu, 0u, false) && interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.phase[0] == 2u &&
               dspic33_read_word(cpu, 0x0908u) == 2u,
           "one-shot pulse reaches completed state");
    clear_interrupt(cpu, 0u);
    dspic33_write_byte(cpu, 0x0901u, 0x1cu);
    expect(state,
           cpu->io.output_compare.phase[0] == 2u &&
               dspic33_read_word(cpu, 0x0908u) == 2u,
           "high control byte write does not restart one-shot");
    dspic33_write_byte(cpu, 0x0900u, 0x04u);
    expect(state,
           output_is(cpu, 0u, false) && cpu->io.output_compare.phase[0] == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "same mode low-byte write restarts one-shot");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance restarted one-shot primary match");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "restarted one-shot detects a new primary match");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance restarted one-shot output pipeline");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "restarted one-shot generates a delayed rising edge");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "output-compare basic test initializes");
    if (initialized) {
        access_cases(&state, &cpu);
        waveform_cases(&state, &cpu);
        boundary_cases(&state, &cpu);
        buffering_cases(&state, &cpu);
        free_running_cases(&state, &cpu);
        instruction_transition_cases(&state, &cpu);
        free_running_buffer_cases(&state, &cpu);
        pps_cases(&state, &cpu);
        interrupt_cases(&state, &cpu);
        dma_cases(&state, &cpu);
        single_compare_cases(&state, &cpu);
        dual_compare_cases(&state, &cpu);
        center_aligned_cases(&state, &cpu);
        immediate_compare_write_cases(&state, &cpu);
        output_control_cases(&state, &cpu);
        channel_mode_matrix_cases(&state, &cpu);
        one_shot_restart_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
