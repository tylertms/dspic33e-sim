#include <stdint.h>

#include "architecture/dspic33/internal.h"
#include "test.h"

static void read_word_as_cpu(Dspic33* cpu, uint32_t address) {
    cpu->instruction_active = true;
    (void)dspic33_read_word(cpu, address);
    cpu->instruction_active = false;
}

static void test_cpu_reads(TestState* state, Dspic33* cpu) {
    dspic33_clear_uninitialized_data_reads(NULL);
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_get_uninitialized_data_read_count(cpu) == 0u,
           "reset starts without uninitialized reads");
    expect(state, dspic33_get_first_uninitialized_data_read(cpu) == UINT32_MAX,
           "reset has no first uninitialized address");

    (void)dspic33_read_word(cpu, 0x4000u);
    expect(state, dspic33_get_uninitialized_data_read_count(cpu) == 0u,
           "host reads do not count as processor reads");

    read_word_as_cpu(cpu, 0x4000u);
    expect(state,
           dspic33_get_uninitialized_data_read_count(cpu) == 1u &&
               dspic33_get_first_uninitialized_data_read(cpu) == 0x4000u,
           "processor read reports uninitialized RAM");

    dspic33_clear_uninitialized_data_reads(cpu);
    dspic33_write_byte(cpu, 0x4000u, 0x5au);
    read_word_as_cpu(cpu, 0x4000u);
    expect(state,
           dspic33_get_uninitialized_data_read_count(cpu) == 1u &&
               dspic33_get_first_uninitialized_data_read(cpu) == 0x4001u,
           "partial initialization reports the first missing byte");

    dspic33_clear_uninitialized_data_reads(cpu);
    dspic33_write_word(cpu, 0x4000u, 0xa55au);
    read_word_as_cpu(cpu, 0x4000u);
    expect(state, dspic33_get_uninitialized_data_read_count(cpu) == 0u,
           "initialized processor read remains clean");
}

static void test_reset_and_copy(TestState* state, Dspic33* cpu, Dspic33* copy) {
    dspic33_write_word(cpu, 0x4000u, 0x1234u);
    dspic33_mclr_reset(cpu);
    read_word_as_cpu(cpu, 0x4000u);
    expect(state, dspic33_get_uninitialized_data_read_count(cpu) == 0u,
           "warm reset retains RAM initialization state");

    expect(state, dspic33_copy(copy, cpu), "copy preserves processor state");
    read_word_as_cpu(copy, 0x4000u);
    expect(state, dspic33_get_uninitialized_data_read_count(copy) == 0u,
           "copy preserves RAM initialization state");

    dspic33_reset(cpu, 0u);
    read_word_as_cpu(cpu, 0x4000u);
    expect(state, dspic33_get_uninitialized_data_read_count(cpu) == 1u,
           "power reset clears RAM initialization state");
}

static void test_dma_read(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->io.dma_transfer_active = true;
    (void)dspic33_read_byte(cpu, 0x5000u);
    cpu->io.dma_transfer_active = false;
    expect(state,
           dspic33_get_uninitialized_data_read_count(cpu) == 1u &&
               dspic33_get_first_uninitialized_data_read(cpu) == 0x5000u,
           "DMA read reports uninitialized RAM");
}

int main(void) {
    TestState state = {0};
    Dspic33 cpu;
    Dspic33 copy;
    const bool cpu_initialized = dspic33_initialize(&cpu);
    const bool copy_initialized = dspic33_initialize(&copy);
    expect(&state, cpu_initialized && copy_initialized, "processors initialize");
    if (cpu_initialized && copy_initialized) {
        test_cpu_reads(&state, &cpu);
        test_reset_and_copy(&state, &cpu, &copy);
        test_dma_read(&state, &cpu);
    }
    if (copy_initialized) {
        dspic33_release(&copy);
    }
    if (cpu_initialized) {
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
