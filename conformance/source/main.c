#include <stdint.h>

#pragma config OSCIOFNC = ON

enum { CONFORMANCE_RESULT_WORDS = 8192 };

typedef struct {
    uint16_t result_words;
    uint16_t results[CONFORMANCE_RESULT_WORDS];
} ConformanceOutput;

volatile ConformanceOutput conformance_output;
volatile uint16_t conformance_scratch[16] __attribute__((near));
volatile uint16_t system_probe_selector __attribute__((near));
volatile uint16_t system_reset_state __attribute__((near));
volatile uint16_t system_trap_state[12] __attribute__((near));
volatile uint16_t system_stack_trap_state[12] __attribute__((near));
volatile uint16_t system_address_trap_state[10] __attribute__((near));
volatile uint16_t system_address_trap_buffer[2] __attribute__((near));
volatile uint16_t system_multi_operand_trap_state[10] __attribute__((near));
volatile uint16_t system_multi_operand_control_state[6] __attribute__((near));
volatile uint16_t system_multi_operand_buffer[2] __attribute__((near));
volatile uint16_t system_data_map_trap_state[11] __attribute__((near));
volatile uint16_t system_data_map_control_state[6] __attribute__((near));
volatile uint16_t interrupt_count __attribute__((near));
volatile uint16_t interrupt_mode __attribute__((near));
volatile uint16_t interrupt_order[4] __attribute__((near));
volatile uint16_t interrupt_entry_stack[4] __attribute__((near));
volatile uint16_t interrupt_handler_status[4] __attribute__((near));
volatile uint16_t interrupt_handler_marker[4] __attribute__((near));
volatile uint16_t interrupt_handler_control[4] __attribute__((near));
volatile uint16_t system_page_zero_trap_state[12] __attribute__((near));
volatile uint16_t system_page_zero_control_state[8] __attribute__((near));
volatile uint16_t system_eds_page_trap_state[13] __attribute__((near));
volatile uint16_t system_program_target_trap_state[11] __attribute__((near));
volatile uint16_t system_sequential_hole_state __attribute__((near));

extern uint16_t run_arithmetic_conformance(volatile uint16_t* results);
extern uint16_t run_adc_conformance(volatile uint16_t* results);
extern uint16_t run_pwm_conformance(volatile uint16_t* results);
extern uint16_t run_spi_conformance(volatile uint16_t* results);
extern uint16_t run_bit_conformance(volatile uint16_t* results);
extern uint16_t run_divide_conformance(volatile uint16_t* results);
extern uint16_t run_dma_conformance(volatile uint16_t* results);
extern uint16_t run_extension_conformance(volatile uint16_t* results);
extern uint16_t run_interrupt_conformance(volatile uint16_t* results);
extern uint16_t run_loop_conformance(volatile uint16_t* results);
extern uint16_t run_loop_high_conformance(volatile uint16_t* results);
extern uint16_t run_move_conformance(volatile uint16_t* results);
extern uint16_t run_multiply_conformance(volatile uint16_t* results);
extern uint16_t run_shift_conformance(volatile uint16_t* results);
extern uint16_t run_sfr_conformance(volatile uint16_t* results);
extern uint16_t run_stack_conformance(volatile uint16_t* results);
extern uint16_t run_system_conformance(volatile uint16_t* results);
extern uint16_t run_table_conformance(volatile uint16_t* results);
extern uint16_t run_timer_conformance(volatile uint16_t* results);
extern void run_system_probe(uint16_t selector);
extern uint16_t run_branch_conformance(volatile uint16_t* results);
extern uint16_t run_can_conformance(volatile uint16_t* results);
extern uint16_t run_usb_conformance(volatile uint16_t* results);
extern uint16_t run_uart_conformance(volatile uint16_t* results);
extern void conformance_complete(void);

int main(void) {
    if (system_probe_selector != 0u) {
        run_system_probe(system_probe_selector);
    }
    conformance_output.result_words = run_sfr_conformance(conformance_output.results);
    conformance_output.result_words += run_dma_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_timer_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_adc_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_pwm_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_spi_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_can_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_usb_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_uart_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_shift_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_branch_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_arithmetic_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_bit_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_divide_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_extension_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_interrupt_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_loop_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_loop_high_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_move_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_multiply_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_stack_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_system_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_table_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_complete();
    return 0;
}
