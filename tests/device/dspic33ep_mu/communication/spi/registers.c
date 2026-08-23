#include "device/dspic33ep_mu/communication/spi/internal.h"

bool dspic33_spi_test_interrupt_flag(Dspic33* cpu, uint8_t interrupt_number) {
    const uint16_t status_address = (uint16_t)(0x0800u + (interrupt_number / 16u) * 2u);

    return (dspic33_read_word(cpu, status_address) & (uint16_t)(1u << (interrupt_number % 16u))) !=
           0u;
}

void dspic33_spi_test_clear_interrupt(Dspic33* cpu, uint8_t interrupt_number) {
    const uint16_t status_address = (uint16_t)(0x0800u + (interrupt_number / 16u) * 2u);

    dspic33_write_word(cpu, status_address,
                       (uint16_t)(dspic33_read_word(cpu, status_address) &
                                  ~(uint16_t)(1u << (interrupt_number % 16u))));
}

bool dspic33_spi_test_transfer_interrupt_after_cycle(Dspic33* cpu, uint8_t interrupt_number) {
    return !dspic33_spi_test_interrupt_flag(cpu, interrupt_number) &&
           dspic33_device_advance(cpu, 1u) &&
           dspic33_spi_test_interrupt_flag(cpu, interrupt_number);
}

uint64_t dspic33_spi_test_transfer_cycles(uint16_t spi_control) {
    static const uint8_t primary_cycles[] = {64u, 16u, 4u, 1u};
    const uint8_t secondary_divisor = (uint8_t)(8u - ((spi_control >> 2u) & 7u));
    const uint8_t transfer_width = (spi_control & 0x0400u) != 0u ? 16u : 8u;

    return (uint64_t)transfer_width * primary_cycles[spi_control & 3u] * secondary_divisor;
}

void dspic33_spi_test_configure_spi(Dspic33* cpu, uint8_t channel_index, uint16_t spi_control,
                                    uint16_t frame_control, uint8_t interrupt_mode) {
    const uint16_t spi_base = bases[channel_index];

    dspic33_write_word(cpu, spi_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(spi_base + 2u), spi_control);
    dspic33_write_word(cpu, (uint16_t)(spi_base + 4u), frame_control);
    dspic33_write_word(cpu, spi_base, (uint16_t)(0x8000u | ((uint16_t)interrupt_mode << 2u)));
}

static bool drive_pps_spi_word(Dspic33* cpu, uint16_t value, uint8_t transfer_width,
                               uint16_t data_mask, uint16_t clock_mask) {
    for (uint8_t bit_index = 0u; bit_index < transfer_width; bit_index++) {
        const uint16_t data_value =
            (value & (uint16_t)(1u << (transfer_width - bit_index - 1u))) != 0u ? data_mask : 0u;

        if (!dspic33_gpio_drive(cpu, 3u, data_value, data_mask) ||
            !dspic33_gpio_drive(cpu, 3u, clock_mask, clock_mask) ||
            !dspic33_gpio_drive(cpu, 3u, 0u, clock_mask)) {
            return false;
        }
    }
    return true;
}

uint16_t dspic33_spi_test_dma_base(uint8_t channel_index) {
    return (uint16_t)(0x0b00u + channel_index * 0x10u);
}

void dspic33_spi_test_configure_dma(Dspic33* cpu, uint8_t channel_index, uint16_t dma_control,
                                    uint8_t request_source, uint32_t memory_address,
                                    uint16_t peripheral_address, uint16_t transfer_count) {
    const uint16_t channel_base = dspic33_spi_test_dma_base(channel_index);

    dspic33_write_word(cpu, channel_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 2u), request_source);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 4u), (uint16_t)memory_address);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 6u), (uint16_t)(memory_address >> 16u));
    dspic33_write_word(cpu, (uint16_t)(channel_base + 0x0cu), peripheral_address);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 0x0eu), transfer_count);
    dspic33_write_word(cpu, channel_base, (uint16_t)(dma_control | 0x8000u));
}

void dspic33_spi_test_register_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        const uint16_t spi_base = bases[channel_index];

        expect(state, dspic33_read_word(cpu, spi_base) == 0u, "status reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 2u)) == 0u, "control one reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 4u)) == 0u, "control two reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 8u)) == 0u, "buffer reset");
        dspic33_write_word(cpu, spi_base, 0xffffu);
        expect(state, dspic33_read_word(cpu, spi_base) == 0xa01cu, "status mask");
        dspic33_write_word(cpu, (uint16_t)(spi_base + 2u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 2u)) == 0x1fffu,
               "control one mask");
        dspic33_write_word(cpu, (uint16_t)(spi_base + 4u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 4u)) == 0xe003u,
               "control two mask");
        dspic33_write_word(cpu, (uint16_t)(spi_base + 2u), 0x0200u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 2u)) == 0u,
               "slave sample phase forced clear");
        dspic33_write_word(cpu, (uint16_t)(spi_base + 2u), 0x0220u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 2u)) == 0x0220u,
               "master sample phase writable");
    }
}

void dspic33_spi_test_split_buffer_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        const uint16_t spi_base = bases[channel_index];
        const uint16_t spi_control = 0x043bu;
        const uint16_t first_transmit = (uint16_t)(0xa100u + channel_index);
        const uint16_t second_transmit = (uint16_t)(0xc200u + channel_index);
        const uint16_t received_value = (uint16_t)(0x5b00u + channel_index);
        const uint64_t transfer_cycles = dspic33_spi_test_transfer_cycles(spi_control);
        bool response_scheduled;
        bool transfer_advanced;

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel_index, spi_control, 0u, 0u);
        response_scheduled =
            dspic33_spi_receive(cpu, channel_index, received_value, transfer_cycles);
        dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), first_transmit);
        expect(state, response_scheduled && dspic33_read_word(cpu, (uint16_t)(spi_base + 8u)) == 0u,
               "accepted transmit is not receive data");
        expect(state,
               cpu->io.spi_shift[channel_index] == first_transmit &&
                   cpu->io.spi_tx[channel_index].count == 2u &&
                   cpu->io.spi_tx[channel_index].bytes[0] == (uint8_t)first_transmit &&
                   cpu->io.spi_tx[channel_index].bytes[1] == (uint8_t)(first_transmit >> 8u),
               "accepted transmit enters transmit path");
        transfer_advanced = dspic33_device_advance(cpu, transfer_cycles);
        dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), second_transmit);
        expect(state,
               transfer_advanced &&
                   dspic33_read_word(cpu, (uint16_t)(spi_base + 8u)) == received_value,
               "queued receive survives transmit write");
        expect(state,
               cpu->io.spi_rx_fifo[channel_index].count == 0u &&
                   (dspic33_read_word(cpu, spi_base) & 0x0001u) == 0u &&
                   cpu->io.spi_shift[channel_index] == second_transmit &&
                   cpu->io.spi_tx[channel_index].count == 4u,
               "receive drain preserves next transmit");
    }
}

void dspic33_spi_test_transmit_output_cases(TestState* state, Dspic33* cpu) {
    uint8_t output_byte;

    for (uint8_t channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        const uint16_t spi_base = bases[channel_index];
        const uint16_t transmitted_value = (uint16_t)(0xa500u + channel_index);

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel_index, 0x043bu, 0u, 0u);
        dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), transmitted_value);

        expect(state,
               dspic33_spi_transmit(cpu, channel_index, &output_byte) &&
                   output_byte == (uint8_t)transmitted_value,
               "transmit output low byte");
        expect(state,
               dspic33_spi_transmit(cpu, channel_index, &output_byte) &&
                   output_byte == (uint8_t)(transmitted_value >> 8u) &&
                   !dspic33_spi_transmit(cpu, channel_index, &output_byte),
               "transmit output high byte and empty state");
    }
    expect(state,
           !dspic33_spi_transmit(cpu, DSPIC33_SPI_COUNT, &output_byte) &&
               !dspic33_spi_transmit(cpu, 0u, NULL),
           "transmit output rejects invalid requests");
}

void dspic33_spi_test_receive_only_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        for (uint8_t master_mode = 0u; master_mode < 2u; master_mode++) {
            for (uint8_t word_mode = 0u; word_mode < 2u; word_mode++) {
                const uint16_t spi_base = bases[channel_index];
                const uint16_t spi_control = (uint16_t)(0x081bu | ((uint16_t)master_mode << 5u) |
                                                        ((uint16_t)word_mode << 10u));
                const uint16_t received_value =
                    (uint16_t)(0xd500u | ((uint16_t)channel_index << 4u) |
                               ((uint16_t)master_mode << 3u) | word_mode);
                const uint16_t expected_value =
                    word_mode != 0u ? received_value : received_value & 0x00ffu;
                const uint64_t transfer_cycles =
                    master_mode != 0u ? dspic33_spi_test_transfer_cycles(spi_control) : 1u;
                uint8_t output_byte;
                bool response_scheduled;

                dspic33_reset(cpu, 0u);
                dspic33_spi_test_configure_spi(cpu, channel_index, spi_control, 0u, 0u);
                response_scheduled =
                    dspic33_spi_receive(cpu, channel_index, received_value, transfer_cycles);
                dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), 0xa55au);

                expect(state,
                       cpu->io.spi_tx[channel_index].count == 0u &&
                           !dspic33_spi_transmit(cpu, channel_index, &output_byte),
                       "receive-only suppresses serial output");
                expect(state, response_scheduled && dspic33_device_advance(cpu, transfer_cycles),
                       "receive-only transfer completes");
                expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 8u)) == expected_value,
                       "receive-only retains input");
                expect(state,
                       dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[channel_index]),
                       "receive-only raises transfer interrupt");
                expect(state, cpu->io.spi_tx[channel_index].count == 0u,
                       "receive-only transfer emits no output");

                dspic33_spi_test_clear_interrupt(cpu, irqs[channel_index]);
                dspic33_write_word(cpu, (uint16_t)(spi_base + 2u),
                                   (uint16_t)(spi_control & ~0x0800u));
                dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), 0x5aa5u);
                expect(state,
                       dspic33_spi_transmit(cpu, channel_index, &output_byte) &&
                           output_byte == 0xa5u &&
                           cpu->io.spi_tx[channel_index].count == (word_mode != 0u ? 1u : 0u),
                       "serial output resumes after receive-only mode");
            }
        }
    }
}

void dspic33_spi_test_physical_slave_input_cases(TestState* state, Dspic33* cpu, Dspic33* copy) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        for (uint8_t word_mode = 0u; word_mode < 2u; word_mode++) {
            for (uint8_t clock_polarity = 0u; clock_polarity < 2u; clock_polarity++) {
                for (uint8_t clock_edge = 0u; clock_edge < 2u; clock_edge++) {
                    const uint16_t spi_base = bases[channel_index];
                    const uint16_t spi_control =
                        (uint16_t)(((uint16_t)word_mode << 10u) | ((uint16_t)clock_edge << 8u) |
                                   0x0080u | ((uint16_t)clock_polarity << 6u) | 0x001bu);
                    const uint16_t received_value =
                        (uint16_t)(0xa500u | ((uint16_t)channel_index << 4u) |
                                   ((uint16_t)clock_polarity << 2u) | ((uint16_t)clock_edge << 1u) |
                                   word_mode);
                    const uint16_t expected_value =
                        word_mode != 0u ? received_value : received_value & 0x00ffu;
                    const uint8_t transfer_width = word_mode != 0u ? 16u : 8u;
                    const bool idle_clock = clock_polarity != 0u;
                    bool input_accepted = true;

                    dspic33_reset(cpu, 0u);
                    input_accepted &=
                        dspic33_spi_pin_input(cpu, channel_index, idle_clock, false, false);
                    dspic33_spi_test_configure_spi(cpu, channel_index, spi_control, 0u, 0u);
                    for (uint8_t bit_index = 0u; bit_index < transfer_width; bit_index++) {
                        const bool data_high =
                            (received_value &
                             (uint16_t)(1u << (transfer_width - bit_index - 1u))) != 0u;

                        input_accepted &= dspic33_spi_pin_input(cpu, channel_index, !idle_clock,
                                                                data_high, false);
                        input_accepted &=
                            dspic33_spi_pin_input(cpu, channel_index, idle_clock, data_high, false);
                        if (bit_index + 1u != transfer_width) {
                            input_accepted &=
                                !dspic33_spi_test_interrupt_flag(cpu, irqs[channel_index]);
                        }
                    }

                    expect(state, input_accepted, "physical slave accepts exact clock edges");
                    expect(
                        state,
                        dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[channel_index]) &&
                            dspic33_read_word(cpu, (uint16_t)(spi_base + 8u)) == expected_value,
                        "physical slave receives serial input");

                    dspic33_spi_test_clear_interrupt(cpu, irqs[channel_index]);
                    input_accepted =
                        dspic33_spi_pin_input(cpu, channel_index, idle_clock, false, true);
                    for (uint8_t bit_index = 0u; bit_index < transfer_width; bit_index++) {
                        input_accepted &=
                            dspic33_spi_pin_input(cpu, channel_index, !idle_clock, true, true);
                        input_accepted &=
                            dspic33_spi_pin_input(cpu, channel_index, idle_clock, true, true);
                    }
                    expect(state,
                           input_accepted &&
                               !dspic33_spi_test_interrupt_flag(cpu, irqs[channel_index]) &&
                               (dspic33_read_word(cpu, spi_base) & 1u) == 0u,
                           "deselected physical slave ignores serial input");
                }
            }
        }
    }
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        for (uint8_t clock_polarity = 0u; clock_polarity < 2u; clock_polarity++) {
            const uint16_t spi_base = bases[channel_index];
            const uint16_t received_value =
                (uint16_t)(0x0060u | ((uint16_t)channel_index << 1u) | clock_polarity);
            const bool active_select = clock_polarity != 0u;
            bool input_accepted = true;

            dspic33_reset(cpu, 0u);
            input_accepted &=
                dspic33_spi_pin_input(cpu, channel_index, false, false, !active_select);
            dspic33_spi_test_configure_spi(cpu, channel_index, 0x001bu,
                                           (uint16_t)(0xc000u | ((uint16_t)clock_polarity << 13u)),
                                           0u);
            input_accepted &=
                dspic33_spi_pin_input(cpu, channel_index, false, false, active_select);
            for (uint8_t bit_index = 0u; bit_index < 8u; bit_index++) {
                const bool data_high = (received_value & (uint16_t)(0x0080u >> bit_index)) != 0u;
                input_accepted &=
                    dspic33_spi_pin_input(cpu, channel_index, true, data_high, active_select);
                input_accepted &=
                    dspic33_spi_pin_input(cpu, channel_index, false, data_high, active_select);
            }
            expect(state,
                   input_accepted &&
                       dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[channel_index]) &&
                       dspic33_read_word(cpu, (uint16_t)(spi_base + 8u)) == received_value,
                   "framed slave accepts selected physical input");

            dspic33_spi_test_clear_interrupt(cpu, irqs[channel_index]);
            input_accepted =
                dspic33_spi_pin_input(cpu, channel_index, false, false, !active_select);
            for (uint8_t bit_index = 0u; bit_index < 8u; bit_index++) {
                input_accepted &=
                    dspic33_spi_pin_input(cpu, channel_index, true, true, !active_select);
                input_accepted &=
                    dspic33_spi_pin_input(cpu, channel_index, false, true, !active_select);
            }
            expect(state,
                   input_accepted && !dspic33_spi_test_interrupt_flag(cpu, irqs[channel_index]),
                   "framed slave rejects inactive physical input");
        }
    }
    expect(state, !dspic33_spi_pin_input(cpu, DSPIC33_SPI_COUNT, false, false, false),
           "physical slave rejects invalid channel");

    dspic33_reset(cpu, 0u);
    dspic33_spi_pin_input(cpu, 0u, false, false, false);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x049bu, 0u, 0u);
    for (uint8_t bit_index = 0u; bit_index < 8u; bit_index++) {
        const bool data_high = (0xa55au & (uint16_t)(1u << (15u - bit_index))) != 0u;

        dspic33_spi_pin_input(cpu, 0u, true, data_high, false);
        dspic33_spi_pin_input(cpu, 0u, false, data_high, false);
    }

    expect(state, dspic33_copy(copy, cpu), "copy partial physical slave input");
    expect(state, copy->io.spi_pin_bits[0] == 8u && copy->io.spi_pin_receive[0] == 0x00a5u,
           "copied physical slave retains partial word");
    for (uint8_t bit_index = 8u; bit_index < 16u; bit_index++) {
        const bool data_high = (0xa55au & (uint16_t)(1u << (15u - bit_index))) != 0u;

        dspic33_spi_pin_input(copy, 0u, true, data_high, false);
        dspic33_spi_pin_input(copy, 0u, false, data_high, false);
    }

    expect(state,
           dspic33_spi_test_transfer_interrupt_after_cycle(copy, irqs[0]) &&
               dspic33_read_word(copy, 0x0248u) == 0xa55au,
           "copied physical slave completes independently");
    expect(state,
           !dspic33_spi_test_interrupt_flag(cpu, irqs[0]) && cpu->io.spi_pin_bits[0] == 8u &&
               cpu->io.spi_pin_receive[0] == 0x00a5u,
           "source physical slave remains partial");

    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.spi_pin_bits[0] == 0u && cpu->io.spi_pin_receive[0] == 0u &&
               cpu->io.spi_pin_clock_high == 0u && cpu->io.spi_pin_data_high == 0u &&
               cpu->io.spi_pin_select_high == 0u,
           "reset clears physical slave state");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x001bu, 0u, 0u);
    cpu->device_cycles = UINT64_MAX;
    for (uint8_t bit_index = 0u; bit_index < 8u; bit_index++) {
        dspic33_spi_pin_input(cpu, 0u, true, true, false);
        dspic33_spi_pin_input(cpu, 0u, false, true, false);
    }
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               !dspic33_spi_test_interrupt_flag(cpu, irqs[0]) &&
               dspic33_read_word(cpu, 0x0248u) == 0xffu,
           "transfer interrupt scheduling failure is fail-closed");
}

void dspic33_spi_test_pps_slave_input_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t channels[] = {0u, 2u, 3u};
    static const uint16_t input_registers[] = {0x06c8u, 0x06dau, 0x06deu};
    static const uint16_t select_registers[] = {0x06cau, 0x06dcu, 0x06e0u};
    for (size_t mapping_index = 0u; mapping_index < sizeof(channels) / sizeof(channels[0]);
         mapping_index++) {
        for (uint8_t word_mode = 0u; word_mode < 2u; word_mode++) {
            const uint8_t channel_index = channels[mapping_index];
            const uint16_t spi_base = bases[channel_index];
            const uint16_t spi_control = (uint16_t)(0x019bu | ((uint16_t)word_mode << 10u));
            const uint16_t received_value = word_mode != 0u ? 0xa55au : 0x005au;
            const uint8_t transfer_width = word_mode != 0u ? 16u : 8u;

            dspic33_reset(cpu, 0u);
            dspic33_write_word(cpu, 0x0e3eu,
                               (uint16_t)(dspic33_read_word(cpu, 0x0e3eu) & ~0x0007u));
            dspic33_write_word(cpu, 0x0e30u, (uint16_t)(dspic33_read_word(cpu, 0x0e30u) | 0x0007u));
            dspic33_gpio_drive(cpu, 3u, 0x0004u, 0x0007u);
            dspic33_write_word(cpu, input_registers[mapping_index], 0x4140u);
            dspic33_write_word(cpu, select_registers[mapping_index], 66u);
            dspic33_spi_test_configure_spi(cpu, channel_index, spi_control, 0u, 0u);
            expect(state,
                   dspic33_gpio_drive(cpu, 3u, 0u, 0x0004u) &&
                       drive_pps_spi_word(cpu, received_value, transfer_width, 0x0001u, 0x0002u) &&
                       dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[channel_index]) &&
                       dspic33_read_word(cpu, (uint16_t)(spi_base + 8u)) == received_value,
                   "mapped PPS slave receives serial input");
        }
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e3eu, (uint16_t)(dspic33_read_word(cpu, 0x0e3eu) & ~0x005fu));
    dspic33_write_word(cpu, 0x0e30u, (uint16_t)(dspic33_read_word(cpu, 0x0e30u) | 0x005fu));
    dspic33_gpio_drive(cpu, 3u, 0u, 0x005fu);
    dspic33_write_word(cpu, 0x06c8u, 0x4640u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x011bu, 0u, 0u);
    expect(state, !dspic33_spi_test_interrupt_flag(cpu, irqs[0]) && cpu->io.spi_pin_bits[0] == 0u,
           "PPS slave starts without a sampled edge");
    dspic33_write_word(cpu, 0x0e3eu, (uint16_t)(dspic33_read_word(cpu, 0x0e3eu) | 0x0040u));
    expect(state, (dspic33_read_word(cpu, 0x0e3eu) & 0x0040u) != 0u,
           "PPS slave clock pin enters analog mode");
    expect(state, drive_pps_spi_word(cpu, 0x00a5u, 8u, 0x0001u, 0x0040u),
           "drive analog PPS slave input");
    expect(state, !dspic33_spi_test_interrupt_flag(cpu, irqs[0]),
           "analog PPS clock suppresses slave interrupt");
    expect(state, cpu->io.spi_pin_bits[0] == 0u, "analog PPS clock suppresses sampled bits");
    dspic33_write_word(cpu, 0x0e3eu, (uint16_t)(dspic33_read_word(cpu, 0x0e3eu) & ~0x0040u));
    expect(state,
           drive_pps_spi_word(cpu, 0x00a5u, 8u, 0x0001u, 0x0040u) &&
               dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[0]) &&
               dspic33_read_word(cpu, 0x0248u) == 0xa5u,
           "digital PPS clock restores slave input");

    dspic33_spi_test_clear_interrupt(cpu, irqs[0]);
    dspic33_write_word(cpu, 0x0e30u, (uint16_t)(dspic33_read_word(cpu, 0x0e30u) & ~0x0040u));
    expect(state,
           drive_pps_spi_word(cpu, 0x005au, 8u, 0x0001u, 0x0040u) &&
               !dspic33_spi_test_interrupt_flag(cpu, irqs[0]),
           "output-configured PPS clock suppresses slave input");
    dspic33_write_word(cpu, 0x0e30u, (uint16_t)(dspic33_read_word(cpu, 0x0e30u) | 0x0040u));
    expect(state,
           drive_pps_spi_word(cpu, 0x005au, 8u, 0x0001u, 0x0040u) &&
               dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[0]) &&
               dspic33_read_word(cpu, 0x0248u) == 0x5au,
           "input-configured PPS clock restores slave input");

    dspic33_spi_test_clear_interrupt(cpu, irqs[0]);
    dspic33_write_word(cpu, 0x06c8u, 0x4443u);
    expect(state,
           drive_pps_spi_word(cpu, 0x00ffu, 8u, 0x0001u, 0x0002u) &&
               !dspic33_spi_test_interrupt_flag(cpu, irqs[0]),
           "live PPS remap releases old slave inputs");
    expect(state,
           drive_pps_spi_word(cpu, 0x003cu, 8u, 0x0008u, 0x0010u) &&
               dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[0]) &&
               dspic33_read_word(cpu, 0x0248u) == 0x3cu,
           "live PPS remap selects new slave inputs");

    dspic33_spi_test_clear_interrupt(cpu, irqs[0]);
    dspic33_write_word(cpu, 0x06c8u, 0x0201u);
    expect(state,
           drive_pps_spi_word(cpu, 0x00ffu, 8u, 0x0008u, 0x0010u) &&
               !dspic33_spi_test_interrupt_flag(cpu, irqs[0]),
           "B1 virtual PPS sources do not drive SPI input");
}

void dspic33_spi_test_master_output_cases(TestState* state, Dspic33* cpu, Dspic33* copy) {
    static const uint16_t mappings[DSPIC33_SPI_COUNT] = {0x0605u, 0u, 0x201fu, 0x2322u};
    bool clock_level;
    bool data_level;

    for (uint8_t channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        for (uint8_t word_mode = 0u; word_mode < 2u; word_mode++) {
            for (uint8_t clock_polarity = 0u; clock_polarity < 2u; clock_polarity++) {
                for (uint8_t clock_edge = 0u; clock_edge < 2u; clock_edge++) {
                    const uint16_t spi_control =
                        (uint16_t)(0x003bu | ((uint16_t)word_mode << 10u) |
                                   ((uint16_t)clock_edge << 8u) | ((uint16_t)clock_polarity << 6u));
                    const uint16_t transmit_value = word_mode != 0u ? 0xa55bu : 0x00a5u;
                    const uint64_t transfer_cycles = dspic33_spi_test_transfer_cycles(spi_control);

                    dspic33_reset(cpu, 0u);
                    if (mappings[channel_index] != 0u) {
                        dspic33_write_word(cpu, 0x0680u, mappings[channel_index]);
                    }
                    dspic33_spi_test_configure_spi(cpu, channel_index, spi_control, 0u, 0u);
                    dspic33_write_word(cpu, (uint16_t)(bases[channel_index] + 8u), transmit_value);

                    expect(state,
                           dspic33_spi_clock_output(cpu, channel_index, &clock_level) &&
                               clock_level == (clock_polarity == 0u) &&
                               dspic33_spi_data_output(cpu, channel_index, &data_level) &&
                               data_level &&
                               (mappings[channel_index] == 0u ||
                                (dspic33_spi_pin(cpu, 64u, &data_level) && data_level &&
                                 dspic33_spi_pin(cpu, 65u, &clock_level) &&
                                 clock_level == (clock_polarity == 0u))),
                           "master output starts on active clock with first data bit");
                    expect(state,
                           dspic33_device_advance(cpu, 1u) &&
                               dspic33_spi_clock_output(cpu, channel_index, &clock_level) &&
                               clock_level == (clock_polarity != 0u) &&
                               dspic33_spi_data_output(cpu, channel_index, &data_level) &&
                               data_level == (clock_edge == 0u),
                           "master output reaches half-clock data phase");
                    expect(state,
                           dspic33_device_advance(cpu, 1u) &&
                               dspic33_spi_clock_output(cpu, channel_index, &clock_level) &&
                               clock_level == (clock_polarity == 0u) &&
                               dspic33_spi_data_output(cpu, channel_index, &data_level) &&
                               !data_level,
                           "master output advances to second data bit");
                    expect(state,
                           dspic33_device_advance(cpu, transfer_cycles - 2u) &&
                               dspic33_spi_clock_output(cpu, channel_index, &clock_level) &&
                               clock_level == (clock_polarity != 0u) &&
                               dspic33_spi_data_output(cpu, channel_index, &data_level) &&
                               data_level,
                           "master output returns to idle clock with final data bit");
                }
            }
        }

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel_index, 0x183bu, 0u, 0u);
        dspic33_write_word(cpu, (uint16_t)(bases[channel_index] + 8u), 0x00a5u);
        expect(state,
               !dspic33_spi_clock_output(cpu, channel_index, &clock_level) &&
                   !dspic33_spi_data_output(cpu, channel_index, &data_level),
               "disabled master pins release clock and data outputs");

        dspic33_reset(cpu, 0u);
        dspic33_device_advance(cpu, 1u);
        dspic33_spi_test_configure_spi(cpu, channel_index, 0x003bu, 0x8000u, 0u);
        expect(state, dspic33_spi_clock_output(cpu, channel_index, &clock_level) && clock_level,
               "framed master clock starts from its enable cycle");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_spi_clock_output(cpu, channel_index, &clock_level) && !clock_level,
               "framed master clock reaches continuous half-cycle");
        dspic33_write_word(cpu, (uint16_t)(bases[channel_index] + 8u), 0x00a5u);
        expect(state, dspic33_spi_clock_output(cpu, channel_index, &clock_level) && !clock_level,
               "framed transmit does not restart continuous clock phase");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x0605u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x003bu, 0u, 0u);
    dspic33_write_word(cpu, 0x0248u, 0x00a5u);
    dspic33_write_word(cpu, 0x0680u, 0u);
    expect(state,
           !dspic33_spi_pin(cpu, 64u, &data_level) && !dspic33_spi_pin(cpu, 65u, &clock_level) &&
               dspic33_spi_data_output(cpu, 0u, &data_level) && data_level,
           "live remap releases master outputs without stopping transfer");
    expect(state,
           !dspic33_spi_clock_output(cpu, DSPIC33_SPI_COUNT, &clock_level) &&
               !dspic33_spi_data_output(cpu, DSPIC33_SPI_COUNT, &data_level) &&
               !dspic33_spi_pin(cpu, 0u, &data_level) && !dspic33_spi_pin(cpu, 64u, NULL),
           "master output rejects invalid requests");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x003bu, 0u, 0u);
    dspic33_write_word(cpu, 0x0248u, 0x00a5u);
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_copy(copy, cpu), "copy active master output phase");
    expect(state,
           dspic33_spi_clock_output(cpu, 0u, &clock_level) && !clock_level &&
               dspic33_spi_clock_output(copy, 0u, &data_level) && !data_level,
           "copied master output retains half-clock phase");
    dspic33_device_advance(cpu, 1u);
    expect(state,
           dspic33_spi_clock_output(cpu, 0u, &clock_level) && clock_level &&
               dspic33_spi_clock_output(copy, 0u, &data_level) && !data_level,
           "copied master output advances independently");
}

static void set_master_input(Dspic33* cpu, uint8_t channel, bool high) {
    if (channel == 1u) {
        dspic33_spi_pin_input(cpu, channel, false, high, false);
    } else {
        dspic33_gpio_drive(cpu, 3u, high ? 1u : 0u, 1u);
    }
}

static void drive_master_input(Dspic33* cpu, uint8_t channel, uint16_t value, uint8_t width) {
    uint8_t index;
    for (index = 0u; index < width; index++) {
        set_master_input(cpu, channel, (value & (uint16_t)(1u << (width - index - 1u))) != 0u);
        dspic33_device_advance(cpu, 2u);
    }
}

void dspic33_spi_test_master_input_cases(TestState* state, Dspic33* cpu, Dspic33* copy) {
    static const uint16_t input_registers[DSPIC33_SPI_COUNT] = {0x06c8u, 0u, 0x06dau, 0x06deu};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint8_t mode16;
        for (mode16 = 0u; mode16 < 2u; mode16++) {
            uint8_t sample_end;
            for (sample_end = 0u; sample_end < 2u; sample_end++) {
                uint16_t control =
                    (uint16_t)(0x003bu | ((uint16_t)mode16 << 10u) | ((uint16_t)sample_end << 9u));
                uint16_t received = mode16 != 0u ? 0xa55au : 0x005au;
                uint8_t width = mode16 != 0u ? 16u : 8u;

                dspic33_reset(cpu, 0u);
                if (input_registers[channel] != 0u) {
                    dspic33_write_word(cpu, 0x0e3eu,
                                       (uint16_t)(dspic33_read_word(cpu, 0x0e3eu) & ~1u));
                    dspic33_write_word(cpu, input_registers[channel], 64u);
                }
                dspic33_spi_test_configure_spi(cpu, channel, control, 0u, 0u);
                set_master_input(cpu, channel, false);
                dspic33_write_word(cpu, (uint16_t)(bases[channel] + 8u), 0xffffu);
                drive_master_input(cpu, channel, received, width);
                expect(state,
                       dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[channel]) &&
                           dspic33_read_word(cpu, (uint16_t)(bases[channel] + 8u)) == received,
                       "master samples physical serial input");
            }
        }
    }

    for (uint8_t sample_end = 0u; sample_end < 2u; sample_end++) {
        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, 1u, (uint16_t)(0x003bu | ((uint16_t)sample_end << 9u)),
                                       0u, 0u);
        set_master_input(cpu, 1u, true);
        dspic33_write_word(cpu, 0x0268u, 0xffu);
        dspic33_device_advance(cpu, 1u);
        set_master_input(cpu, 1u, false);
        dspic33_device_advance(cpu, 1u);
        drive_master_input(cpu, 1u, 0u, 7u);
        expect(state, dspic33_read_word(cpu, 0x0268u) == (sample_end == 0u ? 0x80u : 0u),
               "master sample phase selects middle or end of data period");
    }

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x003bu, 0u, 0u);
    dspic33_write_word(cpu, 0x0248u, 0xa5u);
    dspic33_device_advance(cpu, 16u);
    expect(state, dspic33_read_word(cpu, 0x0248u) == 0xa5u,
           "unmapped master retains logical transport loopback");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 1u, 0x083bu, 0u, 0u);
    set_master_input(cpu, 1u, false);
    dspic33_write_word(cpu, 0x0268u, 0xffu);
    drive_master_input(cpu, 1u, 0xa5u, 8u);
    expect(state,
           dspic33_read_word(cpu, 0x0268u) == 0xa5u &&
               !dspic33_spi_data_output(cpu, 1u, &(bool){false}),
           "receive-only master samples input with serial output released");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 1u, 0x043bu, 0u, 0u);
    set_master_input(cpu, 1u, false);
    dspic33_write_word(cpu, 0x0268u, 0xffffu);
    drive_master_input(cpu, 1u, 0xa5u, 8u);
    expect(state, dspic33_copy(copy, cpu), "copy partial master physical input");
    drive_master_input(cpu, 1u, 0x5au, 8u);
    drive_master_input(copy, 1u, 0xc3u, 8u);
    expect(state, dspic33_read_word(cpu, 0x0268u) == 0xa55au,
           "source master physical input completes independently");
    expect(state, dspic33_read_word(copy, 0x0268u) == 0xa5c3u,
           "copied master physical input completes independently");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x06c8u, 70u);
    dspic33_write_word(cpu, 0x0e3eu, (uint16_t)(dspic33_read_word(cpu, 0x0e3eu) | 0x0040u));
    dspic33_spi_test_configure_spi(cpu, 0u, 0x003bu, 0u, 0u);
    dspic33_write_word(cpu, 0x0248u, 0xffu);
    for (uint8_t index = 0u; index < 8u; index++) {
        dspic33_gpio_drive(cpu, 3u, 0x0040u, 0x0040u);
        dspic33_device_advance(cpu, 2u);
    }
    expect(state, dspic33_read_word(cpu, 0x0248u) == 0u,
           "analog PPS data pin suppresses master input");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e3eu, (uint16_t)(dspic33_read_word(cpu, 0x0e3eu) & ~3u));
    dspic33_write_word(cpu, 0x06c8u, 64u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x003bu, 0u, 0u);
    dspic33_write_word(cpu, 0x0248u, 0xffu);
    for (uint8_t index = 0u; index < 4u; index++) {
        bool high = (0xa5u & (uint16_t)(1u << (7u - index))) != 0u;
        dspic33_gpio_drive(cpu, 3u, high ? 1u : 0u, 1u);
        dspic33_device_advance(cpu, 2u);
    }
    dspic33_write_word(cpu, 0x06c8u, 65u);
    for (uint8_t index = 4u; index < 8u; index++) {
        bool high = (0xa5u & (uint16_t)(1u << (7u - index))) != 0u;
        dspic33_gpio_drive(cpu, 3u, high ? 2u : 0u, 2u);
        dspic33_device_advance(cpu, 2u);
    }
    expect(state, dspic33_read_word(cpu, 0x0248u) == 0xa5u,
           "live PPS remap changes master input source");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 1u, 0x003bu, 0u, 0u);
    set_master_input(cpu, 1u, false);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, 0x0268u, 0xffu);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.count == 0u &&
               (cpu->io.spi_busy & 2u) == 0u,
           "master input scheduling failure aborts transfer");

    dspic33_reset(cpu, 0u);
    set_master_input(cpu, 1u, true);
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->software_reset_count == 1u &&
               (cpu->io.spi_pin_input_enabled & 2u) != 0u && (cpu->io.spi_pin_data_high & 2u) != 0u,
           "warm reset preserves dedicated master input level");
    dspic33_spi_test_configure_spi(cpu, 1u, 0x003bu, 0u, 0u);
    dspic33_write_word(cpu, 0x0268u, 0u);
    expect(state, dspic33_device_advance(cpu, 16u) && dspic33_read_word(cpu, 0x0268u) == 0xffu,
           "retained master input remains observable after warm reset");
}
