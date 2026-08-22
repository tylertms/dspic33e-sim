#include <string.h>

#include "device/dspic33ep_mu/internal.h"
#include "test.h"

static void register_mask_cases(TestState* state, Dspic33* cpu) {
    uint16_t writable;

    expect(state, !dspic33_device_internal_input_capture_register_write_mask(0x0141u, &writable),
           "input capture rejects an unaligned register");
    expect(state, !dspic33_device_internal_output_compare_register_write_mask(0x0901u, &writable),
           "output compare rejects an unaligned register");
    expect(state, !dspic33_device_internal_comparator_register_write_mask(0x0a85u, &writable),
           "comparator rejects an unaligned register");
    expect(state,
           dspic33_device_internal_can_register_write_mask(cpu, 0x0402u, &writable) &&
               writable == 0u,
           "CAN vector register is read-only");
}

static void dma_gpio_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->io.dma_transfer_active = true;
    cpu->io.dma_transfer_width = 2u;
    dspic33_device_internal_update_gpio_latch(cpu, 0x0e02u, 0xa55au);
    expect(state, dspic33_device_internal_raw_word(cpu, 0x0e04u) == (0xa55au & 0xc6ffu),
           "word DMA updates the complete GPIO latch");
}

static void full_queue_cases(TestState* state) {
    Dspic33ByteQueue bytes;
    Dspic33UartQueue uart;
    Dspic33UartFrame frame = {0};
    Dspic33WordQueue words;

    memset(&bytes, 0, sizeof(bytes));
    memset(&uart, 0, sizeof(uart));
    memset(&words, 0, sizeof(words));
    bytes.count = sizeof(bytes.bytes);
    uart.count = DSPIC33_UART_QUEUE_SIZE;
    words.count = sizeof(words.words) / sizeof(words.words[0]);
    expect(state, !dspic33_device_internal_byte_queue_push(&bytes, 1u),
           "full byte queue rejects a value");
    expect(state, !dspic33_device_internal_uart_queue_push(&uart, &frame),
           "full UART queue rejects a frame");
    expect(state, !dspic33_device_internal_word_queue_push(&words, 1u),
           "full word queue rejects a value");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize common boundary processor");
    if (initialized) {
        register_mask_cases(&state, &cpu);
        dma_gpio_case(&state, &cpu);
        full_queue_cases(&state);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
