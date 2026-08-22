#ifndef DSPIC33_OUTPUT_COMPARE_SYNC_TEST_INTERNAL_H
#define DSPIC33_OUTPUT_COMPARE_SYNC_TEST_INTERNAL_H

#include "output_compare.h"

void dspic33_output_compare_sync_test_alternate_clock_batch_cases(TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_alternate_clock_cases(TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_alternate_clock_control_cases(TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_alternate_instruction_activation_cases(TestState* state,
                                                                             Dspic33* cpu);
void dspic33_output_compare_sync_test_channel_trigger_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_coexistence_cases(TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_cross_timer_source_ordering_cases(TestState* state,
                                                                        Dspic33* cpu);
void dspic33_output_compare_sync_test_input_capture_cascade_synchronization_cases(TestState* state,
                                                                                  Dspic33* cpu);
void dspic33_output_compare_sync_test_input_capture_synchronization_cases(TestState* state,
                                                                          Dspic33* cpu);
void dspic33_output_compare_sync_test_input_capture_synchronization_lifecycle_cases(
    TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_synchronization_control_cases(TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_synchronization_lifecycle_cases(TestState* state,
                                                                      Dspic33* cpu);
void dspic33_output_compare_sync_test_synchronization_source_cases(TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_timer_clock_synchronization_cases(TestState* state,
                                                                        Dspic33* cpu);
void dspic33_output_compare_sync_test_trigger_instruction_transition_cases(TestState* state,
                                                                           Dspic33* cpu);
void dspic33_output_compare_sync_test_trigger_one_shot_cases(TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_trigger_source_cases(TestState* state, Dspic33* cpu);
void dspic33_output_compare_sync_test_trigger_source_negative_cases(TestState* state, Dspic33* cpu);

#endif
