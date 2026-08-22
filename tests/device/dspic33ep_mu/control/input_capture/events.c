#include "device/dspic33ep_mu/control/input_capture/internal.h"

static void timer_source_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t no_sources[5] = {0u, 10u, 29u, 30u, 31u};
    uint8_t index;
    for (index = 0u; index < 5u; index++) {
        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, 0u, capture_timer_sources[index],
                                                            true, true, 0u, false);
        dspic33_input_capture_test_configure_timer_source(cpu, index, UINT16_MAX, 0u);
        expect(state, dspic33_device_advance(cpu, 7u), "advance alternate capture clock");
        expect(state,
               dspic33_read_word(cpu, timer_registers[index]) == 7u &&
                   cpu->io.input_capture.timer[0] == 7u,
               "Timer1-5 prescaled clocks advance selected capture timer");
    }

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, capture_timer_sources[0], true,
                                                        true, 0u, false);
    dspic33_input_capture_test_configure_timer_source(cpu, 0u, UINT16_MAX, 0x0010u);
    expect(state, dspic33_device_advance(cpu, 15u), "advance prescaled alternate capture clock");
    expect(state,
           dspic33_read_word(cpu, timer_registers[0]) == 1u && cpu->io.input_capture.timer[0] == 1u,
           "alternate capture clock follows source timer prescaler");

    for (index = 0u; index < sizeof(no_sources) / sizeof(no_sources[0]); index++) {
        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, false,
                                                            no_sources[index], false);
        expect(state, dspic33_device_advance(cpu, 3u),
               "advance normal capture timer without sync source");
        expect(state, cpu->io.input_capture.timer[0] == 3u,
               "all no-source encodings select normal timer operation");
    }

    for (index = 0u; index < 2u; index++) {
        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(
            cpu, 0u, index == 0u ? 0x1400u : 0x1800u, true, true, 0u, false);
        expect(state, dspic33_device_advance(cpu, 4u), "advance reserved capture clock selection");
        expect(state, cpu->io.input_capture.timer[0] == 0u,
               "reserved capture clock selections remain inactive");
    }

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, capture_timer_sources[0], true,
                                                        true, 0u, true);
    dspic33_input_capture_test_configure_capture_source(cpu, 1u, capture_timer_sources[0], true,
                                                        true, 0u, true);
    dspic33_input_capture_test_configure_timer_source(cpu, 0u, UINT16_MAX, 0u);
    expect(state, dspic33_device_advance(cpu, 65537u), "advance cascaded alternate capture clock");
    expect(state, cpu->io.input_capture.timer[0] == 1u && cpu->io.input_capture.timer[1] == 1u,
           "matching alternate clocks advance cascaded timer");

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, capture_timer_sources[0], true,
                                                        true, 0u, true);
    dspic33_input_capture_test_configure_capture_source(cpu, 1u, capture_timer_sources[1], true,
                                                        true, 0u, true);
    dspic33_input_capture_test_configure_timer_source(cpu, 0u, UINT16_MAX, 0u);
    dspic33_input_capture_test_configure_timer_source(cpu, 1u, UINT16_MAX, 0u);
    expect(state, dspic33_device_advance(cpu, 5u), "advance mismatched cascaded capture clocks");
    expect(state, cpu->io.input_capture.timer[0] == 0u && cpu->io.input_capture.timer[1] == 0u,
           "cascaded capture requires matching clock selections");
}

static void sync_trigger_cases(TestState* state, Dspic33* cpu) {
    uint8_t source;
    uint16_t target = dspic33_input_capture_test_capture_base(15u);
    for (source = 0u; source < 5u; source++) {
        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, true, false,
                                                            (uint8_t)(11u + source), false);
        dspic33_input_capture_test_configure_timer_source(cpu, source, 1u, 0u);
        expect(state, dspic33_device_advance(cpu, 1u), "advance timer trigger source");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
               "Timer1-5 period sources trigger capture timer");
    }

    for (source = 0u; source < 9u; source++) {
        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, true, false,
                                                            (uint8_t)(1u + source), false);
        dspic33_input_capture_test_configure_compare_source(cpu, source);
        expect(state, dspic33_device_advance(cpu, 1u), "advance output compare trigger source");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
               "OC1-9 period sources trigger capture timer");
    }

    for (source = 0u; source < 8u; source++) {
        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, source, 0x1c00u, true, true, 0u,
                                                            false);
        dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, true, false,
                                                            (uint8_t)(16u + source), false);
        cpu->io.input_capture.timer[source] = 0xfffeu;
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance input capture timer to sync output");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
               "IC1-8 timer sync outputs trigger capture timer");
    }

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, true, true, 0u, false);
    dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, true, false, 16u, false);
    expect(state, dspic33_input_capture_test_rising_edge(cpu, 0u),
           "advance capture without sync output");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) == 0u,
           "capture event does not substitute for IC timer sync output");

    dspic33_write_word(cpu, dspic33_input_capture_test_capture_base(0u), 0u);
    expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
           "turning source module off asserts IC sync output trigger");

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, true, false, 0u, false);
    dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, false, false, 16u,
                                                        false);
    expect(state, dspic33_device_advance(cpu, 3u), "advance beside reset-held IC sync output");
    expect(state, cpu->io.input_capture.timer[15] == 0u,
           "source timer reset holds synchronized destination clear");
    dspic33_write_word(cpu, (uint16_t)(dspic33_input_capture_test_capture_base(0u) + 2u), 0x00c0u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance after source timer leaves reset");
    expect(state, cpu->io.input_capture.timer[15] == 2u,
           "destination resumes after IC sync output negates");

    for (source = 0u; source < 3u; source++) {
        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, true, false,
                                                            (uint8_t)(24u + source), false);
        dspic33_input_capture_test_configure_comparator_source(cpu, source);
        expect(state,
               dspic33_comparator_input(cpu, source, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "advance comparator trigger source");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
               "CMP1-3 rising sources trigger capture timer");
    }

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, true, false, 27u, false);
    dspic33_write_word(cpu, 0x0320u, 0u);
    dspic33_write_word(cpu, 0x0322u, 0u);
    dspic33_write_word(cpu, 0x0328u, 0u);
    dspic33_write_word(cpu, 0x0320u, 0x8010u);
    dspic33_write_word(cpu, 0x0320u, 0x8012u);
    expect(state, dspic33_adc_trigger(cpu, 0u, 1u, 0u) && dspic33_device_advance(cpu, 0u),
           "advance ADC1 trigger source");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
           "accepted ADC1 conversion triggers capture timer");

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, false, 11u, false);
    expect(state, dspic33_device_advance(cpu, 5u), "seed synchronized timer");
    dspic33_input_capture_test_configure_timer_source(cpu, 0u, 1u, 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance synchronized timer source");
    expect(state, cpu->io.input_capture.timer[0] == 6u,
           "synchronization pulse waits for next selected clock edge");
    expect(state, dspic33_device_advance(cpu, 1u), "advance synchronized timer reset edge");
    expect(state, cpu->io.input_capture.timer[0] == 0u,
           "synchronization pulse clears timer on next FP edge");

    dspic33_write_word(cpu, (uint16_t)(dspic33_input_capture_test_capture_base(0u) + 2u), 12u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance after changing synchronous source");
    expect(state, cpu->io.input_capture.timer[0] == 2u,
           "synchronous source selection change preserves running timer");

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, false, 0u, false);
    dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, false, false, 16u,
                                                        false);
    expect(state, dspic33_device_advance(cpu, 2u), "seed IC synchronized timer");
    cpu->io.input_capture.timer[0] = 0xfffeu;
    expect(state, dspic33_device_advance(cpu, 1u), "assert IC timer synchronization output");
    expect(state, cpu->io.input_capture.timer[15] == 3u,
           "IC synchronization output assertion waits for next clock");
    expect(state, dspic33_device_advance(cpu, 1u), "advance IC synchronization reset edge");
    expect(state, cpu->io.input_capture.timer[15] == 0u,
           "IC synchronization output holds destination clear");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance after IC synchronization output negates");
    expect(state, cpu->io.input_capture.timer[15] == 1u,
           "IC synchronized timer resumes after source leaves FFFF");

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, false, 16u, false);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_input_capture_test_rising_edge(cpu, 0u),
           "advance self-synchronized capture");
    expect(state, cpu->io.input_capture.timer[0] == 0u && cpu->io.input_capture.fifo[0].count == 0u,
           "capture module rejects itself as sync source");

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, true, false, 16u, false);
    expect(state, dspic33_input_capture_test_rising_edge(cpu, 0u),
           "advance self-triggered capture");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(dspic33_input_capture_test_capture_base(0u) + 2u)) &
            0x0040u) == 0u &&
               cpu->io.input_capture.fifo[0].count == 0u,
           "capture module rejects itself as trigger source");

    for (source = 24u; source <= 28u; source++) {
        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, false, false, source,
                                                            false);
        expect(state, dspic33_device_advance(cpu, 3u), "advance invalid synchronization selection");
        expect(state, cpu->io.input_capture.timer[15] == 0u,
               "comparator ADC and reserved sources cannot synchronize capture");
    }

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, true, false, 11u, true);
    dspic33_input_capture_test_configure_capture_source(cpu, 1u, 0x1c00u, true, false, 11u, true);
    dspic33_input_capture_test_configure_timer_source(cpu, 0u, 1u, 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance cascaded hardware trigger");
    expect(
        state,
        (dspic33_read_word(cpu, (uint16_t)(dspic33_input_capture_test_capture_base(0u) + 2u)) &
         0x0040u) != 0u &&
            (dspic33_read_word(cpu, (uint16_t)(dspic33_input_capture_test_capture_base(1u) + 2u)) &
             0x0040u) != 0u,
        "shared source releases both cascaded timers");

    dspic33_write_word(cpu, (uint16_t)(dspic33_input_capture_test_capture_base(0u) + 2u),
                       (uint16_t)(CAPTURE_32_BIT | 0x0080u | 11u));
    expect(
        state,
        cpu->io.input_capture.timer[0] == 0u &&
            (dspic33_read_word(cpu, (uint16_t)(dspic33_input_capture_test_capture_base(0u) + 2u)) &
             0x0040u) == 0u,
        "software trigger clear resets selected cascaded half");
    dspic33_write_word(cpu, timer_registers[0], 0u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance repeated cascaded hardware trigger");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(dspic33_input_capture_test_capture_base(0u) + 2u)) &
            0x0040u) != 0u,
           "hardware source retriggers after software status clear");

    {
        Dspic33 stepped;
        uint16_t triggered_batch;
        uint16_t synchronized_batch;
        bool initialized = dspic33_initialize(&stepped);
        expect(state, initialized, "initialize batched source comparison");
        if (!initialized) {
            return;
        }
        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, true, false, 11u,
                                                            false);
        dspic33_input_capture_test_configure_timer_source(cpu, 0u, 3u, 0u);
        expect(state, dspic33_device_advance(cpu, 10u), "advance batched timer trigger source");
        triggered_batch = cpu->io.input_capture.timer[15];
        dspic33_reset(&stepped, 0u);
        dspic33_input_capture_test_configure_capture_source(&stepped, 15u, 0x1c00u, true, false,
                                                            11u, false);
        dspic33_input_capture_test_configure_timer_source(&stepped, 0u, 3u, 0u);
        for (source = 0u; source < 10u; source++) {
            dspic33_device_advance(&stepped, 1u);
        }
        expect(state,
               triggered_batch == stepped.io.input_capture.timer[15] && triggered_batch == 7u,
               "batched and stepped timer trigger timing agree");

        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, false, false, 11u,
                                                            false);
        dspic33_input_capture_test_configure_timer_source(cpu, 0u, 3u, 0u);
        expect(state, dspic33_device_advance(cpu, 10u),
               "advance batched timer synchronization source");
        synchronized_batch = cpu->io.input_capture.timer[15];
        dspic33_reset(&stepped, 0u);
        dspic33_input_capture_test_configure_capture_source(&stepped, 15u, 0x1c00u, false, false,
                                                            11u, false);
        dspic33_input_capture_test_configure_timer_source(&stepped, 0u, 3u, 0u);
        for (source = 0u; source < 10u; source++) {
            dspic33_device_advance(&stepped, 1u);
        }
        expect(state,
               synchronized_batch == stepped.io.input_capture.timer[15] && synchronized_batch == 2u,
               "batched and stepped timer synchronization timing agree");

        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, 7u, 0x1c00u, false, true, 0u,
                                                            false);
        dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, true, 23u,
                                                            false);
        dspic33_input_capture_test_configure_capture_source(cpu, 1u, 0x1c00u, false, true, 16u,
                                                            false);
        cpu->io.input_capture.timer[7] = 0xfffeu;
        expect(state, dspic33_copy(&stepped, cpu), "copy reverse synchronization chain");
        expect(state, dspic33_device_advance(cpu, 3u),
               "advance batched reverse synchronization chain");
        expect(state,
               dspic33_device_advance(&stepped, 1u) && dspic33_device_advance(&stepped, 1u) &&
                   dspic33_device_advance(&stepped, 1u),
               "advance stepped reverse synchronization chain");
        expect(state,
               cpu->io.input_capture.timer[0] == 1u && cpu->io.input_capture.timer[1] == 1u &&
                   cpu->io.input_capture.timer[7] == 1u &&
                   cpu->io.input_capture.timer[0] == stepped.io.input_capture.timer[0] &&
                   cpu->io.input_capture.timer[1] == stepped.io.input_capture.timer[1] &&
                   cpu->io.input_capture.timer[7] == stepped.io.input_capture.timer[7] &&
                   cpu->io.input_capture.sync_output_high ==
                       stepped.io.input_capture.sync_output_high &&
                   (cpu->io.input_capture.sync_output_high & 0x0083u) == 0u,
               "reverse synchronization chain batch and step agree");

        dspic33_reset(cpu, 0u);
        dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, false, false, 1u,
                                                            false);
        dspic33_input_capture_test_configure_compare_source(cpu, 0u);
        expect(state, dspic33_device_advance(cpu, 1u), "queue synchronization reset before copy");
        expect(state, dspic33_copy(&stepped, cpu), "copy pending synchronization reset");
        expect(state,
               dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&stepped, 1u) &&
                   cpu->io.input_capture.timer[15] == 0u &&
                   stepped.io.input_capture.timer[15] == 0u,
               "copy preserves pending synchronization reset");
        dspic33_reset(cpu, 0u);
        expect(state,
               cpu->io.input_capture.sync_reset_pending == 0u &&
                   cpu->io.input_capture.sync_output_high == 0x00ffu,
               "reset clears pending sync and restores off outputs high");
        dspic33_release(&stepped);
    }
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize input capture processor");
    if (initialized) {
        dspic33_input_capture_test_access_cases(&state, &cpu);
        dspic33_input_capture_test_fifo_cases(&state, &cpu);
        dspic33_input_capture_test_interrupt_rate_cases(&state, &cpu);
        dspic33_input_capture_test_zero_interval_overflow_cases(&state, &cpu);
        dspic33_input_capture_test_mode_cases(&state, &cpu);
        dspic33_input_capture_test_power_cases(&state, &cpu);
        dspic33_input_capture_test_paired_cases(&state, &cpu);
        dspic33_input_capture_test_dma_cases(&state, &cpu);
        dspic33_input_capture_test_lifecycle_cases(&state, &cpu);
        dspic33_input_capture_test_pmd_channel_cases(&state, &cpu);
        dspic33_input_capture_test_pmd_lifecycle_cases(&state, &cpu);
        dspic33_input_capture_test_boundary_cases(&state, &cpu);
        timer_source_cases(&state, &cpu);
        sync_trigger_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
