#ifndef DSPIC33_FAULT_TEST_INTERNAL_H
#define DSPIC33_FAULT_TEST_INTERNAL_H

#include "processor.h"

void dspic33_fault_test_address_error_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_compare_branch_target_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_compare_skip_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_data_map_address_error_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_earlier_deadline_case(TestState* state, Dspic33* cpu);
void dspic33_fault_test_invalid_lnk_case(TestState* state, Dspic33* cpu);
void dspic33_fault_test_invalid_ulnk_case(TestState* state, Dspic33* cpu);
void dspic33_fault_test_page_zero_address_error_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_prepare_timer_source(Dspic33* cpu);
void dspic33_fault_test_program_read_address_error_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_program_target_address_error_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_pseudo_linear_page_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_repeat_exception_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_repeat_interrupt_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_simultaneous_trap_case(TestState* state, Dspic33* cpu);
void dspic33_fault_test_skip_boundary_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_standalone_divide_zero_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_unimplemented_data_page_address_error_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_valid_stack_frame_cases(TestState* state, Dspic33* cpu);
void dspic33_fault_test_w15_write_cases(TestState* state, Dspic33* cpu);

#endif
