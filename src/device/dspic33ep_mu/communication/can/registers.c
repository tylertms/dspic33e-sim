#include "device/dspic33ep_mu/internal.h"

void dspic33_device_internal_can_remove_transmit_events(Dspic33* cpu, uint8_t channel) {
    size_t write_index = 0u;
    for (size_t read_index = 0u; read_index < cpu->events.count; read_index++) {
        Dspic33Event* event = &cpu->events.items[read_index];
        uint32_t event_kind = event->value & CAN_EVENT_KIND_MASK;
        if (event->type != DSPIC33_EVENT_CAN || event->source != channel ||
            (event_kind != CAN_EVENT_TRANSMIT_START && event_kind != CAN_EVENT_TRANSMIT_WORD &&
             event_kind != CAN_EVENT_TRANSMIT_FINISH &&
             event_kind != CAN_EVENT_TRANSMIT_BUS_FINISH &&
             event_kind != CAN_EVENT_TRANSMIT_RETRY &&
             event_kind != CAN_EVENT_TRANSMIT_ERROR_START &&
             event_kind != CAN_EVENT_TRANSMIT_SAMPLE &&
             event_kind != CAN_EVENT_TRANSMIT_SAMPLE_FIRST &&
             event_kind != CAN_EVENT_TRANSMIT_SAMPLE_SECOND)) {
            cpu->events.items[write_index++] = *event;
        }
    }
    cpu->events.count = write_index;
    dspic33_reorder_events(cpu);
}

static void can_abort_transmissions(Dspic33* cpu, uint8_t channel) {
    dspic33_device_internal_can_remove_transmit_events(cpu, channel);
    uint8_t buffer_index;
    for (buffer_index = 0u; buffer_index < 8u; buffer_index++) {
        uint16_t control = dspic33_device_internal_can_buffer_control(cpu, channel, buffer_index);
        if ((control & CAN_BUFFER_REQUEST) != 0u) {
            dspic33_device_internal_can_set_buffer_control(
                cpu, channel, buffer_index,
                (uint16_t)((control & ~CAN_BUFFER_REQUEST) | CAN_BUFFER_ABORTED));
            dspic33_device_internal_can_raise_event(cpu, channel, CAN_INTERRUPT_TRANSMIT,
                                                    buffer_index, 0u);
        }
    }
    dspic33_device_internal_raw_write_word(
        cpu, dspic33_device_can_bases[channel],
        (uint16_t)(dspic33_device_internal_raw_word(cpu, dspic33_device_can_bases[channel]) &
                   ~CAN_ABORT_ALL));
    cpu->io.can_tx_on_bus &= (uint8_t)~(uint8_t)(1u << channel);
    cpu->io.can_tx_busy &= (uint8_t)~(uint8_t)(1u << channel);
    cpu->io.can_tx_retry_wait &= (uint8_t)~(uint8_t)(1u << channel);
    cpu->io.can_tx_error_active &= (uint8_t)~(uint8_t)(1u << channel);
}

static void can_clear_receive_flags(Dspic33* cpu, uint8_t channel, uint16_t address,
                                    uint16_t previous, uint16_t requested) {
    uint16_t cleared_flags;
    uint16_t updated_flags = (uint16_t)(previous & requested);
    uint8_t buffer_offset = address == dspic33_device_can_bases[channel] + 0x22u ? 16u : 0u;
    uint8_t bit_index;

    dspic33_device_internal_raw_write_word(cpu, address, updated_flags);
    if (address != dspic33_device_can_bases[channel] + 0x20u &&
        address != dspic33_device_can_bases[channel] + 0x22u) {
        return;
    }
    cleared_flags = (uint16_t)(previous & ~updated_flags);
    for (bit_index = 0u; bit_index < 16u; bit_index++) {
        uint8_t buffer = (uint8_t)(buffer_offset + bit_index);
        if ((cleared_flags & (uint16_t)(1u << bit_index)) != 0u &&
            buffer >= (dspic33_device_internal_raw_word(
                           cpu, (uint16_t)(dspic33_device_can_bases[channel] + 6u)) &
                       0x001fu)) {
            uint8_t next = dspic33_device_internal_can_next_fifo_buffer(cpu, channel, buffer);
            uint16_t fifo = dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_can_bases[channel] + 8u));
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(dspic33_device_can_bases[channel] + 8u),
                (uint16_t)((fifo & 0x3f00u) | next));
        }
    }
}
static void can_update_transmit_control(Dspic33* cpu, uint8_t channel, uint16_t address,
                                        uint16_t previous) {
    uint8_t first_buffer = (uint8_t)(((address - dspic33_device_can_bases[channel]) - 0x30u));

    for (uint8_t half = 0u; half < 2u; half++) {
        uint8_t buffer_index = (uint8_t)(first_buffer + half);
        uint8_t shift = (uint8_t)(half * 8u);
        uint16_t previous_control = (uint16_t)((previous >> shift) & 0xffu);
        uint16_t current_control =
            dspic33_device_internal_can_buffer_control(cpu, channel, buffer_index);
        if ((current_control & CAN_BUFFER_REQUEST) != 0u &&
            (previous_control & CAN_BUFFER_REQUEST) == 0u) {
            current_control &= (uint16_t)~(CAN_BUFFER_ABORTED | CAN_BUFFER_LOST | CAN_BUFFER_ERROR);
            dspic33_device_internal_can_set_buffer_control(cpu, channel, buffer_index,
                                                           current_control);
            dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u);
        } else if ((current_control & CAN_BUFFER_REQUEST) == 0u &&
                   (previous_control & CAN_BUFFER_REQUEST) != 0u) {
            bool active = (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) != 0u &&
                          cpu->io.can_tx_buffer[channel] == buffer_index;
            dspic33_device_internal_can_set_buffer_control(
                cpu, channel, buffer_index, (uint16_t)(current_control | CAN_BUFFER_ABORTED));
            dspic33_device_internal_can_raise_event(cpu, channel, CAN_INTERRUPT_TRANSMIT,
                                                    buffer_index, 0u);
            if (active) {
                dspic33_device_internal_can_remove_transmit_events(cpu, channel);
                cpu->io.can_tx_on_bus &= (uint8_t)~(uint8_t)(1u << channel);
                cpu->io.can_tx_busy &= (uint8_t)~(uint8_t)(1u << channel);
                cpu->io.can_tx_retry_wait &= (uint8_t)~(uint8_t)(1u << channel);
                cpu->io.can_tx_error_active &= (uint8_t)~(uint8_t)(1u << channel);
                dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u);
            }
        }
    }
}

void dspic33_device_internal_update_can_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                 uint16_t requested) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t register_base = dspic33_device_can_bases[channel];
        uint16_t register_offset = (uint16_t)(address - register_base);
        bool filter_window;
        uint16_t writable;
        if (register_offset > 0x7eu || (register_offset & 1u) != 0u) {
            continue;
        }
        filter_window = (dspic33_device_internal_raw_word(cpu, register_base) & CAN_WINDOW) != 0u;
        if (register_offset == 0u) {
            uint16_t control = dspic33_device_internal_raw_word(cpu, register_base);
            uint16_t mode = (uint16_t)((control & CAN_MODE_MASK) >> CAN_MODE_SHIFT);
            uint16_t active = (uint16_t)((previous >> 5u) & 7u);
            uint16_t prior_request = (uint16_t)((previous & CAN_MODE_MASK) >> CAN_MODE_SHIFT);
            control = (uint16_t)((control & ~0x00e0u) | (active << 5u));
            dspic33_device_internal_raw_write_word(cpu, register_base, control);
            if (mode != prior_request) {
                cpu->io.can_mode_generation[channel]++;
                if (mode != active && !dspic33_device_internal_can_schedule_mode_transition(
                                          cpu, channel, (uint8_t)mode)) {
                    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
                }
            }
            if ((control & CAN_ABORT_ALL) != 0u) {
                can_abort_transmissions(cpu, channel);
            }
            return;
        }
        if ((register_offset == 6u || register_offset == 0x10u || register_offset == 0x12u ||
             register_offset == 0x14u || register_offset == 0x18u || register_offset == 0x1au) &&
            dspic33_device_internal_can_mode(cpu, channel) != CAN_MODE_CONFIGURATION) {
            dspic33_device_internal_raw_write_word(cpu, address, previous);
            return;
        }
        if (register_offset == 0x0au) {
            dspic33_device_internal_raw_write_word(
                cpu, address, (uint16_t)((previous & ~0x00efu) | (previous & requested & 0x00efu)));
            dspic33_device_internal_can_refresh_error_status(cpu, channel);
            dspic33_device_internal_can_update_vector(cpu, channel);
            return;
        }
        if (register_offset == 0x0cu) {
            dspic33_device_internal_can_update_vector(cpu, channel);
            return;
        }
        if (filter_window && register_offset >= 0x20u) {
            uint16_t prior = dspic33_device_internal_can_filter_word(cpu, channel, register_offset);
            if (dspic33_device_internal_can_mode(cpu, channel) == CAN_MODE_CONFIGURATION &&
                dspic33_device_internal_can_register_write_mask(cpu, address, &writable)) {
                cpu->io.can_filter_window[channel][(register_offset - 0x20u) / 2u] =
                    (uint16_t)((prior & ~writable) | (requested & writable));
            }
            dspic33_device_internal_raw_write_word(cpu, address, previous);
            return;
        }
        if (!filter_window && (register_offset == 0x20u || register_offset == 0x22u ||
                               register_offset == 0x28u || register_offset == 0x2au)) {
            can_clear_receive_flags(cpu, channel, address, previous, requested);
            return;
        }
        if (!filter_window && register_offset >= 0x30u && register_offset <= 0x36u) {
            can_update_transmit_control(cpu, channel, address, previous);
            return;
        }
        if (register_offset == 6u) {
            uint8_t channel_bit = (uint8_t)(1u << channel);
            bool low_byte = cpu->io.cpu_write_valid && cpu->io.cpu_write_width == 1u &&
                            cpu->io.cpu_write_address == address;
            bool high_byte = cpu->io.cpu_write_valid && cpu->io.cpu_write_width == 1u &&
                             cpu->io.cpu_write_address == address + 1u;
            if (low_byte) {
                cpu->io.can_fctrl_fsa_ready |= channel_bit;
            } else if (high_byte && (cpu->io.can_fctrl_fsa_ready & channel_bit) != 0u) {
                cpu->io.can_fctrl_fsa_ready &= (uint8_t)~channel_bit;
            } else {
                dspic33_device_internal_raw_write_word(cpu, address, previous);
                cpu->io.can_fctrl_fsa_ready &= (uint8_t)~channel_bit;
                return;
            }
        }
        if (!filter_window && register_offset == 0x42u &&
            (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) != 0u &&
            cpu->io.can_tx_word[channel] != 0u) {
            cpu->io.can_tx_words[channel][cpu->io.can_tx_word[channel] - 1u] =
                dspic33_device_internal_raw_word(cpu, address);
            return;
        }
        if (register_offset == 6u) {
            uint8_t start = (uint8_t)(dspic33_device_internal_raw_word(cpu, address) & 0x001fu);
            cpu->io.can_fifo_write[channel] = start;
            dspic33_device_internal_raw_write_word(cpu, (uint16_t)(register_base + 8u),
                                                   (uint16_t)(((uint16_t)start << 8u) | start));
        }
        return;
    }
}
