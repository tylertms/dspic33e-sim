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

static uint64_t mix(uint64_t fingerprint, uint64_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static void configure_state(Dspic33* cpu, uint32_t scenario) {
    static const uint8_t widths[] = {1u, 8u, 16u, 12u};
    static const uint8_t slots[] = {1u, 2u, 4u, 16u};
    const uint8_t width = widths[(scenario >> 2u) & 3u];
    const uint8_t frame_slots = slots[(scenario >> 4u) & 3u];
    const uint8_t buffers = (uint8_t)(1u + ((scenario >> 6u) & 3u));
    uint16_t control = (uint16_t)((scenario >> 8u) & 3u);
    control |= (scenario & 1u) != 0u ? DCI_ENABLE : 0u;
    control |= (scenario & 2u) != 0u ? DCI_EXTERNAL_CLOCK : 0u;
    control |= (scenario & 4u) != 0u ? DCI_EXTERNAL_FRAME : 0u;
    control |= (scenario & 8u) != 0u ? DCI_TRISTATE : 0u;
    control |= (scenario & 16u) != 0u ? DCI_DATA_JUSTIFY : 0u;
    control |= (scenario & 32u) != 0u ? DCI_SAMPLE_RISING : 0u;
    control |= (scenario & 64u) != 0u ? DCI_LOOPBACK : 0u;
    control |= (scenario & 128u) != 0u ? DCI_UNDERFLOW_LAST : 0u;
    control |= (scenario & 256u) != 0u ? DCI_STOP_IDLE : 0u;

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, control);
    dspic33_device_internal_raw_write_word(
        cpu, DCI_CONTROL2, dspic33_dci_test_configuration(width, frame_slots, buffers));
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3,
                                           (uint16_t)(1u + ((scenario >> 5u) & 7u)));
    dspic33_device_internal_raw_write_word(cpu, DCI_TRANSMIT_SLOTS,
                                           (scenario & 1u) != 0u ? UINT16_MAX : 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_RECEIVE_SLOTS,
                                           (scenario & 2u) != 0u ? UINT16_MAX : 0u);

    cpu->power_state = (Dspic33PowerState)((scenario >> 9u) % 3u);
    cpu->io.dci.generation = 7u;
    cpu->io.dci.pmd_generation = 5u;
    cpu->io.dci.initialized = (scenario & 1u) != 0u;
    cpu->io.dci.started = (scenario & 2u) != 0u;
    cpu->io.dci.disable_pending = (scenario & 4u) != 0u;
    cpu->io.dci.internal_scheduled = (scenario & 8u) != 0u;
    cpu->io.dci.pps_frame_pending = (scenario & 16u) != 0u;
    cpu->io.dci.pps_input_configured = (scenario & 32u) != 0u;
    cpu->io.dci.pmd_disabled = (scenario & 64u) != 0u;
    cpu->io.dci.serial_delay = (scenario & 128u) != 0u;
    cpu->io.dci.output_frame_high = (scenario & 256u) != 0u;
    cpu->io.dci.serial_output_high = (scenario & 512u) != 0u;
    cpu->io.dci.serial_output_driven = (scenario & 1024u) != 0u;
    cpu->io.dci.buffer = (uint8_t)((scenario >> 3u) % buffers);
    cpu->io.dci.slot = (uint8_t)((scenario >> 5u) % frame_slots);
    cpu->io.dci.serial_bits = (uint8_t)((scenario >> 1u) % 17u);
    cpu->io.dci.serial_startup_bits = (uint8_t)((scenario >> 4u) & 3u);
    cpu->io.dci.disable_frames = (uint8_t)(1u + ((scenario >> 6u) & 3u));
    cpu->io.dci.receive_buffered = (uint8_t)(scenario & 15u);
    cpu->io.dci.transmit_buffered = (uint8_t)((scenario >> 2u) & 15u);
    cpu->io.dci.receive_unread = (uint8_t)((scenario >> 4u) & 15u);
    cpu->io.dci.transmit_written = (uint8_t)((scenario >> 6u) & 15u);
    for (uint8_t index = 0u; index < DSPIC33_DCI_BUFFER_COUNT; index++) {
        cpu->io.dci.transmit[index] = (uint16_t)(scenario * 257u + index);
        cpu->io.dci.last_transmit[index] = (uint16_t)~cpu->io.dci.transmit[index];
    }
}

void dspic33_dci_test_state_census_cases(TestState* state, Dspic33* cpu) {
    uint64_t fingerprint = UINT64_C(14695981039346656037);
    uint64_t driven = 0u;
    for (uint32_t scenario = 0u; scenario < 2048u; scenario++) {
        for (uint16_t source = 0u; source < 6u; source++) {
            bool high = false;
            dspic33_reset(cpu, 0u);
            configure_state(cpu, scenario);
            if ((scenario & 3u) == 0u) {
                dspic33_schedule(cpu, DSPIC33_EVENT_DCI, (uint16_t)(scenario % 2u),
                                 cpu->io.dci.generation, scenario & 31u);
            }
            driven += dspic33_device_internal_dci_data_output(cpu, &high);
            fingerprint = mix(fingerprint, high);
            fingerprint = mix(fingerprint, dspic33_device_internal_dci_frame_output(cpu, &high));
            fingerprint = mix(fingerprint, high);
            dspic33_device_internal_dci_update_power_state(cpu);
            dspic33_device_internal_run_dci(cpu, source,
                                            scenario % 3u == 0u ? 7u : (uint16_t)scenario);
            fingerprint = mix(fingerprint, source);
            fingerprint = mix(fingerprint, cpu->io.dci.initialized);
            fingerprint = mix(fingerprint, cpu->io.dci.started);
            fingerprint = mix(fingerprint, cpu->io.dci.serial_bits);
            fingerprint = mix(fingerprint, cpu->events.count);
            fingerprint = mix(fingerprint, cpu->stop_reason);
        }
    }
    expect(state, driven == 7680u && fingerprint == UINT64_C(10288385883272939741),
           "DCI state census matches");
}
