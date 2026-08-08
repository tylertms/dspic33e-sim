#include <stdint.h>

enum { CONFORMANCE_RESULT_WORDS = 256 };

volatile uint16_t conformance_results[CONFORMANCE_RESULT_WORDS];
volatile uint16_t conformance_result_words;

extern uint16_t run_shift_conformance(volatile uint16_t* results);
extern uint16_t run_branch_conformance(volatile uint16_t* results);
extern void conformance_complete(void);

int main(void) {
    conformance_result_words = run_shift_conformance(conformance_results);
    conformance_result_words +=
        run_branch_conformance(conformance_results + conformance_result_words);
    conformance_complete();
    return 0;
}
