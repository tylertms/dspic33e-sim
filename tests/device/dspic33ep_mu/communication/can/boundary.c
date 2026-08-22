#include <string.h>

#include "allocation_failure.h"
#include "device/dspic33ep_mu/communication/can/internal.h"

enum { CAN_TEST_SERIAL_INCOMPLETE, CAN_TEST_SERIAL_VALID, CAN_TEST_SERIAL_INVALID };

enum {
    CAN_TEST_EVENT_RECEIVE_START = 1u,
    CAN_TEST_EVENT_RECEIVE_WORD,
    CAN_TEST_EVENT_RECEIVE_FINISH,
    CAN_TEST_EVENT_TRANSMIT_START,
    CAN_TEST_EVENT_TRANSMIT_WORD,
    CAN_TEST_EVENT_TRANSMIT_FINISH,
    CAN_TEST_EVENT_ERROR,
    CAN_TEST_EVENT_TRANSMIT_BUS_FINISH,
    CAN_TEST_EVENT_CAPTURE_RELEASE,
    CAN_TEST_EVENT_INVALID,
    CAN_TEST_EVENT_RECEIVE_PIN,
    CAN_TEST_EVENT_RECEIVE_SAMPLE,
    CAN_TEST_EVENT_ACK_START,
    CAN_TEST_EVENT_ACK_FINISH,
    CAN_TEST_EVENT_TRANSMIT_RETRY,
    CAN_TEST_EVENT_TRANSMIT_ERROR_START,
    CAN_TEST_EVENT_TRANSMIT_SAMPLE,
    CAN_TEST_EVENT_RECEIVE_ERROR_START,
    CAN_TEST_EVENT_RECEIVE_ERROR_FINISH,
    CAN_TEST_EVENT_MODE_TRANSITION,
    CAN_TEST_EVENT_RECEIVE_SUCCESS,
    CAN_TEST_EVENT_RECEIVE_SAMPLE_FIRST,
    CAN_TEST_EVENT_RECEIVE_SAMPLE_SECOND,
    CAN_TEST_EVENT_TRANSMIT_SAMPLE_FIRST,
    CAN_TEST_EVENT_TRANSMIT_SAMPLE_SECOND,
    CAN_TEST_EVENT_INTERMISSION_FINISH,
    CAN_TEST_EVENT_OVERLOAD_FINISH
};

void dspic33_device_internal_run_can(Dspic33* cpu, uint8_t channel, uint32_t value);
void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_device_internal_can_capture_received_frame(Dspic33* cpu, uint8_t channel);
uint64_t dspic33_device_internal_can_bit_cycles(const Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_can_encode_frame(const Dspic33CanFrame* frame, uint8_t filter,
                                              uint16_t words[8]);
Dspic33CanFrame dspic33_device_internal_can_decode_frame(const uint16_t words[8]);
uint16_t dspic33_device_internal_can_frame_bits(const Dspic33CanFrame* frame, bool bits[160]);
void dspic33_device_internal_can_monitor_transmit_sample(Dspic33* cpu, uint8_t channel,
                                                         bool bus_high);
void dspic33_device_internal_can_receive_error(Dspic33* cpu, uint8_t channel,
                                               const Dspic33CanFrame* frame);
void dspic33_device_internal_can_update_vector(Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_can_select_receive_buffer(Dspic33* cpu, uint8_t channel,
                                                       const Dspic33CanFrame* frame,
                                                       uint8_t* buffer, uint8_t* matched_filter);
void dspic33_device_internal_can_set_buffer_control(Dspic33* cpu, uint8_t channel, uint8_t buffer,
                                                    uint16_t value);
bool dspic33_device_internal_can_dma_ready(const Dspic33* cpu, uint8_t request, uint16_t pad,
                                           bool transmit);
bool dspic33_device_internal_can_power_enabled(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_can_serial_receive_enabled(const Dspic33* cpu, uint8_t channel);
int dspic33_device_internal_can_decode_serial(const Dspic33* cpu, uint8_t channel,
                                              Dspic33CanFrame* frame, uint16_t* tail_start);

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill CAN allocation failure event queue");
    }
}

static uint64_t mix(uint64_t fingerprint, uint32_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static void allocation_failure_cases(TestState* state, Dspic33* cpu) {
    uint64_t fingerprint = UINT64_C(14695981039346656037);
    uint32_t failures = 0u;
    for (uint32_t event = CAN_TEST_EVENT_RECEIVE_START; event <= CAN_TEST_EVENT_OVERLOAD_FINISH;
         event++) {
        dspic33_reset(cpu, 0u);
        dspic33_can_test_set_mode(cpu, 0u, 0u);
        fill_event_queue(state, cpu);
        cpu->io.can_rx_serial_active = 1u;
        cpu->io.can_tx_busy = 1u;
        cpu->io.can_tx_on_bus = 1u;
        cpu->io.can_tx_retry_wait = 1u;
        cpu->io.can_tx_error_active = 1u;
        cpu->io.can_rx_error_active = 1u;
        cpu->io.can_intermission_active = 1u;
        cpu->io.can_overload_active = 1u;
        cpu->io.can_rx_pin_high = 1u;
        cpu->io.can_rx_serial_count[0] = 80u;
        memset(cpu->io.can_rx_serial_bits[0], 1, sizeof(cpu->io.can_rx_serial_bits[0]));
        cpu->io.can_rx[0].count = 1u;
        cpu->io.can_rx[0].frames[0] = dspic33_can_test_frame(0x123u, false, false, 8u, 0x40u);
        cpu->io.can_tx[0].count = 1u;
        cpu->io.can_tx[0].frames[0] = dspic33_can_test_frame(0x321u, false, false, 8u, 0x60u);
        cpu->stop_reason = DSPIC33_RUNNING;
        test_reject_reallocation(true);
        dspic33_device_internal_run_can(cpu, 0u, event);
        test_reject_reallocation(false);
        failures += cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR;
        fingerprint = mix(fingerprint, event);
        fingerprint = mix(fingerprint, cpu->stop_reason);
        fingerprint = mix(fingerprint, cpu->io.can_rx_serial_active);
        fingerprint = mix(fingerprint, cpu->io.can_tx_on_bus);
        fingerprint = mix(fingerprint, (uint32_t)cpu->events.count);
    }
    expect(state, failures == 15u && fingerprint == UINT64_C(1351361066272320567),
           "CAN allocation failure census matches");

    dspic33_reset(cpu, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_device_internal_raw_write_word(cpu, bases[0], 0x0008u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_can_capture_received_frame(cpu, 0u);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "CAN capture allocation failure stops execution");

    dspic33_reset(cpu, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    const Dspic33CanFrame frame = dspic33_can_test_frame(0x123u, false, false, 8u, 0x20u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_can_receive_error(cpu, 0u, &frame);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "CAN receive error allocation failure stops execution");

    dspic33_reset(cpu, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_device_internal_can_encode_frame(&frame, 0u, cpu->io.can_tx_words[0]);
    bool bits[160];
    dspic33_device_internal_can_frame_bits(&frame, bits);
    cpu->io.can_tx_on_bus = 1u;
    cpu->device_cycles = 20u * dspic33_device_internal_can_bit_cycles(cpu, 0u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_can_monitor_transmit_sample(cpu, 0u, !bits[20]);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "CAN transmit error allocation failure stops execution");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, bases[0], 4u << 5u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_can(cpu, 0u, CAN_TEST_EVENT_MODE_TRANSITION);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "blocked CAN mode transition allocation failure stops execution");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, bases[0], 4u << 5u);
    cpu->io.can_rx_pin_high = 1u;
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_can(cpu, 0u, CAN_TEST_EVENT_MODE_TRANSITION);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "completed CAN mode transition allocation failure stops execution");

    dspic33_reset(cpu, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 7u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_can_receive_error(cpu, 0u, &frame);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "listen-all CAN receive allocation failure stops execution");
}
#endif

static void event_guard_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    cpu->io.can_rx_serial_active = 1u;
    cpu->io.can_rx_serial_count[0] = sizeof(cpu->io.can_rx_serial_bits[0]);
    dspic33_device_internal_run_can(cpu, 0u, CAN_TEST_EVENT_RECEIVE_SAMPLE);
    expect(state, cpu->io.can_rx_serial_active == 0u, "oversized CAN serial frame is rejected");
    dspic33_device_internal_run_can(cpu, 0u, CAN_TEST_EVENT_RECEIVE_SAMPLE_FIRST);
    dspic33_device_internal_run_can(cpu, 0u, CAN_TEST_EVENT_RECEIVE_SAMPLE_SECOND);
    dspic33_device_internal_run_can(cpu, 0u, CAN_TEST_EVENT_TRANSMIT_SAMPLE_SECOND);
    dspic33_device_internal_run_can(cpu, 0u, CAN_TEST_EVENT_TRANSMIT_RETRY);
    dspic33_device_internal_run_can(cpu, DSPIC33_CAN_COUNT, CAN_TEST_EVENT_RECEIVE_START);
    expect(state, cpu->stop_reason != DSPIC33_EVENT_QUEUE_ERROR,
           "inactive and out-of-range CAN events are ignored");

    dspic33_can_test_set_mode(cpu, 0u, 3u);
    dspic33_device_internal_run_can(cpu, 0u, CAN_TEST_EVENT_ACK_START);
    expect(state, cpu->io.can_rx_ack == 0u, "listen-only CAN does not acknowledge");

    cpu->io.can_intermission_generation[0] = 0u;
    dspic33_device_internal_run_can(cpu, 0u, CAN_TEST_EVENT_OVERLOAD_FINISH | UINT32_C(1) << 16u);
    expect(state, cpu->io.can_overload_active == 0u, "stale CAN overload event is ignored");
}

static void controller_boundary_cases(TestState* state, Dspic33* cpu) {
    const uint16_t status_address = (uint16_t)(bases[0] + 0x0au);
    const uint16_t enable_address = (uint16_t)(bases[0] + 0x0cu);
    const uint16_t vector_address = (uint16_t)(bases[0] + 4u);

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, status_address, CAN_INTERRUPT_ERROR);
    dspic33_device_internal_raw_write_word(cpu, enable_address, CAN_INTERRUPT_ERROR);
    dspic33_device_internal_can_update_vector(cpu, 0u);
    expect(state, (dspic33_read_word(cpu, vector_address) & 0xffu) == 0x41u,
           "CAN error interrupt selects the error vector");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, status_address, 0x0004u);
    dspic33_device_internal_raw_write_word(cpu, enable_address, 0x0004u);
    dspic33_device_internal_can_update_vector(cpu, 0u);
    expect(state, (dspic33_read_word(cpu, vector_address) & 0xffu) == 0x43u,
           "CAN overflow interrupt selects the overflow vector");

    uint16_t words[8] = {0u};
    words[2] = 0x000fu;
    Dspic33CanFrame decoded = dspic33_device_internal_can_decode_frame(words);
    expect(state, decoded.length == 8u, "CAN decoder clamps invalid payload lengths");

    dspic33_reset(cpu, 0u);
    const Dspic33CanFrame frame = dspic33_can_test_frame(0x123u, false, false, 1u, 0x20u);
    dspic33_device_internal_can_encode_frame(&frame, 0u, cpu->io.can_tx_words[0]);
    bool bits[160];
    uint16_t count = dspic33_device_internal_can_frame_bits(&frame, bits);
    cpu->io.can_tx_on_bus = 1u;
    cpu->io.can_tx_phase_adjustment[0] = 1;
    dspic33_device_internal_can_monitor_transmit_sample(cpu, 0u, bits[0]);
    expect(state, cpu->io.can_tx_on_bus == 1u,
           "CAN transmit sampling clamps negative elapsed time");

    cpu->io.can_tx_phase_adjustment[0] = 0;
    cpu->device_cycles = (uint64_t)count * dspic33_device_internal_can_bit_cycles(cpu, 0u);
    dspic33_device_internal_can_monitor_transmit_sample(cpu, 0u, false);
    expect(state, cpu->io.can_tx_on_bus == 1u, "CAN transmit sampling ignores elapsed frames");

    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, 0u, 0x8000u, 0u, 0u);
    dspic33_can_test_configure_filter(cpu, 0u, 0u, 0x456u, false, 0x7ffu, true, 0u, 0u);
    dspic33_can_test_enable_filter(cpu, 0u, 1u);
    dspic33_device_internal_can_set_buffer_control(cpu, 0u, 0u, 0x008cu);
    Dspic33CanFrame remote = dspic33_can_test_frame(0x456u, false, true, 0u, 0u);
    uint8_t buffer = UINT8_MAX;
    uint8_t filter = UINT8_MAX;
    expect(state,
           !dspic33_device_internal_can_select_receive_buffer(cpu, 0u, &remote, &buffer, &filter) &&
               cpu->events.count != 0u,
           "remote frame requests transmission from a configured response buffer");
}

static void serial_decode_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t valid = 0u;
    uint32_t incomplete = 0u;
    uint32_t invalid = 0u;
    const uint8_t lengths[] = {0u, 1u, 8u, 9u};
    for (uint8_t extended = 0u; extended < 2u; extended++) {
        for (uint8_t remote = 0u; remote < 2u; remote++) {
            for (uint8_t length_index = 0u; length_index < sizeof(lengths) / sizeof(lengths[0]);
                 length_index++) {
                Dspic33CanFrame frame =
                    dspic33_can_test_frame(extended != 0u ? 0x1234567u : 0x456u, extended != 0u,
                                           remote != 0u, lengths[length_index], 0x20u);
                Dspic33CanFrame decoded;
                uint16_t tail = 0u;
                bool bits[160];
                const uint16_t count = dspic33_device_internal_can_frame_bits(&frame, bits);
                dspic33_reset(cpu, 0u);
                for (uint16_t index = 0u; index < count; index++) {
                    cpu->io.can_rx_serial_bits[0][index] = bits[index];
                }
                cpu->io.can_rx_serial_count[0] = count;
                valid += dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded, &tail) ==
                         CAN_TEST_SERIAL_VALID;
                cpu->io.can_rx_serial_count[0] = (uint16_t)(tail + 5u);
                incomplete += dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded, &tail) ==
                              CAN_TEST_SERIAL_INCOMPLETE;
                cpu->io.can_rx_serial_count[0] = count;
                cpu->io.can_rx_serial_bits[0][tail] = false;
                invalid += dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded, &tail) ==
                           CAN_TEST_SERIAL_INVALID;
            }
        }
    }
    expect(state, valid == 16u && incomplete == 16u && invalid == 16u,
           "CAN serial decode matrix matches");
}

static void controller_admission_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t controls[] = {0u,      0x8000u, 0x8000u, 0xc020u,
                                        0x8010u, 0x8020u, 0x8020u, 0xa020u};
    static const uint8_t requests[] = {7u, 6u, 7u, 7u, 7u, 7u, 7u, 7u};
    static const uint16_t pads[] = {0x0440u, 0x0440u, 0x0442u, 0x0440u,
                                    0x0440u, 0x0440u, 0x0440u, 0x0440u};
    static const bool transmit[] = {false, false, false, false, false, true, false, true};
    static const bool expected[] = {false, false, false, false, false, false, true, true};
    for (size_t index = 0u; index < sizeof(controls) / sizeof(controls[0]); index++) {
        dspic33_reset(cpu, 0u);
        dspic33_device_internal_raw_write_word(cpu, 0x0b00u, controls[index]);
        dspic33_device_internal_raw_write_word(cpu, 0x0b02u, requests[index]);
        dspic33_device_internal_raw_write_word(cpu, 0x0b0cu, pads[index]);
        expect(state,
               dspic33_device_internal_can_dma_ready(cpu, 7u, 0x0440u, transmit[index]) ==
                   expected[index],
               "CAN DMA admission matrix matches");
    }

    dspic33_reset(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state, dspic33_device_internal_can_power_enabled(cpu, 0u),
           "active CAN controller is powered");
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state, dspic33_device_internal_can_power_enabled(cpu, 0u),
           "idle CAN controller continues without stop-idle");
    dspic33_device_internal_raw_write_word(cpu, bases[0], 0x2000u);
    expect(state, !dspic33_device_internal_can_power_enabled(cpu, 0u),
           "stop-idle suspends an idle CAN controller");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state, !dspic33_device_internal_can_serial_receive_enabled(cpu, 0u),
           "sleep disables serial CAN reception");
}

void dspic33_can_test_boundary_groups(TestState* state, Dspic33* cpu) {
    event_guard_cases(state, cpu);
    controller_boundary_cases(state, cpu);
    serial_decode_matrix_cases(state, cpu);
    controller_admission_matrix_cases(state, cpu);
#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    allocation_failure_cases(state, cpu);
#else
    (void)state;
    (void)cpu;
#endif
}
