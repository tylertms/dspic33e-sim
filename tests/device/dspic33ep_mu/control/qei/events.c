#include "device/dspic33ep_mu/control/qei/internal.h"

void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_device_internal_run_qei(Dspic33* cpu, uint16_t source, uint32_t value);

static void compare_refresh_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t status_address = (uint16_t)(base + 4u);
        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_configure_interrupt(cpu, channel);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 5u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 5u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 2u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_HIGH_COMPARE_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_HIGH_COMPARE) != 0u &&
                   dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI enable refreshes the active high comparison");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_configure_interrupt(cpu, channel);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 0u);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                                       0x00010000u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_HIGH_COMPARE_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        dspic33_qei_test_clear_interrupt(cpu, channel);
        dspic33_write_word(
            cpu, status_address,
            (uint16_t)(dspic33_read_word(cpu, status_address) & ~QEI_STATUS_HIGH_COMPARE));
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 1u);
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_HIGH_COMPARE) != 0u &&
                   dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI position high write refreshes the active comparison");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_configure_interrupt(cpu, channel);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 2u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 5u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 2u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_LOW_COMPARE_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_LOW_COMPARE) != 0u &&
                   dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI low comparison raises its enabled interrupt");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_configure_interrupt(cpu, channel);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                                       0x12345678u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_INITIALIZED_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | (3u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INITIALIZED) != 0u &&
                   dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI completed homing initialization raises its enabled interrupt");
    }
}

static void power_lifecycle_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_TIMER);
        cpu->power_state = DSPIC33_POWER_IDLE;
        expect(state,
               dspic33_device_advance(cpu, 3u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 3u,
               "QEI continues its internal timer in Idle when enabled");
        cpu->power_state = DSPIC33_POWER_ACTIVE;

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI ignores external count edges in Sleep");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI resumes from the physical input level after Sleep");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_device_advance(cpu, 10u) &&
                   cpu->io.qei.filter_stability[channel][0] == 0u,
               "QEI filter clock stops in Sleep");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        expect(state,
               dspic33_device_advance(cpu, 3u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI filter restarts after Sleep");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true);
        dspic33_reset(cpu, 0u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 0x0008u) != 0u,
               "QEI cold reset preserves the physical input level");
        dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, false);
    }

    {
        bool high;
        expect(state,
               !dspic33_qei_input(cpu, DSPIC33_QEI_COUNT, DSPIC33_QEI_PHASE_A, true, 0u) &&
                   !dspic33_qei_input(cpu, 0u, (Dspic33QeiInput)4u, true, 0u) &&
                   !dspic33_qei_compare_output(cpu, DSPIC33_QEI_COUNT, &high) &&
                   !dspic33_qei_compare_output(cpu, 0u, NULL),
               "QEI public APIs reject invalid arguments");
    }
}

static void pps_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        for (uint8_t source = 0u; source < 4u; source++) {
            uint16_t bit = (uint16_t)(1u << source);
            dspic33_qei_test_reset_qei(cpu);
            dspic33_gpio_release(cpu, 3u, 0x000fu);
            dspic33_gpio_drive(cpu, 3u, 0u, bit);
            dspic33_qei_test_select_pps_input(cpu, channel, source, (uint8_t)(64u + source));
            expect(state,
                   dspic33_gpio_drive(cpu, 3u, bit, bit) &&
                       (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & bit) != 0u,
                   "QEI RPINR input follows its selected physical pin");
            expect(state,
                   dspic33_gpio_drive(cpu, 3u, 0u, bit) &&
                       (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & bit) == 0u,
                   "QEI RPINR input follows the selected pin low");
        }

        dspic33_qei_test_reset_qei(cpu);
        dspic33_gpio_release(cpu, 1u, 0x0001u);
        dspic33_gpio_drive(cpu, 1u, 0x0001u, 0x0001u);
        dspic33_write_word(cpu, 0x0e1eu, 0u);
        dspic33_qei_test_select_pps_input(cpu, channel, DSPIC33_QEI_PHASE_A, 32u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 1u) == 0u,
               "QEI input-only RPI rejects a cleared ANS bit");
        dspic33_write_word(cpu, 0x0e1eu, 1u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 1u) != 0u,
               "QEI input-only RPI accepts a set ANS bit");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_gpio_release(cpu, 3u, 0x0001u);
        dspic33_gpio_drive(cpu, 3u, 0x0001u, 0x0001u);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_UP_DOWN);
        dspic33_qei_test_select_pps_input(cpu, channel, DSPIC33_QEI_PHASE_A, 64u);
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == UINT32_MAX,
               "QEI RPINR remap applies its unfiltered input transition");
        dspic33_gpio_drive(cpu, 3u, 0u, 0x0001u);
        dspic33_gpio_drive(cpu, 3u, 0x0001u, 0x0001u);
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == UINT32_MAX - 1u,
               "QEI RPINR stable mapping counts a later physical edge");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_gpio_release(cpu, 3u, 0x0001u);
        dspic33_gpio_drive(cpu, 3u, 0u, 0x0001u);
        dspic33_qei_test_select_pps_input(cpu, channel, DSPIC33_QEI_PHASE_A, 64u);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_UP_DOWN);
        dspic33_gpio_drive(cpu, 3u, 0x0001u, 0x0001u);
        expect(state,
               dspic33_device_advance(cpu, 2u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 1u) == 0u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI filtered PPS input rejects fewer than three stable samples");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 1u) != 0u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == UINT32_MAX,
               "QEI filtered PPS input accepts its third stable sample");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_gpio_release(cpu, 3u, 0x0003u);
        dspic33_gpio_drive(cpu, 3u, 0u, 0x0003u);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        dspic33_qei_test_select_pps_input(cpu, channel, DSPIC33_QEI_PHASE_A, 64u);
        dspic33_qei_test_select_pps_input(cpu, channel, DSPIC33_QEI_PHASE_B, 64u);
        expect(state,
               dspic33_gpio_drive(cpu, 3u, 0x0001u, 0x0001u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI many-to-one PPS transition applies both phases simultaneously");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_gpio_release(cpu, 3u, 0x0003u);
        dspic33_gpio_drive(cpu, 3u, 0u, 0x0003u);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        dspic33_qei_test_select_pps_input(cpu, channel, DSPIC33_QEI_PHASE_A, 64u);
        dspic33_qei_test_select_pps_input(cpu, channel, DSPIC33_QEI_PHASE_B, 65u);
        expect(state,
               dspic33_gpio_drive(cpu, 3u, 0x0003u, 0x0003u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI masked GPIO transition applies separate phases simultaneously");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_gpio_release(cpu, 3u, (uint16_t)(1u << channel));
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e34u, 0u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 0u);
        dspic33_write_byte(cpu, (uint16_t)(0x0680u + channel), (uint8_t)(47u + channel));
        {
            bool high = true;
            expect(state, dspic33_gpio_pin(cpu, 3u, channel, &high) && !high,
                   "disabled QEI compare output leaves GPIO ownership unchanged");
        }
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_OUTPUT_GREATER_EQUAL);
        {
            bool high = false;
            expect(state, dspic33_gpio_pin(cpu, 3u, channel, &high) && high,
                   "QEI RPOR compare output overrides GPIO direction");
        }
        dspic33_write_word(cpu, 0x0e36u, (uint16_t)(1u << channel));
        dspic33_gpio_drive(cpu, 3u, 0u, (uint16_t)(1u << channel));
        {
            bool high = true;
            expect(state, dspic33_gpio_pin(cpu, 3u, channel, &high) && !high,
                   "QEI open-drain compare high releases to an external low");
        }
        dspic33_gpio_drive(cpu, 3u, (uint16_t)(1u << channel), (uint16_t)(1u << channel));
        {
            bool high = false;
            expect(state, dspic33_gpio_pin(cpu, 3u, channel, &high) && high,
                   "QEI open-drain compare high releases to an external high");
        }
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 1u);
        {
            bool high = true;
            expect(state, dspic33_gpio_pin(cpu, 3u, channel, &high) && !high,
                   "QEI RPOR compare output follows its comparison state");
        }
        dspic33_write_word(cpu, 0x0e36u, 0u);
        dspic33_gpio_release(cpu, 3u, (uint16_t)(1u << channel));
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 0u);
        {
            bool before = false;
            bool after = true;
            bool driven = dspic33_gpio_pin(cpu, 3u, channel, &before) && before;
            dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
            expect(state,
                   driven && dspic33_device_advance(cpu, 1u) &&
                       dspic33_gpio_pin(cpu, 3u, channel, &after) && !after,
                   "QEI PMD releases its mapped RPOR output");
        }
        dspic33_write_byte(cpu, (uint16_t)(0x0680u + channel), 0u);
        {
            bool high = true;
            expect(state, dspic33_gpio_pin(cpu, 3u, channel, &high) && !high,
                   "QEI RPOR removal restores GPIO ownership");
        }

        dspic33_qei_test_reset_qei(cpu);
        dspic33_gpio_release(cpu, 3u, 0x0001u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 1u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_OUTPUT_GREATER_EQUAL);
        dspic33_write_byte(cpu, 0x0680u, (uint8_t)(47u + channel));
        dspic33_write_byte(cpu, 0x06aeu, 64u);
        dspic33_write_word(cpu, CAPTURE_BASE, CAPTURE_FP_RISING);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_TIMER);
        {
            bool pin_high = false;
            expect(state,
                   dspic33_device_advance(cpu, 1u) && dspic33_gpio_pin(cpu, 3u, 0u, &pin_high) &&
                       pin_high && cpu->io.input_capture.fifo[0].count == 0u,
                   "QEI RPOR transition fans out to another PPS input consumer");
            expect(state,
                   dspic33_device_advance(cpu, 1u) && cpu->io.input_capture.fifo[0].count == 1u,
                   "QEI RPOR fanout observes the input capture pipeline delay");
        }

        dspic33_qei_test_reset_qei(cpu);
        dspic33_gpio_release(cpu, 3u, 0x0001u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 0u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 2u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_OUTPUT_OUTSIDE);
        dspic33_write_byte(cpu, 0x0680u, (uint8_t)(47u + channel));
        dspic33_write_byte(cpu, 0x06aeu, 64u);
        dspic33_write_word(cpu, CAPTURE_BASE, CAPTURE_FP_EVERY_EDGE);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_TIMER);
        expect(state, dspic33_device_advance(cpu, 2u) && cpu->io.input_capture.fifo[0].count == 1u,
               "QEI batched timer advance preserves intermediate compare edges");
        expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.input_capture.fifo[0].count == 2u,
               "QEI batched compare edges retain distinct capture deadlines");
    }

    {
        Dspic33 copy;
        bool initialized = dspic33_initialize(&copy);
        dspic33_qei_test_reset_qei(cpu);
        dspic33_gpio_release(cpu, 3u, 0x0003u);
        dspic33_gpio_drive(cpu, 3u, 0x0001u, 0x0003u);
        dspic33_qei_test_select_pps_input(cpu, 0u, DSPIC33_QEI_PHASE_A, 64u);
        if (initialized) {
            initialized = dspic33_copy(&copy, cpu);
        }
        dspic33_qei_test_select_pps_input(cpu, 0u, DSPIC33_QEI_PHASE_A, 65u);
        expect(state,
               initialized && cpu->io.qei.pps_selection[0][0] == 65u &&
                   (dspic33_read_word(cpu, (uint16_t)(bases[0] + 2u)) & 1u) == 0u &&
                   copy.io.qei.pps_selection[0][0] == 64u &&
                   (dspic33_read_word(&copy, (uint16_t)(bases[0] + 2u)) & 1u) != 0u,
               "copied QEI PPS routing and input state diverge independently");
        if (initialized) {
            dspic33_release(&copy);
        }
    }

    dspic33_qei_test_reset_qei(cpu);
    dspic33_gpio_release(cpu, 3u, 0x0001u);
    dspic33_gpio_drive(cpu, 3u, 0x0001u, 0x0001u);
    dspic33_qei_test_select_pps_input(cpu, 0u, DSPIC33_QEI_PHASE_A, 64u);
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_read_word(cpu, pps_input_registers[0][0]) == 0u &&
               (dspic33_read_word(cpu, (uint16_t)(bases[0] + 2u)) & 1u) == 0u &&
               (cpu->io.gpio[3] & cpu->io.gpio_driven[3] & 1u) != 0u,
           "QEI reset clears PPS routing while preserving the external pin level");

    dspic33_gpio_release(cpu, 1u, 0x0001u);
    dspic33_gpio_release(cpu, 3u, 0x000fu);
}

static void large_timer_advance_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t status_mask = QEI_STATUS_VELOCITY_OVERFLOW |
                                        QEI_STATUS_POSITION_OVERFLOW | QEI_STATUS_LOW_COMPARE |
                                        QEI_STATUS_HIGH_COMPARE;
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t inverted;
        for (inverted = 0u; inverted < 2u; inverted++) {
            uint32_t expected = inverted != 0u ? 0xfffffffcu : 4u;
            uint16_t expected_velocity = inverted != 0u ? 0xfffcu : 4u;
            dspic33_qei_test_reset_qei(cpu);
            dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                                           100u);
            dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u),
                                           0xffffff9cu);
            dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true);
            dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true);
            dspic33_write_word(
                cpu, base,
                (uint16_t)(QEI_ENABLE | QEI_MODE_TIMER | (inverted != 0u ? 0x0008u : 0u)));
            expect(state,
                   dspic33_device_advance(cpu, (uint64_t)UINT32_MAX + 5u) &&
                       dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == expected &&
                       dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == expected_velocity &&
                       dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 4u &&
                       dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x16u)) == expected,
                   "QEI large timer advance preserves modular counter results");
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(base + 4u)) & status_mask) == status_mask,
                   "QEI large timer advance preserves crossed status events");
        }
    }
}

static void wrapped_timer_advance_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t starts[] = {0u, 2u, 0x7ffffffdu, 0x7fffffffu, 0x80000000u, 0xfffffffdu};
    for (uint8_t channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        const uint16_t base = bases[channel];
        for (uint8_t index = 0u; index < sizeof(starts) / sizeof(starts[0]); index++) {
            const bool inverted = (index & 1u) != 0u;
            const uint32_t expected = inverted ? starts[index] - 5u : starts[index] + 5u;
            dspic33_qei_test_reset_qei(cpu);
            dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                                           starts[index]);
            dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                                           starts[index] + 2u);
            dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u),
                                           starts[index] - 2u);
            dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), (uint16_t)(inverted ? 2u : 0xfffdu));
            dspic33_write_word(
                cpu, base,
                (uint16_t)(QEI_ENABLE | QEI_MODE_TIMER | (inverted ? QEI_DIRECTION_INVERT : 0u)));
            expect(state,
                   dspic33_device_advance(cpu, 5u) &&
                       dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == expected,
                   "QEI wrapped timer advance preserves modular position");
        }
    }
}

static void pmd_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 3u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 3u,
               "QEI advances before PMD");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               !cpu->io.qei.pmd_disabled[channel] &&
                   dspic33_read_word(cpu, base) == (QEI_ENABLE | QEI_MODE_TIMER),
               "QEI PMD set is delayed one cycle");
        expect(state,
               dspic33_device_advance(cpu, 1u) && cpu->io.qei.pmd_disabled[channel] &&
                   dspic33_read_word(cpu, base) == 0u,
               "QEI PMD disables after one enabled cycle");
        dspic33_write_word(cpu, base, 0u);
        expect(state, dspic33_device_advance(cpu, 5u) && dspic33_read_word(cpu, base) == 0u,
               "QEI PMD blocks register writes and counter progress");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) && !cpu->io.qei.pmd_disabled[channel] &&
                   dspic33_read_word(cpu, base) == (QEI_ENABLE | QEI_MODE_TIMER),
               "QEI PMD clear enables after one disabled cycle");
        expect(state,
               dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 4u &&
                   dspic33_device_advance(cpu, 2u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 6u,
               "QEI PMD preserves and resumes counter state");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | (2u << QEI_DIVIDER_SHIFT) | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 2u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI accumulates partial prescaler phase before PMD");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.qei.pmd_disabled[channel],
               "QEI PMD transition preserves partial prescaler phase");
        expect(state,
               dspic33_device_advance(cpu, 7u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI PMD freezes partial prescaler phase");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) && !cpu->io.qei.pmd_disabled[channel] &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI PMD clear does not consume prescaler phase");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI resumes after the exact remaining prescaler phase");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), QEI_STATUS_INDEX_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 4u)) & QEI_STATUS_INDEX) != 0u,
               "QEI Index match latches before PMD");
        dspic33_write_word(
            cpu, (uint16_t)(base + 4u),
            (uint16_t)(dspic33_read_word(cpu, (uint16_t)(base + 4u)) & ~QEI_STATUS_INDEX));
        dspic33_qei_test_clear_interrupt(cpu, channel);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, false),
               "QEI PMD Index deassertion updates the external level");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 4u)) & QEI_STATUS_INDEX) != 0u &&
                   dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI PMD Index deassertion rearms after resume");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI PMD blocks bypassed input changes");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 1u) != 0u,
               "QEI PMD resume synchronizes bypassed input without an edge");
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI bypassed input counts the next physical edge");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "QEI PMD blocks filtered input changes");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI PMD resume does not bypass the input filter");
        expect(state,
               dspic33_device_advance(cpu, 2u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI filtered input resumes after three stable samples");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, dspic33_device_advance(cpu, 1u) && !cpu->io.qei.pmd_disabled[channel],
               "QEI PMD generation ignores stale transitions");

        {
            uint64_t device_cycles = cpu->device_cycles;
            size_t queued = cpu->events.count;
            cpu->device_cycles = UINT64_MAX;
            expect(state,
                   !dspic33_qei_input(cpu, channel, DSPIC33_QEI_PHASE_A, true, 1u) &&
                       cpu->events.count == queued,
                   "QEI input scheduling failure queues no partial event");
            cpu->device_cycles = device_cycles;
        }

        {
            uint64_t device_cycles = cpu->device_cycles;
            uint16_t generation = cpu->io.qei.pmd_generation[channel];
            bool disabled = cpu->io.qei.pmd_disabled[channel];
            size_t queued = cpu->events.count;
            uint16_t pmd = dspic33_read_word(cpu, pmd_addresses[channel]);
            cpu->device_cycles = UINT64_MAX;
            dspic33_write_word(cpu, pmd_addresses[channel], (uint16_t)(pmd | pmd_masks[channel]));
            expect(state,
                   dspic33_read_word(cpu, pmd_addresses[channel]) == pmd &&
                       cpu->io.qei.pmd_generation[channel] == (uint16_t)(generation + 2u) &&
                       cpu->io.qei.pmd_disabled[channel] == disabled &&
                       cpu->events.count == queued && cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
                   "QEI PMD scheduling failure rolls back and invalidates the event");
            cpu->device_cycles = device_cycles;
            cpu->stop_reason = DSPIC33_RUNNING;
        }
    }
}

static void copy_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize QEI copy destination");
    if (!initialized) {
        return;
    }
    dspic33_qei_test_reset_qei(cpu);
    dspic33_reset(&copy, 0u);
    expect(state,
           dspic33_qei_input(cpu, 0u, DSPIC33_QEI_PHASE_A, true, 2u) &&
               dspic33_qei_input(cpu, 1u, DSPIC33_QEI_HOME, true, 3u) && dspic33_copy(&copy, cpu),
           "copy QEI state with pending input events");
    expect(state,
           dspic33_device_advance(&copy, 3u) && (copy.qei_inputs[0] & 1u) != 0u &&
               (copy.qei_inputs[1] & 8u) != 0u && cpu->qei_inputs[0] == 0u &&
               cpu->qei_inputs[1] == 0u && cpu->events.count == 2u,
           "QEI copied events execute independently");

    dspic33_qei_test_reset_qei(cpu);
    dspic33_qei_test_set_open_comparison_window(cpu, bases[0]);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 2u), (uint16_t)(QEI_FILTER_ENABLE | (1u << 11u)));
    dspic33_write_word(cpu, bases[0], QEI_ENABLE | (2u << QEI_DIVIDER_SHIFT) | QEI_MODE_TIMER);
    dspic33_qei_test_input(cpu, 0u, DSPIC33_QEI_PHASE_A, true);
    expect(state,
           dspic33_device_advance(cpu, 2u) && cpu->io.qei.counter_fraction[0] == 2u &&
               cpu->io.qei.filter_stability[0][0] == 1u && dspic33_copy(&copy, cpu),
           "copy partial QEI counter and filter phases");
    expect(state,
           dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u) &&
               dspic33_qei_test_read_counter(cpu, (uint16_t)(bases[0] + 6u)) == 1u &&
               dspic33_qei_test_read_counter(&copy, (uint16_t)(bases[0] + 6u)) == 1u &&
               cpu->io.qei.filter_stability[0][0] == 2u && copy.io.qei.filter_stability[0][0] == 2u,
           "copied QEI phases resume identically");
    expect(state,
           dspic33_qei_test_input(cpu, 0u, DSPIC33_QEI_PHASE_A, false) &&
               dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u) &&
               (cpu->io.qei.filtered_inputs[0] & 1u) == 0u &&
               (copy.io.qei.filtered_inputs[0] & 1u) != 0u,
           "copied QEI physical and filter state diverge independently");

    dspic33_qei_test_reset_qei(cpu);
    dspic33_write_word(cpu, pmd_addresses[0], pmd_masks[0]);
    expect(state, dspic33_copy(&copy, cpu), "copy pending QEI PMD transition");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u) &&
               cpu->io.qei.pmd_disabled[0] && copy.io.qei.pmd_disabled[0],
           "copied QEI PMD transition completes independently");
    dspic33_release(&copy);
}

static void index_direction_erratum_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | (1u << QEI_INDEX_MATCH_SHIFT));
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   cpu->io.qei.direction[channel] > 0 &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   cpu->stop_reason == DSPIC33_SILICON_RESULT_UNDEFINED,
               "B1 positive-direction quadrature index remains silicon-undefined");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        {
            bool phase_changed = dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                                 dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                                 dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, false) &&
                                 dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, false);
            int8_t direction = cpu->io.qei.direction[channel];
            bool index_changed = dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true);
            expect(state,
                   phase_changed && direction < 0 && index_changed &&
                       cpu->stop_reason == DSPIC33_RUNNING,
                   "negative-direction quadrature index remains outside the B1 erratum");
        }
    }
}

static void pmd_index_latch_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        dspic33_qei_test_reset_qei(cpu);
        cpu->qei_inputs[channel] = 4u;
        cpu->io.qei.pmd_generation[channel] = 3u;
        cpu->io.qei.pmd_disabled[channel] = true;
        dspic33_device_internal_run_qei(cpu, (uint16_t)(0x0100u + channel), 6u);
        expect(state, cpu->io.qei.index_latched[channel],
               "QEI PMD resume latches a matching active index");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_device_internal_raw_write_word(cpu, bases[channel], QEI_ENABLE);
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(bases[channel] + 4u),
                                               QEI_STATUS_INDEX_ENABLE);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI enabled index event raises its interrupt");
    }
}

static void event_boundary_cases(TestState* state, Dspic33* cpu) {
    dspic33_qei_test_reset_qei(cpu);
    dspic33_qei_test_configure_interrupt(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(bases[0] + 4u),
                                           (uint16_t)(QEI_STATUS_INDEX_ENABLE | QEI_STATUS_INDEX));
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u),
                       (uint16_t)(QEI_STATUS_INDEX_ENABLE | QEI_STATUS_INDEX));
    expect(state, dspic33_qei_test_interrupt_set(cpu, 0u),
           "QEI status refresh raises a pending enabled interrupt");

    dspic33_qei_test_reset_qei(cpu);
    cpu->qei_inputs[0] = 4u;
    cpu->io.qei.pmd_disabled[0] = true;
    dspic33_device_internal_raw_write_word(cpu, bases[0], (uint16_t)(1u << QEI_INDEX_MATCH_SHIFT));
    dspic33_device_internal_run_qei(cpu, 0x0100u, 0u);
    dspic33_device_internal_run_qei(cpu, 0x0100u, 2u);
    dspic33_device_internal_run_qei(cpu, 0x0102u, 0u);
    dspic33_device_internal_run_qei(cpu, 8u, 1u);
    expect(state,
           !cpu->io.qei.index_latched[0] && cpu->qei_inputs[0] == 4u &&
               cpu->io.qei.pmd_generation[0] == 0u,
           "QEI event dispatcher rejects mismatched and out-of-range events");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize QEI processor");
    if (initialized) {
        dspic33_qei_test_register_cases(&state, &cpu);
        dspic33_qei_test_quadrature_cases(&state, &cpu);
        dspic33_qei_test_quadrature_transition_cases(&state, &cpu);
        dspic33_qei_test_divider_polarity_output_cases(&state, &cpu);
        dspic33_qei_test_external_mode_cases(&state, &cpu);
        dspic33_qei_test_timer_filter_power_cases(&state, &cpu);
        dspic33_qei_test_interrupt_compare_index_cases(&state, &cpu);
        compare_refresh_cases(&state, &cpu);
        power_lifecycle_cases(&state, &cpu);
        pps_cases(&state, &cpu);
        large_timer_advance_cases(&state, &cpu);
        wrapped_timer_advance_cases(&state, &cpu);
        index_direction_erratum_cases(&state, &cpu);
        pmd_index_latch_cases(&state, &cpu);
        event_boundary_cases(&state, &cpu);
        pmd_cases(&state, &cpu);
        copy_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
