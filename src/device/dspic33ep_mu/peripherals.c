#include "device/dspic33ep_mu/internal.h"

bool dspic33_input_capture_input(Dspic33* cpu, uint8_t channel, bool high, uint64_t delay) {
    return channel < DSPIC33_INPUT_CAPTURE_COUNT &&
           dspic33_schedule_external(
               cpu, DSPIC33_EVENT_INPUT_CAPTURE, channel,
               INPUT_CAPTURE_EVENT_INPUT | (high ? INPUT_CAPTURE_EVENT_HIGH : 0u), delay);
}

bool dspic33_input_capture_pin(Dspic33* cpu, uint8_t pin, bool high, uint64_t delay) {
    return dspic33_device_internal_pps_pin_bonded(cpu, pin) &&
           dspic33_schedule_external(
               cpu, DSPIC33_EVENT_INPUT_CAPTURE, pin,
               INPUT_CAPTURE_EVENT_PIN | (high ? INPUT_CAPTURE_EVENT_HIGH : 0u), delay);
}

bool dspic33_output_compare_output(const Dspic33* cpu, uint8_t channel, bool* output_high) {
    uint16_t control_word;
    uint16_t channel_mask;

    if (channel >= DSPIC33_OUTPUT_COMPARE_COUNT || output_high == NULL ||
        !dspic33_device_internal_output_compare_supported(cpu, channel) ||
        dspic33_device_internal_output_compare_pmd_disabled(cpu, channel) ||
        (dspic33_device_internal_output_compare_cascade_requested(cpu, channel) &&
         (dspic33_device_internal_output_compare_pmd_disabled(
              cpu, dspic33_device_internal_output_compare_pair_low(channel)) ||
          dspic33_device_internal_output_compare_pmd_disabled(
              cpu, dspic33_device_internal_output_compare_pair_high(channel)))) ||
        (dspic33_device_internal_output_compare_cascade_requested(cpu, channel) &&
         channel == dspic33_device_internal_output_compare_pair_low(channel))) {
        return false;
    }
    control_word = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 2u));
    channel_mask = (uint16_t)(1u << channel);
    if ((cpu->io.output_compare.fault_held & channel_mask) != 0u) {
        *output_high = (control_word & OUTPUT_COMPARE_FAULT_OUTPUT) != 0u;
    } else {
        *output_high = dspic33_device_internal_output_compare_high(cpu, channel) !=
                       ((control_word & OUTPUT_COMPARE_INVERT) != 0u);
    }
    return true;
}

bool dspic33_output_compare_pin(const Dspic33* cpu, uint8_t pin, bool* output_high) {
    uint8_t channel;
    uint16_t control_word;
    uint16_t channel_mask;

    if (output_high == NULL ||
        !dspic33_device_internal_output_compare_pin_channel(cpu, pin, &channel)) {
        return false;
    }
    control_word = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 2u));
    channel_mask = (uint16_t)(1u << channel);
    if ((control_word & OUTPUT_COMPARE_TRISTATE) != 0u ||
        ((cpu->io.output_compare.fault_held & channel_mask) != 0u &&
         (control_word & OUTPUT_COMPARE_FAULT_TRISTATE) != 0u)) {
        return false;
    }
    return dspic33_output_compare_output(cpu, channel, output_high);
}

bool dspic33_output_compare_fault(Dspic33* cpu, uint8_t source, bool high, uint64_t delay) {
    return source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_OUTPUT_COMPARE_FAULT, source,
                                     high ? OUTPUT_COMPARE_FAULT_EVENT_HIGH : 0u, delay);
}

bool dspic33_output_compare_fault_pin(Dspic33* cpu, uint8_t pin, bool high, uint64_t delay) {
    return dspic33_device_internal_pps_pin_bonded(cpu, pin) &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_OUTPUT_COMPARE_FAULT, pin,
                                     OUTPUT_COMPARE_FAULT_EVENT_PIN |
                                         (high ? OUTPUT_COMPARE_FAULT_EVENT_HIGH : 0u),
                                     delay);
}

bool dspic33_comparator_input(Dspic33* cpu, uint8_t comparator, Dspic33ComparatorInput input_source,
                              uint16_t input_level, uint64_t delay) {
    uint16_t event_source;

    if (comparator >= DSPIC33_COMPARATOR_COUNT || input_source >= DSPIC33_COMPARATOR_INPUT_COUNT) {
        return false;
    }
    event_source = (uint16_t)(comparator * DSPIC33_COMPARATOR_INPUT_COUNT + input_source);
    return dspic33_schedule_external(cpu, DSPIC33_EVENT_COMPARATOR, event_source, input_level,
                                     delay);
}

bool dspic33_comparator_reference(Dspic33* cpu, Dspic33ComparatorReference reference,
                                  uint16_t reference_level, uint64_t delay) {
    return reference < DSPIC33_COMPARATOR_REFERENCE_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_COMPARATOR,
                                     (uint16_t)(COMPARATOR_EVENT_REFERENCE_FIRST + reference),
                                     reference_level, delay);
}

bool dspic33_comparator_output(const Dspic33* cpu, uint8_t comparator, bool* output_high) {
    if (comparator >= DSPIC33_COMPARATOR_COUNT || output_high == NULL ||
        cpu->io.comparator.pmd_disabled ||
        !dspic33_device_internal_comparator_configuration_supported(cpu, comparator)) {
        return false;
    }
    *output_high = (cpu->io.comparator.output_high & (uint8_t)(1u << comparator)) != 0u;
    return true;
}

bool dspic33_comparator_pin(const Dspic33* cpu, uint8_t pin, bool* output_high) {
    uint8_t comparator;
    if (output_high == NULL ||
        !dspic33_device_internal_comparator_pin_channel(cpu, pin, &comparator) ||
        (dspic33_device_internal_raw_word(cpu,
                                          dspic33_device_internal_comparator_base(comparator)) &
         COMPARATOR_OUTPUT_ENABLE) == 0u) {
        return false;
    }
    return dspic33_comparator_output(cpu, comparator, output_high);
}

bool dspic33_rtcc_clock(Dspic33* cpu, uint32_t edges, uint64_t delay) {
    return edges != 0u && dspic33_schedule_external(cpu, DSPIC33_EVENT_RTCC, 0u, edges, delay);
}

bool dspic33_rtcc_output(const Dspic33* cpu, bool* high) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, RTCC_CONTROL);
    if (high == NULL || cpu->io.rtcc.pmd_disabled || (control & RTCC_OUTPUT_ENABLE) == 0u) {
        return false;
    }
    if ((dspic33_device_internal_raw_word(cpu, RTCC_PAD_CONTROL) & RTCC_SECONDS_OUTPUT) != 0u) {
        *high = (control & RTCC_HALF_SECOND) != 0u;
    } else {
        *high = cpu->io.rtcc.alarm_output;
    }
    return true;
}

bool dspic33_qei_input(Dspic33* cpu, uint8_t channel, Dspic33QeiInput input, bool high,
                       uint64_t delay) {
    return channel < DSPIC33_QEI_COUNT && input <= DSPIC33_QEI_HOME &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_QEI,
                                     (uint16_t)(channel * 4u + (uint8_t)input), high ? 1u : 0u,
                                     delay);
}

bool dspic33_qei_compare_output(const Dspic33* cpu, uint8_t channel, bool* high) {
    return dspic33_device_internal_qei_compare_output_value(cpu, channel, high);
}

void dspic33_dci_input(Dspic33* cpu, uint16_t value) { cpu->io.dci.input = value; }

bool dspic33_dci_clock(Dspic33* cpu, uint16_t value, bool frame_sync, uint64_t delay) {
    return dspic33_schedule_external(cpu, DSPIC33_EVENT_DCI,
                                     frame_sync ? DCI_EVENT_EXTERNAL_FRAME : DCI_EVENT_EXTERNAL,
                                     value, delay);
}

bool dspic33_dci_transmit(Dspic33* cpu, Dspic33DciTransfer* transfer) {
    return transfer != NULL &&
           dspic33_device_internal_dci_output_pop(&cpu->io.dci.output, transfer);
}

bool dspic33_dci_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    uint16_t control;
    if (high == NULL) {
        return false;
    }
    uint8_t function = dspic33_device_internal_pps_output_function(cpu, pin);
    control = dspic33_device_internal_raw_word(cpu, DCI_CONTROL1);
    if (function == 0u || cpu->io.dci.pmd_disabled) {
        return false;
    }
    if (function == DCI_PPS_CLOCK_OUTPUT && (control & DCI_CONTROL_EXTERNAL_CLOCK) == 0u &&
        dspic33_device_internal_raw_word(cpu, DCI_CONTROL3) != 0u) {
        return dspic33_device_internal_dci_internal_clock_high(cpu, high);
    }
    if (!dspic33_device_internal_dci_configuration_supported(cpu) ||
        ((control & DCI_CONTROL_ENABLE) == 0u && !cpu->io.dci.disable_pending)) {
        return false;
    }
    if (function == DCI_PPS_DATA_OUTPUT) {
        return dspic33_device_internal_dci_data_output(cpu, high);
    }
    if (function == DCI_PPS_FRAME_OUTPUT && (control & DCI_CONTROL_EXTERNAL_FRAME) == 0u) {
        return dspic33_device_internal_dci_frame_output(cpu, high);
    }
    return false;
}

bool dspic33_timer_pulse(Dspic33* cpu, uint8_t timer, uint32_t pulses, uint64_t delay) {
    return timer < DSPIC33_TIMER_COUNT && pulses != 0u &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_TIMER, timer, pulses, delay);
}

bool dspic33_timer_gate(Dspic33* cpu, uint8_t timer, bool high, uint64_t delay) {
    return timer < DSPIC33_TIMER_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_TIMER_GATE, timer, high ? 1u : 0u, delay);
}

bool dspic33_adc_trigger(Dspic33* cpu, uint8_t module, uint8_t source, uint64_t delay) {
    uint32_t value;
    if (module >= DSPIC33_ADC_COUNT || source == 0u || source == 6u || source == 7u ||
        source >= 15u) {
        return false;
    }
    value = source | ((uint32_t)UINT16_MAX << ADC_EVENT_GENERATION_SHIFT);
    return dspic33_schedule_external(cpu, DSPIC33_EVENT_ADC, module, value, delay);
}

bool dspic33_pwm_fault(Dspic33* cpu, uint8_t source, bool high, uint64_t delay) {
    return source < DSPIC33_PWM_INPUT_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_PWM_FAULT, source,
                                     high ? PWM_INPUT_HIGH : 0u, delay);
}

bool dspic33_pwm_current_limit(Dspic33* cpu, uint8_t source, bool high, uint64_t delay) {
    return source < DSPIC33_PWM_INPUT_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_PWM_CURRENT_LIMIT, source,
                                     high ? PWM_INPUT_HIGH : 0u, delay);
}

bool dspic33_pwm_dead_time(Dspic33* cpu, uint8_t generator, bool high, uint64_t delay) {
    return generator < dspic33_device_internal_pwm_generator_count(cpu) &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_PWM_DEAD_TIME, generator,
                                     high ? PWM_INPUT_HIGH : 0u, delay);
}

bool dspic33_pwm_sync(Dspic33* cpu, uint8_t input, bool high, uint64_t delay) {
    return input < 2u && dspic33_schedule_external(cpu, DSPIC33_EVENT_PWM_SYNC, input,
                                                   high ? PWM_INPUT_HIGH : 0u, delay);
}

bool dspic33_pwm_sync_output(const Dspic33* cpu, uint8_t time_base) {
    uint16_t control;
    bool active;
    if (time_base >= 2u || dspic33_device_internal_pwm_global_pmd_disabled(cpu)) {
        return false;
    }
    control = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu)));
    if ((control & 0x0100u) == 0u) {
        return false;
    }
    active = cpu->device_cycles < cpu->io.pwm_sync_until[time_base];
    return (control & 0x0200u) != 0u ? !active : active;
}

bool dspic33_pwm_sync_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    uint8_t time_base;
    uint16_t control;
    if (high == NULL || dspic33_device_internal_pwm_global_pmd_disabled(cpu)) {
        return false;
    }
    control = dspic33_device_internal_raw_word(cpu, PWM_GLOBAL_BASE);
    if (cpu->power_state == DSPIC33_POWER_IDLE && (control & PWM_STOP_IDLE) != 0u) {
        return false;
    }
    uint8_t function = dspic33_device_internal_pps_output_function(cpu, pin);
    if (function < 0x2du || function > 0x2eu) {
        return false;
    }
    time_base = (uint8_t)(function - 0x2du);
    control = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu)));
    if ((control & 0x0100u) == 0u) {
        return false;
    }
    *high = dspic33_pwm_sync_output(cpu, time_base);
    return true;
}

bool dspic33_pwm_output(const Dspic33* cpu, uint8_t generator, bool high) {
    uint8_t output = (uint8_t)(generator * 2u + (high ? 0u : 1u));
    return generator < dspic33_device_internal_pwm_generator_count(cpu) &&
           !dspic33_device_internal_pwm_generator_pmd_disabled(cpu, generator) &&
           cpu->io.pwm[output] != 0u;
}

bool dspic33_can_receive(Dspic33* cpu, uint8_t channel, const Dspic33CanFrame* frame,
                         uint64_t delay) {
    Dspic33CanQueue* queue;
    if (channel >= DSPIC33_CAN_COUNT || frame->length > 8u ||
        (!frame->extended ? frame->identifier > 0x7ffu : frame->identifier > 0x1fffffffu)) {
        return false;
    }
    queue = &cpu->io.can_rx[channel];
    if (!dspic33_device_internal_can_queue_push(queue, frame)) {
        return false;
    }
    if (dspic33_schedule_external(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_SUCCESS,
                                  delay)) {
        return true;
    }
    dspic33_device_internal_can_queue_discard_last(queue);
    return false;
}

bool dspic33_can_error(Dspic33* cpu, uint8_t channel, bool transmit, uint8_t count,
                       uint64_t delay) {
    uint32_t value;
    if (channel >= DSPIC33_CAN_COUNT || count == 0u) {
        return false;
    }
    value = CAN_EVENT_ERROR | ((uint32_t)count << CAN_EVENT_ERROR_COUNT_SHIFT);
    if (transmit) {
        value |= CAN_EVENT_TRANSMIT_ERROR;
    }
    return dspic33_schedule_external(cpu, DSPIC33_EVENT_CAN, channel, value, delay);
}

bool dspic33_can_invalid(Dspic33* cpu, uint8_t channel, uint64_t delay) {
    return channel < DSPIC33_CAN_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_INVALID, delay);
}

bool dspic33_can_transmit(Dspic33* cpu, uint8_t channel, Dspic33CanFrame* frame) {
    return channel < DSPIC33_CAN_COUNT &&
           dspic33_device_internal_can_queue_pop(&cpu->io.can_tx[channel], frame);
}

bool dspic33_can_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    uint8_t channel;
    if (high == NULL) {
        return false;
    }
    uint8_t function = dspic33_device_internal_pps_output_function(cpu, pin);
    if (function < 14u || function > 15u) {
        return false;
    }
    channel = (uint8_t)(function - 14u);
    if ((dspic33_device_internal_raw_word(cpu, 0x0760u) & (uint16_t)(2u << channel)) != 0u) {
        return false;
    }
    *high = true;
    if ((dspic33_device_internal_raw_word(cpu,
                                          (uint16_t)(dspic33_device_can_bases[channel] + 0x0au)) &
         CAN_BUS_OFF) != 0u) {
        return true;
    }
    if ((cpu->io.can_tx_error_active & (uint8_t)(1u << channel)) != 0u) {
        uint64_t index = (cpu->device_cycles - cpu->io.can_tx_error_start_cycle[channel]) /
                         dspic33_device_internal_can_bit_cycles(cpu, channel);
        *high = (dspic33_device_internal_raw_word(
                     cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x0au)) &
                 CAN_TRANSMIT_PASSIVE) != 0u ||
                index >= 6u;
        return true;
    }
    if ((cpu->io.can_rx_error_active & (uint8_t)(1u << channel)) != 0u) {
        uint64_t index = (cpu->device_cycles - cpu->io.can_rx_error_start_cycle[channel]) /
                         dspic33_device_internal_can_bit_cycles(cpu, channel);
        *high = (dspic33_device_internal_raw_word(
                     cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x0au)) &
                 CAN_RECEIVE_PASSIVE) != 0u ||
                index >= 6u;
        return true;
    }
    if ((cpu->io.can_overload_active & (uint8_t)(1u << channel)) != 0u) {
        uint64_t index = (cpu->device_cycles - cpu->io.can_overload_start_cycle[channel]) /
                         dspic33_device_internal_can_bit_cycles(cpu, channel);
        *high = index >= 6u;
        return true;
    }
    if ((cpu->io.can_rx_ack & (uint8_t)(1u << channel)) != 0u) {
        *high = false;
        return true;
    }
    if ((cpu->io.can_tx_on_bus & (uint8_t)(1u << channel)) != 0u &&
        dspic33_device_internal_can_power_enabled(cpu, channel) &&
        dspic33_device_internal_can_mode(cpu, channel) == CAN_MODE_NORMAL) {
        Dspic33CanFrame frame =
            dspic33_device_internal_can_decode_frame(cpu->io.can_tx_words[channel]);
        bool bits[160];
        uint16_t count = dspic33_device_internal_can_frame_bits(&frame, bits);
        int64_t elapsed = (int64_t)(cpu->device_cycles - cpu->io.can_tx_start_cycle[channel]) -
                          cpu->io.can_tx_phase_adjustment[channel];
        uint64_t index =
            elapsed > 0 ? (uint64_t)elapsed / dspic33_device_internal_can_bit_cycles(cpu, channel)
                        : 0u;
        if (index < count) {
            *high = bits[index];
        }
    }
    return true;
}

bool dspic33_can_input_pin(Dspic33* cpu, uint8_t pin, bool high, uint64_t delay) {
    return dspic33_device_internal_pps_pin_bonded(cpu, pin) &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_CAN, pin,
                                     CAN_EVENT_RECEIVE_PIN | (high ? CAN_EVENT_PIN_HIGH : 0u),
                                     delay);
}

static bool usb_schedule_pending(Dspic33* cpu, const Dspic33UsbPending* pending, uint64_t delay,
                                 bool external) {
    uint16_t slot;
    for (slot = 0u; slot < DSPIC33_USB_PENDING_COUNT; slot++) {
        if (!cpu->io.usb_pending[slot].active) {
            cpu->io.usb_pending[slot] = *pending;
            cpu->io.usb_pending[slot].active = true;
            bool scheduled =
                external ? dspic33_schedule_external(cpu, DSPIC33_EVENT_USB, slot, 0u, delay)
                         : dspic33_schedule(cpu, DSPIC33_EVENT_USB, slot, 0u, delay);
            if (scheduled) {
                return true;
            }
            cpu->io.usb_pending[slot].active = false;
            return false;
        }
    }
    return false;
}

static bool usb_schedule_token(Dspic33* cpu, uint8_t address, uint8_t endpoint, uint8_t pid,
                               const uint8_t* data, uint16_t size, bool data1,
                               Dspic33UsbHandshake handshake, uint64_t delay) {
    Dspic33UsbPending pending;
    if (endpoint >= DSPIC33_USB_ENDPOINT_COUNT || size > DSPIC33_USB_PACKET_SIZE ||
        (size != 0u && data == NULL)) {
        return false;
    }
    memset(&pending, 0, sizeof(pending));
    pending.packet.address = address;
    pending.packet.endpoint = endpoint;
    pending.packet.pid = pid;
    pending.packet.size = size;
    pending.packet.data1 = data1;
    pending.packet.handshake = handshake;
    if (size != 0u) {
        memcpy(pending.packet.data, data, size);
    }
    return usb_schedule_pending(cpu, &pending, delay, true);
}

bool dspic33_usb_receive_toggle(Dspic33* cpu, uint8_t endpoint, const uint8_t* data, uint16_t size,
                                bool data1, uint64_t delay) {
    return usb_schedule_token(
        cpu, (uint8_t)(dspic33_device_internal_raw_word(cpu, USB_ADDR) & 0x007fu), endpoint,
        DSPIC33_USB_PID_OUT, data, size, data1, DSPIC33_USB_HANDSHAKE_NONE, delay);
}

bool dspic33_usb_receive(Dspic33* cpu, uint8_t endpoint, const uint8_t* data, uint16_t size,
                         uint64_t delay) {
    return dspic33_usb_receive_toggle(cpu, endpoint, data, size, false, delay);
}

bool dspic33_usb_setup(Dspic33* cpu, uint8_t endpoint, const uint8_t* data, uint16_t size,
                       uint64_t delay) {
    return size == 8u &&
           usb_schedule_token(
               cpu, (uint8_t)(dspic33_device_internal_raw_word(cpu, USB_ADDR) & 0x007fu), endpoint,
               DSPIC33_USB_PID_SETUP, data, size, false, DSPIC33_USB_HANDSHAKE_NONE, delay);
}

bool dspic33_usb_request(Dspic33* cpu, uint8_t endpoint, uint64_t delay) {
    return usb_schedule_token(
        cpu, (uint8_t)(dspic33_device_internal_raw_word(cpu, USB_ADDR) & 0x007fu), endpoint,
        DSPIC33_USB_PID_IN, NULL, 0u, false, DSPIC33_USB_HANDSHAKE_NONE, delay);
}

bool dspic33_usb_token(Dspic33* cpu, uint8_t address, uint8_t endpoint, Dspic33UsbPid pid,
                       const uint8_t* data, uint16_t size, bool data1, uint64_t delay) {
    return address <= 0x7fu &&
           (pid == DSPIC33_USB_PID_OUT || pid == DSPIC33_USB_PID_IN ||
            pid == DSPIC33_USB_PID_SETUP) &&
           (pid != DSPIC33_USB_PID_SETUP || size == 8u) &&
           (pid != DSPIC33_USB_PID_IN || size == 0u) &&
           usb_schedule_token(cpu, address, endpoint, (uint8_t)pid, data, size, data1,
                              DSPIC33_USB_HANDSHAKE_NONE, delay);
}

bool dspic33_usb_host_response(Dspic33* cpu, Dspic33UsbHandshake handshake, const uint8_t* data,
                               uint16_t size, bool data1, uint64_t delay) {
    return cpu->io.usb_host_pending &&
           usb_schedule_token(cpu,
                              (uint8_t)(dspic33_device_internal_raw_word(cpu, USB_ADDR) & 0x007fu),
                              cpu->io.usb_host_endpoint, cpu->io.usb_host_pid, data, size, data1,
                              handshake, delay);
}

bool dspic33_usb_bus(Dspic33* cpu, Dspic33UsbBusEvent event, uint16_t value, uint64_t delay) {
    return dspic33_device_internal_usb_schedule_bus_event(cpu, event, value, delay, true);
}

bool dspic33_device_internal_usb_schedule_bus_event(Dspic33* cpu, Dspic33UsbBusEvent event,
                                                    uint16_t value, uint64_t delay, bool external) {
    Dspic33UsbPending pending;
    if (event > DSPIC33_USB_BUS_OTG_STATE) {
        return false;
    }
    memset(&pending, 0, sizeof(pending));
    pending.bus_event = true;
    pending.event = event;
    pending.value = value;
    return usb_schedule_pending(cpu, &pending, delay, external);
}

bool dspic33_usb_transmit(Dspic33* cpu, Dspic33UsbPacket* packet) {
    return dspic33_device_internal_usb_queue_pop(&cpu->io.usb_tx, packet);
}
