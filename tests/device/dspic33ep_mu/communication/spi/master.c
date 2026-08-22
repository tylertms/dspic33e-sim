#include "device/dspic33ep_mu/communication/spi/internal.h"

void dspic33_spi_test_timing_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t mode16;
    uint8_t primary;
    uint8_t secondary;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        for (mode16 = 0u; mode16 < 2u; mode16++) {
            for (primary = 0u; primary < 4u; primary++) {
                for (secondary = 0u; secondary < 8u; secondary++) {
                    uint16_t control;
                    uint16_t sent;
                    uint16_t received;
                    uint64_t cycles;
                    if (primary == 3u && secondary == 7u) {
                        continue;
                    }
                    control = (uint16_t)(0x0020u | primary | ((uint16_t)secondary << 2u) |
                                         (mode16 != 0u ? 0x0400u : 0u));
                    sent =
                        (uint16_t)(0x8100u | (channel << 4u) | (primary << 2u) | (secondary & 3u));
                    received = (uint16_t)(0x5a00u | (secondary << 4u) | primary);
                    cycles = dspic33_spi_test_transfer_cycles(control);
                    dspic33_reset(cpu, 0u);
                    dspic33_spi_test_configure_spi(cpu, channel, control, 0u, 0u);
                    expect(state, dspic33_spi_receive(cpu, channel, received, cycles),
                           "schedule master response matrix");
                    dspic33_write_word(cpu, (uint16_t)(bases[channel] + 8u), sent);
                    expect(state, (dspic33_read_word(cpu, bases[channel]) & 0x0080u) == 0u,
                           "matrix shift register active");
                    expect(state, dspic33_device_advance(cpu, cycles - 1u),
                           "matrix advance before completion");
                    expect(state, (dspic33_read_word(cpu, bases[channel]) & 0x0001u) == 0u,
                           "matrix not complete early");
                    expect(state, dspic33_device_advance(cpu, 1u), "matrix completion advance");
                    expect(state, (dspic33_read_word(cpu, bases[channel]) & 0x0001u) != 0u,
                           "matrix receive flag");
                    expect(state,
                           dspic33_read_word(cpu, (uint16_t)(bases[channel] + 8u)) ==
                               (mode16 != 0u ? received : (uint16_t)(received & 0x00ffu)),
                           "matrix received value");
                    expect(state,
                           dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[channel]),
                           "matrix transfer interrupt");
                }
            }
        }
    }
}

void dspic33_spi_test_standard_buffer_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t control = 0x043bu;
        uint64_t cycles = dspic33_spi_test_transfer_cycles(control);
        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, control, 0u, 0u);
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x1111u, cycles) &&
                   dspic33_spi_receive(cpu, channel, 0x2222u, cycles * 2u),
               "queue standard responses");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xaaaau);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xbbbbu);
        expect(state, (dspic33_read_word(cpu, base) & 0x0002u) != 0u,
               "standard transmit buffer full");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xccccu);
        expect(state, cpu->io.spi_tx_fifo[channel].count == 1u, "standard full write ignored");
        expect(state, dspic33_device_advance(cpu, cycles), "standard first completion");
        expect(state, (dspic33_read_word(cpu, base) & 0x0002u) == 0u,
               "standard queued word moved to shift");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x1111u,
               "standard first response order");
        expect(state, dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[channel]),
               "standard first interrupt follows one-cycle latency");
        dspic33_spi_test_clear_interrupt(cpu, irqs[channel]);
        expect(state, dspic33_device_advance(cpu, cycles - 1u), "standard second completion");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x2222u,
               "standard second response order");
        expect(state, dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[channel]),
               "standard second interrupt");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, 0u, 0u, 0u);
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x0031u, 1u) &&
                   dspic33_spi_receive(cpu, channel, 0x0032u, 2u),
               "queue standard slave overflow");
        expect(state, dspic33_device_advance(cpu, 2u), "standard slave overflow advance");
        expect(state, (dspic33_read_word(cpu, base) & 0x0041u) == 0x0041u,
               "standard overflow flags");
        expect(state, dspic33_spi_test_interrupt_flag(cpu, error_irqs[channel]),
               "standard overflow error interrupt");
        dspic33_write_word(cpu, base, (uint16_t)(dspic33_read_word(cpu, base) & ~0x0040u));
        expect(state, (dspic33_read_word(cpu, base) & 0x0040u) == 0u,
               "standard overflow software clear");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x0031u,
               "standard overflow preserves unread value");
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x0033u, 1u) && dspic33_device_advance(cpu, 1u),
               "standard receive resumes after clear");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x0033u,
               "standard recovered receive value");
    }
}

void dspic33_spi_test_enhanced_fifo_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t control = 0x043bu;
        uint64_t cycles = dspic33_spi_test_transfer_cycles(control);
        uint8_t index;
        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, control, 1u, 5u);
        for (index = 0u; index < 9u; index++) {
            dspic33_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)(0x1000u + index));
        }
        expect(state, cpu->io.spi_tx_fifo[channel].count == 8u, "enhanced eight pending words");
        expect(state, (dspic33_read_word(cpu, base) & 0x0702u) == 0x0702u,
               "enhanced full count encoding");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xdeadu);
        expect(state, cpu->io.spi_tx_fifo[channel].count == 8u, "enhanced full write ignored");
        for (index = 0u; index < 9u; index++) {
            expect(state, dspic33_device_advance(cpu, cycles), "enhanced transmit completion");
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == (uint16_t)(0x1000u + index),
                   "enhanced transmit receive order");
        }
        expect(state, (dspic33_read_word(cpu, base) & 0x00a2u) == 0x00a0u, "enhanced queues empty");
        expect(state, dspic33_spi_test_interrupt_flag(cpu, irqs[channel]),
               "enhanced final completion interrupt");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, 0x0400u, 1u, 3u);
        for (index = 0u; index < 8u; index++) {
            expect(state,
                   dspic33_spi_receive(cpu, channel, (uint16_t)(0x3000u + index), 1u) &&
                       dspic33_device_advance(cpu, 1u),
                   "enhanced slave receive fill");
        }
        expect(state, cpu->io.spi_rx_fifo[channel].count == 8u, "enhanced receive depth");
        expect(state, (dspic33_read_word(cpu, base) & 0x0721u) == 0x0701u,
               "enhanced receive full status");
        expect(state, dspic33_spi_test_interrupt_flag(cpu, irqs[channel]),
               "enhanced receive full interrupt");
        dspic33_spi_test_clear_interrupt(cpu, irqs[channel]);
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x3fffu, 1u) && dspic33_device_advance(cpu, 1u),
               "enhanced overflow advance");
        expect(state, (dspic33_read_word(cpu, base) & 0x0040u) != 0u, "enhanced overflow flag");
        expect(state, dspic33_spi_test_interrupt_flag(cpu, error_irqs[channel]),
               "enhanced overflow error interrupt");
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x3eeeu, 1u) && dspic33_device_advance(cpu, 1u),
               "enhanced overflow blocks receive");
        expect(state, cpu->io.spi_rx_fifo[channel].count == 8u,
               "enhanced blocked receive preserves depth");
        dspic33_write_word(cpu, base, (uint16_t)(dspic33_read_word(cpu, base) & ~0x0040u));
        expect(state, (dspic33_read_word(cpu, base) & 0x0040u) == 0u,
               "enhanced overflow software clear");
        expect(state, cpu->io.spi_rx_fifo[channel].count == 8u,
               "enhanced clear preserves unread words");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x3000u,
               "enhanced first preserved word");
        expect(state, cpu->io.spi_rx_fifo[channel].count == 7u, "enhanced read frees receive slot");
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x3dddu, 1u) && dspic33_device_advance(cpu, 1u),
               "enhanced receive resumes after clear");
        expect(state, (dspic33_read_word(cpu, base) & 0x0040u) == 0u,
               "enhanced recovered receive avoids overflow");
        expect(state, cpu->io.spi_rx_fifo[channel].count == 8u,
               "enhanced recovered receive fills slot");
        for (index = 1u; index < 8u; index++) {
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == (uint16_t)(0x3000u + index),
                   "enhanced receive read order");
        }
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x3dddu,
               "enhanced recovered receive tail");
        expect(state, (dspic33_read_word(cpu, base) & 0x0021u) == 0x0020u,
               "enhanced receive empty status");
    }
}

void dspic33_spi_test_interrupt_mode_cases(TestState* state, Dspic33* cpu) {
    uint16_t base = bases[0];
    uint16_t master = 0x043bu;
    uint64_t cycles = dspic33_spi_test_transfer_cycles(master);
    uint8_t index;

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, master, 1u, 7u);
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x1000u);
    for (index = 0u; index < 7u; index++) {
        dspic33_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)(0x1001u + index));
    }
    expect(state, !dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode seven not early");
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x1008u);
    expect(state, dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode seven transmit full");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, master, 1u, 6u);
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x2000u);
    expect(state, dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode six transmit fifo empty");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, master, 1u, 5u);
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x3000u);
    expect(state, !dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode five not early");
    expect(state, dspic33_device_advance(cpu, cycles), "mode five completion advance");
    expect(state, dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode five transfer complete");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, master, 1u, 4u);
    for (index = 0u; index < 9u; index++) {
        dspic33_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)(0x4000u + index));
    }
    expect(state, !dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode four not early");
    expect(state, dspic33_device_advance(cpu, cycles), "mode four opening advance");
    expect(state, dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode four one opening");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x0400u, 1u, 1u);
    expect(state, dspic33_spi_receive(cpu, 0u, 0x5000u, 1u) && dspic33_device_advance(cpu, 1u),
           "mode one receive advance");
    expect(state, dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode one data received");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x0400u, 1u, 2u);
    for (index = 0u; index < 5u; index++) {
        expect(state, dspic33_spi_receive(cpu, 0u, index, 1u) && dspic33_device_advance(cpu, 1u),
               "mode two below threshold advance");
    }
    expect(state, !dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode two below threshold");
    expect(state, dspic33_spi_receive(cpu, 0u, 5u, 1u) && dspic33_device_advance(cpu, 1u),
           "mode two threshold advance");
    expect(state, dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode two three quarters full");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x0400u, 1u, 3u);
    for (index = 0u; index < 7u; index++) {
        expect(state, dspic33_spi_receive(cpu, 0u, index, 1u) && dspic33_device_advance(cpu, 1u),
               "mode three below full advance");
    }
    expect(state, !dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode three below full");
    expect(state, dspic33_spi_receive(cpu, 0u, 7u, 1u) && dspic33_device_advance(cpu, 1u),
           "mode three full advance");
    expect(state, dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode three receive full");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x0400u, 1u, 0u);
    expect(state, dspic33_spi_receive(cpu, 0u, 0x6000u, 1u) && dspic33_device_advance(cpu, 1u),
           "mode zero receive advance");
    expect(state, !dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode zero not on receive");
    expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x6000u, "mode zero read value");
    expect(state, dspic33_spi_test_interrupt_flag(cpu, irqs[0]), "mode zero receive empty");
}

void dspic33_spi_test_mode_transition_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, 0x003bu, 0u, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x00aau);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x043bu);
        expect(state,
               cpu->io.spi_tx_fifo[channel].count == 0u &&
                   (cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u,
               "mode width change resets transfer");
        expect(state, (dspic33_read_word(cpu, base) & 0x0003u) == 0u,
               "mode width change clears flags");

        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xaaaau);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 1u);
        expect(state,
               cpu->io.spi_tx_fifo[channel].count == 0u &&
                   (cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u,
               "enhanced mode change resets transfer");
        expect(state, (dspic33_read_word(cpu, base) & 0x00a0u) == 0x00a0u,
               "enhanced mode reset status");

        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xbbbbu);
        dspic33_write_word(cpu, base, 0u);
        expect(state,
               cpu->io.spi_tx_fifo[channel].count == 0u &&
                   (cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u,
               "module disable resets transfer");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "module disable clears buffer");
    }
}

void dspic33_spi_test_selection_and_frame_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, 0x0480u, 0u, 0u);
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x1111u, 1u) && dspic33_device_advance(cpu, 1u),
               "unselected slave transaction advance");
        expect(state, (dspic33_read_word(cpu, base) & 1u) == 0u,
               "unselected slave transaction ignored");
        expect(state, dspic33_spi_select(cpu, channel, true, 0u) && dspic33_device_advance(cpu, 0u),
               "select slave");
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x2222u, 1u) && dspic33_device_advance(cpu, 1u),
               "selected slave transaction advance");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x2222u,
               "selected slave transaction received");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, 0x0400u, 0xc000u, 0u);
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x3333u, 1u) && dspic33_device_advance(cpu, 1u),
               "inactive frame transaction advance");
        expect(state, (dspic33_read_word(cpu, base) & 1u) == 0u,
               "inactive frame transaction ignored");
        expect(state,
               dspic33_spi_select(cpu, channel, true, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_spi_receive(cpu, channel, 0x4444u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "active frame transaction advance");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x4444u,
               "active frame transaction received");
    }
}

void dspic33_spi_test_slave_select_retry_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t data_functions[DSPIC33_SPI_COUNT] = {5u, 0u, 31u, 34u};
    for (uint8_t channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t received = (uint16_t)(0x5aa0u + channel);
        uint16_t transmitted = (uint16_t)(0xa550u + channel);
        uint8_t bit = (uint8_t)(1u << channel);
        bool data = false;

        dspic33_reset(cpu, 0u);
        if (data_functions[channel] != 0u) {
            dspic33_write_word(cpu, 0x0680u, data_functions[channel]);
        }
        dspic33_spi_test_configure_spi(cpu, channel, 0x0480u, 0u, 0u);
        dspic33_spi_pin_input(cpu, channel, false, false, true);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), transmitted);
        expect(state, (cpu->io.spi_busy & bit) == 0u && cpu->io.spi_tx_fifo[channel].count == 1u,
               "inactive slave select holds pending data");
        expect(state,
               !dspic33_spi_data_output(cpu, channel, &data) &&
                   (data_functions[channel] == 0u || !dspic33_spi_pin(cpu, 64u, &data)),
               "inactive slave select tri-states output");

        dspic33_spi_pin_input(cpu, channel, false, false, false);
        expect(state,
               (cpu->io.spi_busy & bit) != 0u && cpu->io.spi_shift[channel] == transmitted &&
                   dspic33_spi_data_output(cpu, channel, &data) && data &&
                   (data_functions[channel] == 0u || (dspic33_spi_pin(cpu, 64u, &data) && data)),
               "active slave select starts the held transmission");

        for (uint8_t index = 0u; index < 4u; index++) {
            bool high = (received & (uint16_t)(1u << (15u - index))) != 0u;
            dspic33_spi_pin_input(cpu, channel, true, high, false);
            dspic33_spi_pin_input(cpu, channel, false, high, false);
        }
        dspic33_spi_pin_input(cpu, channel, false, false, true);
        expect(state,
               (cpu->io.spi_busy & bit) == 0u && cpu->io.spi_tx_fifo[channel].count == 1u &&
                   cpu->io.spi_pin_bits[channel] == 0u &&
                   !dspic33_spi_data_output(cpu, channel, &data) &&
                   !dspic33_spi_test_interrupt_flag(cpu, irqs[channel]),
               "slave deselection aborts and retains the incomplete word");

        dspic33_spi_pin_input(cpu, channel, false, false, false);
        expect(state,
               (cpu->io.spi_busy & bit) != 0u && cpu->io.spi_shift[channel] == transmitted &&
                   cpu->io.spi_tx_fifo[channel].count == 0u,
               "slave reselection retries the retained word from its first bit");
        for (uint8_t index = 0u; index < 16u; index++) {
            bool high = (received & (uint16_t)(1u << (15u - index))) != 0u;
            dspic33_spi_pin_input(cpu, channel, true, high, false);
            dspic33_spi_pin_input(cpu, channel, false, high, false);
        }
        expect(state,
               (cpu->io.spi_busy & bit) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == received &&
                   !dspic33_spi_test_interrupt_flag(cpu, irqs[channel]) &&
                   dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[channel]),
               "retried slave word completes once with delayed interrupt");
    }
}

void dspic33_spi_test_b1_frame_output_cases(TestState* state, Dspic33* cpu, Dspic33* copy) {
    static const uint8_t frame_functions[DSPIC33_SPI_COUNT] = {7u, 10u, 33u, 36u};
    uint8_t channel;
    bool high = false;
    bool data = false;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t bit = (uint8_t)(1u << channel);
        uint8_t master;
        for (master = 0u; master < 2u; master++) {
            uint8_t polarity;
            for (polarity = 0u; polarity < 2u; polarity++) {
                uint8_t delay;
                for (delay = 0u; delay < 2u; delay++) {
                    uint16_t control = master != 0u ? 0x043bu : 0x0400u;
                    uint16_t control2 =
                        (uint16_t)(0x8000u | ((uint16_t)polarity << 13u) | ((uint16_t)delay << 1u));
                    uint16_t received =
                        (uint16_t)(0xd800u + ((uint16_t)channel << 4u) + ((uint16_t)master << 3u) +
                                   ((uint16_t)polarity << 2u) + delay);
                    uint16_t transmitted =
                        (uint16_t)(0xa500u + ((uint16_t)channel << 4u) + ((uint16_t)master << 3u) +
                                   ((uint16_t)polarity << 2u) + delay);
                    dspic33_reset(cpu, 0u);
                    dspic33_spi_test_configure_spi(cpu, channel, control, control2, 0u);
                    expect(state,
                           dspic33_spi_frame_output(cpu, channel, &high) &&
                               high == (polarity == 0u),
                           "B1 frame output starts at inactive polarity");
                    dspic33_write_word(cpu, (uint16_t)(base + 8u), transmitted);
                    expect(state,
                           dspic33_read_word(cpu, (uint16_t)(base + 4u)) == control2 &&
                               dspic33_spi_frame_output(cpu, channel, &high) &&
                               (master != 0u ? (high == (polarity == 0u) &&
                                                (cpu->io.spi_frame_active & bit) == 0u &&
                                                (cpu->io.spi_frame_output_pending & bit) != 0u)
                                             : (high == (polarity == 0u) &&
                                                (cpu->io.spi_frame_active & bit) == 0u &&
                                                (cpu->io.spi_frame_output_pending & bit) != 0u)),
                           "B1 framed master waits for the transmit edge");
                    if (master == 0u) {
                        expect(state, dspic33_spi_data_output(cpu, channel, &data) && data,
                               "B1 slave transfer preloads its first data bit");
                    }
                    if (master != 0u) {
                        uint64_t cycles = dspic33_spi_test_transfer_cycles(control);
                        uint64_t clock = cycles / 16u;
                        expect(state,
                               dspic33_device_advance(cpu, 0u) &&
                                   dspic33_spi_frame_output(cpu, channel, &high) && high &&
                                   ((cpu->io.spi_frame_active & bit) != 0u) == (polarity != 0u) &&
                                   ((cpu->io.spi_busy & bit) != 0u) == (delay != 0u),
                               "master frame begins on the transmit edge");
                        if (delay == 0u) {
                            expect(state,
                                   dspic33_device_advance(cpu, clock) &&
                                       (cpu->io.spi_busy & bit) != 0u &&
                                       dspic33_spi_frame_output(cpu, channel, &high) && high,
                                   "preceding frame starts data one clock later");
                            expect(state,
                                   dspic33_device_advance(cpu, cycles - clock) &&
                                       (cpu->io.spi_busy & bit) != 0u &&
                                       dspic33_spi_frame_output(cpu, channel, &high) &&
                                       high == (polarity == 0u),
                                   "frame pulse lasts one data-frame interval");
                            expect(state, dspic33_device_advance(cpu, clock),
                                   "preceded data transfer reaches completion");
                        } else {
                            expect(state, dspic33_device_advance(cpu, cycles),
                                   "coincident data transfer reaches completion");
                        }
                    } else {
                        bool data_high = (received & 0x8000u) != 0u;
                        expect(state,
                               dspic33_spi_pin_input(cpu, channel, true, data_high, false) &&
                                   dspic33_spi_frame_output(cpu, channel, &high) && high &&
                                   ((cpu->io.spi_frame_active & bit) != 0u) == (polarity != 0u) &&
                                   (cpu->io.spi_frame_output_pending & bit) == 0u &&
                                   (cpu->io.spi_frame_output_clear_pending & bit) != 0u,
                               "slave frame pulse begins on the transmit edge");
                        expect(state,
                               dspic33_spi_pin_input(cpu, channel, false, data_high, false) &&
                                   dspic33_spi_frame_output(cpu, channel, &high) && high &&
                                   cpu->io.spi_pin_bits[channel] == 0u,
                               "slave frame pulse lasts one full serial clock");
                        expect(state,
                               dspic33_spi_pin_input(cpu, channel, true, data_high, false) &&
                                   dspic33_spi_frame_output(cpu, channel, &high) &&
                                   high == (polarity == 0u) &&
                                   (cpu->io.spi_frame_active & bit) == 0u &&
                                   (cpu->io.spi_frame_output_clear_pending & bit) == 0u,
                               "slave data begins after the frame pulse");
                        dspic33_spi_pin_input(cpu, channel, false, data_high, false);
                        for (uint8_t index = 1u; index < 16u; index++) {
                            data_high = (received & (uint16_t)(1u << (15u - index))) != 0u;
                            dspic33_spi_pin_input(cpu, channel, true, data_high, false);
                            dspic33_spi_pin_input(cpu, channel, false, data_high, false);
                        }
                    }
                    expect(state,
                           dspic33_spi_frame_output(cpu, channel, &high) &&
                               high == (polarity == 0u) && (cpu->io.spi_frame_active & bit) == 0u &&
                               dspic33_read_word(cpu, (uint16_t)(base + 8u)) ==
                                   (master != 0u ? transmitted : received),
                           "B1 frame returns to inactive polarity after transfer");
                }
            }
        }

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, 0x0400u, 0xa002u, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xf100u);
        dspic33_spi_pin_input(cpu, channel, true, true, false);
        expect(state, dspic33_copy(copy, cpu), "copy active slave frame pulse");
        dspic33_spi_pin_input(cpu, channel, false, true, false);
        dspic33_spi_pin_input(cpu, channel, true, true, false);
        expect(state,
               dspic33_spi_frame_output(cpu, channel, &high) && !high &&
                   dspic33_spi_frame_output(copy, channel, &high) && high,
               "copied slave frame pulse advances independently");
        dspic33_spi_pin_input(copy, channel, false, true, false);
        dspic33_spi_pin_input(copy, channel, true, true, false);
        expect(state,
               dspic33_spi_frame_output(copy, channel, &high) && !high &&
                   copy->io.spi_frame_output_clear_pending == 0u,
               "copied slave frame pulse retains its remaining edge");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, 0x0400u, 0xa002u, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xf200u);
        dspic33_spi_pin_input(cpu, channel, true, true, false);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 0u);
        expect(state,
               cpu->io.spi_frame_active == 0u && cpu->io.spi_frame_output_pending == 0u &&
                   cpu->io.spi_frame_output_clear_pending == 0u &&
                   !dspic33_spi_frame_output(cpu, channel, &high),
               "slave frame reconfiguration cancels every pulse state");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, 0x043bu, 0xa000u, 0u);
        dspic33_write_word(cpu, 0x0680u, frame_functions[channel]);
        expect(state, dspic33_spi_frame_pin(cpu, 64u, &high) && !high,
               "mapped B1 frame pin starts inactive");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)(0xe100u + channel));
        expect(state,
               dspic33_device_advance(cpu, 0u) && dspic33_spi_frame_pin(cpu, 64u, &high) && high,
               "mapped B1 frame pin follows active pulse");
        dspic33_write_word(cpu, 0x0680u, 0u);
        expect(state, !dspic33_spi_frame_pin(cpu, 64u, &high),
               "B1 frame pin releases after live remap");
        dspic33_write_word(cpu, 0x0680u, frame_functions[channel]);
        expect(state,
               dspic33_device_advance(cpu, dspic33_spi_test_transfer_cycles(0x043bu) +
                                               dspic33_spi_test_transfer_cycles(0x043bu) / 16u) &&
                   dspic33_spi_frame_pin(cpu, 64u, &high) && !high,
               "remapped B1 frame pin follows transfer completion");
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 0x8000u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)(0xe200u + channel));
        expect(state,
               dspic33_device_advance(cpu, 0u) && dspic33_spi_frame_pin(cpu, 64u, &high) && high &&
                   (cpu->io.spi_frame_active & bit) == 0u,
               "mapped B1 frame pin suppresses active-low pulse");
        expect(state,
               dspic33_device_advance(cpu, dspic33_spi_test_transfer_cycles(0x043bu) +
                                               dspic33_spi_test_transfer_cycles(0x043bu) / 16u) &&
                   dspic33_spi_frame_pin(cpu, 64u, &high) && high,
               "mapped B1 active-low frame remains inactive after transfer");
    }

    for (uint8_t clock_mode = 0u; clock_mode < 4u; clock_mode++) {
        uint16_t control = (uint16_t)(((clock_mode & 1u) != 0u ? 0x0100u : 0u) |
                                      ((clock_mode & 2u) != 0u ? 0x0040u : 0u));
        bool sample_high = (control & 0x0040u) != 0u;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0680u, 5u);
        dspic33_spi_pin_input(cpu, 0u, sample_high, false, false);
        dspic33_spi_test_configure_spi(cpu, 0u, control, 0xa002u, 0u);
        dspic33_write_word(cpu, 0x0248u, 0xa5u);
        expect(state,
               dspic33_spi_data_output(cpu, 0u, &data) && data &&
                   dspic33_spi_pin(cpu, 64u, &data) && data,
               "eight-bit slave preloads its first physical data bit");
        dspic33_spi_pin_input(cpu, 0u, !sample_high, true, false);
        expect(state,
               dspic33_spi_frame_output(cpu, 0u, &high) && high &&
                   (cpu->io.spi_frame_output_clear_pending & 1u) != 0u,
               "eight-bit slave frame starts on every transmit-edge mode");
        dspic33_spi_pin_input(cpu, 0u, sample_high, true, false);
        expect(state,
               dspic33_spi_frame_output(cpu, 0u, &high) && high && cpu->io.spi_pin_bits[0] == 0u,
               "eight-bit slave frame spans one clock in every edge mode");
        dspic33_spi_pin_input(cpu, 0u, !sample_high, true, false);
        dspic33_spi_pin_input(cpu, 0u, sample_high, true, false);
        dspic33_spi_pin_input(cpu, 0u, !sample_high, true, false);
        expect(state,
               dspic33_spi_data_output(cpu, 0u, &data) && !data &&
                   dspic33_spi_pin(cpu, 64u, &data) && !data,
               "slave data advances on the transmit edge after its first sample");
        dspic33_spi_pin_input(cpu, 0u, sample_high, true, false);
        for (uint8_t index = 2u; index < 8u; index++) {
            dspic33_spi_pin_input(cpu, 0u, !sample_high, true, false);
            dspic33_spi_pin_input(cpu, 0u, sample_high, true, false);
        }
        expect(state,
               dspic33_spi_frame_output(cpu, 0u, &high) && !high &&
                   dspic33_read_word(cpu, 0x0248u) == 0xffu && (cpu->io.spi_busy & 1u) == 0u,
               "eight-bit slave data follows its frame in every edge mode");
    }

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x0800u, 0xa002u, 0u);
    dspic33_write_word(cpu, 0x0248u, 0xa5u);
    expect(state,
           !dspic33_spi_data_output(cpu, 0u, &data) && dspic33_spi_frame_output(cpu, 0u, &high) &&
               !high,
           "disabled slave data output releases without disabling its frame output");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x0400u, 0xa002u, 0u);
    dspic33_write_word(cpu, 0x0248u, 0xa55au);
    dspic33_spi_pin_input(cpu, 0u, true, true, false);
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->software_reset_count == 1u &&
               cpu->io.spi_frame_active == 0u && cpu->io.spi_frame_output_pending == 0u &&
               cpu->io.spi_frame_output_clear_pending == 0u && cpu->io.spi_pin_output_started == 0u,
           "warm reset clears active slave frame and data output state");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x043bu, 0xa000u, 0u);
    dspic33_write_word(cpu, 0x0248u, 0xea5eu);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_copy(copy, cpu), "copy active B1 frame output");
    expect(state,
           dspic33_spi_frame_output(cpu, 0u, &high) && high &&
               dspic33_spi_frame_output(copy, 0u, &high) && high,
           "copied B1 frame output remains active");
    expect(state,
           dspic33_device_advance(copy, dspic33_spi_test_transfer_cycles(0x043bu) +
                                            dspic33_spi_test_transfer_cycles(0x043bu) / 16u),
           "copied B1 frame transfer completes");
    expect(state,
           dspic33_spi_frame_output(copy, 0u, &high) && !high &&
               dspic33_spi_frame_output(cpu, 0u, &high) && high,
           "copied B1 frame output advances independently");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x043bu, 0xa002u, 0u);
    dspic33_device_advance(cpu, 1u);
    dspic33_write_word(cpu, 0x0248u, 0xec01u);
    expect(state,
           (cpu->io.spi_frame_output_pending & 1u) != 0u && (cpu->io.spi_frame_active & 1u) == 0u &&
               (cpu->io.spi_busy & 1u) == 0u,
           "master frame waits for a nonzero clock phase");
    expect(state,
           dspic33_device_advance(cpu, 1u) && (cpu->io.spi_frame_output_pending & 1u) == 0u &&
               (cpu->io.spi_frame_active & 1u) != 0u && (cpu->io.spi_busy & 1u) != 0u,
           "master frame starts at the next phased transmit edge");
    expect(state,
           dspic33_device_advance(cpu, dspic33_spi_test_transfer_cycles(0x043bu)) &&
               (cpu->io.spi_frame_active & 1u) == 0u && (cpu->io.spi_busy & 1u) == 0u,
           "phased master frame completes at its original duration");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x043bu, 0xa000u, 0u);
    dspic33_write_word(cpu, 0x0248u, 0xec02u);
    dspic33_write_word(cpu, 0x0244u, 0u);
    expect(state,
           (cpu->io.spi_frame_output_pending & 1u) == 0u && (cpu->io.spi_frame_active & 1u) == 0u &&
               (cpu->io.spi_busy & 1u) != 0u && cpu->events.count == 1u,
           "frame configuration change cancels the stale pulse and starts data");
    expect(state,
           dspic33_device_advance(cpu, dspic33_spi_test_transfer_cycles(0x043bu)) &&
               (cpu->io.spi_busy & 1u) == 0u && cpu->io.spi_tx_fifo[0].count == 0u,
           "reconfigured unframed transfer completes without a stale pulse");

    dspic33_reset(cpu, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x043bu, 0xa000u, 0u);
    dspic33_device_advance(cpu, 1u);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, 0x0248u, 0xec03u);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.count == 0u &&
               cpu->io.spi_tx_fifo[0].count == 0u &&
               (cpu->io.spi_frame_output_pending & 1u) == 0u && (cpu->io.spi_busy & 1u) == 0u,
           "master frame scheduling failure is fail-closed");

    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.spi_frame_active == 0u && !dspic33_spi_frame_output(cpu, 0u, &high) &&
               !dspic33_spi_frame_output(cpu, DSPIC33_SPI_COUNT, &high) &&
               !dspic33_spi_frame_output(cpu, 0u, NULL) && !dspic33_spi_frame_pin(cpu, 0u, &high) &&
               !dspic33_spi_frame_pin(cpu, 64u, NULL),
           "reset and invalid access release B1 frame output");
}
