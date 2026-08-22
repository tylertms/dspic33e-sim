#ifndef DSPIC33_TIMING_TEST_INTERNAL_H
#define DSPIC33_TIMING_TEST_INTERNAL_H

#include "processor.h"

void dspic33_timing_test_address_register_dependency_cases(TestState* state, Dspic33* cpu);
void dspic33_timing_test_call_stack_timing_case(TestState* state, Dspic33* cpu);
void dspic33_timing_test_direct_file_move_cases(TestState* state, Dspic33* cpu);
void dspic33_timing_test_dsp_prefetch_address_error_cases(TestState* state, Dspic33* cpu);
void dspic33_timing_test_dsp_x_prefetch_page_cases(TestState* state, Dspic33* cpu);
void dspic33_timing_test_instruction_cycle_cases(TestState* state, Dspic33* cpu);
void dspic33_timing_test_move_double_mode_cases(TestState* state, Dspic33* cpu);
void dspic33_timing_test_non_cpu_sfr_timing_cases(TestState* state, Dspic33* cpu);
void dspic33_timing_test_psv_program_hole_cases(TestState* state, Dspic33* cpu);
void dspic33_timing_test_psv_repeat_timing_cases(TestState* state, Dspic33* cpu);
void dspic33_timing_test_psv_timing_cases(TestState* state, Dspic33* cpu);
void dspic33_timing_test_register_move_instruction_cases(TestState* state, Dspic33* cpu);

#endif
