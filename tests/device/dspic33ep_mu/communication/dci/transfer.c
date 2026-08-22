#include "device/dspic33ep_mu/communication/dci/internal.h"

void dspic33_dci_test_pps_qualification_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_INPUTS,
                       (uint16_t)((PPS_CLOCK_PIN << 8u) | PPS_ANALOG_CLOCK_PIN));
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_ANALOG_CLOCK_MASK));
    dspic33_write_word(cpu, GPIO_ANALOG_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) | GPIO_ANALOG_CLOCK_MASK));
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state,
           dspic33_dci_test_drive_mapped_serial_word(cpu, 0xf000u, 4u, true, GPIO_ANALOG_CLOCK_MASK,
                                                     GPIO_CLOCK_MASK) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "analog CSDI selection samples low");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(cpu, GPIO_ANALOG_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) & ~GPIO_ANALOG_CLOCK_MASK));
    expect(state,
           dspic33_dci_test_drive_mapped_serial_word(cpu, 0xf000u, 4u, true, GPIO_ANALOG_CLOCK_MASK,
                                                     GPIO_CLOCK_MASK) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xf000u,
           "digital CSDI selection resumes sampling");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) & ~GPIO_DATA_MASK));
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "output-configured CSDI selection samples low");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_DATA_MASK));
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xf000u,
           "input-configured CSDI selection resumes sampling");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_FRAME, PPS_ANALOG_CLOCK_PIN);
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_ANALOG_CLOCK_MASK));
    dspic33_write_word(cpu, GPIO_ANALOG_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) | GPIO_ANALOG_CLOCK_MASK));
    dspic33_dci_test_configure_external(
        cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY, 4u, 1u, 1u, 0u, 1u);
    dspic33_dci_test_activate_serial_clock(cpu, true, GPIO_CLOCK_MASK);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_ANALOG_CLOCK_MASK, GPIO_ANALOG_CLOCK_MASK);
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) && cpu->io.dci.initialized &&
               !cpu->io.dci.started && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "analog COFS selection cannot start a frame");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(cpu, GPIO_ANALOG_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) & ~GPIO_ANALOG_CLOCK_MASK));
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xf000u,
           "digital COFS selection starts a frame");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) & ~GPIO_FRAME_MASK));
    dspic33_dci_test_configure_external(
        cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY, 4u, 1u, 1u, 0u, 1u);
    dspic33_dci_test_activate_serial_clock(cpu, true, GPIO_CLOCK_MASK);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) && cpu->io.dci.initialized &&
               !cpu->io.dci.started && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "output-configured COFS selection cannot start a frame");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_FRAME_MASK));
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xf000u,
           "input-configured COFS selection starts a frame");
}

void dspic33_dci_test_pps_frame_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME, 4u, 1u, 1u, 0u,
                                        1u);
    dspic33_dci_test_activate_serial_clock(cpu, true, GPIO_CLOCK_MASK);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
    expect(state, dspic33_dci_test_drive_serial_edge(cpu, true, true, GPIO_CLOCK_MASK),
           "sample default-justified DCI frame pulse");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK);
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xa000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "default DJST begins data one serial clock after frame");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(
        cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY, 4u, 1u, 1u, 0u, 1u);
    dspic33_dci_test_activate_serial_clock(cpu, true, GPIO_CLOCK_MASK);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK | GPIO_DATA_MASK,
                       GPIO_FRAME_MASK | GPIO_DATA_MASK);
    expect(state, dspic33_dci_test_drive_serial_bit(cpu, true, true),
           "sample same-cycle-justified DCI frame pulse");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK);
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "DJST begins data during frame serial clock");
    expect(state, dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true),
           "clock falling multi-channel frame transition");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "multi-channel mode ignores falling frame transition");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(
        cpu, DCI_MODE_I2S | DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY, 4u, 1u, 1u,
        0u, 1u);
    dspic33_dci_test_activate_serial_clock(cpu, true, GPIO_CLOCK_MASK);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK | GPIO_DATA_MASK,
                       GPIO_FRAME_MASK | GPIO_DATA_MASK);
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0x8000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x8000u,
           "I2S rising frame edge starts serial word");
    dspic33_read_word(cpu, DCI_RECEIVE_BASE);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK | GPIO_DATA_MASK);
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0x5000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5000u,
           "I2S falling frame edge starts serial word");
}

void dspic33_dci_test_pps_serial_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t mode;
    for (mode = 0u; mode < 2u; mode++) {
        uint8_t width;
        for (width = 4u; width <= 16u; width++) {
            uint8_t rising;
            for (rising = 0u; rising < 2u; rising++) {
                uint8_t immediate;
                for (immediate = 0u; immediate < 2u; immediate++) {
                    uint16_t value = (uint16_t)(0xa55au & dspic33_dci_test_serial_word_mask(width));
                    uint16_t control = DCI_EXTERNAL_FRAME;
                    if (mode != 0u) {
                        control |= DCI_MODE_I2S;
                    }
                    if (rising != 0u) {
                        control |= DCI_SAMPLE_RISING;
                    }
                    if (immediate != 0u) {
                        control |= DCI_DATA_JUSTIFY;
                    }
                    dspic33_reset(cpu, 0u);
                    dspic33_dci_test_configure_serial_pins(cpu);
                    dspic33_dci_test_configure_external(cpu, control, width, 1u, 1u, 0u, 1u);
                    dspic33_dci_test_activate_serial_clock(cpu, rising != 0u, GPIO_CLOCK_MASK);
                    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
                    if (immediate == 0u) {
                        expect(state,
                               dspic33_dci_test_drive_serial_edge(cpu, false, rising != 0u,
                                                                  GPIO_CLOCK_MASK),
                               "clock default-justified PPS matrix frame");
                        if (mode == 0u) {
                            dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK);
                        }
                    }
                    expect(state,
                           dspic33_dci_test_drive_serial_word(cpu, value, width, rising != 0u) &&
                               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == value &&
                               !cpu->io.dci.started,
                           "PPS serial matrix captures framed word");
                }
            }
        }
    }
}

void dspic33_dci_test_pps_startup_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 0u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0xa000u);
    dspic33_write_word(cpu, DCI_CONTROL1,
                       DCI_ENABLE | DCI_EXTERNAL_CLOCK | DCI_SAMPLE_RISING | DCI_DATA_JUSTIFY);
    expect(state,
           cpu->io.dci.serial_startup_bits == 3u && !cpu->io.dci.initialized &&
               !dspic33_dci_test_interrupt_set(cpu, DCI_TRANSFER_IRQ),
           "external DCI waits for three startup clocks");
    expect(state,
           dspic33_dci_test_drive_serial_edge(cpu, false, true, GPIO_CLOCK_MASK) &&
               cpu->io.dci.serial_startup_bits == 2u && !cpu->io.dci.initialized &&
               !dspic33_dci_test_interrupt_set(cpu, DCI_TRANSFER_IRQ),
           "first startup clock leaves DCI buffers unavailable");
    expect(state,
           dspic33_dci_test_drive_serial_edge(cpu, false, true, GPIO_CLOCK_MASK) &&
               cpu->io.dci.serial_startup_bits == 1u && !cpu->io.dci.initialized &&
               !dspic33_dci_test_interrupt_set(cpu, DCI_TRANSFER_IRQ),
           "second startup clock leaves DCI buffers unavailable");
    expect(state,
           dspic33_dci_test_drive_serial_edge(cpu, false, true, GPIO_CLOCK_MASK) &&
               cpu->io.dci.serial_startup_bits == 0u && cpu->io.dci.initialized &&
               cpu->io.dci.started && dspic33_dci_test_interrupt_set(cpu, DCI_TRANSFER_IRQ) &&
               (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_EMPTY) != 0u,
           "third startup clock transfers buffers and raises DCI interrupt");
}

void dspic33_dci_test_pps_internal_input_cases(TestState* state, Dspic33* cpu) {
    uint8_t mode;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_INPUTS, 0u);
    dspic33_dci_input(cpu, UINT16_MAX);
    dspic33_dci_test_configure_internal(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state,
           dspic33_dci_test_drive_internal_pin_slot(cpu, UINT16_MAX, 4u, 12u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "internal DCI VSS selection ignores logical input latch");

    for (mode = DCI_MODE_I2S - 1u; mode <= DCI_MODE_I2S; mode++) {
        uint8_t width;
        for (width = 4u; width <= 16u; width++) {
            uint8_t rising;
            for (rising = 0u; rising < 2u; rising++) {
                uint16_t value = (uint16_t)(0xa55au & dspic33_dci_test_serial_word_mask(width));
                uint16_t control = mode;
                if (rising != 0u) {
                    control |= DCI_SAMPLE_RISING;
                }
                dspic33_reset(cpu, 0u);
                dspic33_dci_test_configure_serial_pins(cpu);
                dspic33_dci_test_configure_internal(cpu, control, width, 1u, 1u, 0u, 1u);
                expect(state,
                       dspic33_dci_test_drive_internal_pin_slot(cpu, value, width, 12u) &&
                           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == value,
                       "internally clocked DCI samples mapped CSDI pin");
            }
        }
    }

    for (mode = DCI_MODE_AC_LINK_16; mode <= DCI_MODE_AC_LINK_20; mode++) {
        uint8_t rising;
        for (rising = 0u; rising < 2u; rising++) {
            uint16_t control = mode;
            if (rising != 0u) {
                control |= DCI_SAMPLE_RISING;
            }
            dspic33_reset(cpu, 0u);
            dspic33_dci_test_configure_serial_pins(cpu);
            dspic33_dci_test_configure_internal(cpu, control, 4u, 1u, 1u, 0u, 1u);
            expect(state,
                   dspic33_dci_test_drive_internal_pin_slot(cpu, 0x5aa5u, 16u, 12u) &&
                       dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5aa5u,
                   "internally clocked AC-Link samples mapped CSDI pin");
        }
    }
}

void dspic33_dci_test_pps_internal_frame_cases(TestState* state, Dspic33* cpu) {
    bool high;
    uint8_t mode;
    for (mode = 0u; mode < 4u; mode++) {
        uint8_t rising;
        for (rising = 0u; rising < 2u; rising++) {
            uint8_t immediate;
            for (immediate = 0u; immediate < 2u; immediate++) {
                uint16_t control = (uint16_t)(mode | DCI_EXTERNAL_FRAME);
                uint16_t value = mode >= DCI_MODE_AC_LINK_16 ? 0x5aa5u : 0xa000u;
                uint8_t width = mode == DCI_MODE_AC_LINK_16   ? 16u
                                : mode == DCI_MODE_AC_LINK_20 ? 16u
                                                              : 4u;
                uint64_t delay;
                if (rising != 0u) {
                    control |= DCI_SAMPLE_RISING;
                }
                if (immediate != 0u) {
                    control |= DCI_DATA_JUSTIFY;
                }
                delay = mode < DCI_MODE_AC_LINK_16 && immediate != 0u ? 0u : 4u;
                dspic33_reset(cpu, 0u);
                dspic33_dci_test_configure_serial_pins(cpu);
                dspic33_dci_test_configure_internal(cpu, control, 4u, 1u, 1u, 0u, 1u);
                expect(state,
                       dspic33_device_advance(cpu, 12u) && cpu->io.dci.initialized &&
                           !cpu->io.dci.started,
                       "internal CSCK waits for mapped COFS edge");
                dspic33_gpio_drive(cpu, GPIO_PORT_D,
                                   GPIO_FRAME_MASK |
                                       ((value & 0x8000u) != 0u ? GPIO_DATA_MASK : 0u),
                                   GPIO_FRAME_MASK | GPIO_DATA_MASK);
                expect(state,
                       dspic33_dci_test_drive_internal_pin_slot(cpu, value, width, delay) &&
                           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == value,
                       "mapped COFS starts internally clocked DCI frame");
            }
        }
    }

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x000cu);
    dspic33_dci_test_configure_internal(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME, 4u, 1u, 1u, 0u,
                                        1u);
    expect(state,
           dspic33_device_advance(cpu, 14u) && !cpu->io.dci.started &&
               dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) && !high,
           "internal CSCK remains active while waiting for external COFS");
}

void dspic33_dci_test_pps_external_frame_output_cases(TestState* state, Dspic33* cpu) {
    bool high;
    uint8_t immediate;
    for (immediate = 0u; immediate < 2u; immediate++) {
        uint16_t justify = immediate != 0u ? DCI_DATA_JUSTIFY : 0u;
        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_serial_pins(cpu);
        dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d00u);
        dspic33_dci_test_configure_external(cpu, (uint16_t)(DCI_SAMPLE_RISING | justify), 4u, 1u,
                                            1u, 0u, 0u);
        dspic33_dci_test_activate_serial_clock(cpu, true, GPIO_CLOCK_MASK);
        expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && high,
               "external CSCK multi-channel master asserts COFS");
        expect(state,
               dspic33_dci_test_drive_serial_edge(cpu, false, true, GPIO_CLOCK_MASK) &&
                   dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
               "external CSCK multi-channel master emits one-clock COFS");

        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_serial_pins(cpu);
        dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d00u);
        dspic33_dci_test_configure_external(
            cpu, (uint16_t)(DCI_MODE_I2S | DCI_SAMPLE_RISING | justify), 4u, 1u, 1u, 0u, 0u);
        dspic33_dci_test_activate_serial_clock(cpu, true, GPIO_CLOCK_MASK);
        expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && high,
               "external CSCK I2S master starts with right-channel COFS");
        if (immediate == 0u) {
            dspic33_dci_test_drive_serial_edge(cpu, false, true, GPIO_CLOCK_MASK);
        }
        expect(state,
               dspic33_dci_test_drive_serial_word(cpu, 0u, 4u, true) &&
                   dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
               "external CSCK I2S master toggles COFS at half-frame boundary");
    }

    {
        uint8_t mode;
        for (mode = DCI_MODE_AC_LINK_16; mode <= DCI_MODE_AC_LINK_20; mode++) {
            for (immediate = 0u; immediate < 2u; immediate++) {
                uint16_t control = (uint16_t)(mode | DCI_SAMPLE_RISING |
                                              (immediate != 0u ? DCI_DATA_JUSTIFY : 0u));
                dspic33_reset(cpu, 0u);
                dspic33_dci_test_configure_serial_pins(cpu);
                dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d00u);
                dspic33_dci_test_configure_external(cpu, control, 4u, 1u, 1u, 0u, 0u);
                dspic33_dci_test_activate_serial_clock(cpu, true, GPIO_CLOCK_MASK);
                expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && high,
                       "external CSCK AC-Link master asserts tag COFS");
                expect(state,
                       dspic33_dci_test_drive_serial_word(cpu, 0u, 16u, true) &&
                           dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
                       "AC-Link master negates COFS after sixteen clocks");
                expect(state,
                       dspic33_dci_test_drive_serial_word(cpu, 0u, 240u, true) &&
                           dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && high,
                       "AC-Link master starts next 256-clock frame independent of DJST");
            }
        }
    }
}

void dspic33_dci_test_pps_internal_frame_output_cases(TestState* state, Dspic33* cpu) {
    bool high;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d00u);
    dspic33_dci_test_configure_internal(cpu, DCI_MODE_I2S, 4u, 1u, 1u, 0u, 0u);
    expect(state,
           dspic33_device_advance(cpu, 7u) && dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) &&
               !high,
           "default I2S COFS remains low before final startup clock");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) &&
               high,
           "default I2S COFS rises one clock before right-channel data");
    expect(state,
           dspic33_device_advance(cpu, 4u) && dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) &&
               high,
           "default I2S COFS has no extra transition at data boundary");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d00u);
    dspic33_dci_test_configure_internal(cpu, DCI_MODE_AC_LINK_16, 4u, 1u, 1u, 0u, 0u);
    expect(state,
           dspic33_device_advance(cpu, 8u) && dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) &&
               high,
           "internal AC-Link COFS asserts for tag interval");
    expect(state,
           dspic33_device_advance(cpu, 63u) && dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) &&
               high,
           "internal AC-Link COFS remains high through fifteen data clocks");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) &&
               !high,
           "internal AC-Link COFS negates after sixteen clocks");
}

void dspic33_dci_test_pps_bcg_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool high;
    bool copy_high;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x000cu);
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state,
           dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) && high &&
               (dspic33_read_word(cpu, DCI_CONTROL1) & DCI_ENABLE) == 0u,
           "nonzero BCG drives CSCK while DCI is disabled");
    expect(state,
           dspic33_device_advance(cpu, 2u) && dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) &&
               !high,
           "standalone BCG reaches falling half-cycle");
    expect(state,
           dspic33_device_advance(cpu, 2u) && dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) &&
               high,
           "standalone BCG completes full cycle");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 9u) && dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) &&
               high,
           "Sleep retains standalone BCG phase");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 2u) && dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) &&
               !high,
           "wake resumes standalone BCG phase");
    expect(state, dspic33_initialize(&copy), "initialize standalone BCG copy");
    expect(state,
           dspic33_copy(&copy, cpu) && dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) &&
               dspic33_dci_pin(&copy, PPS_CLOCK_OUTPUT_PIN, &copy_high) && high == copy_high,
           "copy preserves standalone BCG phase");
    dspic33_release(&copy);
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high),
           "PMD releases standalone BCG output");
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high),
           "PMD clear restores standalone BCG output");

    dspic33_write_word(cpu, DCI_CONTROL1, DCI_STOP_IDLE);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 7u) && dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high),
           "DCISIDL retains standalone BCG phase in Idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) &&
               high,
           "Idle wake resumes standalone BCG phase");

    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               !dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) &&
               dspic33_read_word(cpu, DCI_CONTROL3) == 0u,
           "warm reset clears standalone BCG output");
}

void dspic33_dci_test_pps_internal_sample_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    uint8_t bit;
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_internal(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_DATA_MASK, GPIO_DATA_MASK);
    expect(state,
           dspic33_device_advance(cpu, 12u) && cpu->io.dci.serial_bits == 1u &&
               cpu->events.count == 2u,
           "internal CSDI sampler retains independent bit and word events");
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.dci.pmd_disabled && cpu->events.count == 2u &&
               cpu->events.items[0].paused && cpu->events.items[1].paused,
           "PMD pauses internal CSDI sample and completion events");
    expect(state,
           dspic33_device_advance(cpu, 40u) && cpu->io.dci.serial_bits == 1u &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "PMD holds physical CSDI shift state");
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state, dspic33_device_advance(cpu, 1u) && !cpu->io.dci.pmd_disabled,
           "PMD clear resumes physical CSDI sampler");
    for (bit = 1u; bit < 4u; bit++) {
        uint16_t high = bit == 2u ? GPIO_DATA_MASK : 0u;
        dspic33_gpio_drive(cpu, GPIO_PORT_D, high, GPIO_DATA_MASK);
        dspic33_device_advance(cpu, bit == 1u ? 3u : 4u);
    }
    expect(state,
           dspic33_device_advance(cpu, 4u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "resumed physical CSDI sampler completes retained word");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_internal(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_DATA_MASK, GPIO_DATA_MASK);
    expect(state, dspic33_device_advance(cpu, 12u), "advance internal CSDI sampler before copy");
    expect(state, dspic33_initialize(&copy), "initialize internal CSDI copy");
    expect(state, dspic33_copy(&copy, cpu), "copy active internal CSDI sampler");
    for (bit = 1u; bit < 4u; bit++) {
        dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_DATA_MASK);
        dspic33_gpio_drive(&copy, GPIO_PORT_D, GPIO_DATA_MASK, GPIO_DATA_MASK);
        dspic33_device_advance(cpu, 4u);
        dspic33_device_advance(&copy, 4u);
    }
    expect(state,
           dspic33_device_advance(cpu, 4u) && dspic33_device_advance(&copy, 4u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x8000u &&
               dspic33_read_word(&copy, DCI_RECEIVE_BASE) == 0xf000u,
           "copied CSDI samplers shift physical inputs independently");
    dspic33_release(&copy);
}

void dspic33_dci_test_pps_selection_cases(TestState* state, Dspic33* cpu) {
    uint8_t selection;
    bool high;
    for (selection = 0u; selection < 16u; selection++) {
        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_serial_pins(cpu);
        dspic33_write_word(cpu, DCI_PPS_INPUTS, (uint16_t)((selection << 8u) | PPS_DATA_PIN));
        dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
        expect(state,
               dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) &&
                   !cpu->io.dci.initialized,
               "DCI virtual and reserved clock selections remain inaccessible");

        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_serial_pins(cpu);
        dspic33_write_word(cpu, DCI_PPS_INPUTS, (uint16_t)((PPS_CLOCK_PIN << 8u) | selection));
        dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
        expect(state,
               dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) &&
                   dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
               "DCI virtual and reserved data selections resolve low");

        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_serial_pins(cpu);
        dspic33_write_word(cpu, DCI_PPS_FRAME, selection);
        dspic33_dci_test_configure_external(
            cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY, 4u, 1u, 1u, 0u, 1u);
        dspic33_dci_test_activate_serial_clock(cpu, true, GPIO_CLOCK_MASK);
        dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
        expect(state,
               dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) &&
                   cpu->io.dci.initialized && !cpu->io.dci.started &&
                   dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
               "DCI virtual and reserved frame selections remain inaccessible");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0xa000u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_DATA_JUSTIFY);
    dspic33_device_advance(cpu, 12u);
    for (selection = 0u; selection < 64u; selection++) {
        dspic33_write_word(cpu, DCI_PPS_DATA_OUTPUT, (uint16_t)((uint16_t)selection << 8u));
        expect(state,
               dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) ==
                   (selection >= 11u && selection <= 13u),
               "DCI RPOR function admission matches target table");
    }
}

void dspic33_dci_test_pps_output_cases(TestState* state, Dspic33* cpu) {
    bool high;
    expect(state,
           !dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, NULL) && !dspic33_dci_pin(cpu, 0u, &high),
           "DCI pin API rejects invalid queries");
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_DATA_OUTPUT, 0x0b00u);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME, 4u, 1u, 1u, 1u,
                                        0u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0xa000u);
    dspic33_dci_test_activate_serial_clock(cpu, true, GPIO_CLOCK_MASK);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
    expect(state, dspic33_dci_test_drive_serial_edge(cpu, false, true, GPIO_CLOCK_MASK),
           "start PPS DCI data output frame");
    expect(state, dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) && high,
           "CSDO RPOR mapping drives transmit MSb");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    expect(state, dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) && high,
           "CSDO holds transmit MSb before first data sample");
    expect(state, dspic33_dci_test_drive_serial_bit(cpu, true, true),
           "sample first PPS DCI output bit");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    expect(state, dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) && !high,
           "CSDO advances on opposite serial clock edge");
    dspic33_write_word(cpu, DCI_PPS_DATA_OUTPUT, 0x000bu);
    expect(state,
           !dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) &&
               dspic33_dci_pin(cpu, PPS_FRAME_PIN, &high) && !high,
           "CSDO output follows live RPOR remapping");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_DATA_OUTPUT, 0x0b00u);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING | DCI_TRISTATE, 4u, 1u, 1u, 0u, 0u);
    expect(state, dspic33_dci_test_drive_serial_bit(cpu, false, true),
           "start tri-stated PPS DCI slot");
    expect(state, !dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high),
           "CSDOM releases disabled CSDO slot");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d0cu);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0xa000u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_DATA_JUSTIFY);
    expect(state,
           dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) && high &&
               dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
           "master DCI drives CSCK during startup with inactive COFS");
    expect(state, dspic33_device_advance(cpu, 12u), "advance master DCI through startup clocks");
    expect(state, dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) && high,
           "master DCI begins data word with asserted CSCK");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && high,
           "master multi-channel DCI asserts COFS for first clock");
    expect(state, dspic33_device_advance(cpu, 4u), "advance master DCI beyond first serial clock");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
           "master multi-channel DCI negates COFS after first clock");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_DATA_OUTPUT, 0x0b00u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0xa000u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_DATA_JUSTIFY | DCI_SAMPLE_RISING);
    expect(state, dspic33_device_advance(cpu, 12u),
           "advance rising-sample master through startup clocks");
    expect(state, dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) && high,
           "rising-sample master presents first CSDO bit before rising edge");
    expect(state, dspic33_device_advance(cpu, 2u), "advance rising-sample master to falling edge");
    expect(state, dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) && !high,
           "rising-sample master advances CSDO on falling edge");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d0cu);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 8u),
           "advance default-justified master to final startup clock");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && high,
           "default DJST asserts COFS one clock before data");
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance default-justified master to data boundary");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
           "default DJST negates COFS when first data bit begins");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d0cu);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_MODE_I2S | DCI_DATA_JUSTIFY);
    expect(state, dspic33_device_advance(cpu, 12u), "advance I2S master through startup clocks");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && high,
           "I2S master drives right-channel COFS high first");
    expect(state, dspic33_device_advance(cpu, 16u),
           "advance I2S master through right-channel word");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
           "I2S master toggles COFS for left-channel word");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d0cu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME, 4u, 1u, 1u, 0u,
                                        0u);
    expect(state,
           !dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) &&
               !dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high),
           "slave DCI releases externally directed CSCK and COFS outputs");
}
