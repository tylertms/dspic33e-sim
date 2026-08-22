#include "device/dspic33ep_mu/control/output_compare_sync/internal.h"

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "output-compare sync test initializes");
    if (initialized) {
        dspic33_output_compare_sync_test_alternate_clock_cases(&state, &cpu);
        dspic33_output_compare_sync_test_trigger_source_cases(&state, &cpu);
        dspic33_output_compare_sync_test_trigger_one_shot_cases(&state, &cpu);
        dspic33_output_compare_sync_test_synchronization_source_cases(&state, &cpu);
        dspic33_output_compare_sync_test_input_capture_synchronization_cases(&state, &cpu);
        dspic33_output_compare_sync_test_input_capture_cascade_synchronization_cases(&state, &cpu);
        dspic33_output_compare_sync_test_input_capture_synchronization_lifecycle_cases(&state,
                                                                                       &cpu);
        dspic33_output_compare_sync_test_alternate_clock_batch_cases(&state, &cpu);
        dspic33_output_compare_sync_test_timer_clock_synchronization_cases(&state, &cpu);
        dspic33_output_compare_sync_test_cross_timer_source_ordering_cases(&state, &cpu);
        dspic33_output_compare_sync_test_synchronization_control_cases(&state, &cpu);
        dspic33_output_compare_sync_test_alternate_clock_control_cases(&state, &cpu);
        dspic33_output_compare_sync_test_alternate_instruction_activation_cases(&state, &cpu);
        dspic33_output_compare_sync_test_trigger_instruction_transition_cases(&state, &cpu);
        dspic33_output_compare_sync_test_synchronization_lifecycle_cases(&state, &cpu);
        dspic33_output_compare_sync_test_channel_trigger_matrix_cases(&state, &cpu);
        dspic33_output_compare_sync_test_trigger_source_negative_cases(&state, &cpu);
        dspic33_output_compare_sync_test_coexistence_cases(&state, &cpu);
        dspic33_output_compare_sync_test_lifecycle_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
