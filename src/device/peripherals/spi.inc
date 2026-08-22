
static bool spi_module_disabled(const Dspic33* cpu, uint8_t channel) {
    if (channel < 2u) {
        return (raw_word(cpu, 0x0760u) & (uint16_t)(0x0008u << channel)) != 0u;
    }
    return (raw_word(cpu, 0x076au) & (uint16_t)(1u << (channel - 2u))) != 0u;
}

static bool spi_master(const Dspic33* cpu, uint8_t channel) {
    return (raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u)) & SPI_MASTER) != 0u;
}

static bool spi_enhanced(const Dspic33* cpu, uint8_t channel) {
    return (raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u)) & SPI_ENHANCED) != 0u;
}

static void spi_refresh_status(Dspic33* cpu, uint8_t channel) {
    uint16_t base = spi_bases[channel];
    uint16_t status = raw_word(cpu, base);
    uint8_t pending;
    status &= (uint16_t)~(SPI_BUFFER_COUNT_MASK | SPI_SHIFT_EMPTY | SPI_RX_EMPTY | SPI_TX_FULL |
                          SPI_RX_FULL);
    if (spi_enhanced(cpu, channel)) {
        if ((cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u) {
            status |= SPI_SHIFT_EMPTY;
        }
        if (cpu->io.spi_rx_fifo[channel].count == 0u) {
            status |= SPI_RX_EMPTY;
        }
        if (cpu->io.spi_tx_fifo[channel].count == 8u) {
            status |= SPI_TX_FULL;
        }
        if (cpu->io.spi_rx_fifo[channel].count == 8u) {
            status |= SPI_RX_FULL;
        }
        pending = spi_master(cpu, channel) ? cpu->io.spi_tx_fifo[channel].count
                                           : cpu->io.spi_rx_fifo[channel].count;
        if (pending > 7u) {
            pending = 7u;
        }
        status |= (uint16_t)pending << 8u;
    } else {
        if (cpu->io.spi_tx_fifo[channel].count != 0u) {
            status |= SPI_TX_FULL;
        }
        if (cpu->io.spi_rx_fifo[channel].count != 0u) {
            status |= SPI_RX_FULL;
        }
    }
    raw_write_word(cpu, base, status);
}

static void spi_clear_buffers(Dspic33* cpu, uint8_t channel) {
    memset(&cpu->io.spi_tx_fifo[channel], 0, sizeof(cpu->io.spi_tx_fifo[channel]));
    memset(&cpu->io.spi_rx_fifo[channel], 0, sizeof(cpu->io.spi_rx_fifo[channel]));
    cpu->io.spi_busy &= (uint8_t)~(1u << channel);
    cpu->io.spi_frame_active &= (uint8_t)~(1u << channel);
    cpu->io.spi_frame_output_pending &= (uint8_t)~(1u << channel);
    cpu->io.spi_frame_output_clear_pending &= (uint8_t)~(1u << channel);
    cpu->io.spi_shift[channel] = 0u;
    cpu->io.spi_pin_receive[channel] = 0u;
    cpu->io.spi_pin_bits[channel] = 0u;
    cpu->io.spi_pin_output_index[channel] = 0u;
    cpu->io.spi_pin_output_started &= (uint8_t)~(1u << channel);
    cpu->io.spi_generation[channel] =
        (uint16_t)((cpu->io.spi_generation[channel] + 1u) & SPI_EVENT_GENERATION_MASK);
    raw_write_word(cpu, (uint16_t)(spi_bases[channel] + 8u), 0u);
    spi_refresh_status(cpu, channel);
}

static void spi_begin_frame(Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u));
    uint8_t bit = (uint8_t)(1u << channel);
    if (spi_master(cpu, channel) &&
        (control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == SPI_FRAME_ENABLE) {
        return;
    }
    if (!spi_master(cpu, channel) &&
        (control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == SPI_FRAME_ENABLE) {
        cpu->io.spi_frame_output_pending |= bit;
        cpu->io.spi_frame_output_clear_pending &= (uint8_t)~bit;
        cpu->io.spi_frame_active &= (uint8_t)~bit;
        cpu->io.spi_pin_output_started &= (uint8_t)~bit;
        return;
    }
    cpu->io.spi_frame_active &= (uint8_t)~bit;
    if ((control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE | SPI_FRAME_ACTIVE_HIGH)) ==
        (SPI_FRAME_ENABLE | SPI_FRAME_ACTIVE_HIGH)) {
        if (spi_master(cpu, channel) && (control & SPI_FRAME_DELAY) != 0u) {
            uint16_t data_control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u));
            uint8_t bits = (data_control & SPI_MODE_16) != 0u ? 16u : 8u;
            uint32_t event_value = SPI_EVENT_FRAME | ((uint32_t)cpu->io.spi_generation[channel]
                                                      << SPI_EVENT_GENERATION_SHIFT);
            dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, event_value,
                             spi_transfer_cycles(cpu, channel) / bits);
        } else {
            cpu->io.spi_frame_active |= bit;
        }
    }
}

static bool spi_power_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t status = raw_word(cpu, spi_bases[channel]);
    if (!spi_master(cpu, channel)) {
        return cpu->power_state != DSPIC33_POWER_IDLE || (status & SPI_STOP_IDLE) == 0u;
    }
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    return cpu->power_state == DSPIC33_POWER_IDLE && (status & SPI_STOP_IDLE) == 0u;
}

static bool spi_selected(const Dspic33* cpu, uint8_t channel) {
    uint16_t control1 = raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u));
    uint16_t control2 = raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u));
    bool required = (control2 & SPI_FRAME_ENABLE) != 0u
                        ? (control2 & SPI_FRAME_SLAVE) != 0u
                        : !spi_master(cpu, channel) && (control1 & 0x0080u) != 0u;
    return !required || (cpu->io.spi_selected & (uint8_t)(1u << channel)) != 0u;
}

static bool spi_master_frame_slave(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u));
    return spi_master(cpu, channel) &&
           (control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE);
}

static bool spi_master_frame_master(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u));
    return spi_master(cpu, channel) &&
           (control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == SPI_FRAME_ENABLE;
}

static bool spi_slave_frame_master(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u));
    return !spi_master(cpu, channel) &&
           (control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == SPI_FRAME_ENABLE;
}

static bool spi_pps_input_high(const Dspic33* cpu, uint8_t selection) {
    bool high;
    return pps_physical_input_high(cpu, selection, &high) && high;
}

static bool spi_master_input_high(const Dspic33* cpu, uint8_t channel) {
    static const uint16_t input_registers[DSPIC33_SPI_COUNT] = {0x06c8u, 0u, 0x06dau, 0x06deu};
    if (input_registers[channel] != 0u) {
        return spi_pps_input_high(cpu,
                                  (uint8_t)(raw_word(cpu, input_registers[channel]) & 0x007fu));
    }
    return (cpu->io.spi_pin_data_high & (uint8_t)(1u << channel)) != 0u;
}

static bool spi_master_pin_input_enabled(const Dspic33* cpu, uint8_t channel) {
    static const uint16_t input_registers[DSPIC33_SPI_COUNT] = {0x06c8u, 0u, 0x06dau, 0x06deu};
    return (cpu->io.spi_pin_input_enabled & (uint8_t)(1u << channel)) != 0u ||
           (input_registers[channel] != 0u &&
            (raw_word(cpu, input_registers[channel]) & 0x007fu) != 0u);
}

static void spi_refresh_pps_inputs(Dspic33* cpu) {
    static const uint8_t channels[] = {0u, 2u, 3u};
    static const uint16_t input_registers[] = {0x06c8u, 0x06dau, 0x06deu};
    static const uint16_t select_registers[] = {0x06cau, 0x06dcu, 0x06e0u};
    size_t index;
    for (index = 0u; index < sizeof(channels) / sizeof(channels[0]); index++) {
        uint16_t inputs = raw_word(cpu, input_registers[index]);
        uint16_t select = raw_word(cpu, select_registers[index]);
        uint8_t channel = channels[index];
        if (inputs == 0u && select == 0u) {
            continue;
        }
        bool data_high = spi_pps_input_high(cpu, (uint8_t)(inputs & 0x007fu));
        bool clock_high = spi_pps_input_high(cpu, (uint8_t)((inputs >> 8u) & 0x007fu));
        bool select_high = spi_pps_input_high(cpu, (uint8_t)(select & 0x007fu));
        dspic33_spi_pin_input(cpu, channel, clock_high, data_high, select_high);
    }
}

static uint64_t spi_transfer_cycles(const Dspic33* cpu, uint8_t channel) {
    static const uint8_t primary[] = {64u, 16u, 4u, 1u};
    uint16_t control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u));
    uint8_t secondary = (uint8_t)(8u - ((control >> 2u) & 7u));
    uint8_t bits = (control & SPI_MODE_16) != 0u ? 16u : 8u;
    return (uint64_t)bits * primary[control & 3u] * secondary;
}

static uint8_t spi_interrupt_mode(const Dspic33* cpu, uint8_t channel) {
    return (uint8_t)((raw_word(cpu, spi_bases[channel]) & SPI_INTERRUPT_MODE_MASK) >> 2u);
}

static void spi_raise_mode(Dspic33* cpu, uint8_t channel, uint8_t mode) {
    if (!spi_enhanced(cpu, channel) || spi_interrupt_mode(cpu, channel) == mode) {
        dspic33_raise_interrupt(cpu, spi_irqs[channel]);
    }
}

static void spi_raise_transfer_interrupt(Dspic33* cpu, uint8_t channel) {
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, spi_irqs[channel], 0u, 1u)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void spi_remove_internal_events(Dspic33* cpu, uint8_t channel) {
    size_t destination = 0u;
    size_t source;
    for (source = 0u; source < cpu->events.count; source++) {
        Dspic33Event* event = &cpu->events.items[source];
        if (event->type != DSPIC33_EVENT_SPI || event->source != channel ||
            (event->value & SPI_EVENT_KIND_MASK) == SPI_EVENT_EXTERNAL) {
            cpu->events.items[destination++] = *event;
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
}

static void spi_remove_external_events(Dspic33* cpu, uint8_t channel) {
    size_t destination = 0u;
    size_t source;
    for (source = 0u; source < cpu->events.count; source++) {
        Dspic33Event* event = &cpu->events.items[source];
        if (event->type != DSPIC33_EVENT_SPI || event->source != channel ||
            (event->value & SPI_EVENT_KIND_MASK) != SPI_EVENT_EXTERNAL) {
            cpu->events.items[destination++] = *event;
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
}

static void spi_update_power_state(Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t status = raw_word(cpu, spi_bases[channel]);
        bool abort = spi_master(cpu, channel) &&
                     (cpu->power_state == DSPIC33_POWER_SLEEP ||
                      (cpu->power_state == DSPIC33_POWER_IDLE && (status & SPI_STOP_IDLE) != 0u));
        if (abort) {
            spi_remove_internal_events(cpu, channel);
            spi_clear_buffers(cpu, channel);
        }
    }
}

static void spi_schedule_current(Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u));
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t bits;
    uint8_t index;
    uint32_t event_value;
    uint64_t period;
    uint64_t sample;
    bool scheduled = true;
    if ((cpu->io.spi_busy & bit) == 0u || !spi_master(cpu, channel) ||
        (control & SPI_DISABLE_CLOCK) != 0u || !spi_power_enabled(cpu, channel)) {
        return;
    }
    event_value = SPI_EVENT_INTERNAL |
                  ((uint32_t)cpu->io.spi_generation[channel] << SPI_EVENT_GENERATION_SHIFT) |
                  cpu->io.spi_shift[channel];
    if (spi_master_pin_input_enabled(cpu, channel)) {
        bits = (control & SPI_MODE_16) != 0u ? 16u : 8u;
        period = spi_transfer_cycles(cpu, channel) / bits;
        sample = (control & SPI_SAMPLE_END) != 0u ? period : (period + 1u) / 2u;
        for (index = 0u; index < bits; index++) {
            scheduled &=
                dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel,
                                 SPI_EVENT_SAMPLE | ((uint32_t)cpu->io.spi_generation[channel]
                                                     << SPI_EVENT_GENERATION_SHIFT),
                                 sample + (uint64_t)index * period);
        }
    }
    scheduled &= dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, event_value,
                                  spi_transfer_cycles(cpu, channel));
    if (!scheduled) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        spi_remove_internal_events(cpu, channel);
        spi_clear_buffers(cpu, channel);
    }
}

static void spi_start_next_admitted(Dspic33* cpu, uint8_t channel, bool frame_admitted) {
    uint16_t base = spi_bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + 2u));
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t previous_count = cpu->io.spi_tx_fifo[channel].count;
    uint16_t value;
    if (!frame_admitted && spi_master_frame_master(cpu, channel) && previous_count != 0u &&
        (cpu->io.spi_frame_output_pending & bit) == 0u) {
        uint8_t bits = (control & SPI_MODE_16) != 0u ? 16u : 8u;
        uint64_t period = spi_transfer_cycles(cpu, channel) / bits;
        uint64_t phase = (cpu->device_cycles - cpu->io.spi_clock_start_cycle[channel]) % period;
        uint64_t delay = phase == 0u ? 0u : period - phase;
        uint32_t event_value = SPI_EVENT_FRAME_START | ((uint32_t)cpu->io.spi_generation[channel]
                                                        << SPI_EVENT_GENERATION_SHIFT);
        if (dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, event_value, delay)) {
            cpu->io.spi_frame_output_pending |= bit;
        } else {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            spi_clear_buffers(cpu, channel);
        }
        spi_refresh_status(cpu, channel);
        return;
    }
    if ((cpu->io.spi_busy & bit) != 0u || previous_count == 0u ||
        (raw_word(cpu, base) & SPI_ENABLE) == 0u || spi_module_disabled(cpu, channel) ||
        (!spi_master(cpu, channel) && (control & SPI_SLAVE_SELECT) != 0u &&
         !spi_selected(cpu, channel)) ||
        (spi_master_frame_slave(cpu, channel) && !frame_admitted) ||
        !word_queue_pop(&cpu->io.spi_tx_fifo[channel], &value)) {
        spi_refresh_status(cpu, channel);
        return;
    }
    if ((cpu->io.spi_busy & bit) == 0u &&
        (raw_word(cpu, (uint16_t)(base + 4u)) & SPI_FRAME_ENABLE) == 0u) {
        cpu->io.spi_clock_start_cycle[channel] = cpu->device_cycles;
    }
    cpu->io.spi_busy |= bit;
    cpu->io.spi_shift[channel] = value;
    cpu->io.spi_start_cycle[channel] = cpu->device_cycles;
    cpu->io.spi_pin_receive[channel] = 0u;
    cpu->io.spi_pin_bits[channel] = 0u;
    cpu->io.spi_pin_output_index[channel] = 0u;
    if (!spi_master(cpu, channel) && (control & SPI_CLOCK_EDGE) != 0u) {
        cpu->io.spi_pin_output_started |= bit;
    } else {
        cpu->io.spi_pin_output_started &= (uint8_t)~bit;
    }
    spi_begin_frame(cpu, channel);
    if ((control & SPI_DISABLE_OUTPUT) == 0u) {
        byte_queue_push(&cpu->io.spi_tx[channel], (uint8_t)value);
        if ((control & SPI_MODE_16) != 0u) {
            byte_queue_push(&cpu->io.spi_tx[channel], (uint8_t)(value >> 8u));
        }
    }
    spi_refresh_status(cpu, channel);
    if (spi_enhanced(cpu, channel)) {
        if (previous_count == 8u) {
            spi_raise_mode(cpu, channel, 4u);
        }
        if (cpu->io.spi_tx_fifo[channel].count == 0u) {
            spi_raise_mode(cpu, channel, 6u);
        }
    }
    spi_schedule_current(cpu, channel);
}

static void spi_start_next(Dspic33* cpu, uint8_t channel) {
    spi_start_next_admitted(cpu, channel, false);
}

static void spi_update_slave_selection(Dspic33* cpu, uint8_t channel, bool previous,
                                       bool selected) {
    uint16_t control1 = raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u));
    uint16_t control2 = raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u));
    uint8_t bit = (uint8_t)(1u << channel);
    if (previous == selected || spi_master(cpu, channel) || (control1 & SPI_SLAVE_SELECT) == 0u ||
        (control2 & SPI_FRAME_ENABLE) != 0u) {
        return;
    }
    if (!selected && (cpu->io.spi_busy & bit) != 0u) {
        if (!word_queue_push_front(&cpu->io.spi_tx_fifo[channel], cpu->io.spi_shift[channel])) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            spi_clear_buffers(cpu, channel);
            return;
        }
        cpu->io.spi_busy &= (uint8_t)~bit;
        cpu->io.spi_frame_active &= (uint8_t)~bit;
        cpu->io.spi_frame_output_pending &= (uint8_t)~bit;
        cpu->io.spi_frame_output_clear_pending &= (uint8_t)~bit;
        cpu->io.spi_pin_receive[channel] = 0u;
        cpu->io.spi_pin_bits[channel] = 0u;
        cpu->io.spi_pin_output_index[channel] = 0u;
        cpu->io.spi_pin_output_started &= (uint8_t)~bit;
        cpu->io.spi_generation[channel] =
            (uint16_t)((cpu->io.spi_generation[channel] + 1u) & SPI_EVENT_GENERATION_MASK);
        spi_remove_external_events(cpu, channel);
        spi_refresh_status(cpu, channel);
    } else if (selected) {
        spi_start_next(cpu, channel);
    }
}

static void spi_schedule_frame_input_sample(Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u));
    uint8_t bits = (control & SPI_MODE_16) != 0u ? 16u : 8u;
    uint64_t period = spi_transfer_cycles(cpu, channel) / bits;
    uint64_t phase = (cpu->device_cycles - cpu->io.spi_clock_start_cycle[channel]) % period;
    uint64_t sample = period / 2u;
    uint64_t delay = phase <= sample ? sample - phase : period - phase + sample;
    uint32_t value = SPI_EVENT_FRAME_INPUT |
                     ((uint32_t)cpu->io.spi_generation[channel] << SPI_EVENT_GENERATION_SHIFT);
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, value, delay)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void spi_receive_word(Dspic33* cpu, uint8_t channel, uint16_t value) {
    uint16_t base = spi_bases[channel];
    uint16_t status = raw_word(cpu, base);
    uint8_t capacity = spi_enhanced(cpu, channel) ? 8u : 1u;
    if ((raw_word(cpu, (uint16_t)(base + 2u)) & SPI_MODE_16) == 0u) {
        value &= 0x00ffu;
    }
    if ((status & SPI_OVERFLOW) != 0u) {
        return;
    }
    if (cpu->io.spi_rx_fifo[channel].count >= capacity ||
        !word_queue_push(&cpu->io.spi_rx_fifo[channel], value)) {
        raw_write_word(cpu, base, (uint16_t)(status | SPI_OVERFLOW));
        dspic33_raise_interrupt(cpu, spi_error_irqs[channel]);
        dspic33_raise_interrupt(cpu, spi_irqs[channel]);
        return;
    }
    if (cpu->io.spi_rx_fifo[channel].count == 1u) {
        raw_write_word(cpu, (uint16_t)(base + 8u), value);
    }
    spi_refresh_status(cpu, channel);
    if (spi_enhanced(cpu, channel)) {
        spi_raise_mode(cpu, channel, 1u);
        if (cpu->io.spi_rx_fifo[channel].count >= 6u) {
            spi_raise_mode(cpu, channel, 2u);
        }
        if (cpu->io.spi_rx_fifo[channel].count == 8u) {
            spi_raise_mode(cpu, channel, 3u);
        }
    }
}

static void spi_complete_transfer(Dspic33* cpu, uint8_t channel, uint16_t value) {
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.spi_busy &= (uint8_t)~bit;
    cpu->io.spi_frame_active &= (uint8_t)~bit;
    cpu->io.spi_frame_output_pending &= (uint8_t)~bit;
    cpu->io.spi_frame_output_clear_pending &= (uint8_t)~bit;
    cpu->io.spi_pin_receive[channel] = 0u;
    cpu->io.spi_pin_bits[channel] = 0u;
    cpu->io.spi_pin_output_index[channel] = 0u;
    cpu->io.spi_pin_output_started &= (uint8_t)~bit;
    spi_receive_word(cpu, channel, value);
    dspic33_dma_request(cpu, spi_dma_requests[channel], (uint16_t)(spi_bases[channel] + 8u), 0u);
    spi_start_next(cpu, channel);
    if (!spi_enhanced(cpu, channel)) {
        spi_raise_transfer_interrupt(cpu, channel);
    } else if ((cpu->io.spi_busy & bit) == 0u && cpu->io.spi_tx_fifo[channel].count == 0u) {
        spi_raise_mode(cpu, channel, 5u);
    }
    spi_refresh_status(cpu, channel);
}

static void run_spi(Dspic33* cpu, uint8_t channel, uint32_t event_value) {
    uint16_t base;
    uint16_t value = (uint16_t)event_value;
    uint8_t bit;
    uint32_t kind = event_value & SPI_EVENT_KIND_MASK;
    if (channel >= DSPIC33_SPI_COUNT) {
        return;
    }
    base = spi_bases[channel];
    bit = (uint8_t)(1u << channel);
    if (kind == 0u) {
        raw_write_word(cpu, (uint16_t)(base + 8u), value);
        raw_write_word(cpu, base, (uint16_t)(raw_word(cpu, base) | SPI_RX_FULL));
        dspic33_raise_interrupt(cpu, spi_irqs[channel]);
        return;
    }
    if ((raw_word(cpu, base) & SPI_ENABLE) == 0u || spi_module_disabled(cpu, channel)) {
        return;
    }
    if (kind == SPI_EVENT_FRAME) {
        uint16_t generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);
        uint16_t control = raw_word(cpu, (uint16_t)(base + 4u));
        if (generation == cpu->io.spi_generation[channel] && (cpu->io.spi_busy & bit) != 0u &&
            spi_power_enabled(cpu, channel) &&
            (control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE | SPI_FRAME_ACTIVE_HIGH)) ==
                (SPI_FRAME_ENABLE | SPI_FRAME_ACTIVE_HIGH)) {
            cpu->io.spi_frame_active |= bit;
        }
        return;
    }
    if (kind == SPI_EVENT_INTERNAL) {
        uint16_t generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);
        if (generation != cpu->io.spi_generation[channel] || (cpu->io.spi_busy & bit) == 0u) {
            return;
        }
        if (!spi_power_enabled(cpu, channel)) {
            spi_clear_buffers(cpu, channel);
            return;
        }
        spi_complete_transfer(
            cpu, channel,
            spi_master_pin_input_enabled(cpu, channel) ? cpu->io.spi_pin_receive[channel] : value);
        return;
    }
    if (kind == SPI_EVENT_SAMPLE) {
        uint16_t generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);
        if (generation == cpu->io.spi_generation[channel] && (cpu->io.spi_busy & bit) != 0u &&
            spi_power_enabled(cpu, channel)) {
            cpu->io.spi_pin_receive[channel] =
                (uint16_t)((cpu->io.spi_pin_receive[channel] << 1u) |
                           (spi_master_input_high(cpu, channel) ? 1u : 0u));
            cpu->io.spi_pin_bits[channel]++;
        }
        return;
    }
    if (kind == SPI_EVENT_FRAME_INPUT) {
        uint16_t generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);
        if (generation == cpu->io.spi_generation[channel] && spi_master_frame_slave(cpu, channel) &&
            spi_selected(cpu, channel)) {
            uint16_t control = raw_word(cpu, (uint16_t)(base + 2u));
            uint8_t bits = (control & SPI_MODE_16) != 0u ? 16u : 8u;
            uint64_t period = spi_transfer_cycles(cpu, channel) / bits;
            uint32_t frame_value =
                SPI_EVENT_FRAME_START | ((uint32_t)generation << SPI_EVENT_GENERATION_SHIFT);
            if (!dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, frame_value,
                                  period - period / 2u)) {
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            }
        }
        return;
    }
    if (kind == SPI_EVENT_FRAME_START) {
        uint16_t generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);
        if (generation != cpu->io.spi_generation[channel]) {
            return;
        }
        if ((event_value & SPI_EVENT_FRAME_DATA) == 0u) {
            cpu->io.spi_frame_output_pending &= (uint8_t)~bit;
        }
        if (spi_master_frame_slave(cpu, channel)) {
            spi_start_next_admitted(cpu, channel, true);
        } else if (spi_master_frame_master(cpu, channel)) {
            uint16_t control = raw_word(cpu, (uint16_t)(base + 2u));
            uint16_t frame = raw_word(cpu, (uint16_t)(base + 4u));
            uint8_t bits = (control & SPI_MODE_16) != 0u ? 16u : 8u;
            uint64_t period = spi_transfer_cycles(cpu, channel) / bits;
            bool scheduled;
            if ((event_value & SPI_EVENT_FRAME_DATA) != 0u) {
                spi_start_next_admitted(cpu, channel, true);
                return;
            }
            if ((frame & SPI_FRAME_ACTIVE_HIGH) != 0u) {
                cpu->io.spi_frame_active |= bit;
            }
            scheduled = dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel,
                                         SPI_EVENT_FRAME_CLEAR |
                                             ((uint32_t)generation << SPI_EVENT_GENERATION_SHIFT),
                                         spi_transfer_cycles(cpu, channel));
            if ((frame & SPI_FRAME_DELAY) != 0u) {
                spi_start_next_admitted(cpu, channel, true);
            } else {
                scheduled &=
                    dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel,
                                     SPI_EVENT_FRAME_START | SPI_EVENT_FRAME_DATA |
                                         ((uint32_t)generation << SPI_EVENT_GENERATION_SHIFT),
                                     period);
            }
            if (!scheduled) {
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
                spi_remove_internal_events(cpu, channel);
                spi_clear_buffers(cpu, channel);
            }
        }
        return;
    }
    if (kind == SPI_EVENT_FRAME_CLEAR) {
        uint16_t generation =
            (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) & SPI_EVENT_GENERATION_MASK);
        if (generation == cpu->io.spi_generation[channel]) {
            cpu->io.spi_frame_active &= (uint8_t)~bit;
        }
        return;
    }
    if (!spi_power_enabled(cpu, channel)) {
        return;
    }
    if (!spi_selected(cpu, channel) ||
        (spi_master(cpu, channel) && (cpu->io.spi_busy & bit) == 0u)) {
        return;
    }
    if (spi_master(cpu, channel)) {
        cpu->io.spi_generation[channel] =
            (uint16_t)((cpu->io.spi_generation[channel] + 1u) & SPI_EVENT_GENERATION_MASK);
    } else if ((cpu->io.spi_busy & bit) == 0u) {
        cpu->io.spi_busy |= bit;
        cpu->io.spi_shift[channel] = raw_word(cpu, (uint16_t)(base + 8u));
    }
    spi_complete_transfer(cpu, channel, value);
}

static void run_spi_select(Dspic33* cpu, uint8_t channel, bool selected) {
    uint8_t bit;
    bool previous;
    if (channel >= DSPIC33_SPI_COUNT) {
        return;
    }
    bit = (uint8_t)(1u << channel);
    previous = (cpu->io.spi_selected & bit) != 0u;
    if (selected) {
        cpu->io.spi_selected |= bit;
    } else {
        cpu->io.spi_selected &= (uint8_t)~bit;
    }
    spi_update_slave_selection(cpu, channel, previous, selected);
    if (previous != selected && spi_master_frame_slave(cpu, channel)) {
        spi_schedule_frame_input_sample(cpu, channel);
    }
}
