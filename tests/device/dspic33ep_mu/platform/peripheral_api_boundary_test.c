#include <string.h>

#include "allocation_failure.h"
#include "architecture/dspic33/internal.h"
#include "device/dspic33ep_mu/internal.h"
#include "test.h"

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill peripheral API event queue");
    }
}
#endif

static void timing_and_control_cases(TestState* state, Dspic33* cpu) {
    cpu->device_cycles = 1u;
    expect(state,
           !dspic33_input_capture_input(cpu, DSPIC33_INPUT_CAPTURE_COUNT, false, 0u) &&
               !dspic33_input_capture_input(cpu, 0u, false, UINT64_MAX) &&
               !dspic33_input_capture_pin(cpu, UINT8_MAX, false, 0u) &&
               !dspic33_input_capture_pin(cpu, 0u, false, UINT64_MAX) &&
               !dspic33_output_compare_fault(cpu, DSPIC33_OUTPUT_COMPARE_FAULT_COUNT, false, 0u) &&
               !dspic33_output_compare_fault(cpu, 0u, false, UINT64_MAX) &&
               !dspic33_output_compare_fault_pin(cpu, UINT8_MAX, false, 0u) &&
               !dspic33_output_compare_fault_pin(cpu, 0u, false, UINT64_MAX),
           "capture and compare APIs reject invalid boundaries");

    expect(
        state,
        !dspic33_comparator_input(cpu, DSPIC33_COMPARATOR_COUNT, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                  0u, 0u) &&
            !dspic33_comparator_input(cpu, 0u, (Dspic33ComparatorInput)4u, 0u, 0u) &&
            !dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, UINT64_MAX) &&
            !dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_COUNT, 0u, 0u) &&
            !dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_AVDD, 0u, UINT64_MAX) &&
            !dspic33_rtcc_clock(cpu, 0u, 0u) && !dspic33_rtcc_clock(cpu, 1u, UINT64_MAX),
        "analog and RTCC APIs reject invalid boundaries");

    expect(state,
           !dspic33_qei_input(cpu, DSPIC33_QEI_COUNT, DSPIC33_QEI_PHASE_A, false, 0u) &&
               !dspic33_qei_input(cpu, 0u, (Dspic33QeiInput)4u, false, 0u) &&
               !dspic33_qei_input(cpu, 0u, DSPIC33_QEI_PHASE_A, false, UINT64_MAX) &&
               !dspic33_timer_pulse(cpu, DSPIC33_TIMER_COUNT, 1u, 0u) &&
               !dspic33_timer_pulse(cpu, 0u, 0u, 0u) &&
               !dspic33_timer_pulse(cpu, 0u, 1u, UINT64_MAX) &&
               !dspic33_timer_gate(cpu, DSPIC33_TIMER_COUNT, false, 0u) &&
               !dspic33_timer_gate(cpu, 0u, false, UINT64_MAX),
           "QEI and timer APIs reject invalid boundaries");
}

static void conversion_and_pwm_cases(TestState* state, Dspic33* cpu) {
    expect(state,
           !dspic33_adc_trigger(cpu, DSPIC33_ADC_COUNT, 1u, 0u) &&
               !dspic33_adc_trigger(cpu, 0u, 0u, 0u) && !dspic33_adc_trigger(cpu, 0u, 6u, 0u) &&
               !dspic33_adc_trigger(cpu, 0u, 7u, 0u) && !dspic33_adc_trigger(cpu, 0u, 15u, 0u) &&
               !dspic33_adc_trigger(cpu, 0u, 1u, UINT64_MAX),
           "ADC API rejects invalid boundaries");
    expect(state,
           !dspic33_pwm_fault(cpu, DSPIC33_PWM_INPUT_COUNT, false, 0u) &&
               !dspic33_pwm_fault(cpu, 0u, false, UINT64_MAX) &&
               !dspic33_pwm_current_limit(cpu, DSPIC33_PWM_INPUT_COUNT, false, 0u) &&
               !dspic33_pwm_current_limit(cpu, 0u, false, UINT64_MAX) &&
               !dspic33_pwm_dead_time(cpu, DSPIC33_PWM_MAX_COUNT, false, 0u) &&
               !dspic33_pwm_dead_time(cpu, 0u, false, UINT64_MAX) &&
               !dspic33_pwm_sync(cpu, 2u, false, 0u) &&
               !dspic33_pwm_sync(cpu, 0u, false, UINT64_MAX),
           "PWM APIs reject invalid boundaries");
}

static void can_cases(TestState* state, Dspic33* cpu) {
    Dspic33CanFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.length = 9u;
    expect(state, !dspic33_can_receive(cpu, 0u, &frame, 0u),
           "CAN receive rejects an oversized frame");
    frame.length = 0u;
    frame.identifier = 0x800u;
    expect(state, !dspic33_can_receive(cpu, 0u, &frame, 0u),
           "CAN receive rejects an oversized standard identifier");
    frame.extended = true;
    frame.identifier = 0x20000000u;
    expect(state, !dspic33_can_receive(cpu, 0u, &frame, 0u),
           "CAN receive rejects an oversized extended identifier");
    frame.identifier = 0u;
    cpu->io.can_rx[0].count = 64u;
    expect(state, !dspic33_can_receive(cpu, 0u, &frame, 0u), "CAN receive rejects a full queue");
    cpu->io.can_rx[0].count = 0u;
    expect(state,
           !dspic33_can_receive(cpu, DSPIC33_CAN_COUNT, &frame, 0u) &&
               !dspic33_can_receive(cpu, 0u, &frame, UINT64_MAX) &&
               !dspic33_can_error(cpu, DSPIC33_CAN_COUNT, false, 1u, 0u) &&
               !dspic33_can_error(cpu, 0u, false, 0u, 0u) &&
               !dspic33_can_error(cpu, 0u, false, 1u, UINT64_MAX) &&
               !dspic33_can_invalid(cpu, DSPIC33_CAN_COUNT, 0u) &&
               !dspic33_can_invalid(cpu, 0u, UINT64_MAX) &&
               !dspic33_can_transmit(cpu, DSPIC33_CAN_COUNT, &frame),
           "CAN APIs reject invalid boundaries");
}

static void can_output_cases(TestState* state, Dspic33* cpu) {
    Dspic33CanFrame frame = {0};
    bool high = false;
    dspic33_reset(cpu, 0u);
    dspic33_write_byte(cpu, 0x0680u, 13u);
    expect(state, !dspic33_can_pin(cpu, 64u, &high),
           "CAN output rejects a lower adjacent PPS function");
    dspic33_write_byte(cpu, 0x0680u, 16u);
    expect(state, !dspic33_can_pin(cpu, 64u, &high),
           "CAN output rejects an upper adjacent PPS function");
    dspic33_write_byte(cpu, 0x0680u, 14u);
    dspic33_device_internal_raw_write_word(cpu, 0x0760u, 2u);
    expect(state, !dspic33_can_pin(cpu, 64u, &high), "CAN output rejects channel PMD");

    dspic33_device_internal_raw_write_word(cpu, 0x0760u, 0u);
    dspic33_device_internal_raw_write_word(cpu, 0x040au, 0x2000u);
    expect(state, dspic33_can_pin(cpu, 64u, &high) && high, "bus-off CAN output remains recessive");

    dspic33_device_internal_raw_write_word(cpu, 0x040au, 0u);
    cpu->io.can_tx_error_active = 1u;
    cpu->io.can_tx_error_start_cycle[0] = cpu->device_cycles;
    expect(state, dspic33_can_pin(cpu, 64u, &high) && !high,
           "active CAN transmit error starts dominant");
    cpu->io.can_tx_error_active = 0u;
    cpu->io.can_rx_error_active = 1u;
    cpu->io.can_rx_error_start_cycle[0] = cpu->device_cycles;
    expect(state, dspic33_can_pin(cpu, 64u, &high) && !high,
           "active CAN receive error starts dominant");
    cpu->io.can_rx_error_active = 0u;
    cpu->io.can_overload_active = 1u;
    cpu->io.can_overload_start_cycle[0] = cpu->device_cycles;
    expect(state, dspic33_can_pin(cpu, 64u, &high) && !high, "active CAN overload starts dominant");
    cpu->io.can_overload_active = 0u;
    cpu->io.can_rx_ack = 1u;
    expect(state, dspic33_can_pin(cpu, 64u, &high) && !high,
           "CAN acknowledgement drives the bus dominant");

    cpu->io.can_rx_ack = 0u;
    cpu->io.can_tx_on_bus = 1u;
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_internal_raw_write_word(cpu, 0x0400u, 0u);
    cpu->device_cycles = 1u;
    cpu->io.can_tx_start_cycle[0] = 1u;
    cpu->io.can_tx_phase_adjustment[0] = 0;
    dspic33_device_internal_can_encode_frame(&frame, 0u, cpu->io.can_tx_words[0]);
    expect(state, dspic33_can_pin(cpu, 64u, &high) && !high,
           "active CAN transmission exposes its first bus bit");
    cpu->device_cycles = (uint64_t)INT64_MAX;
    expect(state, dspic33_can_pin(cpu, 64u, &high) && high,
           "elapsed CAN transmission returns to the recessive bus state");
}

static void usb_cases(TestState* state, Dspic33* cpu) {
    uint8_t packet[8] = {0u};
    expect(state,
           !dspic33_usb_receive(cpu, DSPIC33_USB_ENDPOINT_COUNT, packet, 1u, 0u) &&
               !dspic33_usb_receive(cpu, 0u, packet, DSPIC33_USB_PACKET_SIZE + 1u, 0u) &&
               !dspic33_usb_receive(cpu, 0u, NULL, 1u, 0u) &&
               !dspic33_usb_setup(cpu, 0u, packet, 7u, 0u) &&
               !dspic33_usb_token(cpu, 0x80u, 0u, DSPIC33_USB_PID_OUT, NULL, 0u, false, 0u) &&
               !dspic33_usb_token(cpu, 0u, 0u, (Dspic33UsbPid)0u, NULL, 0u, false, 0u) &&
               !dspic33_usb_token(cpu, 0u, 0u, DSPIC33_USB_PID_SETUP, packet, 7u, false, 0u) &&
               !dspic33_usb_token(cpu, 0u, 0u, DSPIC33_USB_PID_IN, packet, 1u, false, 0u) &&
               !dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_ACK, NULL, 0u, false, 0u) &&
               !dspic33_usb_bus(cpu, (Dspic33UsbBusEvent)8u, 0u, 0u),
           "USB APIs reject invalid boundaries");
}

static void output_state_cases(TestState* state, Dspic33* cpu) {
    bool high;
    Dspic33DciTransfer transfer;

    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_input_capture_pin(cpu, 16u, true, 0u) &&
               dspic33_output_compare_fault_pin(cpu, 16u, true, 0u),
           "mapped peripheral pins accept external transitions");

    dspic33_reset(cpu, 0u);
    cpu->io.rtcc.pmd_disabled = true;
    expect(state, !dspic33_rtcc_output(cpu, &high), "PMD-disabled RTCC has no output");
    cpu->io.rtcc.pmd_disabled = false;
    expect(state, !dspic33_rtcc_output(cpu, &high), "disabled RTCC output is unavailable");
    dspic33_device_internal_raw_write_word(cpu, RTCC_CONTROL, RTCC_OUTPUT_ENABLE);
    expect(state, dspic33_rtcc_output(cpu, &high), "enabled RTCC exposes its alarm output");

    expect(state, !dspic33_dci_transmit(cpu, &transfer) && !dspic33_dci_transmit(cpu, NULL),
           "empty and null DCI output requests cannot pop");

    cpu->io.pwm_pmd_disabled = 1u;
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "PMD-disabled PWM has no sync output");
    cpu->io.pwm_pmd_disabled = 0u;
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "disabled PWM sync output is low");
    dspic33_device_internal_raw_write_word(cpu, PWM_GLOBAL_BASE, 0x0100u);
    cpu->io.pwm_sync_until[0] = 1u;
    expect(state, dspic33_pwm_sync_output(cpu, 0u), "active PWM sync pulse is high");
    dspic33_device_internal_raw_write_word(cpu, PWM_GLOBAL_BASE, 0x0300u);
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "inverted PWM sync pulse is low");

    dspic33_reset(cpu, 0u);
    dspic33_write_byte(cpu, 0x0680u, 0x2du);
    expect(state, !dspic33_pwm_sync_pin(cpu, 64u, &high),
           "disabled PWM time base has no mapped synchronization output");
    dspic33_write_byte(cpu, 0x0680u, 0x2cu);
    expect(state, !dspic33_pwm_sync_pin(cpu, 64u, &high),
           "PWM synchronization rejects a lower adjacent PPS function");
    dspic33_write_byte(cpu, 0x0680u, 0x2fu);
    expect(state, !dspic33_pwm_sync_pin(cpu, 64u, &high),
           "PWM synchronization rejects an upper adjacent PPS function");
    cpu->io.pwm_pmd_disabled = 1u;
    expect(state, !dspic33_pwm_sync_pin(cpu, 64u, &high), "PWM synchronization rejects global PMD");
    cpu->io.pwm_pmd_disabled = 0u;
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_internal_raw_write_word(cpu, PWM_GLOBAL_BASE, PWM_STOP_IDLE);
    expect(state, !dspic33_pwm_sync_pin(cpu, 64u, &high), "PWM synchronization stops in Idle");
    expect(state, !dspic33_pwm_sync_pin(cpu, 64u, NULL),
           "PWM synchronization rejects a null output pointer");
    dspic33_device_internal_raw_write_word(cpu, PWM_GLOBAL_BASE, 0u);
    expect(state, !dspic33_pwm_sync_pin(cpu, 64u, &high),
           "disabled PWM synchronization remains unavailable in Idle");
}

static void usb_state_cases(TestState* state, Dspic33* cpu) {
    uint8_t data = 0u;
    dspic33_reset(cpu, 0u);
    cpu->io.usb_host_pending = true;
    cpu->io.usb_host_endpoint = 0u;
    cpu->io.usb_host_pid = DSPIC33_USB_PID_IN;
    expect(state, dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_ACK, &data, 0u, false, 0u),
           "pending USB host token accepts a response");

    dspic33_reset(cpu, 0u);
    for (uint16_t slot = 0u; slot < DSPIC33_USB_PENDING_COUNT; slot++) {
        cpu->io.usb_pending[slot].active = true;
    }
    expect(state, !dspic33_usb_request(cpu, 0u, 0u), "full USB pending table rejects a request");

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    dspic33_reset(cpu, 0u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    const bool requested = dspic33_usb_request(cpu, 0u, 0u);
    const bool capture_pin = dspic33_input_capture_pin(cpu, 16u, true, 0u);
    const bool fault_pin = dspic33_output_compare_fault_pin(cpu, 16u, true, 0u);
    const bool can_pin = dspic33_can_input_pin(cpu, 16u, true, 0u);
    cpu->io.usb_host_pending = true;
    cpu->io.usb_host_endpoint = 0u;
    cpu->io.usb_host_pid = DSPIC33_USB_PID_IN;
    const bool host_response =
        dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_ACK, &data, 0u, false, 0u);
    test_reject_reallocation(false);
    expect(state,
           !requested && !capture_pin && !fault_pin && !can_pin && !host_response &&
               !cpu->io.usb_pending[0].active,
           "peripheral APIs reject event allocation failure");
#endif
}

int main(void) {
    TestState state = {0};
    Dspic33 cpu;
    const bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "cpu initialized");
    if (initialized) {
        timing_and_control_cases(&state, &cpu);
        conversion_and_pwm_cases(&state, &cpu);
        can_cases(&state, &cpu);
        can_output_cases(&state, &cpu);
        usb_cases(&state, &cpu);
        output_state_cases(&state, &cpu);
        usb_state_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
