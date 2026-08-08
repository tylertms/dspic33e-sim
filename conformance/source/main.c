#include <stdint.h>

enum { CONFORMANCE_RESULT_WORDS = 2048 };

typedef struct {
    uint16_t result_words;
    uint16_t results[CONFORMANCE_RESULT_WORDS];
} ConformanceOutput;

volatile ConformanceOutput conformance_output;
volatile uint16_t conformance_scratch[16] __attribute__((near));

extern uint16_t run_arithmetic_conformance(volatile uint16_t* results);
extern uint16_t run_bit_conformance(volatile uint16_t* results);
extern uint16_t run_extension_conformance(volatile uint16_t* results);
extern uint16_t run_move_conformance(volatile uint16_t* results);
extern uint16_t run_multiply_conformance(volatile uint16_t* results);
extern uint16_t run_shift_conformance(volatile uint16_t* results);
extern uint16_t run_branch_conformance(volatile uint16_t* results);
extern void conformance_complete(void);

int main(void) {
    conformance_output.result_words = run_shift_conformance(conformance_output.results);
    conformance_output.result_words += run_branch_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_arithmetic_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_bit_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_extension_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_move_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_output.result_words += run_multiply_conformance(
        conformance_output.results + conformance_output.result_words);
    conformance_complete();
    return 0;
}
