#include "device/dspic33ep_mu/communication/dci/internal.h"

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize DCI processor");
    if (initialized) {
        dspic33_dci_test_access_cases(&state, &cpu);
        dspic33_dci_test_boundary_cases(&state, &cpu);
        dspic33_dci_test_state_census_cases(&state, &cpu);
        dspic33_dci_test_width_and_lane_cases(&state, &cpu);
        dspic33_dci_test_slot_buffer_status_cases(&state, &cpu);
        dspic33_dci_test_admission_and_clock_cases(&state, &cpu);
        dspic33_dci_test_protocol_geometry_cases(&state, &cpu);
        dspic33_dci_test_protocol_frame_cases(&state, &cpu);
        dspic33_dci_test_ac_link_cases(&state, &cpu);
        dspic33_dci_test_protocol_integration_cases(&state, &cpu);
        dspic33_dci_test_pps_serial_input_cases(&state, &cpu);
        dspic33_dci_test_pps_frame_cases(&state, &cpu);
        dspic33_dci_test_pps_serial_matrix_cases(&state, &cpu);
        dspic33_dci_test_pps_startup_cases(&state, &cpu);
        dspic33_dci_test_pps_internal_input_cases(&state, &cpu);
        dspic33_dci_test_pps_internal_frame_cases(&state, &cpu);
        dspic33_dci_test_pps_external_frame_output_cases(&state, &cpu);
        dspic33_dci_test_pps_internal_frame_output_cases(&state, &cpu);
        dspic33_dci_test_pps_bcg_cases(&state, &cpu);
        dspic33_dci_test_pps_internal_sample_lifecycle_cases(&state, &cpu);
        dspic33_dci_test_pps_selection_cases(&state, &cpu);
        dspic33_dci_test_pps_qualification_cases(&state, &cpu);
        dspic33_dci_test_pps_output_cases(&state, &cpu);
        dspic33_dci_test_pps_disable_timing_cases(&state, &cpu);
        dspic33_dci_test_pps_lifecycle_cases(&state, &cpu);
        dspic33_dci_test_mode_and_status_cases(&state, &cpu);
        dspic33_dci_test_interrupt_dma_cases(&state, &cpu);
        dspic33_dci_test_generation_and_frame_cases(&state, &cpu);
        dspic33_dci_test_disable_timing_cases(&state, &cpu);
        dspic33_dci_test_internal_clock_lifecycle_cases(&state, &cpu);
        dspic33_dci_test_lifecycle_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
