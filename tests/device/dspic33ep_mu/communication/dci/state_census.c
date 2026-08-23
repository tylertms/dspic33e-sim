#include "device/dspic33ep_mu/communication/dci/internal.h"

void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
bool dspic33_device_internal_dci_data_output(const Dspic33* cpu, bool* high);
bool dspic33_device_internal_dci_frame_output(const Dspic33* cpu, bool* high);
void dspic33_device_internal_dci_update_power_state(Dspic33* cpu);
void dspic33_device_internal_run_dci(Dspic33* cpu, uint16_t source, uint32_t value);

enum {
    DCI_CENSUS_EVENT_START,
    DCI_CENSUS_EVENT_INTERNAL,
    DCI_CENSUS_EVENT_EXTERNAL,
    DCI_CENSUS_EVENT_EXTERNAL_FRAME,
    DCI_CENSUS_EVENT_SAMPLE,
    DCI_CENSUS_EVENT_FRAME_START
};

static uint64_t mix_fingerprint(uint64_t fingerprint, uint64_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static void configure_state(Dspic33* cpu, uint32_t scenario_id) {
    static const uint8_t serial_widths[] = {1u, 8u, 16u, 12u};
    static const uint8_t frame_slot_counts[] = {1u, 2u, 4u, 16u};
    const uint8_t serial_width = serial_widths[(scenario_id >> 2u) & 3u];
    const uint8_t frame_slot_count = frame_slot_counts[(scenario_id >> 4u) & 3u];
    const uint8_t buffer_count = (uint8_t)(1u + ((scenario_id >> 6u) & 3u));
    uint16_t control_word = (uint16_t)((scenario_id >> 8u) & 3u);
    control_word |= (scenario_id & 1u) != 0u ? DCI_ENABLE : 0u;
    control_word |= (scenario_id & 2u) != 0u ? DCI_EXTERNAL_CLOCK : 0u;
    control_word |= (scenario_id & 4u) != 0u ? DCI_EXTERNAL_FRAME : 0u;
    control_word |= (scenario_id & 8u) != 0u ? DCI_TRISTATE : 0u;
    control_word |= (scenario_id & 16u) != 0u ? DCI_DATA_JUSTIFY : 0u;
    control_word |= (scenario_id & 32u) != 0u ? DCI_SAMPLE_RISING : 0u;
    control_word |= (scenario_id & 64u) != 0u ? DCI_LOOPBACK : 0u;
    control_word |= (scenario_id & 128u) != 0u ? DCI_UNDERFLOW_LAST : 0u;
    control_word |= (scenario_id & 256u) != 0u ? DCI_STOP_IDLE : 0u;

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, control_word);
    dspic33_device_internal_raw_write_word(
        cpu, DCI_CONTROL2,
        dspic33_dci_test_configuration(serial_width, frame_slot_count, buffer_count));
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3,
                                           (uint16_t)(1u + ((scenario_id >> 5u) & 7u)));
    dspic33_device_internal_raw_write_word(cpu, DCI_TRANSMIT_SLOTS,
                                           (scenario_id & 1u) != 0u ? UINT16_MAX : 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_RECEIVE_SLOTS,
                                           (scenario_id & 2u) != 0u ? UINT16_MAX : 0u);

    cpu->power_state = (Dspic33PowerState)((scenario_id >> 9u) % 3u);
    cpu->io.dci.generation = 7u;
    cpu->io.dci.pmd_generation = 5u;
    cpu->io.dci.initialized = (scenario_id & 1u) != 0u;
    cpu->io.dci.started = (scenario_id & 2u) != 0u;
    cpu->io.dci.disable_pending = (scenario_id & 4u) != 0u;
    cpu->io.dci.internal_scheduled = (scenario_id & 8u) != 0u;
    cpu->io.dci.pps_frame_pending = (scenario_id & 16u) != 0u;
    cpu->io.dci.pps_input_configured = (scenario_id & 32u) != 0u;
    cpu->io.dci.pmd_disabled = (scenario_id & 64u) != 0u;
    cpu->io.dci.serial_delay = (scenario_id & 128u) != 0u;
    cpu->io.dci.output_frame_high = (scenario_id & 256u) != 0u;
    cpu->io.dci.serial_output_high = (scenario_id & 512u) != 0u;
    cpu->io.dci.serial_output_driven = (scenario_id & 1024u) != 0u;
    cpu->io.dci.buffer = (uint8_t)((scenario_id >> 3u) % buffer_count);
    cpu->io.dci.slot = (uint8_t)((scenario_id >> 5u) % frame_slot_count);
    cpu->io.dci.serial_bits = (uint8_t)((scenario_id >> 1u) % 17u);
    cpu->io.dci.serial_startup_bits = (uint8_t)((scenario_id >> 4u) & 3u);
    cpu->io.dci.disable_frames = (uint8_t)(1u + ((scenario_id >> 6u) & 3u));
    cpu->io.dci.receive_buffered = (uint8_t)(scenario_id & 15u);
    cpu->io.dci.transmit_buffered = (uint8_t)((scenario_id >> 2u) & 15u);
    cpu->io.dci.receive_unread = (uint8_t)((scenario_id >> 4u) & 15u);
    cpu->io.dci.transmit_written = (uint8_t)((scenario_id >> 6u) & 15u);
    for (uint8_t buffer_index = 0u; buffer_index < DSPIC33_DCI_BUFFER_COUNT; buffer_index++) {
        cpu->io.dci.transmit[buffer_index] = (uint16_t)(scenario_id * 257u + buffer_index);
        cpu->io.dci.last_transmit[buffer_index] = (uint16_t)~cpu->io.dci.transmit[buffer_index];
    }
}

void dspic33_dci_test_state_census_cases(TestState* state, Dspic33* cpu) {
    uint64_t fingerprint = UINT64_C(14695981039346656037);
    uint64_t driven_output_count = 0u;

    for (uint32_t scenario_id = 0u; scenario_id < 2048u; scenario_id++) {
        for (uint16_t event_source = 0u; event_source < 6u; event_source++) {
            bool is_high = false;
            dspic33_reset(cpu, 0u);
            configure_state(cpu, scenario_id);
            if ((scenario_id & 3u) == 0u) {
                dspic33_schedule(cpu, DSPIC33_EVENT_DCI, (uint16_t)(scenario_id % 2u),
                                 cpu->io.dci.generation, scenario_id & 31u);
            }
            driven_output_count += dspic33_device_internal_dci_data_output(cpu, &is_high);
            fingerprint = mix_fingerprint(fingerprint, is_high);
            fingerprint = mix_fingerprint(fingerprint,
                                          dspic33_device_internal_dci_frame_output(cpu, &is_high));
            fingerprint = mix_fingerprint(fingerprint, is_high);
            dspic33_device_internal_dci_update_power_state(cpu);
            dspic33_device_internal_run_dci(cpu, event_source,
                                            scenario_id % 3u == 0u ? 7u : (uint16_t)scenario_id);
            fingerprint = mix_fingerprint(fingerprint, event_source);
            fingerprint = mix_fingerprint(fingerprint, cpu->io.dci.initialized);
            fingerprint = mix_fingerprint(fingerprint, cpu->io.dci.started);
            fingerprint = mix_fingerprint(fingerprint, cpu->io.dci.serial_bits);
            fingerprint = mix_fingerprint(fingerprint, cpu->events.count);
            fingerprint = mix_fingerprint(fingerprint, cpu->stop_reason);
        }
    }
    expect(state, driven_output_count == 7680u && fingerprint == UINT64_C(10288385883272939741),
           "DCI state census matches");
}
