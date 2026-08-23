#include "device/dspic33ep_mu/internal.h"

bool dspic33_device_internal_spi_module_disabled(const Dspic33* cpu, uint8_t channel) {
    if (channel < 2u) {
        return (dspic33_device_internal_raw_word(cpu, 0x0760u) & (uint16_t)(0x0008u << channel)) !=
               0u;
    }
    return (dspic33_device_internal_raw_word(cpu, 0x076au) & (uint16_t)(1u << (channel - 2u))) !=
           0u;
}

bool dspic33_device_internal_spi_master(const Dspic33* cpu, uint8_t channel) {
    return (dspic33_device_internal_raw_word(cpu,
                                             (uint16_t)(dspic33_device_spi_bases[channel] + 2u)) &
            SPI_MASTER) != 0u;
}

bool dspic33_device_internal_spi_enhanced(const Dspic33* cpu, uint8_t channel) {
    return (dspic33_device_internal_raw_word(cpu,
                                             (uint16_t)(dspic33_device_spi_bases[channel] + 4u)) &
            SPI_ENHANCED) != 0u;
}

void dspic33_device_internal_spi_refresh_status(Dspic33* cpu, uint8_t channel) {
    uint16_t spi_base = dspic33_device_spi_bases[channel];
    uint16_t status_word = dspic33_device_internal_raw_word(cpu, spi_base);

    status_word &= (uint16_t)~(SPI_BUFFER_COUNT_MASK | SPI_SHIFT_EMPTY | SPI_RX_EMPTY |
                               SPI_TX_FULL | SPI_RX_FULL);

    if (dspic33_device_internal_spi_enhanced(cpu, channel)) {
        if ((cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u) {
            status_word |= SPI_SHIFT_EMPTY;
        }
        if (cpu->io.spi_rx_fifo[channel].count == 0u) {
            status_word |= SPI_RX_EMPTY;
        }

        if (cpu->io.spi_tx_fifo[channel].count == 8u) {
            status_word |= SPI_TX_FULL;
        }
        if (cpu->io.spi_rx_fifo[channel].count == 8u) {
            status_word |= SPI_RX_FULL;
        }

        uint8_t pending_word_count = dspic33_device_internal_spi_master(cpu, channel)
                                         ? cpu->io.spi_tx_fifo[channel].count
                                         : cpu->io.spi_rx_fifo[channel].count;
        if (pending_word_count > 7u) {
            pending_word_count = 7u;
        }
        status_word |= (uint16_t)pending_word_count << 8u;
    } else {
        if (cpu->io.spi_tx_fifo[channel].count != 0u) {
            status_word |= SPI_TX_FULL;
        }
        if (cpu->io.spi_rx_fifo[channel].count != 0u) {
            status_word |= SPI_RX_FULL;
        }
    }
    dspic33_device_internal_raw_write_word(cpu, spi_base, status_word);
}

void dspic33_device_internal_spi_clear_buffers(Dspic33* cpu, uint8_t channel) {
    uint8_t channel_mask = (uint8_t)(1u << channel);

    memset(&cpu->io.spi_tx_fifo[channel], 0, sizeof(cpu->io.spi_tx_fifo[channel]));
    memset(&cpu->io.spi_rx_fifo[channel], 0, sizeof(cpu->io.spi_rx_fifo[channel]));
    cpu->io.spi_busy &= (uint8_t)~channel_mask;
    cpu->io.spi_frame_active &= (uint8_t)~channel_mask;
    cpu->io.spi_frame_output_pending &= (uint8_t)~channel_mask;
    cpu->io.spi_frame_output_clear_pending &= (uint8_t)~channel_mask;
    cpu->io.spi_shift[channel] = 0u;
    cpu->io.spi_pin_receive[channel] = 0u;
    cpu->io.spi_pin_bits[channel] = 0u;
    cpu->io.spi_pin_output_index[channel] = 0u;
    cpu->io.spi_pin_output_started &= (uint8_t)~channel_mask;
    cpu->io.spi_generation[channel] =
        (uint16_t)((cpu->io.spi_generation[channel] + 1u) & SPI_EVENT_GENERATION_MASK);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 8u),
                                           0u);
    dspic33_device_internal_spi_refresh_status(cpu, channel);
}

static void spi_begin_frame(Dspic33* cpu, uint8_t channel) {
    uint16_t frame_control_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 4u));
    uint8_t channel_mask = (uint8_t)(1u << channel);

    if (dspic33_device_internal_spi_master(cpu, channel) &&
        (frame_control_word & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == SPI_FRAME_ENABLE) {
        return;
    }
    if (!dspic33_device_internal_spi_master(cpu, channel) &&
        (frame_control_word & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == SPI_FRAME_ENABLE) {
        cpu->io.spi_frame_output_pending |= channel_mask;
        cpu->io.spi_frame_output_clear_pending &= (uint8_t)~channel_mask;
        cpu->io.spi_frame_active &= (uint8_t)~channel_mask;
        cpu->io.spi_pin_output_started &= (uint8_t)~channel_mask;
        return;
    }
    cpu->io.spi_frame_active &= (uint8_t)~channel_mask;
}

bool dspic33_device_internal_spi_power_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t status_word = dspic33_device_internal_raw_word(cpu, dspic33_device_spi_bases[channel]);
    if (!dspic33_device_internal_spi_master(cpu, channel)) {
        return cpu->power_state != DSPIC33_POWER_IDLE || (status_word & SPI_STOP_IDLE) == 0u;
    }
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    return cpu->power_state == DSPIC33_POWER_IDLE && (status_word & SPI_STOP_IDLE) == 0u;
}

bool dspic33_device_internal_spi_selected(const Dspic33* cpu, uint8_t channel) {
    uint16_t control_word1 =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 2u));
    uint16_t control_word2 =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 4u));
    bool selection_required =
        (control_word2 & SPI_FRAME_ENABLE) != 0u
            ? (control_word2 & SPI_FRAME_SLAVE) != 0u
            : !dspic33_device_internal_spi_master(cpu, channel) && (control_word1 & 0x0080u) != 0u;
    return !selection_required || (cpu->io.spi_selected & (uint8_t)(1u << channel)) != 0u;
}

bool dspic33_device_internal_spi_master_frame_slave(const Dspic33* cpu, uint8_t channel) {
    uint16_t control =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 4u));
    return dspic33_device_internal_spi_master(cpu, channel) &&
           (control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE);
}

static bool spi_master_frame_master(const Dspic33* cpu, uint8_t channel) {
    uint16_t control =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 4u));
    return dspic33_device_internal_spi_master(cpu, channel) &&
           (control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == SPI_FRAME_ENABLE;
}

bool dspic33_device_internal_spi_slave_frame_master(const Dspic33* cpu, uint8_t channel) {
    uint16_t control =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 4u));
    return !dspic33_device_internal_spi_master(cpu, channel) &&
           (control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == SPI_FRAME_ENABLE;
}

static bool spi_pps_input_high(const Dspic33* cpu, uint8_t selection) {
    bool input_is_high;
    return dspic33_device_internal_pps_physical_input_high(cpu, selection, &input_is_high) &&
           input_is_high;
}

static bool spi_master_input_high(const Dspic33* cpu, uint8_t channel) {
    static const uint16_t input_registers[DSPIC33_SPI_COUNT] = {0x06c8u, 0u, 0x06dau, 0x06deu};
    if (input_registers[channel] != 0u) {
        return spi_pps_input_high(
            cpu,
            (uint8_t)(dspic33_device_internal_raw_word(cpu, input_registers[channel]) & 0x007fu));
    }
    return (cpu->io.spi_pin_data_high & (uint8_t)(1u << channel)) != 0u;
}

static bool spi_master_pin_input_enabled(const Dspic33* cpu, uint8_t channel) {
    static const uint16_t input_registers[DSPIC33_SPI_COUNT] = {0x06c8u, 0u, 0x06dau, 0x06deu};
    return (cpu->io.spi_pin_input_enabled & (uint8_t)(1u << channel)) != 0u ||
           (input_registers[channel] != 0u &&
            (dspic33_device_internal_raw_word(cpu, input_registers[channel]) & 0x007fu) != 0u);
}

void dspic33_device_internal_spi_refresh_pps_inputs(Dspic33* cpu) {
    static const uint8_t channels[] = {0u, 2u, 3u};
    static const uint16_t input_registers[] = {0x06c8u, 0x06dau, 0x06deu};
    static const uint16_t select_registers[] = {0x06cau, 0x06dcu, 0x06e0u};
    for (size_t mapping_index = 0u; mapping_index < sizeof(channels) / sizeof(channels[0]);
         mapping_index++) {
        uint16_t input_mapping =
            dspic33_device_internal_raw_word(cpu, input_registers[mapping_index]);
        uint16_t select_mapping =
            dspic33_device_internal_raw_word(cpu, select_registers[mapping_index]);
        uint8_t channel = channels[mapping_index];

        if (input_mapping == 0u && select_mapping == 0u) {
            continue;
        }
        bool data_is_high = spi_pps_input_high(cpu, (uint8_t)(input_mapping & 0x007fu));
        bool clock_is_high = spi_pps_input_high(cpu, (uint8_t)((input_mapping >> 8u) & 0x007fu));
        bool select_is_high = spi_pps_input_high(cpu, (uint8_t)(select_mapping & 0x007fu));
        dspic33_spi_pin_input(cpu, channel, clock_is_high, data_is_high, select_is_high);
    }
}

uint64_t dspic33_device_internal_spi_transfer_cycles(const Dspic33* cpu, uint8_t channel) {
    static const uint8_t primary_divisors[] = {64u, 16u, 4u, 1u};
    uint16_t control_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 2u));
    uint8_t secondary_divisor = (uint8_t)(8u - ((control_word >> 2u) & 7u));
    uint8_t transfer_bit_count = (control_word & SPI_MODE_16) != 0u ? 16u : 8u;
    return (uint64_t)transfer_bit_count * primary_divisors[control_word & 3u] * secondary_divisor;
}

static uint8_t spi_interrupt_mode(const Dspic33* cpu, uint8_t channel) {
    return (uint8_t)((dspic33_device_internal_raw_word(cpu, dspic33_device_spi_bases[channel]) &
                      SPI_INTERRUPT_MODE_MASK) >>
                     2u);
}

void dspic33_device_internal_spi_raise_mode(Dspic33* cpu, uint8_t channel, uint8_t mode) {
    if (!dspic33_device_internal_spi_enhanced(cpu, channel) ||
        spi_interrupt_mode(cpu, channel) == mode) {
        dspic33_raise_interrupt(cpu, dspic33_device_spi_irqs[channel]);
    }
}

static void spi_raise_transfer_interrupt(Dspic33* cpu, uint8_t channel) {
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, dspic33_device_spi_irqs[channel], 0u, 1u)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_internal_spi_remove_internal_events(Dspic33* cpu, uint8_t channel) {
    size_t retained_event_count = 0u;

    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        Dspic33Event* event = &cpu->events.items[event_index];
        if (event->type != DSPIC33_EVENT_SPI || event->source != channel ||
            (event->value & SPI_EVENT_KIND_MASK) == SPI_EVENT_EXTERNAL) {
            cpu->events.items[retained_event_count++] = *event;
        }
    }
    cpu->events.count = retained_event_count;
    dspic33_reorder_events(cpu);
}

static void spi_remove_external_events(Dspic33* cpu, uint8_t channel) {
    size_t retained_event_count = 0u;

    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        Dspic33Event* event = &cpu->events.items[event_index];
        if (event->type != DSPIC33_EVENT_SPI || event->source != channel ||
            (event->value & SPI_EVENT_KIND_MASK) != SPI_EVENT_EXTERNAL) {
            cpu->events.items[retained_event_count++] = *event;
        }
    }
    cpu->events.count = retained_event_count;
    dspic33_reorder_events(cpu);
}

void dspic33_device_internal_spi_update_power_state(Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t status_word =
            dspic33_device_internal_raw_word(cpu, dspic33_device_spi_bases[channel]);
        bool should_abort =
            dspic33_device_internal_spi_master(cpu, channel) &&
            (cpu->power_state == DSPIC33_POWER_SLEEP ||
             (cpu->power_state == DSPIC33_POWER_IDLE && (status_word & SPI_STOP_IDLE) != 0u));

        if (should_abort) {
            dspic33_device_internal_spi_remove_internal_events(cpu, channel);
            dspic33_device_internal_spi_clear_buffers(cpu, channel);
        }
    }
}

void dspic33_device_internal_spi_schedule_current(Dspic33* cpu, uint8_t channel) {
    uint16_t control_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 2u));
    uint8_t channel_mask = (uint8_t)(1u << channel);
    uint8_t transfer_bit_count;
    uint32_t event_value;
    uint64_t bit_period;
    uint64_t first_sample_delay;
    bool scheduled = true;
    if ((cpu->io.spi_busy & channel_mask) == 0u ||
        !dspic33_device_internal_spi_master(cpu, channel) ||
        (control_word & SPI_DISABLE_CLOCK) != 0u ||
        !dspic33_device_internal_spi_power_enabled(cpu, channel)) {
        return;
    }
    event_value = SPI_EVENT_INTERNAL |
                  ((uint32_t)cpu->io.spi_generation[channel] << SPI_EVENT_GENERATION_SHIFT) |
                  cpu->io.spi_shift[channel];
    if (spi_master_pin_input_enabled(cpu, channel)) {
        transfer_bit_count = (control_word & SPI_MODE_16) != 0u ? 16u : 8u;
        bit_period = dspic33_device_internal_spi_transfer_cycles(cpu, channel) / transfer_bit_count;
        first_sample_delay =
            (control_word & SPI_SAMPLE_END) != 0u ? bit_period : (bit_period + 1u) / 2u;
        for (uint8_t bit_index = 0u; bit_index < transfer_bit_count; bit_index++) {
            scheduled &=
                dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel,
                                 SPI_EVENT_SAMPLE | ((uint32_t)cpu->io.spi_generation[channel]
                                                     << SPI_EVENT_GENERATION_SHIFT),
                                 first_sample_delay + (uint64_t)bit_index * bit_period);
        }
    }
    scheduled &= dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, event_value,
                                  dspic33_device_internal_spi_transfer_cycles(cpu, channel));
    if (!scheduled) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        dspic33_device_internal_spi_remove_internal_events(cpu, channel);
        dspic33_device_internal_spi_clear_buffers(cpu, channel);
    }
}

static void spi_start_next_admitted(Dspic33* cpu, uint8_t channel, bool frame_admitted) {
    uint16_t register_base = dspic33_device_spi_bases[channel];
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 2u));
    uint8_t channel_mask = (uint8_t)(1u << channel);
    uint8_t previous_fifo_count = cpu->io.spi_tx_fifo[channel].count;
    uint16_t transmit_word;

    if (!frame_admitted && spi_master_frame_master(cpu, channel) && previous_fifo_count != 0u &&
        (cpu->io.spi_frame_output_pending & channel_mask) == 0u) {
        uint8_t transfer_bit_count = (control_word & SPI_MODE_16) != 0u ? 16u : 8u;
        uint64_t bit_period =
            dspic33_device_internal_spi_transfer_cycles(cpu, channel) / transfer_bit_count;
        uint64_t phase_offset =
            (cpu->device_cycles - cpu->io.spi_clock_start_cycle[channel]) % bit_period;
        uint64_t frame_start_delay = phase_offset == 0u ? 0u : bit_period - phase_offset;
        uint32_t event_value = SPI_EVENT_FRAME_START | ((uint32_t)cpu->io.spi_generation[channel]
                                                        << SPI_EVENT_GENERATION_SHIFT);
        if (dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, event_value, frame_start_delay)) {
            cpu->io.spi_frame_output_pending |= channel_mask;
        } else {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            dspic33_device_internal_spi_clear_buffers(cpu, channel);
        }
        dspic33_device_internal_spi_refresh_status(cpu, channel);
        return;
    }
    if ((cpu->io.spi_busy & channel_mask) != 0u || previous_fifo_count == 0u ||
        (dspic33_device_internal_raw_word(cpu, register_base) & SPI_ENABLE) == 0u ||
        dspic33_device_internal_spi_module_disabled(cpu, channel) ||
        (!dspic33_device_internal_spi_master(cpu, channel) &&
         (control_word & SPI_SLAVE_SELECT) != 0u &&
         !dspic33_device_internal_spi_selected(cpu, channel)) ||
        (dspic33_device_internal_spi_master_frame_slave(cpu, channel) && !frame_admitted) ||
        !dspic33_device_internal_word_queue_pop(&cpu->io.spi_tx_fifo[channel], &transmit_word)) {
        dspic33_device_internal_spi_refresh_status(cpu, channel);
        return;
    }
    if ((cpu->io.spi_busy & channel_mask) == 0u &&
        (dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 4u)) &
         SPI_FRAME_ENABLE) == 0u) {
        cpu->io.spi_clock_start_cycle[channel] = cpu->device_cycles;
    }
    cpu->io.spi_busy |= channel_mask;
    cpu->io.spi_shift[channel] = transmit_word;
    cpu->io.spi_start_cycle[channel] = cpu->device_cycles;
    cpu->io.spi_pin_receive[channel] = 0u;
    cpu->io.spi_pin_bits[channel] = 0u;
    cpu->io.spi_pin_output_index[channel] = 0u;
    if (!dspic33_device_internal_spi_master(cpu, channel) &&
        (control_word & SPI_CLOCK_EDGE) != 0u) {
        cpu->io.spi_pin_output_started |= channel_mask;
    } else {
        cpu->io.spi_pin_output_started &= (uint8_t)~channel_mask;
    }
    spi_begin_frame(cpu, channel);
    if ((control_word & SPI_DISABLE_OUTPUT) == 0u) {
        dspic33_device_internal_byte_queue_push(&cpu->io.spi_tx[channel], (uint8_t)transmit_word);
        if ((control_word & SPI_MODE_16) != 0u) {
            dspic33_device_internal_byte_queue_push(&cpu->io.spi_tx[channel],
                                                    (uint8_t)(transmit_word >> 8u));
        }
    }
    dspic33_device_internal_spi_refresh_status(cpu, channel);
    if (dspic33_device_internal_spi_enhanced(cpu, channel)) {
        if (previous_fifo_count == 8u) {
            dspic33_device_internal_spi_raise_mode(cpu, channel, 4u);
        }
        if (cpu->io.spi_tx_fifo[channel].count == 0u) {
            dspic33_device_internal_spi_raise_mode(cpu, channel, 6u);
        }
    }
    dspic33_device_internal_spi_schedule_current(cpu, channel);
}

void dspic33_device_internal_spi_start_next(Dspic33* cpu, uint8_t channel) {
    spi_start_next_admitted(cpu, channel, false);
}

void dspic33_device_internal_spi_update_slave_selection(Dspic33* cpu, uint8_t channel,
                                                        bool was_selected, bool is_selected) {
    uint16_t control_word1 =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 2u));
    uint16_t control_word2 =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 4u));
    uint8_t channel_mask = (uint8_t)(1u << channel);

    if (was_selected == is_selected || dspic33_device_internal_spi_master(cpu, channel) ||
        (control_word1 & SPI_SLAVE_SELECT) == 0u || (control_word2 & SPI_FRAME_ENABLE) != 0u) {
        return;
    }
    if (!is_selected && (cpu->io.spi_busy & channel_mask) != 0u) {
        if (!dspic33_device_internal_word_queue_push_front(&cpu->io.spi_tx_fifo[channel],
                                                           cpu->io.spi_shift[channel])) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            dspic33_device_internal_spi_clear_buffers(cpu, channel);
            return;
        }
        cpu->io.spi_busy &= (uint8_t)~channel_mask;
        cpu->io.spi_frame_active &= (uint8_t)~channel_mask;
        cpu->io.spi_frame_output_pending &= (uint8_t)~channel_mask;
        cpu->io.spi_frame_output_clear_pending &= (uint8_t)~channel_mask;
        cpu->io.spi_pin_receive[channel] = 0u;
        cpu->io.spi_pin_bits[channel] = 0u;
        cpu->io.spi_pin_output_index[channel] = 0u;
        cpu->io.spi_pin_output_started &= (uint8_t)~channel_mask;
        cpu->io.spi_generation[channel] =
            (uint16_t)((cpu->io.spi_generation[channel] + 1u) & SPI_EVENT_GENERATION_MASK);
        spi_remove_external_events(cpu, channel);
        dspic33_device_internal_spi_refresh_status(cpu, channel);
    } else if (is_selected) {
        dspic33_device_internal_spi_start_next(cpu, channel);
    }
}

void dspic33_device_internal_spi_schedule_frame_input_sample(Dspic33* cpu, uint8_t channel) {
    uint16_t control_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 2u));
    uint8_t transfer_bit_count = (control_word & SPI_MODE_16) != 0u ? 16u : 8u;
    uint64_t bit_period =
        dspic33_device_internal_spi_transfer_cycles(cpu, channel) / transfer_bit_count;
    uint64_t phase_offset =
        (cpu->device_cycles - cpu->io.spi_clock_start_cycle[channel]) % bit_period;
    uint64_t sample_offset = bit_period / 2u;
    uint64_t sample_delay = phase_offset <= sample_offset
                                ? sample_offset - phase_offset
                                : bit_period - phase_offset + sample_offset;
    uint32_t event_value = SPI_EVENT_FRAME_INPUT | ((uint32_t)cpu->io.spi_generation[channel]
                                                    << SPI_EVENT_GENERATION_SHIFT);
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, event_value, sample_delay)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void spi_receive_word(Dspic33* cpu, uint8_t channel, uint16_t received_word) {
    uint16_t register_base = dspic33_device_spi_bases[channel];
    uint16_t status_word = dspic33_device_internal_raw_word(cpu, register_base);
    uint8_t receive_fifo_capacity = dspic33_device_internal_spi_enhanced(cpu, channel) ? 8u : 1u;

    if ((dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 2u)) & SPI_MODE_16) ==
        0u) {
        received_word &= 0x00ffu;
    }

    if ((status_word & SPI_OVERFLOW) != 0u) {
        return;
    }

    if (cpu->io.spi_rx_fifo[channel].count >= receive_fifo_capacity ||
        !dspic33_device_internal_word_queue_push(&cpu->io.spi_rx_fifo[channel], received_word)) {
        dspic33_device_internal_raw_write_word(cpu, register_base,
                                               (uint16_t)(status_word | SPI_OVERFLOW));
        dspic33_raise_interrupt(cpu, dspic33_device_spi_error_irqs[channel]);
        dspic33_raise_interrupt(cpu, dspic33_device_spi_irqs[channel]);
        return;
    }

    if (cpu->io.spi_rx_fifo[channel].count == 1u) {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(register_base + 8u), received_word);
    }

    dspic33_device_internal_spi_refresh_status(cpu, channel);
    if (dspic33_device_internal_spi_enhanced(cpu, channel)) {
        dspic33_device_internal_spi_raise_mode(cpu, channel, 1u);
        if (cpu->io.spi_rx_fifo[channel].count >= 6u) {
            dspic33_device_internal_spi_raise_mode(cpu, channel, 2u);
        }
        if (cpu->io.spi_rx_fifo[channel].count == 8u) {
            dspic33_device_internal_spi_raise_mode(cpu, channel, 3u);
        }
    }
}

void dspic33_device_internal_spi_complete_transfer(Dspic33* cpu, uint8_t channel,
                                                   uint16_t received_word) {
    uint8_t channel_mask = (uint8_t)(1u << channel);

    cpu->io.spi_busy &= (uint8_t)~channel_mask;
    cpu->io.spi_frame_active &= (uint8_t)~channel_mask;
    cpu->io.spi_frame_output_pending &= (uint8_t)~channel_mask;
    cpu->io.spi_frame_output_clear_pending &= (uint8_t)~channel_mask;
    cpu->io.spi_pin_receive[channel] = 0u;
    cpu->io.spi_pin_bits[channel] = 0u;
    cpu->io.spi_pin_output_index[channel] = 0u;
    cpu->io.spi_pin_output_started &= (uint8_t)~channel_mask;

    spi_receive_word(cpu, channel, received_word);
    dspic33_dma_request(cpu, dspic33_device_spi_dma_requests[channel],
                        (uint16_t)(dspic33_device_spi_bases[channel] + 8u), 0u);
    dspic33_device_internal_spi_start_next(cpu, channel);

    if (!dspic33_device_internal_spi_enhanced(cpu, channel)) {
        spi_raise_transfer_interrupt(cpu, channel);
    } else if ((cpu->io.spi_busy & channel_mask) == 0u &&
               cpu->io.spi_tx_fifo[channel].count == 0u) {
        dspic33_device_internal_spi_raise_mode(cpu, channel, 5u);
    }

    dspic33_device_internal_spi_refresh_status(cpu, channel);
}

void dspic33_device_internal_run_spi(Dspic33* cpu, uint8_t channel, uint32_t event_value) {
    uint16_t register_base;
    uint16_t event_data = (uint16_t)event_value;
    uint8_t channel_mask;
    uint32_t event_kind = event_value & SPI_EVENT_KIND_MASK;

    if (channel >= DSPIC33_SPI_COUNT) {
        return;
    }

    register_base = dspic33_device_spi_bases[channel];
    channel_mask = (uint8_t)(1u << channel);

    if (event_kind == 0u) {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(register_base + 8u), event_data);
        dspic33_device_internal_raw_write_word(
            cpu, register_base,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, register_base) | SPI_RX_FULL));
        dspic33_raise_interrupt(cpu, dspic33_device_spi_irqs[channel]);
        return;
    }

    if ((dspic33_device_internal_raw_word(cpu, register_base) & SPI_ENABLE) == 0u ||
        dspic33_device_internal_spi_module_disabled(cpu, channel)) {
        return;
    }

    if (event_kind == SPI_EVENT_FRAME) {
        uint16_t event_generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);
        uint16_t frame_control_word =
            dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 4u));

        if (event_generation == cpu->io.spi_generation[channel] &&
            (cpu->io.spi_busy & channel_mask) != 0u &&
            dspic33_device_internal_spi_power_enabled(cpu, channel) &&
            (frame_control_word & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE | SPI_FRAME_ACTIVE_HIGH)) ==
                (SPI_FRAME_ENABLE | SPI_FRAME_ACTIVE_HIGH)) {
            cpu->io.spi_frame_active |= channel_mask;
        }
        return;
    }

    if (event_kind == SPI_EVENT_INTERNAL) {
        uint16_t event_generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);

        if (event_generation != cpu->io.spi_generation[channel] ||
            (cpu->io.spi_busy & channel_mask) == 0u) {
            return;
        }

        if (!dspic33_device_internal_spi_power_enabled(cpu, channel)) {
            dspic33_device_internal_spi_clear_buffers(cpu, channel);
            return;
        }

        dspic33_device_internal_spi_complete_transfer(cpu, channel,
                                                      spi_master_pin_input_enabled(cpu, channel)
                                                          ? cpu->io.spi_pin_receive[channel]
                                                          : event_data);
        return;
    }

    if (event_kind == SPI_EVENT_SAMPLE) {
        uint16_t event_generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);

        if (event_generation == cpu->io.spi_generation[channel] &&
            (cpu->io.spi_busy & channel_mask) != 0u &&
            dspic33_device_internal_spi_power_enabled(cpu, channel)) {
            cpu->io.spi_pin_receive[channel] =
                (uint16_t)((cpu->io.spi_pin_receive[channel] << 1u) |
                           (spi_master_input_high(cpu, channel) ? 1u : 0u));
            cpu->io.spi_pin_bits[channel]++;
        }
        return;
    }

    if (event_kind == SPI_EVENT_FRAME_INPUT) {
        uint16_t event_generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);
        if (event_generation == cpu->io.spi_generation[channel] &&
            dspic33_device_internal_spi_master_frame_slave(cpu, channel) &&
            dspic33_device_internal_spi_selected(cpu, channel)) {
            uint16_t control_word =
                dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 2u));
            uint8_t transfer_bit_count = (control_word & SPI_MODE_16) != 0u ? 16u : 8u;
            uint64_t bit_period =
                dspic33_device_internal_spi_transfer_cycles(cpu, channel) / transfer_bit_count;
            uint32_t frame_event_value =
                SPI_EVENT_FRAME_START | ((uint32_t)event_generation << SPI_EVENT_GENERATION_SHIFT);

            if (!dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, frame_event_value,
                                  bit_period - bit_period / 2u)) {
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            }
        }
        return;
    }

    if (event_kind == SPI_EVENT_FRAME_START) {
        uint16_t event_generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);

        if (event_generation != cpu->io.spi_generation[channel]) {
            return;
        }
        if ((event_value & SPI_EVENT_FRAME_DATA) == 0u) {
            cpu->io.spi_frame_output_pending &= (uint8_t)~channel_mask;
        }
        if (dspic33_device_internal_spi_master_frame_slave(cpu, channel)) {
            spi_start_next_admitted(cpu, channel, true);
        } else if (spi_master_frame_master(cpu, channel)) {
            uint16_t control_word =
                dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 2u));
            uint16_t frame_control_word =
                dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 4u));
            uint8_t transfer_bit_count = (control_word & SPI_MODE_16) != 0u ? 16u : 8u;
            uint64_t bit_period =
                dspic33_device_internal_spi_transfer_cycles(cpu, channel) / transfer_bit_count;
            bool events_scheduled;

            if ((event_value & SPI_EVENT_FRAME_DATA) != 0u) {
                spi_start_next_admitted(cpu, channel, true);
                return;
            }
            if ((frame_control_word & SPI_FRAME_ACTIVE_HIGH) != 0u) {
                cpu->io.spi_frame_active |= channel_mask;
            }
            events_scheduled = dspic33_schedule(
                cpu, DSPIC33_EVENT_SPI, channel,
                SPI_EVENT_FRAME_CLEAR | ((uint32_t)event_generation << SPI_EVENT_GENERATION_SHIFT),
                dspic33_device_internal_spi_transfer_cycles(cpu, channel));
            if ((frame_control_word & SPI_FRAME_DELAY) != 0u) {
                spi_start_next_admitted(cpu, channel, true);
            } else {
                events_scheduled &=
                    dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel,
                                     SPI_EVENT_FRAME_START | SPI_EVENT_FRAME_DATA |
                                         ((uint32_t)event_generation << SPI_EVENT_GENERATION_SHIFT),
                                     bit_period);
            }
            if (!events_scheduled) {
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
                dspic33_device_internal_spi_remove_internal_events(cpu, channel);
                dspic33_device_internal_spi_clear_buffers(cpu, channel);
            }
        }
        return;
    }

    if (event_kind == SPI_EVENT_FRAME_CLEAR) {
        uint16_t event_generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);

        if (event_generation == cpu->io.spi_generation[channel]) {
            cpu->io.spi_frame_active &= (uint8_t)~channel_mask;
        }
        return;
    }

    if (!dspic33_device_internal_spi_power_enabled(cpu, channel)) {
        return;
    }
    if (!dspic33_device_internal_spi_selected(cpu, channel) ||
        (dspic33_device_internal_spi_master(cpu, channel) &&
         (cpu->io.spi_busy & channel_mask) == 0u)) {
        return;
    }

    if (dspic33_device_internal_spi_master(cpu, channel)) {
        cpu->io.spi_generation[channel] =
            (uint16_t)((cpu->io.spi_generation[channel] + 1u) & SPI_EVENT_GENERATION_MASK);
    } else if ((cpu->io.spi_busy & channel_mask) == 0u) {
        cpu->io.spi_busy |= channel_mask;
        cpu->io.spi_shift[channel] =
            dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 8u));
    }
    dspic33_device_internal_spi_complete_transfer(cpu, channel, event_data);
}

void dspic33_device_internal_run_spi_select(Dspic33* cpu, uint8_t channel, bool is_selected) {
    uint8_t channel_mask;
    bool was_selected;

    if (channel >= DSPIC33_SPI_COUNT) {
        return;
    }

    channel_mask = (uint8_t)(1u << channel);
    was_selected = (cpu->io.spi_selected & channel_mask) != 0u;
    if (is_selected) {
        cpu->io.spi_selected |= channel_mask;
    } else {
        cpu->io.spi_selected &= (uint8_t)~channel_mask;
    }

    dspic33_device_internal_spi_update_slave_selection(cpu, channel, was_selected, is_selected);
    if (was_selected != is_selected &&
        dspic33_device_internal_spi_master_frame_slave(cpu, channel)) {
        dspic33_device_internal_spi_schedule_frame_input_sample(cpu, channel);
    }
}
