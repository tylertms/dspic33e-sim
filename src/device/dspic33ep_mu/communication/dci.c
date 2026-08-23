#include "device/dspic33ep_mu/internal.h"

static uint8_t dci_buffer_count(const Dspic33* cpu) {
    return (uint8_t)(((dspic33_device_internal_raw_word(cpu, DCI_CONTROL2) &
                       DCI_CONTROL2_BUFFER_MASK) >>
                      10u) +
                     1u);
}

static uint8_t dci_mode(const Dspic33* cpu) {
    return (uint8_t)(dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_MODE_MASK);
}

static uint8_t dci_frame_count(const Dspic33* cpu) {
    uint8_t mode_value = dci_mode(cpu);

    if (mode_value == 2u) {
        return 13u;
    }
    if (mode_value == 3u) {
        return 16u;
    }
    return (
        uint8_t)(((dspic33_device_internal_raw_word(cpu, DCI_CONTROL2) & DCI_CONTROL2_FRAME_MASK) >>
                  5u) +
                 1u);
}

static uint8_t dci_word_width(const Dspic33* cpu) {
    if (dci_mode(cpu) >= 2u) {
        return 16u;
    }
    return (
        uint8_t)((dspic33_device_internal_raw_word(cpu, DCI_CONTROL2) & DCI_CONTROL2_WORD_MASK) +
                 1u);
}

static uint8_t dci_slot_width(const Dspic33* cpu, uint8_t slot_number) {
    return dci_mode(cpu) == 2u && slot_number != 0u ? 20u : dci_word_width(cpu);
}

static uint16_t dci_slot_mask(const Dspic33* cpu) {
    return dci_mode(cpu) == 2u ? 0x1fffu : UINT16_MAX;
}

static uint16_t dci_word_mask(const Dspic33* cpu) {
    uint8_t word_width = dci_word_width(cpu);
    return word_width == 16u ? UINT16_MAX : (uint16_t)(UINT16_MAX << (16u - word_width));
}

static uint8_t dci_active_transmit_buffers(const Dspic33* cpu) {
    uint16_t slot_domain_mask = dci_slot_mask(cpu);
    uint16_t configured_transmit_slots =
        (uint16_t)(dspic33_device_internal_raw_word(cpu, DCI_TRANSMIT_SLOTS) & slot_domain_mask);
    uint16_t configured_active_slots =
        (uint16_t)(configured_transmit_slots |
                   (dspic33_device_internal_raw_word(cpu, DCI_RECEIVE_SLOTS) & slot_domain_mask));
    uint8_t buffer_count = dci_buffer_count(cpu);
    uint8_t active_buffer_index = 0u;
    uint8_t transmit_buffer_mask = 0u;

    for (uint8_t frame_number = 0u; frame_number < buffer_count; frame_number++) {
        for (uint8_t slot_number = 0u; slot_number < dci_frame_count(cpu); slot_number++) {
            uint16_t slot_bit = (uint16_t)(1u << slot_number);

            if ((configured_transmit_slots & slot_bit) != 0u) {
                transmit_buffer_mask |= (uint8_t)(1u << active_buffer_index);
            }
            if ((configured_active_slots & slot_bit) != 0u) {
                active_buffer_index = (uint8_t)((active_buffer_index + 1u) % buffer_count);
            }
        }
    }
    return transmit_buffer_mask;
}

bool dspic33_device_internal_dci_configuration_supported(const Dspic33* cpu) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    uint16_t divider = dspic33_device_internal_raw_word(cpu, DCI_CONTROL3);
    return (control & (uint16_t)~DCI_CONTROL_SUPPORTED_MASK) == 0u && dci_word_width(cpu) >= 4u &&
           (((control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u && divider == 0u) ||
            ((control & DCI_CONTROL_EXTERNAL_CLOCK) == 0u && divider != 0u));
}

static uint64_t dci_bit_cycles(const Dspic33* cpu) {
    return ((uint64_t)(dspic33_device_internal_raw_word(cpu, DCI_CONTROL3) & 0x0fffu) + 1u) * 2u;
}

static uint64_t dci_word_cycles(const Dspic33* cpu) {
    uint64_t bit_cycle_count = dci_bit_cycles(cpu);
    uint8_t slot_width = dci_slot_width(cpu, cpu->io.dci.slot);
    return bit_cycle_count * slot_width;
}

static bool dci_bcg_running(const Dspic33* cpu) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    return dspic33_device_internal_raw_word(cpu, DCI_CONTROL3) != 0u && !cpu->io.dci.pmd_disabled &&
           cpu->power_state != DSPIC33_POWER_SLEEP &&
           (cpu->power_state != DSPIC33_POWER_IDLE || (control & DCI_CONTROL_STOP_IDLE) == 0u);
}

static uint64_t dci_bcg_phase(const Dspic33* cpu) {
    uint64_t bit_cycle_period = dci_bit_cycles(cpu);
    uint64_t phase_offset = cpu->io.dci.bcg_phase % bit_cycle_period;

    if (!cpu->io.dci.bcg_paused) {
        phase_offset =
            (phase_offset + (cpu->device_cycles - cpu->io.dci.bcg_cycle) % bit_cycle_period) %
            bit_cycle_period;
    }
    return phase_offset;
}

static void dci_update_bcg(Dspic33* cpu, bool reset) {
    Dspic33Dci* dci = &cpu->io.dci;
    bool running = dci_bcg_running(cpu);
    if (reset) {
        dci->bcg_cycle = cpu->device_cycles;
        dci->bcg_phase = 0u;
        dci->bcg_paused = !running;
        return;
    }
    if (!dci->bcg_paused && !running) {
        dci->bcg_phase = dci_bcg_phase(cpu);
        dci->bcg_cycle = cpu->device_cycles;
        dci->bcg_paused = true;
    } else if (dci->bcg_paused && running) {
        dci->bcg_cycle = cpu->device_cycles;
        dci->bcg_paused = false;
    }
}

static bool dci_output_push(Dspic33* cpu, uint16_t output_value, uint8_t slot_index,
                            bool output_driven) {
    Dspic33DciQueue* queue = &cpu->io.dci.output;

    if (queue->count == DSPIC33_DCI_QUEUE_SIZE) {
        return false;
    }
    uint8_t tail_index = (uint8_t)((queue->head + queue->count) % DSPIC33_DCI_QUEUE_SIZE);
    queue->transfers[tail_index].cycle = cpu->device_cycles;
    queue->transfers[tail_index].value = output_value;
    queue->transfers[tail_index].slot = slot_index;
    queue->transfers[tail_index].driven = output_driven;
    queue->count++;
    return true;
}

bool dspic33_device_internal_dci_output_pop(Dspic33DciQueue* queue, Dspic33DciTransfer* transfer) {
    if (queue->count == 0u) {
        return false;
    }
    *transfer = queue->transfers[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % DSPIC33_DCI_QUEUE_SIZE);
    queue->count--;
    return true;
}

static void dci_refresh_status(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t status_word = (uint16_t)((uint16_t)dci->slot << 8u);
    if (dci->receive_overflow != 0u) {
        status_word |= DCI_STATUS_RECEIVE_OVERFLOW;
    }
    if (dci->receive_unread != 0u) {
        status_word |= DCI_STATUS_RECEIVE_FULL;
    }
    if (dci->transmit_underflow != 0u) {
        status_word |= DCI_STATUS_TRANSMIT_UNDERFLOW;
    }
    if (dci->transmit_empty) {
        status_word |= DCI_STATUS_TRANSMIT_EMPTY;
    }
    dspic33_device_internal_raw_write_word(cpu, DCI_STATUS, status_word);
}

static void dci_abort(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    dspic33_device_internal_dci_discard_internal_events(cpu);
    dci->generation++;
    dci->started = false;
    dci->initialized = false;
    dci->disable_pending = false;
    dci->internal_scheduled = false;
    dci->disable_frames = 0u;
    dci->buffer = 0u;
    dci->slot = 0u;
    dci->serial_input = 0u;
    dci->serial_bits = 0u;
    dci->serial_startup_bits = 0u;
    dci->serial_frame_bits = 0u;
    dci->serial_output_high = false;
    dci->serial_output_driven = false;
    dci->serial_delay = false;
    dci->pps_frame_pending = false;
    dci->receive_buffered = 0u;
    dci->transmit_buffered = 0u;
    dspic33_device_internal_raw_write_word(
        cpu, DCI_CONTROL1,
        (uint16_t)(dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) & ~DCI_CONTROL_ENABLE));
    dci_refresh_status(cpu);
}

static bool dci_schedule_internal(Dspic33* cpu, uint16_t source, uint64_t delay) {
    Dspic33Dci* dci = &cpu->io.dci;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_DCI, source, dci->generation, delay)) {
        dci_abort(cpu);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    dci->internal_scheduled = true;
    return true;
}

static bool dci_schedule_sample(Dspic33* cpu, uint64_t delay) {
    if (dspic33_schedule(cpu, DSPIC33_EVENT_DCI, DCI_EVENT_SAMPLE, cpu->io.dci.generation, delay)) {
        return true;
    }
    dci_abort(cpu);
    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    return false;
}

static bool dci_begin_internal_word(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint64_t word_cycle_count = dci_word_cycles(cpu);
    uint64_t sample_delay_cycles =
        (dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_SAMPLE_RISING) != 0u
            ? 0u
            : dci_bit_cycles(cpu) / 2u;

    dci->serial_input = 0u;
    dci->serial_bits = 0u;
    if (!dci_schedule_internal(cpu, DCI_EVENT_INTERNAL, word_cycle_count)) {
        return false;
    }
    return !dci->pps_input_configured || dci_schedule_sample(cpu, sample_delay_cycles);
}

static bool dci_clock_running(const Dspic33* cpu) {
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);

    if (cpu->io.dci.pmd_disabled || cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE || (control_word & DCI_CONTROL_STOP_IDLE) == 0u;
}

static bool dci_internal_event(const Dspic33Event* event) {
    return event->type == DSPIC33_EVENT_DCI &&
           (event->source == DCI_EVENT_START || event->source == DCI_EVENT_INTERNAL ||
            event->source == DCI_EVENT_SAMPLE || event->source == DCI_EVENT_FRAME_START);
}

static void dci_pause_events(Dspic33* cpu) {
    bool changed = false;
    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        Dspic33Event* event = &cpu->events.items[event_index];
        if (!dci_internal_event(event) || event->paused) {
            continue;
        }
        event->paused_remaining = event->cycle - cpu->device_cycles;
        event->paused = true;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void dci_resume_events(Dspic33* cpu) {
    bool changed = false;
    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        Dspic33Event* event = &cpu->events.items[event_index];
        if (!dci_internal_event(event) || !event->paused) {
            continue;
        }
        if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            continue;
        }
        event->cycle = cpu->device_cycles + event->paused_remaining;
        event->paused_remaining = 0u;
        event->paused = false;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

void dspic33_device_internal_dci_discard_internal_events(Dspic33* cpu) {
    size_t retained_event_count = 0u;

    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        if (!dci_internal_event(&cpu->events.items[event_index])) {
            cpu->events.items[retained_event_count++] = cpu->events.items[event_index];
        }
    }
    cpu->events.count = retained_event_count;
    dspic33_reorder_events(cpu);
}

void dspic33_device_internal_dci_update_power_state(Dspic33* cpu) {
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    bool uses_internal_clock = (control_word & DCI_CONTROL_EXTERNAL_CLOCK) == 0u;
    bool clock_is_running =
        !uses_internal_clock ||
        (dspic33_device_internal_raw_word(cpu, DCI_CONTROL3) != 0u && dci_clock_running(cpu));

    if (clock_is_running) {
        dci_resume_events(cpu);
    } else {
        dci_pause_events(cpu);
    }
    dci_update_bcg(cpu, false);
}

static bool dci_dma_request(Dspic33* cpu) {
    if (dci_buffer_count(cpu) != 1u) {
        return true;
    }
    if (dspic33_dma_request(cpu, DCI_DMA_REQUEST, 0u, 0u)) {
        return true;
    }
    dci_abort(cpu);
    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    return false;
}

static bool dci_transfer_buffers(Dspic33* cpu, bool receive, bool transmit) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint8_t buffer_count = dci_buffer_count(cpu);
    bool transfer_error = false;

    for (uint8_t buffer_index = 0u; buffer_index < buffer_count; buffer_index++) {
        uint8_t buffer_mask = (uint8_t)(1u << buffer_index);

        if (receive && (dci->receive_buffered & buffer_mask) != 0u) {
            if ((dci->receive_unread & buffer_mask) != 0u) {
                dci->receive_overflow |= buffer_mask;
                transfer_error = true;
            }
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(DCI_RECEIVE_BASE + buffer_index * 2u), dci->receive[buffer_index]);
            dci->receive_unread |= buffer_mask;
        }
        if (transmit && (dci->transmit_buffered & buffer_mask) != 0u) {
            if ((dci->transmit_written & buffer_mask) != 0u) {
                uint16_t value = dspic33_device_internal_raw_word(
                    cpu, (uint16_t)(DCI_TRANSMIT_BASE + buffer_index * 2u));
                dci->transmit[buffer_index] = value;
                dci->last_transmit[buffer_index] = value;
            } else {
                dci->transmit_underflow |= buffer_mask;
                dci->transmit[buffer_index] = (dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) &
                                               DCI_CONTROL_UNDERFLOW_LAST) != 0u
                                                  ? dci->last_transmit[buffer_index]
                                                  : 0u;
                transfer_error = true;
            }
            dci->transmit_written &= (uint8_t)~buffer_mask;
        }
    }
    dci->receive_buffered = 0u;
    dci->transmit_buffered = 0u;
    if (transmit) {
        dci->transmit_empty = true;
    }
    dci_refresh_status(cpu);
    if (receive || transmit) {
        dspic33_raise_interrupt(cpu, DCI_TRANSFER_IRQ);
        if (!dci_dma_request(cpu)) {
            return false;
        }
    }
    if (transfer_error) {
        dspic33_raise_interrupt(cpu, DCI_ERROR_IRQ);
    }
    return true;
}

static bool dci_startup_transfer(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    uint8_t active_transmit_mask = dci_active_transmit_buffers(cpu);

    dci->started = true;
    dci->initialized = true;
    dci->buffer = 0u;
    dci->slot = 0u;
    dci->serial_input = 0u;
    dci->serial_bits = 0u;
    dci->serial_startup_bits = 0u;
    dci->serial_frame_bits = 0u;
    dci->serial_delay = (control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u &&
                        (control_word & DCI_CONTROL_EXTERNAL_FRAME) == 0u &&
                        dci_mode(cpu) < DCI_MODE_AC_LINK_16 &&
                        (control_word & DCI_CONTROL_DATA_JUSTIFY) == 0u;
    dci->output_frame_high = true;
    dci->transmit_buffered = active_transmit_mask;
    return active_transmit_mask == 0u || dci_transfer_buffers(cpu, false, true);
}

static bool dci_finish_frame(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    bool frame_complete = true;

    if (dci->disable_pending && dci->disable_frames > 1u) {
        dci->disable_frames--;
        return true;
    }
    if (dci->disable_pending && (dci->receive_buffered != 0u || dci->transmit_buffered != 0u)) {
        frame_complete =
            dci_transfer_buffers(cpu, dci->receive_buffered != 0u, dci->transmit_buffered != 0u);
    }
    if (dci->disable_pending) {
        dci->generation++;
        dci->started = false;
        dci->initialized = false;
        dci->disable_pending = false;
        dci->internal_scheduled = false;
        dci->disable_frames = 0u;
        dci->buffer = 0u;
        dci->slot = 0u;
        dci_refresh_status(cpu);
    }
    return frame_complete;
}

static bool dci_process_word(Dspic33* cpu, uint16_t input_word) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    uint16_t slot_domain_mask = dci_slot_mask(cpu);
    uint16_t transmit_slots =
        (uint16_t)(dspic33_device_internal_raw_word(cpu, DCI_TRANSMIT_SLOTS) & slot_domain_mask);
    uint16_t receive_slots =
        (uint16_t)(dspic33_device_internal_raw_word(cpu, DCI_RECEIVE_SLOTS) & slot_domain_mask);
    uint8_t buffer_mask = (uint8_t)(1u << dci->buffer);
    uint16_t slot_bit = (uint16_t)(1u << dci->slot);
    bool is_transmit_slot = (transmit_slots & slot_bit) != 0u;
    bool is_receive_slot = (receive_slots & slot_bit) != 0u;
    bool output_driven = is_transmit_slot || (control_word & DCI_CONTROL_TRISTATE) == 0u;
    uint16_t output_word =
        is_transmit_slot ? (uint16_t)(dci->transmit[dci->buffer] & dci_word_mask(cpu)) : 0u;

    if (!dci_output_push(cpu, output_word, dci->slot, output_driven)) {
        dci_abort(cpu);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    if (is_transmit_slot) {
        dci->transmit_buffered |= buffer_mask;
    }
    if (is_receive_slot) {
        dci->receive[dci->buffer] = (control_word & DCI_CONTROL_LOOPBACK) != 0u
                                        ? output_word
                                        : (uint16_t)(input_word & dci_word_mask(cpu));
        dci->receive_buffered |= buffer_mask;
    }
    if (is_transmit_slot || is_receive_slot) {
        dci->buffer++;
        if (dci->buffer == dci_buffer_count(cpu) &&
            !dci_transfer_buffers(cpu, dci->receive_buffered != 0u, dci->transmit_buffered != 0u)) {
            return false;
        }
        if (dci->buffer == dci_buffer_count(cpu)) {
            dci->buffer = 0u;
        }
    }
    dci->slot++;
    if (dci->slot == dci_frame_count(cpu)) {
        dci->slot = 0u;
        dci->serial_frame_bits = 0u;
        if (dci_mode(cpu) == 1u && (control_word & DCI_CONTROL_EXTERNAL_FRAME) == 0u) {
            dci->output_frame_high = !dci->output_frame_high;
        }
        if (!dci_finish_frame(cpu)) {
            return false;
        }
        if ((control_word & DCI_CONTROL_EXTERNAL_FRAME) != 0u) {
            dci->started = false;
        } else if ((control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u &&
                   dci_mode(cpu) < DCI_MODE_AC_LINK_16 &&
                   (control_word & DCI_CONTROL_DATA_JUSTIFY) == 0u) {
            dci->serial_delay = true;
        }
    }
    dci_refresh_status(cpu);
    return true;
}

uint8_t dspic33_device_internal_dci_pps_selection(const Dspic33* cpu, uint16_t address,
                                                  uint8_t shift) {
    return (uint8_t)((dspic33_device_internal_raw_word(cpu, address) >> shift) & 0x007fu);
}

bool dspic33_device_internal_dci_pps_input_high(const Dspic33* cpu, uint8_t selection) {
    bool input_is_high;

    if (selection == 0u) {
        return false;
    }
    return dspic33_device_internal_pps_physical_input_high(cpu, selection, &input_is_high) &&
           input_is_high;
}

static void dci_refresh_serial_output(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    uint16_t slot_bit = (uint16_t)(1u << dci->slot);
    bool is_transmit_slot =
        (dspic33_device_internal_raw_word(cpu, DCI_TRANSMIT_SLOTS) & slot_bit) != 0u;
    uint16_t output_word = is_transmit_slot ? dci->transmit[dci->buffer] : 0u;

    dci->serial_output_driven = is_transmit_slot || (control_word & DCI_CONTROL_TRISTATE) == 0u;
    dci->serial_output_high = dci->initialized && dci->started && !dci->serial_delay &&
                              dci->serial_bits < 16u &&
                              (output_word & (uint16_t)(0x8000u >> dci->serial_bits)) != 0u;
}

static void dci_begin_serial_frame(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    if (!dci->initialized && !dci_startup_transfer(cpu)) {
        return;
    }
    dci->slot = 0u;
    dci->started = true;
    dci->serial_input = 0u;
    dci->serial_bits = 0u;
    dci->serial_frame_bits = 0u;
    dci->serial_delay =
        dci_mode(cpu) < DCI_MODE_AC_LINK_16 &&
        (dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_DATA_JUSTIFY) == 0u;
    dci_refresh_serial_output(cpu);
    dci_refresh_status(cpu);
}

static void dci_sample_serial_input(Dspic33* cpu, bool input_is_high) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint8_t slot_width = dci_slot_width(cpu, dci->slot);

    if (dci->serial_bits < 16u && input_is_high) {
        dci->serial_input |= (uint16_t)(0x8000u >> dci->serial_bits);
    }
    dci->serial_bits++;
    dci->serial_frame_bits++;
    if (dci->serial_bits == slot_width) {
        uint16_t input_word = dci->serial_input;
        dci->serial_input = 0u;
        dci->serial_bits = 0u;
        dci_process_word(cpu, input_word);
    }
}

void dspic33_device_internal_dci_refresh_pps_inputs(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    bool uses_external_clock = (control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u;
    bool is_operating =
        ((control_word & DCI_CONTROL_ENABLE) != 0u || dci->disable_pending) &&
        dspic33_device_internal_dci_configuration_supported(cpu) && !dci->pmd_disabled &&
        (cpu->power_state != DSPIC33_POWER_IDLE || (control_word & DCI_CONTROL_STOP_IDLE) == 0u);
    bool clock_is_high = dspic33_device_internal_dci_pps_input_high(
        cpu, dspic33_device_internal_dci_pps_selection(cpu, DCI_PPS_INPUTS, 8u));
    bool frame_is_high = dspic33_device_internal_dci_pps_input_high(
        cpu, dspic33_device_internal_dci_pps_selection(cpu, DCI_PPS_FRAME, 0u));
    bool clock_changed = clock_is_high != dci->pps_clock_high;
    bool frame_changed = frame_is_high != dci->pps_frame_high;
    bool sample_edge =
        clock_changed && (((control_word & DCI_CONTROL_SAMPLE_RISING) != 0u) == clock_is_high);
    bool frame_edge = (control_word & DCI_CONTROL_EXTERNAL_FRAME) != 0u &&
                      ((dci_mode(cpu) == DCI_MODE_I2S && frame_changed) ||
                       (dci_mode(cpu) != DCI_MODE_I2S && frame_changed && frame_is_high));
    dci->pps_clock_high = clock_is_high;
    dci->pps_frame_high = frame_is_high;

    if (is_operating && dci->initialized && frame_edge && !dci->started) {
        if (uses_external_clock) {
            dci->pps_frame_pending = true;
        } else if (dci_clock_running(cpu) && !dci->internal_scheduled) {
            uint64_t frame_start_delay = dci_mode(cpu) < DCI_MODE_AC_LINK_16 &&
                                                 (control_word & DCI_CONTROL_DATA_JUSTIFY) != 0u
                                             ? 0u
                                             : dci_bit_cycles(cpu);
            dci->pps_frame_pending = true;
            if (frame_start_delay == 0u) {
                dci->pps_frame_pending = false;
                dci_begin_serial_frame(cpu);
                dci_begin_internal_word(cpu);
            } else {
                dci_schedule_internal(cpu, DCI_EVENT_FRAME_START, frame_start_delay);
            }
        }
    }
    if (!clock_changed || !uses_external_clock || !is_operating) {
        return;
    }
    if (!sample_edge) {
        dci_refresh_serial_output(cpu);
        return;
    }
    if (!dci->initialized && dci->serial_startup_bits != 0u) {
        dci->serial_startup_bits--;
        if (dci->serial_startup_bits != 0u || !dci_startup_transfer(cpu)) {
            return;
        }
        if ((control_word & DCI_CONTROL_EXTERNAL_FRAME) != 0u) {
            dci->started = false;
        }
        dci_refresh_serial_output(cpu);
        return;
    }
    if ((control_word & DCI_CONTROL_EXTERNAL_FRAME) != 0u) {
        if (dci->pps_frame_pending) {
            dci->pps_frame_pending = false;
            if (!dci->started) {
                dci_begin_serial_frame(cpu);
            }
        }
        if (!dci->started) {
            return;
        }
    } else if (!dci->initialized) {
        if (!dci_startup_transfer(cpu)) {
            return;
        }
        dci_refresh_serial_output(cpu);
    }
    if (dci->serial_delay) {
        dci->serial_delay = false;
        dci->serial_frame_bits++;
        dci_refresh_serial_output(cpu);
        return;
    }
    dci_sample_serial_input(
        cpu, dspic33_device_internal_dci_pps_input_high(
                 cpu, dspic33_device_internal_dci_pps_selection(cpu, DCI_PPS_INPUTS, 0u)));
}

static bool dci_internal_event_phase(const Dspic33* cpu, uint16_t* event_source,
                                     uint64_t* elapsed_cycles) {
    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        const Dspic33Event* event = &cpu->events.items[event_index];
        uint64_t total_event_cycles;
        uint64_t remaining_event_cycles;

        if (event->type != DSPIC33_EVENT_DCI ||
            (event->source != DCI_EVENT_START && event->source != DCI_EVENT_INTERNAL) ||
            (uint16_t)event->value != cpu->io.dci.generation) {
            continue;
        }
        total_event_cycles =
            event->source == DCI_EVENT_START ? dci_bit_cycles(cpu) * 3u : dci_word_cycles(cpu);
        remaining_event_cycles =
            event->paused ? event->paused_remaining : event->cycle - cpu->device_cycles;
        if (remaining_event_cycles > total_event_cycles) {
            return false;
        }
        *event_source = event->source;
        *elapsed_cycles = total_event_cycles - remaining_event_cycles;
        return true;
    }
    return false;
}

bool dspic33_device_internal_dci_internal_clock_high(const Dspic33* cpu, bool* high) {
    uint64_t half_bit_cycle = dci_bit_cycles(cpu) / 2u;

    if (dspic33_device_internal_raw_word(cpu, DCI_CONTROL3) == 0u || cpu->io.dci.pmd_disabled ||
        (dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_EXTERNAL_CLOCK) != 0u ||
        half_bit_cycle == 0u) {
        return false;
    }
    *high = dci_bcg_phase(cpu) < half_bit_cycle;
    return true;
}

bool dspic33_device_internal_dci_data_output(const Dspic33* cpu, bool* high) {
    const Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    uint16_t slot_bit = (uint16_t)(1u << dci->slot);
    bool is_transmit_slot =
        (dspic33_device_internal_raw_word(cpu, DCI_TRANSMIT_SLOTS) & slot_bit) != 0u;
    bool output_driven = is_transmit_slot || (control_word & DCI_CONTROL_TRISTATE) == 0u;
    uint8_t serial_bit_index = dci->serial_bits;
    uint16_t output_word = is_transmit_slot ? dci->transmit[dci->buffer] : 0u;

    if ((control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        *high = dci->serial_output_high;
        return dci->serial_output_driven;
    }
    if (!output_driven) {
        return false;
    }
    {
        uint16_t event_source;
        uint64_t elapsed_cycles;

        if (dci_internal_event_phase(cpu, &event_source, &elapsed_cycles) &&
            event_source == DCI_EVENT_INTERNAL) {
            if ((control_word & DCI_CONTROL_SAMPLE_RISING) != 0u) {
                elapsed_cycles += dci_bit_cycles(cpu) / 2u;
            }
            serial_bit_index = (uint8_t)(elapsed_cycles / dci_bit_cycles(cpu));
        }
    }
    *high = dci->initialized && dci->started && serial_bit_index < 16u &&
            (output_word & (uint16_t)(0x8000u >> serial_bit_index)) != 0u;
    return true;
}

bool dspic33_device_internal_dci_frame_output(const Dspic33* cpu, bool* high) {
    uint8_t dci_mode_value = dci_mode(cpu);
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    uint16_t event_source;
    uint64_t elapsed_cycles;
    uint64_t bit_cycle_count = dci_bit_cycles(cpu);
    bool immediate_data = (control_word & DCI_CONTROL_DATA_JUSTIFY) != 0u;

    if ((control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        if (!cpu->io.dci.initialized || !cpu->io.dci.started) {
            *high = false;
        } else if (dci_mode_value == DCI_MODE_I2S) {
            *high = cpu->io.dci.output_frame_high;
        } else if (dci_mode_value >= DCI_MODE_AC_LINK_16) {
            *high = cpu->io.dci.serial_frame_bits < 16u;
        } else {
            *high = cpu->io.dci.serial_delay ||
                    (immediate_data && cpu->io.dci.slot == 0u && cpu->io.dci.serial_bits == 0u);
        }
        return true;
    }
    if (!dci_internal_event_phase(cpu, &event_source, &elapsed_cycles)) {
        *high = false;
        return true;
    }
    if (dci_mode_value == DCI_MODE_I2S) {
        *high = event_source == DCI_EVENT_START
                    ? !immediate_data && elapsed_cycles >= bit_cycle_count * 2u
                    : cpu->io.dci.output_frame_high;
        if (!immediate_data && event_source == DCI_EVENT_INTERNAL &&
            cpu->io.dci.slot + 1u == dci_frame_count(cpu) &&
            elapsed_cycles + bit_cycle_count >= dci_word_cycles(cpu)) {
            *high = !*high;
        }
        return true;
    }
    if (dci_mode_value >= DCI_MODE_AC_LINK_16) {
        *high = (event_source == DCI_EVENT_START && elapsed_cycles >= bit_cycle_count * 2u) ||
                (event_source == DCI_EVENT_INTERNAL && cpu->io.dci.slot == 0u &&
                 elapsed_cycles < bit_cycle_count * 15u);
        return true;
    }
    if (immediate_data) {
        *high = event_source == DCI_EVENT_INTERNAL && cpu->io.dci.slot == 0u &&
                elapsed_cycles < bit_cycle_count;
    } else {
        *high =
            (event_source == DCI_EVENT_START && elapsed_cycles >= bit_cycle_count * 2u) ||
            (event_source == DCI_EVENT_INTERNAL && cpu->io.dci.slot + 1u == dci_frame_count(cpu) &&
             elapsed_cycles + bit_cycle_count >= dci_word_cycles(cpu));
    }
    return true;
}

static void dci_run_internal(Dspic33* cpu, uint16_t event_generation) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);

    if (event_generation != dci->generation) {
        return;
    }
    dci->internal_scheduled = false;
    if (((control_word & DCI_CONTROL_ENABLE) == 0u && !dci->disable_pending) ||
        (control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        return;
    }
    if (!dspic33_device_internal_dci_configuration_supported(cpu) || !dci_clock_running(cpu) ||
        !dci->started) {
        dci_begin_internal_word(cpu);
        return;
    }
    if (dci_process_word(cpu, dci->pps_input_configured ? dci->serial_input : dci->input) &&
        ((dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_ENABLE) != 0u ||
         dci->disable_pending) &&
        (!(dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_EXTERNAL_FRAME) ||
         dci->started)) {
        dci_begin_internal_word(cpu);
    }
}

static void dci_run_sample(Dspic33* cpu, uint16_t event_generation) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    uint8_t slot_width = dci_slot_width(cpu, dci->slot);
    uint8_t pps_selection;
    bool sampled_high;

    if (event_generation != dci->generation ||
        ((control_word & DCI_CONTROL_ENABLE) == 0u && !dci->disable_pending) ||
        (control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u ||
        !dspic33_device_internal_dci_configuration_supported(cpu) || !dci_clock_running(cpu) ||
        !dci->started || dci->serial_bits >= slot_width) {
        return;
    }
    pps_selection = dspic33_device_internal_dci_pps_selection(cpu, DCI_PPS_INPUTS, 0u);
    sampled_high =
        !dci->pps_input_configured
            ? dci->serial_bits < 16u && (dci->input & (uint16_t)(0x8000u >> dci->serial_bits)) != 0u
            : dspic33_device_internal_dci_pps_input_high(cpu, pps_selection);
    if (dci->serial_bits < 16u && sampled_high) {
        dci->serial_input |= (uint16_t)(0x8000u >> dci->serial_bits);
    }
    dci->serial_bits++;
    if (dci->serial_bits < slot_width) {
        dci_schedule_sample(cpu, dci_bit_cycles(cpu));
    }
}

static void dci_run_external(Dspic33* cpu, uint16_t input_word, bool frame_sync) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);

    if ((control_word & DCI_CONTROL_ENABLE) == 0u && !dci->disable_pending) {
        return;
    }
    if (!dspic33_device_internal_dci_configuration_supported(cpu) || dci->pmd_disabled ||
        (cpu->power_state == DSPIC33_POWER_IDLE && (control_word & DCI_CONTROL_STOP_IDLE) != 0u)) {
        return;
    }
    dci->input = input_word;
    frame_sync = frame_sync && (control_word & DCI_CONTROL_EXTERNAL_FRAME) != 0u;
    if ((control_word & DCI_CONTROL_EXTERNAL_CLOCK) == 0u && !frame_sync) {
        return;
    }
    if (!dci->initialized) {
        if ((control_word & DCI_CONTROL_EXTERNAL_CLOCK) == 0u) {
            return;
        }
        dci->serial_startup_bits = 0u;
        if (!dci_startup_transfer(cpu)) {
            return;
        }
    }
    if ((control_word & DCI_CONTROL_EXTERNAL_FRAME) != 0u && !frame_sync && dci->slot == 0u) {
        dci->started = false;
    }
    if ((control_word & DCI_CONTROL_EXTERNAL_FRAME) != 0u && frame_sync && !dci->started) {
        dci->slot = 0u;
        dci->started = true;
    }
    if (!dci->started) {
        if ((control_word & DCI_CONTROL_EXTERNAL_FRAME) != 0u) {
            return;
        }
        dci->started = true;
    }
    if ((control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        if (dci_process_word(cpu, input_word)) {
            dci_refresh_serial_output(cpu);
        }
    } else if (frame_sync && !dci->internal_scheduled) {
        dci_begin_internal_word(cpu);
    }
}

void dspic33_device_internal_run_dci(Dspic33* cpu, uint16_t event_source, uint32_t event_value) {
    Dspic33Dci* dci = &cpu->io.dci;
    if (event_source == DCI_EVENT_PMD) {
        uint16_t event_generation = (uint16_t)(event_value >> DCI_EVENT_GENERATION_SHIFT);

        if (event_generation == dci->pmd_generation) {
            dci->pmd_disabled = (event_value & DCI_EVENT_DISABLED) != 0u;
            dspic33_device_internal_dci_update_power_state(cpu);
        }
        return;
    }
    if (event_source == DCI_EVENT_START) {
        if ((uint16_t)event_value != dci->generation) {
            return;
        }
        dci->internal_scheduled = false;
        if (dci->pmd_disabled ||
            (dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_ENABLE) == 0u) {
            return;
        }
        if (dci_startup_transfer(cpu)) {
            if ((dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) &
                 DCI_CONTROL_EXTERNAL_FRAME) != 0u) {
                dci->started = false;
            } else {
                dci_begin_internal_word(cpu);
            }
        }
        return;
    }
    if (event_source == DCI_EVENT_INTERNAL) {
        dci_run_internal(cpu, (uint16_t)event_value);
        return;
    }
    if (event_source == DCI_EVENT_SAMPLE) {
        dci_run_sample(cpu, (uint16_t)event_value);
        return;
    }
    if (event_source == DCI_EVENT_FRAME_START) {
        if ((uint16_t)event_value != dci->generation) {
            return;
        }
        dci->internal_scheduled = false;
        if (!dci->pps_frame_pending || dci->started ||
            !dspic33_device_internal_dci_configuration_supported(cpu) || !dci_clock_running(cpu)) {
            return;
        }
        dci->pps_frame_pending = false;
        dci_begin_serial_frame(cpu);
        dci_begin_internal_word(cpu);
        return;
    }
    if (event_source == DCI_EVENT_EXTERNAL || event_source == DCI_EVENT_EXTERNAL_FRAME) {
        dci_run_external(cpu, (uint16_t)event_value, event_source == DCI_EVENT_EXTERNAL_FRAME);
    }
}

static void dci_start(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);

    dspic33_device_internal_dci_discard_internal_events(cpu);
    dci->generation++;
    dci->started = false;
    dci->initialized = false;
    dci->disable_pending = false;
    dci->internal_scheduled = false;
    dci->disable_frames = 0u;
    dci->buffer = 0u;
    dci->slot = 0u;
    dci->serial_input = 0u;
    dci->serial_bits = 0u;
    dci->serial_startup_bits = (control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u ? 3u : 0u;
    dci->serial_frame_bits = 0u;
    dci->serial_output_high = false;
    dci->serial_output_driven = false;
    dci->serial_delay = false;
    dci->pps_frame_pending = false;
    dci->output_frame_high = true;
    dci->receive_buffered = 0u;
    dci->transmit_buffered = 0u;
    if (!dspic33_device_internal_dci_configuration_supported(cpu)) {
        return;
    }
    if ((control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        return;
    }
    dci_schedule_internal(cpu, DCI_EVENT_START, dci_bit_cycles(cpu) * 3u);
}

static bool dci_frame_remaining_cycles(const Dspic33* cpu, uint64_t* remaining_cycles) {
    uint64_t bit_cycle_count = dci_bit_cycles(cpu);

    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        const Dspic33Event* event = &cpu->events.items[event_index];
        if (event->type != DSPIC33_EVENT_DCI || event->source != DCI_EVENT_INTERNAL ||
            (uint16_t)event->value != cpu->io.dci.generation) {
            continue;
        }
        *remaining_cycles =
            event->paused ? event->paused_remaining : event->cycle - cpu->device_cycles;
        for (uint8_t slot_index = (uint8_t)(cpu->io.dci.slot + 1u);
             slot_index < dci_frame_count(cpu); slot_index++) {
            *remaining_cycles += bit_cycle_count * dci_slot_width(cpu, slot_index);
        }
        return true;
    }
    return false;
}

static void dci_disable(Dspic33* cpu, uint16_t control_word) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint64_t remaining_cycles;

    if (!dci->initialized || !dci->started) {
        dci_abort(cpu);
        return;
    }
    dci->disable_pending = true;
    dci->disable_frames = 1u;
    if ((control_word & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        remaining_cycles = dci_slot_width(cpu, dci->slot) - dci->serial_bits;
        for (uint8_t slot_index = (uint8_t)(dci->slot + 1u); slot_index < dci_frame_count(cpu);
             slot_index++) {
            remaining_cycles += dci_slot_width(cpu, slot_index);
        }
        if (remaining_cycles < 3u) {
            dci->disable_frames = 2u;
        }
    } else {
        if (!dci_frame_remaining_cycles(cpu, &remaining_cycles)) {
            dci_abort(cpu);
            return;
        }
        if (remaining_cycles < dci_bit_cycles(cpu) * 3u) {
            dci->disable_frames = 2u;
        }
    }
}

static void dci_update_pmd(Dspic33* cpu, uint16_t previous_pmd) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t current_pmd = dspic33_device_internal_raw_word(cpu, DCI_PMD_ADDRESS);

    if (((previous_pmd ^ current_pmd) & DCI_PMD) == 0u) {
        return;
    }
    bool pmd_is_disabled = (current_pmd & DCI_PMD) != 0u;

    dci->pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_DCI, DCI_EVENT_PMD,
                          ((uint32_t)dci->pmd_generation << DCI_EVENT_GENERATION_SHIFT) |
                              (pmd_is_disabled ? DCI_EVENT_DISABLED : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        dspic33_device_internal_raw_write_word(cpu, DCI_PMD_ADDRESS, previous_pmd);
        dci->pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_internal_update_dci_register(Dspic33* cpu, uint16_t register_address,
                                                 uint16_t previous_value) {
    Dspic33Dci* dci = &cpu->io.dci;
    if (register_address == DCI_PMD_ADDRESS) {
        dci_update_pmd(cpu, previous_value);
        return;
    }
    if (register_address == DCI_PPS_INPUTS) {
        dci->pps_input_configured = true;
        return;
    }
    if (register_address < DCI_BASE || register_address > DCI_TRANSMIT_BASE + 6u) {
        return;
    }
    if (dci->pmd_disabled) {
        dspic33_device_internal_raw_write_word(cpu, register_address, previous_value);
        return;
    }
    if (register_address == DCI_CONTROL1) {
        bool was_enabled = (previous_value & DCI_CONTROL_ENABLE) != 0u;
        bool currently_enabled =
            (dspic33_device_internal_raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_ENABLE) != 0u;
        if (!was_enabled && currently_enabled) {
            dci_start(cpu);
        } else if (was_enabled && !currently_enabled) {
            dci_disable(cpu, previous_value);
        }
        dspic33_device_internal_dci_update_power_state(cpu);
        return;
    }
    if (register_address == DCI_CONTROL3) {
        dci_update_bcg(cpu, true);
        dspic33_device_internal_dci_update_power_state(cpu);
        return;
    }
    if (register_address >= DCI_TRANSMIT_BASE && register_address <= DCI_TRANSMIT_BASE + 6u) {
        uint8_t transmit_buffer_index = (uint8_t)((register_address - DCI_TRANSMIT_BASE) / 2u);
        uint8_t buffer_mask = (uint8_t)(1u << transmit_buffer_index);

        if (transmit_buffer_index < dci_buffer_count(cpu)) {
            dci->transmit_written |= buffer_mask;
            dci->transmit_underflow &= (uint8_t)~buffer_mask;
        }
        if ((dci_active_transmit_buffers(cpu) & buffer_mask) != 0u) {
            dci->transmit_empty = false;
        }
        dci_refresh_status(cpu);
    }
}

bool dspic33_device_internal_dci_read_register(Dspic33* cpu, uint16_t register_address,
                                               uint8_t* read_value) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t register_base = (uint16_t)(register_address & 0xfffeu);

    if (register_base < DCI_BASE || register_base > DCI_TRANSMIT_BASE + 6u ||
        register_base == 0x028au || register_base == 0x028eu) {
        return false;
    }
    if (dci->pmd_disabled) {
        *read_value = 0u;
        return true;
    }
    if (register_base >= DCI_TRANSMIT_BASE) {
        *read_value = 0u;
        return true;
    }
    if (register_base >= DCI_RECEIVE_BASE && register_base <= DCI_RECEIVE_BASE + 6u &&
        ((!cpu->io.cpu_read_valid && !cpu->io.dma_transfer_active) ||
         (cpu->io.dma_transfer_active && cpu->io.dma_transfer_width == 1u) ||
         (!cpu->io.dma_transfer_active && cpu->io.cpu_read_width == 1u) ||
         (cpu->io.dma_transfer_active && cpu->io.dma_transfer_width == 2u &&
          (register_address & 1u) != 0u) ||
         (!cpu->io.dma_transfer_active && cpu->io.cpu_read_valid &&
          register_address == cpu->io.cpu_read_address + 1u))) {
        uint8_t receive_buffer_index = (uint8_t)((register_base - DCI_RECEIVE_BASE) / 2u);
        uint8_t buffer_mask = (uint8_t)(1u << receive_buffer_index);

        dci->receive_unread &= (uint8_t)~buffer_mask;
        dci->receive_overflow &= (uint8_t)~buffer_mask;
        dci_refresh_status(cpu);
    }
    return true;
}
