#include "device/dspic33ep_mu/internal.h"

uint8_t dspic33_device_internal_uart_transmit_interrupt_mode(const Dspic33* cpu, uint8_t channel) {
    uint16_t status =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 2u));
    return (uint8_t)(((status & UART_STATUS_TX_INTERRUPT_LOW) != 0u ? 1u : 0u) |
                     ((status & UART_STATUS_TX_INTERRUPT_HIGH) != 0u ? 2u : 0u));
}

static uint8_t uart_receive_interrupt_threshold(const Dspic33* cpu, uint8_t channel) {
    uint16_t mode =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 2u)) &
        UART_STATUS_RX_INTERRUPT_MASK;
    if (mode == 0x0080u) {
        return 3u;
    }
    if (mode == 0x00c0u) {
        return 4u;
    }
    return 1u;
}

static Dspic33UartParity uart_parity(uint16_t mode) {
    uint16_t selection = mode & UART_MODE_DATA_MASK;
    if (selection == 0x0002u) {
        return DSPIC33_UART_PARITY_EVEN;
    }
    if (selection == 0x0004u) {
        return DSPIC33_UART_PARITY_ODD;
    }
    return DSPIC33_UART_PARITY_NONE;
}

static bool uart_frame_data_parity(const Dspic33UartFrame* frame) {
    uint16_t value = frame->value;
    bool odd = false;
    uint8_t bit;
    for (bit = 0u; bit < frame->data_bits; bit++) {
        odd = odd != ((value & (uint16_t)(1u << bit)) != 0u);
    }
    return frame->parity == DSPIC33_UART_PARITY_EVEN ? odd : !odd;
}

static bool uart_frame_logical_bit(const Dspic33UartFrame* frame, uint8_t bit) {
    if (bit == 0u) {
        return false;
    }
    bit--;
    if (bit < frame->data_bits) {
        return (frame->value & (uint16_t)(1u << bit)) != 0u;
    }
    bit = (uint8_t)(bit - frame->data_bits);
    if (frame->parity != DSPIC33_UART_PARITY_NONE) {
        if (bit == 0u) {
            return uart_frame_data_parity(frame);
        }
        bit--;
    }
    return true;
}

static uint8_t uart_frame_bits(const Dspic33UartFrame* frame) {
    return (uint8_t)(1u + frame->data_bits + frame->stop_bits +
                     (frame->parity == DSPIC33_UART_PARITY_NONE ? 0u : 1u));
}

void dspic33_device_internal_uart_refresh_status(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_device_uart_bases[channel];
    uint16_t status = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
    uint8_t bit = (uint8_t)(1u << channel);
    Dspic33UartFrame frame;
    status &= (uint16_t)~(UART_STATUS_TX_FULL | UART_STATUS_TX_EMPTY | UART_STATUS_RX_IDLE |
                          UART_STATUS_PARITY_ERROR | UART_STATUS_FRAMING_ERROR |
                          UART_STATUS_RX_AVAILABLE);
    if (cpu->io.uart_tx_fifo[channel].count == DSPIC33_UART_FIFO_SIZE) {
        status |= UART_STATUS_TX_FULL;
    }
    if ((cpu->io.uart_tx_active & bit) == 0u && cpu->io.uart_tx_fifo[channel].count == 0u) {
        status |= UART_STATUS_TX_EMPTY;
    }
    if ((cpu->io.uart_rx_active & bit) == 0u) {
        status |= UART_STATUS_RX_IDLE;
    }
    if (dspic33_device_internal_uart_fifo_front(&cpu->io.uart_rx_fifo[channel], &frame)) {
        status |= UART_STATUS_RX_AVAILABLE;
        if (frame.parity_error) {
            status |= UART_STATUS_PARITY_ERROR;
        }
        if (frame.framing_error) {
            status |= UART_STATUS_FRAMING_ERROR;
        }
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 6u), frame.value & 0x01ffu);
    } else {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 6u), 0u);
    }
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 2u), status);
}

void dspic33_device_internal_uart_clear_receive(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    memset(&cpu->io.uart_rx_fifo[channel], 0, sizeof(cpu->io.uart_rx_fifo[channel]));
    memset(&cpu->io.uart_rx_hold[channel], 0, sizeof(cpu->io.uart_rx_hold[channel]));
    memset(&cpu->io.uart_rx_shift[channel], 0, sizeof(cpu->io.uart_rx_shift[channel]));
    cpu->io.uart_rx_hold_valid &= (uint8_t)~bit;
    cpu->io.uart_rx_active &= (uint8_t)~bit;
    cpu->io.uart_rx_generation[channel]++;
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 2u),
        (uint16_t)(dspic33_device_internal_raw_word(
                       cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 2u)) &
                   ~UART_STATUS_OVERRUN));
    dspic33_device_internal_uart_refresh_status(cpu, channel);
}

void dspic33_device_internal_uart_clear_transmit(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    memset(&cpu->io.uart_tx_fifo[channel], 0, sizeof(cpu->io.uart_tx_fifo[channel]));
    memset(&cpu->io.uart_tx_shift[channel], 0, sizeof(cpu->io.uart_tx_shift[channel]));
    cpu->io.uart_tx_active &= (uint8_t)~bit;
    cpu->io.uart_tx_scheduled &= (uint8_t)~bit;
    cpu->io.uart_generation[channel]++;
    dspic33_device_internal_uart_refresh_status(cpu, channel);
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}

void dspic33_device_internal_uart_reset_runtime(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_device_uart_bases[channel];
    uint16_t status = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
    uint16_t controls = status & (UART_STATUS_TX_INTERRUPT_HIGH | UART_STATUS_TX_INVERT |
                                  UART_STATUS_TX_INTERRUPT_LOW | UART_STATUS_RX_INTERRUPT_MASK |
                                  UART_STATUS_ADDRESS_DETECT);
    dspic33_device_internal_uart_clear_transmit(cpu, channel);
    dspic33_device_internal_uart_clear_receive(cpu, channel);
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(base + 2u),
        (uint16_t)(controls | UART_STATUS_TX_EMPTY | UART_STATUS_RX_IDLE));
}

void dspic33_device_internal_uart_disable_module(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_device_uart_bases[channel];
    uint16_t status = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
    bool preserve_write_pointer = (status & UART_STATUS_TX_ENABLE) == 0u;
    Dspic33UartFifo fifo = cpu->io.uart_tx_fifo[channel];
    dspic33_device_internal_uart_reset_runtime(cpu, channel);
    if (preserve_write_pointer) {
        cpu->io.uart_tx_fifo[channel] = fifo;
        dspic33_device_internal_uart_refresh_status(cpu, channel);
    }
}

static bool uart_transmitter_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_device_uart_bases[channel];
    return !dspic33_device_internal_uart_module_disabled(cpu, channel) &&
           (dspic33_device_internal_raw_word(cpu, base) & UART_MODE_ENABLE) != 0u &&
           (dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) & UART_STATUS_TX_ENABLE) !=
               0u;
}

static bool uart_cts_allows(const Dspic33* cpu, uint8_t channel) {
    uint16_t mode = dspic33_device_internal_raw_word(cpu, dspic33_device_uart_bases[channel]);
    return (mode & UART_MODE_UEN_MASK) != 0x0200u ||
           (cpu->io.uart_cts & (uint8_t)(1u << channel)) != 0u;
}

static uint64_t uart_frame_cycles(const Dspic33* cpu, uint8_t channel,
                                  const Dspic33UartFrame* frame) {
    uint16_t mode = dspic33_device_internal_raw_word(cpu, dspic33_device_uart_bases[channel]);
    uint64_t clocks = (mode & UART_MODE_HIGH_SPEED) != 0u ? 4u : 16u;
    uint64_t bits = frame->break_signal
                        ? 14u
                        : (uint64_t)(1u + frame->data_bits + frame->stop_bits +
                                     (frame->parity == DSPIC33_UART_PARITY_NONE ? 0u : 1u));
    return ((uint64_t)dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 8u)) +
            1u) *
           clocks * bits;
}

void dspic33_device_internal_uart_raise_transmit(Dspic33* cpu, uint8_t channel, bool dma) {
    dspic33_raise_interrupt(cpu, dspic33_device_uart_tx_irqs[channel]);
    if (dma) {
        dspic33_dma_request(cpu, dspic33_device_uart_tx_irqs[channel],
                            (uint16_t)(dspic33_device_uart_bases[channel] + 4u), 0u);
    }
}

void dspic33_device_internal_uart_schedule_transmit(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint64_t unit;
    uint64_t total;
    uint8_t phase;
    if ((cpu->io.uart_tx_active & bit) == 0u || (cpu->io.uart_tx_scheduled & bit) != 0u ||
        !uart_cts_allows(cpu, channel)) {
        return;
    }
    unit = (uint64_t)dspic33_device_internal_raw_word(
               cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 8u)) +
           1u;
    total = uart_frame_cycles(cpu, channel, &cpu->io.uart_tx_shift[channel]);
    cpu->io.uart_tx_start_cycle[channel] = cpu->device_cycles;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_UART, channel,
                          UART_EVENT_TRANSMIT | cpu->io.uart_generation[channel], total)) {
        return;
    }
    for (phase = 1u; phase < uart_frame_bits(&cpu->io.uart_tx_shift[channel]); phase++) {
        uint64_t delay = (uint64_t)phase * cpu->io.uart_tx_clocks[channel] * unit;
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_UART, channel,
                              UART_EVENT_PIN | UART_EVENT_PIN_TRANSMIT |
                                  ((uint32_t)phase << UART_EVENT_PIN_PHASE_SHIFT) |
                                  cpu->io.uart_generation[channel],
                              delay)) {
            cpu->io.uart_generation[channel]++;
            cpu->io.uart_tx_active &= (uint8_t)~bit;
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
    }
    if (cpu->io.uart_tx_shift[channel].irda) {
        for (phase = 0u; phase < uart_frame_bits(&cpu->io.uart_tx_shift[channel]); phase++) {
            uint8_t edge;
            for (edge = 0u; edge < 2u; edge++) {
                uint64_t delay = ((uint64_t)phase * 16u + (edge == 0u ? 7u : 10u)) * unit;
                if (!dspic33_schedule(cpu, DSPIC33_EVENT_UART, channel,
                                      UART_EVENT_PIN | UART_EVENT_PIN_TRANSMIT |
                                          ((uint32_t)phase << UART_EVENT_PIN_PHASE_SHIFT) |
                                          cpu->io.uart_generation[channel],
                                      delay)) {
                    cpu->io.uart_generation[channel]++;
                    cpu->io.uart_tx_active &= (uint8_t)~bit;
                    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
                    return;
                }
            }
        }
    }
    cpu->io.uart_tx_scheduled |= bit;
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}

static bool uart_rx_pin_level(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint8_t selection = (uint8_t)(dspic33_device_internal_raw_word(
                                      cpu, dspic33_device_uart_pps_registers[channel]) &
                                  0x007fu);
    if (selection == 0u) {
        *high = false;
        return true;
    }
    return dspic33_device_internal_pps_physical_input_high(cpu, selection, high);
}

bool dspic33_device_internal_uart_rx_logical_level(const Dspic33* cpu, uint8_t channel,
                                                   bool* high) {
    bool physical;
    if (!uart_rx_pin_level(cpu, channel, &physical)) {
        return false;
    }
    *high =
        physical != ((dspic33_device_internal_raw_word(cpu, dspic33_device_uart_bases[channel]) &
                      0x0010u) != 0u);
    return true;
}

bool dspic33_device_internal_uart_receiver_operating(const Dspic33* cpu, uint8_t channel) {
    uint16_t mode = dspic33_device_internal_raw_word(cpu, dspic33_device_uart_bases[channel]);
    return !dspic33_device_internal_uart_module_disabled(cpu, channel) &&
           (mode & (UART_MODE_ENABLE | UART_MODE_LOOPBACK | UART_MODE_IREN)) == UART_MODE_ENABLE &&
           cpu->power_state != DSPIC33_POWER_SLEEP &&
           (cpu->power_state != DSPIC33_POWER_IDLE || (mode & UART_MODE_STOP_IDLE) == 0u);
}

void dspic33_device_internal_uart_cancel_physical_receive(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.uart_rx_active &= (uint8_t)~bit;
    cpu->io.uart_rx_generation[channel]++;
    memset(&cpu->io.uart_rx_shift[channel], 0, sizeof(cpu->io.uart_rx_shift[channel]));
    cpu->io.uart_rx_votes[channel] = 0u;
    dspic33_device_internal_uart_refresh_status(cpu, channel);
}

void dspic33_device_internal_uart_reset_auto_baud(Dspic33* cpu, uint8_t channel) {
    uint8_t mask = (uint8_t)(1u << channel);
    cpu->io.uart_auto_baud_active &= (uint8_t)~mask;
    cpu->io.uart_auto_baud_edges[channel] = 0u;
    cpu->io.uart_auto_baud_first_cycle[channel] = 0u;
}

void dspic33_device_internal_uart_auto_baud_edge(Dspic33* cpu, uint8_t channel, bool previous_high,
                                                 bool high) {
    uint16_t base = dspic33_device_uart_bases[channel];
    uint16_t mode = dspic33_device_internal_raw_word(cpu, base);
    uint8_t mask = (uint8_t)(1u << channel);
    if ((mode & UART_MODE_AUTO_BAUD) == 0u || previous_high == high) {
        return;
    }
    if (previous_high && !high) {
        if ((cpu->io.uart_auto_baud_active & mask) == 0u) {
            dspic33_device_internal_uart_reset_auto_baud(cpu, channel);
            cpu->io.uart_auto_baud_active |= mask;
        }
        return;
    }
    if ((cpu->io.uart_auto_baud_active & mask) == 0u) {
        return;
    }
    cpu->io.uart_auto_baud_edges[channel]++;
    if (cpu->io.uart_auto_baud_edges[channel] == 1u) {
        cpu->io.uart_auto_baud_first_cycle[channel] = cpu->device_cycles;
        return;
    }
    if (cpu->io.uart_auto_baud_edges[channel] == 5u) {
        uint64_t elapsed = cpu->device_cycles - cpu->io.uart_auto_baud_first_cycle[channel];
        uint64_t clocks = (mode & UART_MODE_HIGH_SPEED) != 0u ? 4u : 16u;
        uint64_t bit_cycles = elapsed / 8u;
        uint16_t baud = bit_cycles >= clocks ? (uint16_t)(bit_cycles / clocks - 1u) : 0u;
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 8u), baud);
        dspic33_device_internal_raw_write_word(cpu, base, (uint16_t)(mode & ~UART_MODE_AUTO_BAUD));
        dspic33_device_internal_uart_reset_auto_baud(cpu, channel);
        dspic33_raise_interrupt(cpu, dspic33_device_uart_rx_irqs[channel]);
    }
}

void dspic33_device_internal_uart_begin_physical_receive(Dspic33* cpu, uint8_t channel) {
    uint16_t mode = dspic33_device_internal_raw_word(cpu, dspic33_device_uart_bases[channel]);
    uint64_t unit = (uint64_t)dspic33_device_internal_raw_word(
                        cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 8u)) +
                    1u;
    uint8_t samples = (mode & UART_MODE_HIGH_SPEED) != 0u ? 1u : 3u;
    uint8_t clocks = samples == 1u ? 4u : 16u;
    uint8_t sample_first = samples == 1u ? 2u : 7u;
    uint8_t bit_count;
    uint8_t frame_bit;
    uint8_t sample;
    uint8_t mask = (uint8_t)(1u << channel);
    Dspic33UartFrame* frame = &cpu->io.uart_rx_shift[channel];
    memset(frame, 0, sizeof(*frame));
    frame->data_bits = (mode & UART_MODE_NINE_BIT) == UART_MODE_NINE_BIT ? 9u : 8u;
    frame->stop_bits = (mode & UART_MODE_TWO_STOP_BITS) != 0u ? 2u : 1u;
    frame->parity = uart_parity(mode);
    frame->baud_period =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 8u));
    frame->inverted = (mode & 0x0010u) != 0u;
    cpu->io.uart_rx_generation[channel]++;
    cpu->io.uart_rx_samples[channel] = samples;
    cpu->io.uart_rx_votes[channel] = 0u;
    cpu->io.uart_rx_active |= mask;
    bit_count = uart_frame_bits(frame);
    for (frame_bit = 0u; frame_bit < bit_count; frame_bit++) {
        for (sample = 0u; sample < samples; sample++) {
            uint8_t phase = (uint8_t)(frame_bit * 3u + sample);
            uint64_t delay = ((uint64_t)frame_bit * clocks + sample_first + sample) * unit;
            if (!dspic33_schedule(cpu, DSPIC33_EVENT_UART, channel,
                                  UART_EVENT_PIN | ((uint32_t)phase << UART_EVENT_PIN_PHASE_SHIFT) |
                                      cpu->io.uart_rx_generation[channel],
                                  delay)) {
                dspic33_device_internal_uart_cancel_physical_receive(cpu, channel);
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
                return;
            }
        }
    }
    dspic33_device_internal_uart_refresh_status(cpu, channel);
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}

static void uart_sample_physical_receive(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint16_t generation = (uint16_t)value;
    uint8_t phase = (uint8_t)((value & UART_EVENT_PIN_PHASE_MASK) >> UART_EVENT_PIN_PHASE_SHIFT);
    uint8_t samples = cpu->io.uart_rx_samples[channel];
    uint8_t frame_bit = (uint8_t)(phase / 3u);
    uint8_t sample = (uint8_t)(phase % 3u);
    uint8_t mask = (uint8_t)(1u << channel);
    bool high;
    Dspic33UartFrame* frame = &cpu->io.uart_rx_shift[channel];
    if (generation != cpu->io.uart_rx_generation[channel] || samples == 0u ||
        (cpu->io.uart_rx_active & mask) == 0u ||
        !dspic33_device_internal_uart_rx_logical_level(cpu, channel, &high)) {
        return;
    }
    if (high) {
        cpu->io.uart_rx_votes[channel]++;
    }
    if (sample + 1u < samples) {
        return;
    }
    high = cpu->io.uart_rx_votes[channel] > samples / 2u;
    cpu->io.uart_rx_votes[channel] = 0u;
    if (frame_bit == 0u) {
        if (high) {
            dspic33_device_internal_uart_cancel_physical_receive(cpu, channel);
        }
        return;
    }
    frame_bit--;
    if (frame_bit < frame->data_bits) {
        if (high) {
            frame->value |= (uint16_t)(1u << frame_bit);
        }
        return;
    }
    frame_bit = (uint8_t)(frame_bit - frame->data_bits);
    if (frame->parity != DSPIC33_UART_PARITY_NONE) {
        if (frame_bit == 0u) {
            frame->parity_error = high != uart_frame_data_parity(frame);
            return;
        }
        frame_bit--;
    }
    if (!high) {
        frame->framing_error = true;
    }
    if (frame_bit + 1u == frame->stop_bits) {
        Dspic33UartFrame completed = *frame;
        cpu->io.uart_rx_active &= (uint8_t)~mask;
        dspic33_device_internal_uart_refresh_status(cpu, channel);
        dspic33_device_internal_uart_receive_complete(cpu, channel, &completed);
        dspic33_device_internal_refresh_physical_pin_inputs(cpu);
    }
}

void dspic33_device_internal_uart_start_transmit(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t interrupt_mode;
    Dspic33UartFrame* frame;
    uint16_t mode;
    uint16_t status;
    if (!uart_transmitter_enabled(cpu, channel) || (cpu->io.uart_tx_active & bit) != 0u ||
        !dspic33_device_internal_uart_fifo_pop(&cpu->io.uart_tx_fifo[channel],
                                               &cpu->io.uart_tx_shift[channel])) {
        dspic33_device_internal_uart_refresh_status(cpu, channel);
        return;
    }
    frame = &cpu->io.uart_tx_shift[channel];
    mode = dspic33_device_internal_raw_word(cpu, dspic33_device_uart_bases[channel]);
    status =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 2u));
    frame->break_signal = (status & UART_STATUS_BREAK) != 0u;
    frame->data_bits = (mode & UART_MODE_NINE_BIT) == UART_MODE_NINE_BIT ? 9u : 8u;
    frame->value &= frame->data_bits == 9u ? 0x01ffu : 0x00ffu;
    frame->stop_bits = (mode & UART_MODE_TWO_STOP_BITS) != 0u ? 2u : 1u;
    frame->parity = uart_parity(mode);
    frame->inverted = (status & UART_STATUS_TX_INVERT) != 0u;
    frame->irda = (mode & UART_MODE_IREN) != 0u;
    frame->baud_period =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 8u));
    cpu->io.uart_tx_clocks[channel] = (mode & UART_MODE_HIGH_SPEED) != 0u ? 4u : 16u;
    if (frame->break_signal) {
        frame->value = 0u;
        frame->data_bits = 12u;
        frame->stop_bits = 1u;
        frame->parity = DSPIC33_UART_PARITY_NONE;
    }
    cpu->io.uart_tx_active |= bit;
    interrupt_mode = dspic33_device_internal_uart_transmit_interrupt_mode(cpu, channel);
    if (!frame->break_signal && interrupt_mode == 1u && cpu->io.uart_tx_fifo[channel].count == 0u) {
        dspic33_device_internal_uart_refresh_status(cpu, channel);
        cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
        return;
    }
    if (!frame->break_signal && interrupt_mode == 0u) {
        dspic33_device_internal_uart_raise_transmit(cpu, channel, true);
    } else if (!frame->break_signal && interrupt_mode == 2u &&
               cpu->io.uart_tx_fifo[channel].count == 0u) {
        dspic33_device_internal_uart_raise_transmit(cpu, channel, false);
    }
    dspic33_device_internal_uart_refresh_status(cpu, channel);
    dspic33_device_internal_uart_schedule_transmit(cpu, channel);
}

void dspic33_device_internal_uart_receive_complete(Dspic33* cpu, uint8_t channel,
                                                   const Dspic33UartFrame* incoming) {
    uint16_t base = dspic33_device_uart_bases[channel];
    uint16_t mode = dspic33_device_internal_raw_word(cpu, base);
    uint16_t status = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
    uint8_t bit = (uint8_t)(1u << channel);
    Dspic33UartFrame frame = *incoming;
    if (dspic33_device_internal_uart_module_disabled(cpu, channel) ||
        (mode & UART_MODE_ENABLE) == 0u) {
        return;
    }
    if ((mode & UART_MODE_WAKE) != 0u) {
        dspic33_device_internal_raw_write_word(cpu, base, (uint16_t)(mode & ~UART_MODE_WAKE));
        dspic33_raise_interrupt(cpu, dspic33_device_uart_rx_irqs[channel]);
        return;
    }
    if ((mode & UART_MODE_AUTO_BAUD) != 0u) {
        if (frame.baud_period != 0u) {
            dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 8u), frame.baud_period);
        }
        dspic33_device_internal_raw_write_word(cpu, base, (uint16_t)(mode & ~UART_MODE_AUTO_BAUD));
        dspic33_raise_interrupt(cpu, dspic33_device_uart_rx_irqs[channel]);
        return;
    }
    frame.data_bits = (mode & UART_MODE_NINE_BIT) == UART_MODE_NINE_BIT ? 9u : 8u;
    frame.value &= frame.data_bits == 9u ? 0x01ffu : 0x00ffu;
    frame.parity = uart_parity(mode);
    if (frame.data_bits == 9u) {
        frame.parity_error = false;
    }
    if ((status & UART_STATUS_ADDRESS_DETECT) != 0u && frame.data_bits == 9u &&
        (frame.value & 0x0100u) == 0u) {
        return;
    }
    if ((status & UART_STATUS_OVERRUN) != 0u) {
        return;
    }
    if (!dspic33_device_internal_uart_fifo_push(&cpu->io.uart_rx_fifo[channel], &frame)) {
        cpu->io.uart_rx_hold[channel] = frame;
        cpu->io.uart_rx_hold_valid |= bit;
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 2u),
                                               (uint16_t)(status | UART_STATUS_OVERRUN));
        dspic33_device_internal_uart_refresh_status(cpu, channel);
        dspic33_raise_interrupt(cpu, dspic33_device_uart_error_irqs[channel]);
        return;
    }
    dspic33_device_internal_uart_refresh_status(cpu, channel);
    if (cpu->io.uart_rx_fifo[channel].count >= uart_receive_interrupt_threshold(cpu, channel)) {
        dspic33_raise_interrupt(cpu, dspic33_device_uart_rx_irqs[channel]);
    }
    if (uart_receive_interrupt_threshold(cpu, channel) == 1u) {
        dspic33_dma_request(cpu, dspic33_device_uart_rx_irqs[channel], (uint16_t)(base + 6u), 0u);
    }
    if (frame.parity_error || frame.framing_error) {
        dspic33_raise_interrupt(cpu, dspic33_device_uart_error_irqs[channel]);
    }
}

static void uart_transmit_complete(Dspic33* cpu, uint8_t channel, uint16_t generation) {
    uint8_t bit = (uint8_t)(1u << channel);
    Dspic33UartFrame frame;
    if (generation != cpu->io.uart_generation[channel] || (cpu->io.uart_tx_active & bit) == 0u ||
        (cpu->io.uart_tx_scheduled & bit) == 0u) {
        return;
    }
    frame = cpu->io.uart_tx_shift[channel];
    cpu->io.uart_tx_active &= (uint8_t)~bit;
    cpu->io.uart_tx_scheduled &= (uint8_t)~bit;
    dspic33_device_internal_uart_queue_push(&cpu->io.uart_tx[channel], &frame);
    if ((dspic33_device_internal_raw_word(cpu, dspic33_device_uart_bases[channel]) &
         UART_MODE_LOOPBACK) != 0u) {
        Dspic33UartFrame received = frame;
        received.framing_error = frame.break_signal;
        received.parity_error = false;
        received.break_signal = false;
        dspic33_device_internal_uart_receive_complete(cpu, channel, &received);
    }
    if (frame.break_signal) {
        if (dspic33_cpu_rmw_matches(cpu, dspic33_device_uart_bases[channel] + 2u, 2u)) {
            cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
            return;
        }
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 2u),
            (uint16_t)(dspic33_device_internal_raw_word(
                           cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 2u)) &
                       ~UART_STATUS_BREAK));
    }
    dspic33_device_internal_uart_start_transmit(cpu, channel);
    if ((cpu->io.uart_tx_active & bit) == 0u &&
        dspic33_device_internal_uart_transmit_interrupt_mode(cpu, channel) == 1u) {
        dspic33_device_internal_uart_raise_transmit(cpu, channel, false);
    }
    dspic33_device_internal_uart_refresh_status(cpu, channel);
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}

static bool uart_tx_output_level(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t mode = dspic33_device_internal_raw_word(cpu, dspic33_device_uart_bases[channel]);
    uint16_t status =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 2u));
    uint8_t mask = (uint8_t)(1u << channel);
    bool inverted = (status & UART_STATUS_TX_INVERT) != 0u;
    if (dspic33_device_internal_uart_module_disabled(cpu, channel) ||
        (mode & UART_MODE_ENABLE) == 0u || (status & UART_STATUS_TX_ENABLE) == 0u) {
        return false;
    }
    if ((cpu->io.uart_tx_scheduled & mask) == 0u || cpu->power_state == DSPIC33_POWER_SLEEP ||
        (cpu->power_state == DSPIC33_POWER_IDLE && (mode & UART_MODE_STOP_IDLE) != 0u)) {
        *high = (mode & UART_MODE_IREN) != 0u ? inverted : !inverted;
        return true;
    }
    {
        const Dspic33UartFrame* frame = &cpu->io.uart_tx_shift[channel];
        uint64_t unit = (uint64_t)frame->baud_period + 1u;
        uint64_t clocks = cpu->io.uart_tx_clocks[channel];
        uint64_t elapsed = cpu->device_cycles - cpu->io.uart_tx_start_cycle[channel];
        uint8_t frame_bit = (uint8_t)(elapsed / (unit * clocks));
        bool logical =
            frame_bit < uart_frame_bits(frame) ? uart_frame_logical_bit(frame, frame_bit) : true;
        if (!frame->irda) {
            *high = logical != frame->inverted;
        } else {
            uint8_t clock = (uint8_t)((elapsed / unit) % 16u);
            bool pulse = !logical && clock >= 7u && clock < 10u;
            *high = pulse != frame->inverted;
        }
    }
    return true;
}

static bool uart_rts_output_level(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t mode = dspic33_device_internal_raw_word(cpu, dspic33_device_uart_bases[channel]);
    uint16_t status =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 2u));
    uint16_t uen = mode & UART_MODE_UEN_MASK;
    uint8_t mask = (uint8_t)(1u << channel);
    if (dspic33_device_internal_uart_module_disabled(cpu, channel) ||
        (mode & UART_MODE_ENABLE) == 0u || (uen != 0x0100u && uen != 0x0200u)) {
        return false;
    }
    if ((mode & 0x0800u) != 0u) {
        *high = (status & UART_STATUS_TX_EMPTY) != 0u;
    } else {
        *high = cpu->io.uart_rx_fifo[channel].count == DSPIC33_UART_FIFO_SIZE ||
                (status & UART_STATUS_OVERRUN) != 0u || (cpu->io.uart_rx_active & mask) != 0u;
    }
    return true;
}

bool dspic33_device_internal_uart_pps_output_value(const Dspic33* cpu, uint8_t port, uint8_t pin,
                                                   bool* high) {
    const Dspic33PpsPin* mapping = NULL;
    uint8_t function;
    uint8_t channel;
    size_t index;
    if (high == NULL) {
        return false;
    }
    for (index = 0u; index < sizeof(dspic33_device_pps_pins) / sizeof(dspic33_device_pps_pins[0]);
         index++) {
        if (dspic33_device_pps_pins[index].port == port &&
            dspic33_device_pps_pins[index].bit == pin) {
            mapping = &dspic33_device_pps_pins[index];
            break;
        }
    }
    if (mapping == NULL) {
        return false;
    }
    function = dspic33_device_internal_pps_output_function(cpu, mapping->pin);
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        if (function == dspic33_device_uart_tx_functions[channel]) {
            return uart_tx_output_level(cpu, channel, high);
        }
        if (function == dspic33_device_uart_rts_functions[channel]) {
            return uart_rts_output_level(cpu, channel, high);
        }
    }
    return false;
}

void dspic33_device_internal_run_uart(Dspic33* cpu, uint8_t channel, uint32_t event_value) {
    uint32_t kind = event_value & UART_EVENT_KIND_MASK;
    if (channel >= DSPIC33_UART_COUNT) {
        return;
    }
    if (kind == UART_EVENT_TRANSMIT) {
        uart_transmit_complete(cpu, channel, (uint16_t)event_value);
    } else if (kind == UART_EVENT_CTS) {
        uint8_t bit = (uint8_t)(1u << channel);
        cpu->io.uart_cts_direct |= bit;
        if ((event_value & 1u) != 0u) {
            cpu->io.uart_cts |= bit;
            dspic33_device_internal_uart_schedule_transmit(cpu, channel);
        } else {
            cpu->io.uart_cts &= (uint8_t)~bit;
        }
    } else if (kind == UART_EVENT_PIN) {
        if ((event_value & UART_EVENT_PIN_TRANSMIT) != 0u) {
            if ((uint16_t)event_value == cpu->io.uart_generation[channel] &&
                (cpu->io.uart_tx_scheduled & (uint8_t)(1u << channel)) != 0u) {
                dspic33_device_internal_refresh_physical_pin_inputs(cpu);
            }
        } else {
            uart_sample_physical_receive(cpu, channel, event_value);
        }
    } else {
        Dspic33UartFrame frame;
        memset(&frame, 0, sizeof(frame));
        frame.value = (uint16_t)(event_value & 0x01ffu);
        frame.parity_error = (event_value & UART_EVENT_PARITY_ERROR) != 0u;
        frame.framing_error = (event_value & UART_EVENT_FRAMING_ERROR) != 0u;
        frame.baud_period =
            (uint16_t)((event_value & UART_EVENT_BAUD_MASK) >> UART_EVENT_BAUD_SHIFT);
        dspic33_device_internal_uart_receive_complete(cpu, channel, &frame);
    }
}
