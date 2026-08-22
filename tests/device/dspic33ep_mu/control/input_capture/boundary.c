#include "device/dspic33ep_mu/control/input_capture/internal.h"

bool dspic33_device_internal_input_capture_pair_configured(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_pps_physical_input_enabled(const Dspic33* cpu, uint8_t pin);
void dspic33_device_internal_run_input_capture(Dspic33* cpu, uint16_t source, uint32_t value);

void dspic33_input_capture_test_boundary_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state,
           !dspic33_device_internal_input_capture_pair_configured(cpu, DSPIC33_INPUT_CAPTURE_COUNT),
           "input capture rejects an invalid pair");
    expect(state, !dspic33_device_internal_pps_physical_input_enabled(cpu, UINT8_MAX),
           "input capture rejects an invalid physical pin");
    dspic33_device_internal_run_input_capture(cpu, DSPIC33_INPUT_CAPTURE_COUNT, 1u);
    expect(state, cpu->stop_reason == DSPIC33_RUNNING,
           "input capture ignores an invalid event source");
}
