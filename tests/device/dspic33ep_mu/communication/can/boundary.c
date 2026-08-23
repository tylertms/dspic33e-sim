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
    uint32_t failure_count = 0u;

    for (uint32_t event_code = CAN_TEST_EVENT_RECEIVE_START;
         event_code <= CAN_TEST_EVENT_OVERLOAD_FINISH; event_code++) {
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
        dspic33_device_internal_run_can(cpu, 0u, event_code);
        test_reject_reallocation(false);
        failure_count += cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR;
        fingerprint = mix(fingerprint, event_code);
        fingerprint = mix(fingerprint, cpu->stop_reason);
        fingerprint = mix(fingerprint, cpu->io.can_rx_serial_active);
        fingerprint = mix(fingerprint, cpu->io.can_tx_on_bus);
        fingerprint = mix(fingerprint, (uint32_t)cpu->events.count);
    }
    expect(state, failure_count == 15u && fingerprint == UINT64_C(1351361066272320567),
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
    const Dspic33CanFrame input_frame = dspic33_can_test_frame(0x123u, false, false, 8u, 0x20u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_can_receive_error(cpu, 0u, &input_frame);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "CAN receive error allocation failure stops execution");

    dspic33_reset(cpu, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_device_internal_can_encode_frame(&input_frame, 0u, cpu->io.can_tx_words[0]);
    bool frame_bits[160];
    dspic33_device_internal_can_frame_bits(&input_frame, frame_bits);
    cpu->io.can_tx_on_bus = 1u;
    cpu->device_cycles = 20u * dspic33_device_internal_can_bit_cycles(cpu, 0u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_can_monitor_transmit_sample(cpu, 0u, !frame_bits[20]);
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
    dspic33_device_internal_can_receive_error(cpu, 0u, &input_frame);
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
    uint32_t valid_count = 0u;
    uint32_t incomplete_count = 0u;
    uint32_t invalid_count = 0u;
    const uint8_t payload_lengths[] = {0u, 1u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u};

    for (uint8_t is_extended = 0u; is_extended < 2u; is_extended++) {
        for (uint8_t is_remote = 0u; is_remote < 2u; is_remote++) {
            for (uint8_t length_index = 0u;
                 length_index < sizeof(payload_lengths) / sizeof(payload_lengths[0]);
                 length_index++) {
                Dspic33CanFrame input_frame = dspic33_can_test_frame(
                    is_extended != 0u ? 0x1234567u : 0x456u, is_extended != 0u, is_remote != 0u,
                    payload_lengths[length_index], 0x20u);
                Dspic33CanFrame decoded_frame;
                uint16_t tail_index = 0u;
                bool frame_bits[160];
                const uint16_t bit_count =
                    dspic33_device_internal_can_frame_bits(&input_frame, frame_bits);

                dspic33_reset(cpu, 0u);
                for (uint16_t bit_index = 0u; bit_index < bit_count; bit_index++) {
                    cpu->io.can_rx_serial_bits[0][bit_index] = frame_bits[bit_index];
                }
                cpu->io.can_rx_serial_count[0] = bit_count;
                int decode_status =
                    dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded_frame, &tail_index);
                valid_count += decode_status == CAN_TEST_SERIAL_VALID;
                expect(state,
                       decode_status != CAN_TEST_SERIAL_VALID ||
                           decoded_frame.length == (payload_lengths[length_index] > 8u
                                                        ? 8u
                                                        : payload_lengths[length_index]),
                       "CAN serial decoder applies classical DLC length");
                cpu->io.can_rx_serial_count[0] = (uint16_t)(tail_index + 5u);
                incomplete_count +=
                    dspic33_device_internal_can_decode_serial(
                        cpu, 0u, &decoded_frame, &tail_index) == CAN_TEST_SERIAL_INCOMPLETE;
                cpu->io.can_rx_serial_count[0] = bit_count;
                cpu->io.can_rx_serial_bits[0][tail_index] = false;
                invalid_count += dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded_frame,
                                                                           &tail_index) ==
                                 CAN_TEST_SERIAL_INVALID;
            }
        }
    }
    expect(state, valid_count == 40u && incomplete_count == 40u && invalid_count == 40u,
           "CAN serial decode matrix matches");
}

static uint16_t load_serial_vector(Dspic33* cpu, const char* vector) {
    const uint16_t count = (uint16_t)strlen(vector);
    for (uint16_t index = 0u; index < count; index++) {
        cpu->io.can_rx_serial_bits[0][index] = vector[index] == '1';
    }
    cpu->io.can_rx_serial_count[0] = count;
    return count;
}

static bool encoded_vector_matches(const Dspic33CanFrame* frame, const char* vector) {
    bool bits[160];
    const uint16_t count = dspic33_device_internal_can_frame_bits(frame, bits);
    if (count != strlen(vector)) {
        return false;
    }
    for (uint16_t index = 0u; index < count; index++) {
        if (bits[index] != (vector[index] == '1')) {
            return false;
        }
    }
    return true;
}

static void independent_serial_vector_cases(TestState* state, Dspic33* cpu) {
    static const char standard_vector[] =
        "000100100011000100100010001001000100011001101000100010101010110011001110111100010001"
        "101001011010011111111111111";
    static const char extended_vector[] =
        "000001100100011110100010101100111000111110101111010101101101111100111011110000010010"
        "010001101000101011001111011000010001011111111111111";
    static const char dominant_srr_vector[] =
        "000100100011010001000101011001110000011100010010001101000101011010111101000101011111"
        "11111111";
    const Dspic33CanFrame standard = {
        0x123u, {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u}, 9u, false, false};
    const Dspic33CanFrame extended = {
        0x01234567u, {0xdeu, 0xadu, 0xbeu, 0xefu, 0x01u, 0x23u, 0x45u, 0x67u}, 15u, true, false};
    Dspic33CanFrame decoded;
    uint16_t tail;

    expect(state, encoded_vector_matches(&standard, standard_vector),
           "standard CAN encoder matches the independent wire vector");
    expect(state, encoded_vector_matches(&extended, extended_vector),
           "extended CAN encoder matches the independent wire vector");

    dspic33_reset(cpu, 0u);
    load_serial_vector(cpu, standard_vector);
    expect(state,
           dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded, &tail) ==
                   CAN_TEST_SERIAL_VALID &&
               tail == 98u && decoded.identifier == standard.identifier && !decoded.extended &&
               !decoded.remote && decoded.length == 8u &&
               memcmp(decoded.data, standard.data, sizeof(decoded.data)) == 0,
           "standard independent CAN vector decodes with raw DLC nine");

    dspic33_reset(cpu, 0u);
    load_serial_vector(cpu, standard_vector);
    cpu->io.can_rx_serial_bits[0][107u] = false;
    expect(state,
           dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded, &tail) ==
                   CAN_TEST_SERIAL_VALID &&
               tail == 98u && decoded.identifier == standard.identifier,
           "dominant final EOF bit preserves a valid CAN frame");

    dspic33_reset(cpu, 0u);
    load_serial_vector(cpu, extended_vector);
    expect(state,
           dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded, &tail) ==
                   CAN_TEST_SERIAL_VALID &&
               tail == 122u && decoded.identifier == extended.identifier && decoded.extended &&
               !decoded.remote && decoded.length == 8u &&
               memcmp(decoded.data, extended.data, sizeof(decoded.data)) == 0,
           "extended independent CAN vector decodes with raw DLC fifteen");

    dspic33_reset(cpu, 0u);
    load_serial_vector(cpu, dominant_srr_vector);
    expect(state,
           dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded, &tail) ==
                   CAN_TEST_SERIAL_VALID &&
               tail == 79u && decoded.identifier == 0x048c4567u && decoded.extended &&
               !decoded.remote && decoded.length == 3u && decoded.data[0] == 0x12u &&
               decoded.data[1] == 0x34u && decoded.data[2] == 0x56u,
           "extended CAN vector with dominant SRR decodes");

    const uint16_t invalid_indices[] = {5u, 13u, 14u, 110u, 122u, 124u, 125u};
    for (size_t index = 0u; index < sizeof(invalid_indices) / sizeof(invalid_indices[0]); index++) {
        dspic33_reset(cpu, 0u);
        load_serial_vector(cpu, extended_vector);
        cpu->io.can_rx_serial_bits[0][invalid_indices[index]] =
            !cpu->io.can_rx_serial_bits[0][invalid_indices[index]];
        expect(state,
               dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded, &tail) ==
                   CAN_TEST_SERIAL_INVALID,
               "corrupted independent CAN vector is rejected");
    }
}

static void independent_remote_serial_vector_cases(TestState* state, Dspic33* cpu) {
    static const char standard_vector[] = "010001010110100100001111100101100011111111111111";
    static const char extended_vector[] =
        "00000110010001111010001010110011110001011000101100110011111111111111";
    static const uint8_t empty_data[8] = {0};
    const Dspic33CanFrame standard = {
        0x456u, {0xdeu, 0xadu, 0xbeu, 0xefu, 0x01u, 0x23u, 0x45u, 0x67u}, 8u, false, true};
    const Dspic33CanFrame extended = {
        0x01234567u, {0x89u, 0xabu, 0xcdu, 0xefu, 0x10u, 0x32u, 0x54u, 0x76u}, 5u, true, true};
    const Dspic33CanFrame frames[] = {standard, extended};
    const char* vectors[] = {standard_vector, extended_vector};
    const uint16_t tails[] = {35u, 55u};
    const uint16_t rtr_indices[] = {12u, 33u};
    const uint16_t crc_indices[] = {27u, 47u};

    for (size_t index = 0u; index < sizeof(frames) / sizeof(frames[0]); index++) {
        Dspic33CanFrame decoded;
        uint16_t tail = 0u;
        expect(state, encoded_vector_matches(&frames[index], vectors[index]),
               "remote CAN encoder matches the independent wire vector");
        dspic33_reset(cpu, 0u);
        load_serial_vector(cpu, vectors[index]);
        expect(state,
               dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded, &tail) ==
                       CAN_TEST_SERIAL_VALID &&
                   tail == tails[index] && decoded.identifier == frames[index].identifier &&
                   decoded.extended == frames[index].extended && decoded.remote &&
                   decoded.length == frames[index].length &&
                   memcmp(decoded.data, empty_data, sizeof(decoded.data)) == 0,
               "independent remote CAN vector decodes without payload data");

        const uint16_t corruptions[] = {rtr_indices[index], crc_indices[index]};
        for (size_t corruption = 0u; corruption < sizeof(corruptions) / sizeof(corruptions[0]);
             corruption++) {
            dspic33_reset(cpu, 0u);
            load_serial_vector(cpu, vectors[index]);
            cpu->io.can_rx_serial_bits[0][corruptions[corruption]] =
                !cpu->io.can_rx_serial_bits[0][corruptions[corruption]];
            expect(state,
                   dspic33_device_internal_can_decode_serial(cpu, 0u, &decoded, &tail) !=
                       CAN_TEST_SERIAL_VALID,
                   "corrupted independent remote CAN vector is rejected");
        }
    }
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
    independent_serial_vector_cases(state, cpu);
    independent_remote_serial_vector_cases(state, cpu);
    controller_admission_matrix_cases(state, cpu);
#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    allocation_failure_cases(state, cpu);
#else
    (void)state;
    (void)cpu;
#endif
}
