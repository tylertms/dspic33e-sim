#include "device/dspic33ep_mu/communication/spi/internal.h"

static void master_frame_slave_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        for (uint8_t polarity_index = 0u; polarity_index < 2u; polarity_index++) {
            const bool frame_active = polarity_index != 0u;

            for (uint8_t word_mode = 0u; word_mode < 2u; word_mode++) {
                const uint16_t spi_control = (uint16_t)(0x003bu | ((uint16_t)word_mode << 10u));
                const uint16_t frame_control =
                    (uint16_t)(0xc000u | ((uint16_t)polarity_index << 13u));
                const uint16_t transmitted_value = word_mode != 0u ? 0xa55au : 0x005au;
                const uint64_t transfer_cycles = dspic33_spi_test_transfer_cycles(spi_control);
                bool clock_high;

                dspic33_reset(cpu, 0u);
                dspic33_spi_test_configure_spi(cpu, channel_index, spi_control, frame_control, 0u);
                dspic33_spi_pin_input(cpu, channel_index, false, false, !frame_active);
                dspic33_write_word(cpu, (uint16_t)(bases[channel_index] + 8u), transmitted_value);
                expect(state,
                       (cpu->io.spi_busy & (uint8_t)(1u << channel_index)) == 0u &&
                           cpu->io.spi_tx_fifo[channel_index].count == 1u &&
                           dspic33_spi_clock_output(cpu, channel_index, &clock_high),
                       "master framed slave waits with free-running clock");
                dspic33_spi_pin_input(cpu, channel_index, false, false, frame_active);
                expect(state,
                       dspic33_device_advance(cpu, 1u) &&
                           (cpu->io.spi_busy & (uint8_t)(1u << channel_index)) == 0u,
                       "master framed slave samples pulse before transmitting");
                dspic33_spi_pin_input(cpu, channel_index, false, false, !frame_active);
                expect(state,
                       dspic33_device_advance(cpu, 1u) &&
                           (cpu->io.spi_busy & (uint8_t)(1u << channel_index)) != 0u,
                       "sampled frame pulse starts transfer on next transmit edge");
                expect(state,
                       dspic33_device_advance(cpu, transfer_cycles) &&
                           dspic33_read_word(cpu, (uint16_t)(bases[channel_index] + 8u)) == 0u &&
                           !dspic33_spi_test_interrupt_flag(cpu, irqs[channel_index]),
                       "master framed slave completes after full data frame");
                expect(state,
                       dspic33_spi_test_transfer_interrupt_after_cycle(cpu, irqs[channel_index]),
                       "master framed slave retains interrupt latency");
            }

            dspic33_reset(cpu, 0u);
            dspic33_spi_test_configure_spi(cpu, channel_index, 0x003bu,
                                           (uint16_t)(0xc000u | ((uint16_t)polarity_index << 13u)),
                                           0u);
            dspic33_spi_pin_input(cpu, channel_index, false, false, !frame_active);
            dspic33_write_word(cpu, (uint16_t)(bases[channel_index] + 8u), 0x00a5u);
            dspic33_spi_pin_input(cpu, channel_index, false, false, frame_active);
            dspic33_spi_pin_input(cpu, channel_index, false, false, !frame_active);
            expect(state,
                   dspic33_device_advance(cpu, 2u) &&
                       (cpu->io.spi_busy & (uint8_t)(1u << channel_index)) == 0u &&
                       cpu->io.spi_tx_fifo[channel_index].count == 1u,
                   "frame pulse released before sample edge is ignored");
        }
    }
}

static void clock_and_power_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        const uint16_t spi_base = bases[channel_index];
        const uint16_t spi_control = 0x143bu;
        const uint64_t transfer_cycles = dspic33_spi_test_transfer_cycles(spi_control);
        const uint8_t channel_bit = (uint8_t)(1u << channel_index);

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel_index, spi_control, 0u, 0u);
        dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), 0x1111u);
        expect(state, cpu->events.count == 0u, "disabled clock does not schedule");
        dspic33_write_word(cpu, (uint16_t)(spi_base + 2u), 0x043bu);
        expect(state, cpu->events.count == 1u, "clock enable resumes transfer");
        expect(state, dspic33_device_advance(cpu, transfer_cycles),
               "resumed clock completion advance");
        expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 8u)) == 0x1111u,
               "resumed clock transfer value");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel_index, 0x043bu, 0u, 0u);
        dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), 0x2222u);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        dspic33_device_power_state_changed(cpu);
        expect(state, (cpu->io.spi_busy & channel_bit) == 0u && cpu->events.count == 0u,
               "master sleep aborts transfer immediately");
        expect(state, dspic33_device_advance(cpu, transfer_cycles),
               "master sleep transfer advance");
        expect(state,
               (dspic33_read_word(cpu, spi_base) & 1u) == 0u &&
                   (cpu->io.spi_busy & channel_bit) == 0u,
               "master sleep aborts transfer");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel_index, 0x0400u, 0u, 0u);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state,
               dspic33_spi_receive(cpu, channel_index, 0x3333u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "slave sleep transaction advance");
        expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 8u)) == 0x3333u,
               "slave sleep completes transfer");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel_index, 0u, 0u, 0u);
        dspic33_write_word(cpu, spi_base, 0xa000u);
        cpu->power_state = DSPIC33_POWER_IDLE;
        expect(state,
               dspic33_spi_receive(cpu, channel_index, 0x55u, 1u) &&
                   dspic33_device_advance(cpu, 1u) && (dspic33_read_word(cpu, spi_base) & 1u) == 0u,
               "stopped-idle slave ignores logical transfer input");
        for (uint8_t edge_index = 0u; edge_index < 8u; edge_index++) {
            dspic33_spi_pin_input(cpu, channel_index, true, true, false);
            dspic33_spi_pin_input(cpu, channel_index, false, true, false);
        }
        expect(state,
               cpu->io.spi_pin_bits[channel_index] == 0u &&
                   (dspic33_read_word(cpu, spi_base) & 1u) == 0u,
               "stopped-idle slave ignores physical clock edges");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        for (uint8_t edge_index = 0u; edge_index < 8u; edge_index++) {
            dspic33_spi_pin_input(cpu, channel_index, true, true, false);
            dspic33_spi_pin_input(cpu, channel_index, false, true, false);
        }
        expect(state, dspic33_read_word(cpu, (uint16_t)(spi_base + 8u)) == 0xffu,
               "stopped-idle slave resumes on the next active physical edge");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel_index, 0x043bu, 0u, 0u);
        dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), 0x4444u);
        cpu->power_state = DSPIC33_POWER_IDLE;
        expect(state, dspic33_device_advance(cpu, transfer_cycles), "master idle running advance");
        expect(state, (dspic33_read_word(cpu, spi_base) & 1u) != 0u,
               "master idle continues by default");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel_index, 0x043bu, 0u, 0u);
        dspic33_write_word(cpu, spi_base, 0xa000u);
        dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), 0x5555u);
        cpu->power_state = DSPIC33_POWER_IDLE;
        dspic33_device_power_state_changed(cpu);
        expect(state, (cpu->io.spi_busy & channel_bit) == 0u && cpu->events.count == 0u,
               "stopped-idle master aborts transfer immediately");
        expect(state, dspic33_device_advance(cpu, transfer_cycles), "master stopped idle advance");
        expect(state,
               (dspic33_read_word(cpu, spi_base) & 1u) == 0u &&
                   (cpu->io.spi_busy & channel_bit) == 0u,
               "master stopped idle aborts transfer");
    }
}

static void pmd_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        const uint16_t spi_base = bases[channel_index];
        const uint16_t pmd_address = channel_index < 2u ? 0x0760u : 0x076au;
        const uint16_t pmd_bit = channel_index < 2u ? (uint16_t)(0x0008u << channel_index)
                                                    : (uint16_t)(1u << (channel_index - 2u));

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel_index, 0x043bu, 0u, 0u);
        dspic33_write_word(cpu, pmd_address,
                           (uint16_t)(dspic33_read_word(cpu, pmd_address) | pmd_bit));
        dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), 0xaaaau);
        expect(state, (cpu->io.spi_busy & (uint8_t)(1u << channel_index)) == 0u,
               "pmd blocks transfer");
        dspic33_write_word(cpu, pmd_address,
                           (uint16_t)(dspic33_read_word(cpu, pmd_address) & ~pmd_bit));
        dspic33_write_word(cpu, (uint16_t)(spi_base + 8u), 0xbbbbu);
        expect(state, (cpu->io.spi_busy & (uint8_t)(1u << channel_index)) != 0u,
               "pmd clear restores transfer");
    }
}

static void dma_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t control = 0x043bu;
        uint64_t cycles = dspic33_spi_test_transfer_cycles(control);
        uint16_t rx_memory = (uint16_t)(0x5000u + channel * 0x20u);
        uint16_t tx_memory = (uint16_t)(rx_memory + 0x10u);
        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, control, 0u, 0u);
        dspic33_spi_test_configure_dma(cpu, 0u, 0x0001u, requests[channel], rx_memory,
                                       (uint16_t)(base + 8u), 0u);
        expect(state, dspic33_spi_receive(cpu, channel, 0x6a00u + channel, cycles),
               "schedule dma receive response");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x1000u + channel);
        expect(state, dspic33_device_advance(cpu, cycles), "dma receive completion advance");
        expect(state, dspic33_read_word(cpu, rx_memory) == 0x6a00u + channel, "dma receive value");
        expect(state,
               (dspic33_read_word(cpu, dspic33_spi_test_dma_base(0u)) & 0x8000u) != 0u &&
                   !dspic33_spi_test_interrupt_flag(cpu, 4u) && cpu->io.dma_index[0] == 0u,
               "dma receive active before completion");
        expect(state, dspic33_device_advance(cpu, 1u), "dma receive controller completion advance");
        expect(state,
               (dspic33_read_word(cpu, dspic33_spi_test_dma_base(0u)) & 0x8000u) == 0u &&
                   dspic33_spi_test_interrupt_flag(cpu, 4u),
               "dma receive one shot complete");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, control, 0u, 0u);
        dspic33_write_word(cpu, tx_memory, 0x7b00u + channel);
        dspic33_spi_test_configure_dma(cpu, 1u, 0x2001u, requests[channel], tx_memory,
                                       (uint16_t)(base + 8u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x2000u + channel);
        expect(state, dspic33_device_advance(cpu, cycles), "dma transmit request advance");
        expect(state, cpu->io.spi_shift[channel] == 0x7b00u + channel,
               "dma transmit loads next shift");
        expect(state,
               (dspic33_read_word(cpu, dspic33_spi_test_dma_base(1u)) & 0x8000u) != 0u &&
                   !dspic33_spi_test_interrupt_flag(cpu, 14u) && cpu->io.dma_index[1] == 0u,
               "dma transmit active before completion");
        expect(state, dspic33_device_advance(cpu, 1u),
               "dma transmit controller completion advance");
        expect(state,
               (dspic33_read_word(cpu, dspic33_spi_test_dma_base(1u)) & 0x8000u) == 0u &&
                   dspic33_spi_test_interrupt_flag(cpu, 14u),
               "dma transmit one shot complete");
        expect(state, dspic33_device_advance(cpu, cycles), "dma transmitted word completion");
        expect(state, cpu->io.spi_tx[channel].count == 4u, "dma transmit trace contains two words");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, control, 0u, 0u);
        dspic33_write_word(cpu, tx_memory, 0x8c00u + channel);
        dspic33_spi_test_configure_dma(cpu, 0u, 0x0001u, requests[channel], rx_memory,
                                       (uint16_t)(base + 8u), 0u);
        dspic33_spi_test_configure_dma(cpu, 1u, 0x2001u, requests[channel], tx_memory,
                                       (uint16_t)(base + 8u), 0u);
        expect(state, dspic33_spi_receive(cpu, channel, 0x9d00u + channel, cycles),
               "schedule duplex dma response");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x3000u + channel);
        expect(state, dspic33_device_advance(cpu, cycles), "duplex dma completion advance");
        expect(state, dspic33_read_word(cpu, rx_memory) == 0x9d00u + channel,
               "duplex dma receive value");
        expect(state, cpu->io.spi_shift[channel] == 0x3000u + channel && cpu->io.dma_active == 1u,
               "duplex DMA receive channel has controller priority");
        expect(state, dspic33_device_advance(cpu, 1u),
               "duplex DMA receive completion and transmit start");
        expect(state, cpu->io.spi_shift[channel] == 0x8c00u + channel, "duplex dma transmit value");

        dspic33_reset(cpu, 0u);
        dspic33_spi_test_configure_spi(cpu, channel, 0x0080u, 0u, 0u);
        dspic33_spi_test_configure_dma(cpu, 0u, 0x4000u, requests[channel], rx_memory,
                                       (uint16_t)(base + 8u), 2u);
        expect(state,
               dspic33_spi_select(cpu, channel, true, 0u) &&
                   dspic33_spi_receive(cpu, channel, 0xa1u, 1u) &&
                   dspic33_spi_receive(cpu, channel, 0xb2u, 4u) &&
                   dspic33_spi_receive(cpu, channel, 0xc3u, 7u) && dspic33_device_advance(cpu, 10u),
               "schedule byte dma receive block");
        expect(state,
               dspic33_read_byte(cpu, rx_memory) == 0xa1u &&
                   dspic33_read_byte(cpu, (uint16_t)(rx_memory + 1u)) == 0xb2u &&
                   dspic33_read_byte(cpu, (uint16_t)(rx_memory + 2u)) == 0xc3u,
               "byte dma receive preserves each SPI byte");
        expect(state,
               cpu->io.spi_rx_fifo[channel].count == 0u &&
                   (dspic33_read_word(cpu, base) & 0x0041u) == 0u,
               "byte dma receive drains SPI input");
        expect(state,
               dspic33_spi_test_interrupt_flag(cpu, 4u) && cpu->io.dma_index[0] == 0u &&
                   (dspic33_read_word(cpu, dspic33_spi_test_dma_base(0u)) & 0x8000u) != 0u,
               "byte dma receive completes the block");
    }
}

static void copy_and_reset_cases(TestState* state, Dspic33* cpu, Dspic33* copy) {
    uint64_t cycles = dspic33_spi_test_transfer_cycles(0x043bu);
    dspic33_reset(cpu, 0u);
    dspic33_reset(copy, 0u);
    dspic33_spi_test_configure_spi(cpu, 0u, 0x043bu, 1u, 5u);
    dspic33_write_word(cpu, 0x0248u, 0xaaaau);
    dspic33_write_word(cpu, 0x0248u, 0xbbbbu);
    expect(state, dspic33_copy(copy, cpu), "copy spi state");
    expect(state,
           copy->io.spi_busy == cpu->io.spi_busy &&
               copy->io.spi_tx_fifo[0].count == cpu->io.spi_tx_fifo[0].count,
           "copied spi queues");
    expect(state, dspic33_device_advance(copy, cycles), "advance copied spi state");
    expect(state, copy->io.spi_rx_fifo[0].count == 1u && copy->io.spi_shift[0] == 0xbbbbu,
           "copied spi completes independently");
    expect(state, (dspic33_read_word(cpu, 0x0240u) & 1u) == 0u, "source spi remains pending");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.spi_busy == 0u && cpu->io.spi_tx_fifo[0].count == 0u &&
               cpu->io.spi_rx_fifo[0].count == 0u,
           "reset clears spi state");
    expect(state, dspic33_read_word(cpu, 0x0240u) == 0u && dspic33_read_word(cpu, 0x0248u) == 0u,
           "reset clears spi registers");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    Dspic33 copy;
    bool initialized = dspic33_initialize(&cpu);
    bool copy_initialized = dspic33_initialize(&copy);
    expect(&state, initialized, "initialize SPI processor");
    expect(&state, copy_initialized, "initialize SPI copy");
    if (initialized && copy_initialized) {
        dspic33_spi_test_register_cases(&state, &cpu);
        dspic33_spi_test_split_buffer_cases(&state, &cpu);
        dspic33_spi_test_transmit_output_cases(&state, &cpu);
        dspic33_spi_test_receive_only_cases(&state, &cpu);
        dspic33_spi_test_physical_slave_input_cases(&state, &cpu, &copy);
        dspic33_spi_test_pps_slave_input_cases(&state, &cpu);
        dspic33_spi_test_master_output_cases(&state, &cpu, &copy);
        dspic33_spi_test_master_input_cases(&state, &cpu, &copy);
        dspic33_spi_test_timing_matrix_cases(&state, &cpu);
        dspic33_spi_test_standard_buffer_cases(&state, &cpu);
        dspic33_spi_test_enhanced_fifo_cases(&state, &cpu);
        dspic33_spi_test_interrupt_mode_cases(&state, &cpu);
        dspic33_spi_test_mode_transition_cases(&state, &cpu);
        dspic33_spi_test_selection_and_frame_cases(&state, &cpu);
        dspic33_spi_test_slave_select_retry_cases(&state, &cpu);
        dspic33_spi_test_b1_frame_output_cases(&state, &cpu, &copy);
        dspic33_spi_test_boundary_cases(&state, &cpu);
        master_frame_slave_cases(&state, &cpu);
        clock_and_power_cases(&state, &cpu);
        pmd_cases(&state, &cpu);
        dma_cases(&state, &cpu);
        copy_and_reset_cases(&state, &cpu, &copy);
    }
    if (copy_initialized) {
        dspic33_release(&copy);
    }
    if (initialized) {
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
