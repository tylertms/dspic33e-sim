#include <stdint.h>

#include "dspic33.h"
#include "test.h"

typedef struct {
    uint32_t address;
    uint32_t opcode;
    uint32_t count;
} TraceCapture;

static void capture_trace(void* context, uint32_t address, uint32_t opcode) {
    TraceCapture* capture = context;
    capture->address = address;
    capture->opcode = opcode;
    capture->count++;
}

static void test_lifecycle(TestState* state) {
    Dspic33* source = dspic33_create();
    Dspic33* destination = dspic33_create();
    expect(state, source != NULL, "source != NULL");
    expect(state, destination != NULL, "destination != NULL");
    expect(state, !dspic33_copy(NULL, source), "!dspic33_copy(NULL, source)");
    expect(state, !dspic33_copy(destination, NULL), "!dspic33_copy(destination, NULL)");
    expect(state, dspic33_copy(destination, source), "dspic33_copy(destination, source)");
    dspic33_destroy(destination);
    dspic33_destroy(source);
    dspic33_destroy(NULL);
}

static void test_execution(TestState* state) {
    Dspic33* cpu = dspic33_create();
    expect(state, cpu != NULL, "cpu != NULL");
    expect(state, dspic33_load_program_word(cpu, 0u, 0u), "dspic33_load_program_word(cpu, 0u, 0u)");
    dspic33_reset(cpu, 0u);

    Dspic33Result step = dspic33_step_result(cpu);
    expect(state, step.stop == DSPIC33_RUNNING, "step.stop == DSPIC33_RUNNING");
    expect(state, step.instructions == 1u, "step.instructions == 1u");
    expect(state, step.pc == 2u, "step.pc == 2u");
    expect(state, dspic33_get_instruction_count(cpu) == step.instructions,
           "dspic33_get_instruction_count(cpu) == step.instructions");
    expect(state, dspic33_get_cycle_count(cpu) == step.cycles,
           "dspic33_get_cycle_count(cpu) == step.cycles");
    expect(state, dspic33_get_program_counter(cpu) == step.pc,
           "dspic33_get_program_counter(cpu) == step.pc");
    expect(state, dspic33_get_executed_program_counter(cpu) == 0u,
           "executed program counter identifies the stepped instruction");
    expect(state, dspic33_get_trap_count(cpu) == 0u, "dspic33_get_trap_count(cpu) == 0u");
    expect(state, dspic33_get_interrupt_count(cpu) == 0u, "dspic33_get_interrupt_count(cpu) == 0u");
    expect(state, dspic33_get_last_interrupt(cpu) == UINT16_MAX,
           "dspic33_get_last_interrupt(cpu) == UINT16_MAX");
    expect(state, dspic33_get_interrupt_depth(cpu) == 0u, "dspic33_get_interrupt_depth(cpu) == 0u");

    Dspic33Result run = dspic33_run_with_limits(cpu, (Dspic33RunLimits){1u, 0u});
    expect(state, run.stop == DSPIC33_INSTRUCTION_LIMIT, "run.stop == DSPIC33_INSTRUCTION_LIMIT");
    expect(state, run.instructions == 2u, "run.instructions == 2u");

    dspic33_reset(cpu, 0u);
    Dspic33Result cycle_limited = dspic33_run_with_limits(cpu, (Dspic33RunLimits){0u, 1u});
    expect(state, cycle_limited.stop == DSPIC33_INSTRUCTION_LIMIT,
           "cycle-limited run stops at its limit");
    expect(state, cycle_limited.cycles >= 1u, "cycle-limited run consumes a cycle");
    expect(state, dspic33_get_stop(cpu) == DSPIC33_RUNNING,
           "execution limit does not change the processor stop reason");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_run_until(cpu, 2u, 2u) == DSPIC33_STOPPED,
           "dspic33_run_until(cpu, 2u, 2u) == DSPIC33_STOPPED");
    dspic33_set_stop_on_trap(cpu, true);
    dspic33_set_stop_on_trap(cpu, false);
    dspic33_set_stop_on_trap(NULL, true);
    dspic33_destroy(cpu);

    expect(state, dspic33_step_result(NULL).stop == DSPIC33_HALTED,
           "dspic33_step_result(NULL).stop == DSPIC33_HALTED");
    expect(state, dspic33_run_with_limits(NULL, (Dspic33RunLimits){0u, 0u}).stop == DSPIC33_HALTED,
           "dspic33_run_with_limits(NULL, limits).stop == DSPIC33_HALTED");
}

static void test_trace(TestState* state) {
    Dspic33* source = dspic33_create();
    Dspic33* destination = dspic33_create();
    TraceCapture source_capture = {0};
    TraceCapture destination_capture = {0};
    expect(state, source != NULL && destination != NULL, "trace processors are created");
    expect(state, dspic33_load_program_word(source, 0u, 0x200123u), "trace opcode is loaded");
    dspic33_set_trace(source, capture_trace, &source_capture);
    dspic33_set_trace(destination, capture_trace, &destination_capture);
    expect(state, dspic33_copy(destination, source), "trace processor is copied");
    dspic33_reset(destination, 0u);
    dspic33_step(destination);
    expect(state, destination_capture.count == 1u, "destination trace is called once");
    expect(state, destination_capture.address == 0u, "destination trace reports the address");
    expect(state, destination_capture.opcode == 0x200123u, "destination trace reports the opcode");
    expect(state, source_capture.count == 0u, "copy preserves the destination trace context");
    dspic33_reset(destination, 0u);
    dspic33_step(destination);
    expect(state, destination_capture.count == 2u, "reset preserves the trace callback");
    dspic33_set_trace(destination, NULL, NULL);
    dspic33_reset(destination, 0u);
    dspic33_step(destination);
    expect(state, destination_capture.count == 2u, "cleared trace is not called");
    dspic33_set_trace(NULL, capture_trace, &destination_capture);
    dspic33_destroy(destination);
    dspic33_destroy(source);
}

static void test_execution_boundaries(TestState* state) {
    Dspic33* cpu = dspic33_create();
    expect(state, cpu != NULL, "boundary cpu != NULL");
    if (cpu == NULL) {
        return;
    }

    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state, dspic33_step(cpu) == DSPIC33_SLEEPING, "sleeping CPU remains stopped");
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state, dspic33_step(cpu) == DSPIC33_IDLING, "idling CPU remains stopped");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_program_word(cpu, 0u, 0x060000u), "load boundary return opcode");
    expect(state, dspic33_run(cpu, 0u) == DSPIC33_RETURNED, "run returns at an empty call stack");
    expect(state, dspic33_get_executed_program_counter(cpu) == 0u,
           "completed return reports its program counter");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_program_word(cpu, 0u, 0x051232u),
           "load boundary RETLW opcode");
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_begin_call(cpu, 0u, false), "begin RETLW call");
    expect(state,
           dspic33_run(cpu, 0u) == DSPIC33_RETURNED && cpu->w[2] == 0x0123u &&
               cpu->w[15] == 0x5000u && cpu->call_depth == 0u,
           "RETLW terminates an empty call frame without stack underflow");

    dspic33_reset(cpu, 0u);
    Dspic33Result returned = dspic33_run_with_limits(cpu, (Dspic33RunLimits){0u, 0u});
    expect(state, returned.stop == DSPIC33_RETURNED, "limited runner reports a completed return");

    cpu->stop_reason = DSPIC33_TRAPPED;
    cpu->address_error_return = 0x120u;
    expect(state, dspic33_get_fault_address(cpu) == 0x120u,
           "fault getter prefers the address-error return");
    cpu->address_error_return = 0u;
    cpu->current_instruction_pc = 0x220u;
    expect(state, dspic33_get_fault_address(cpu) == 0x220u,
           "fault getter falls back to the current instruction");
    cpu->trap_count = 3u;
    expect(state, dspic33_get_trap_count(cpu) == 3u, "trap getter reports handled traps");
    dspic33_destroy(cpu);
}

static void test_host_operations(TestState* state) {
    Dspic33* cpu = dspic33_create();
    expect(state, cpu != NULL, "cpu != NULL");
    const uint8_t bytes[] = {0x34u, 0x12u};
    expect(state, dspic33_seed_data(cpu, 0x100u, bytes, sizeof(bytes)),
           "dspic33_seed_data(cpu, 0x100u, bytes, sizeof(bytes))");
    expect(state, dspic33_read_word(cpu, 0x100u) == 0x1234u,
           "dspic33_read_word(cpu, 0x100u) == 0x1234u");
    expect(state, dspic33_get_register(cpu, 16u) == 0u, "dspic33_get_register(cpu, 16u) == 0u");
    expect(state, !dspic33_seed_data(cpu, DSPIC33_DATA_SIZE, bytes, sizeof(bytes)),
           "!dspic33_seed_data(cpu, DSPIC33_DATA_SIZE, bytes, sizeof(bytes))");
    expect(state, dspic33_seed_data(cpu, DSPIC33_DATA_SIZE, NULL, 0u),
           "dspic33_seed_data(cpu, DSPIC33_DATA_SIZE, NULL, 0u)");
    expect(state, dspic33_begin_call(cpu, 0u, false), "dspic33_begin_call(cpu, 0u, false)");
    expect(state, !dspic33_begin_call(cpu, 1u, false), "!dspic33_begin_call(cpu, 1u, false)");
    expect(state, !dspic33_begin_call(cpu, DSPIC33_PROGRAM_LIMIT, false),
           "!dspic33_begin_call(cpu, DSPIC33_PROGRAM_LIMIT, false)");
    bool high = false;
    expect(state, dspic33_gpio_drive(cpu, 1u, 1u, 1u), "dspic33_gpio_drive(cpu, 1u, 1u, 1u)");
    expect(state, dspic33_gpio_signal(cpu, 1u, 0u, &high) && high,
           "dspic33_gpio_signal(cpu, 1u, 0u, &high) && high");
    expect(state, !dspic33_gpio_signal(cpu, DSPIC33_GPIO_PORT_COUNT, 0u, &high),
           "!dspic33_gpio_signal(cpu, DSPIC33_GPIO_PORT_COUNT, 0u, &high)");
    dspic33_destroy(cpu);
}

static void test_null_getters(TestState* state) {
    expect(state, dspic33_get_register(NULL, 0u) == 0u, "dspic33_get_register(NULL, 0u) == 0u");
    expect(state, dspic33_get_program_counter(NULL) == 0u,
           "dspic33_get_program_counter(NULL) == 0u");
    expect(state, dspic33_get_executed_program_counter(NULL) == 0u,
           "dspic33_get_executed_program_counter(NULL) == 0u");
    expect(state, dspic33_get_instruction_count(NULL) == 0u,
           "dspic33_get_instruction_count(NULL) == 0u");
    expect(state, dspic33_get_cycle_count(NULL) == 0u, "dspic33_get_cycle_count(NULL) == 0u");
    expect(state, dspic33_get_stop(NULL) == DSPIC33_HALTED,
           "dspic33_get_stop(NULL) == DSPIC33_HALTED");
    expect(state, dspic33_get_fault_address(NULL) == 0u, "dspic33_get_fault_address(NULL) == 0u");
    expect(state, dspic33_get_trap_count(NULL) == 0u, "dspic33_get_trap_count(NULL) == 0u");
    expect(state, dspic33_get_interrupt_count(NULL) == 0u,
           "dspic33_get_interrupt_count(NULL) == 0u");
    expect(state, dspic33_get_last_interrupt(NULL) == UINT16_MAX,
           "dspic33_get_last_interrupt(NULL) == UINT16_MAX");
    expect(state, dspic33_get_interrupt_depth(NULL) == 0u,
           "dspic33_get_interrupt_depth(NULL) == 0u");
}

static void test_memory_guards(TestState* state) {
    Dspic33* cpu = dspic33_create();
    static const uint32_t null_program_addresses[] = {
        0u,
        0x2000u,
        DSPIC33_AUXILIARY_PROGRAM_BASE,
        DSPIC33_CONFIGURATION_BASE + 4u,
        0xff0000u,
        DSPIC33_PERSISTENT_PROGRAM_BASE,
        DSPIC33_WRITE_LATCH_BASE,
    };
    expect(state, cpu != NULL, "cpu != NULL");
    Dspic33epMuDevice device = DSPIC33EP_MU_DEVICE_COUNT;
    expect(state, dspic33_create_for_device(DSPIC33EP_MU_DEVICE_COUNT) == NULL,
           "dspic33_create_for_device(DSPIC33EP_MU_DEVICE_COUNT) == NULL");
    expect(state, dspic33_device_profile(NULL) == NULL, "dspic33_device_profile(NULL) == NULL");
    expect(state, !dspic33ep_mu_device_from_name(NULL, &device),
           "!dspic33ep_mu_device_from_name(NULL, &device)");
    expect(state, !dspic33ep_mu_device_from_name("256MU806", NULL),
           "!dspic33ep_mu_device_from_name(\"256MU806\", NULL)");
    dspic33_set_working_register(cpu, 16u, 0u);
    expect(state, !dspic33_load_program_word(NULL, 0u, 0u),
           "!dspic33_load_program_word(NULL, 0u, 0u)");
    expect(state, !dspic33_load_program_word(cpu, 1u, 0u),
           "!dspic33_load_program_word(cpu, 1u, 0u)");
    expect(state, !dspic33_program_range_implemented(UINT32_MAX, 2u),
           "!dspic33_program_range_implemented(UINT32_MAX, 2u)");
    expect(state, !dspic33_device_program_range_implemented(NULL, 0u, 2u),
           "!dspic33_device_program_range_implemented(NULL, 0u, 2u)");
    expect(state, !dspic33_data_range_valid(UINT32_MAX, 2u),
           "!dspic33_data_range_valid(UINT32_MAX, 2u)");
    expect(state, !dspic33_device_data_range_implemented(NULL, 0u, 2u),
           "!dspic33_device_data_range_implemented(NULL, 0u, 2u)");
    expect(state, !dspic33_load_configuration_word(NULL, DSPIC33_CONFIGURATION_BASE, 0u),
           "configuration load rejects a null processor");
    expect(state, !dspic33_load_configuration_word(cpu, DSPIC33_CONFIGURATION_BASE + 1u, 0u),
           "configuration load rejects an odd address");
    expect(state, !dspic33_load_configuration_word(cpu, UINT32_MAX, 0u),
           "configuration load rejects a wrapping address");
    expect(state,
           !dspic33_load_configuration_word(
               cpu, DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE, 0u),
           "configuration load rejects an address above range");
    expect(state,
           dspic33_load_configuration_word(
               cpu, DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE - 2u, 0x1234u) &&
               dspic33_read_configuration_byte(cpu, DSPIC33_CONFIGURATION_BASE +
                                                        DSPIC33_CONFIGURATION_SIZE - 2u) == 0x34u &&
               dspic33_read_configuration_byte(cpu, DSPIC33_CONFIGURATION_BASE +
                                                        DSPIC33_CONFIGURATION_SIZE - 1u) == 0x12u,
           "configuration load accepts the final aligned word");
    for (size_t index = 0u;
         index < sizeof(null_program_addresses) / sizeof(null_program_addresses[0]); index++) {
        expect(state, dspic33_read_program_word(NULL, null_program_addresses[index]) == 0xffffffu,
               "null program reads return the erased word");
    }
    expect(state, dspic33_read_program_byte(NULL, 0u) == 0xffu,
           "dspic33_read_program_byte(NULL, 0u) == 0xffu");
    expect(state, !dspic33_seed_data(NULL, 0u, NULL, 0u), "!dspic33_seed_data(NULL, 0u, NULL, 0u)");
    expect(state, !dspic33_seed_data(cpu, 0u, NULL, 1u), "!dspic33_seed_data(cpu, 0u, NULL, 1u)");
    expect(state, !dspic33_seed_data(cpu, 0u, &device, SIZE_MAX),
           "seed data rejects a host-size overflow");
    expect(state, !dspic33_begin_call(NULL, 0u, false), "begin call rejects a null processor");
    expect(state, dspic33_read_configuration_byte(cpu, DSPIC33_CONFIGURATION_BASE - 1u) == 0xffu,
           "configuration read rejects address below range");
    expect(state,
           dspic33_read_configuration_byte(cpu, DSPIC33_CONFIGURATION_BASE +
                                                    DSPIC33_CONFIGURATION_SIZE) == 0xffu,
           "configuration read rejects address above range");
    cpu->nvm.control = 0u;
    cpu->nvm.address = DSPIC33_CONFIGURATION_BASE - 1u;
    dspic33_complete_nvm(cpu);
    dspic33_destroy(cpu);
}

static void test_peripheral_guards(TestState* state) {
    Dspic33* cpu = dspic33_create();
    expect(state, cpu != NULL, "cpu != NULL");
    bool high = false;
    uint64_t edges = 0u;
    Dspic33UartFrame uart = {0};
    Dspic33I2cTransfer i2c = {0};
    Dspic33CanFrame can = {0};
    expect(state, !dspic33_uart_receive(cpu, DSPIC33_UART_COUNT, 0u, 0u),
           "!dspic33_uart_receive(cpu, DSPIC33_UART_COUNT, 0u, 0u)");
    expect(state, !dspic33_uart_transmit(cpu, DSPIC33_UART_COUNT, &uart),
           "!dspic33_uart_transmit(cpu, DSPIC33_UART_COUNT, &uart)");
    expect(state, !dspic33_spi_receive(cpu, DSPIC33_SPI_COUNT, 0u, 0u),
           "!dspic33_spi_receive(cpu, DSPIC33_SPI_COUNT, 0u, 0u)");
    expect(state, !dspic33_spi_transmit(cpu, DSPIC33_SPI_COUNT, NULL),
           "!dspic33_spi_transmit(cpu, DSPIC33_SPI_COUNT, NULL)");
    expect(state, !dspic33_spi_clock_output(cpu, DSPIC33_SPI_COUNT, &high),
           "!dspic33_spi_clock_output(cpu, DSPIC33_SPI_COUNT, &high)");
    expect(state, !dspic33_i2c_respond(cpu, DSPIC33_I2C_COUNT, 0u, false, 0u),
           "!dspic33_i2c_respond(cpu, DSPIC33_I2C_COUNT, 0u, false, 0u)");
    expect(state, !dspic33_i2c_transmit(cpu, DSPIC33_I2C_COUNT, &i2c),
           "!dspic33_i2c_transmit(cpu, DSPIC33_I2C_COUNT, &i2c)");
    expect(state, !dspic33_i2c_pin(cpu, DSPIC33_GPIO_PORT_COUNT, 0u, &high),
           "!dspic33_i2c_pin(cpu, DSPIC33_GPIO_PORT_COUNT, 0u, &high)");
    expect(state, !dspic33_input_capture_input(cpu, DSPIC33_INPUT_CAPTURE_COUNT, false, 0u),
           "!dspic33_input_capture_input(cpu, DSPIC33_INPUT_CAPTURE_COUNT, false, 0u)");
    expect(state, !dspic33_output_compare_output(cpu, DSPIC33_OUTPUT_COMPARE_COUNT, &high),
           "!dspic33_output_compare_output(cpu, DSPIC33_OUTPUT_COMPARE_COUNT, &high)");
    expect(state, !dspic33_output_compare_fault(cpu, DSPIC33_OUTPUT_COMPARE_FAULT_COUNT, false, 0u),
           "!dspic33_output_compare_fault(cpu, DSPIC33_OUTPUT_COMPARE_FAULT_COUNT, false, 0u)");
    expect(state, !dspic33_comparator_output(cpu, DSPIC33_COMPARATOR_COUNT, &high),
           "!dspic33_comparator_output(cpu, DSPIC33_COMPARATOR_COUNT, &high)");
    expect(state, !dspic33_qei_compare_output(cpu, DSPIC33_QEI_COUNT, &high),
           "!dspic33_qei_compare_output(cpu, DSPIC33_QEI_COUNT, &high)");
    expect(state, !dspic33_timer_pulse(cpu, DSPIC33_TIMER_COUNT, 1u, 0u),
           "!dspic33_timer_pulse(cpu, DSPIC33_TIMER_COUNT, 1u, 0u)");
    expect(state, !dspic33_adc_trigger(cpu, DSPIC33_ADC_COUNT, 0u, 0u),
           "!dspic33_adc_trigger(cpu, DSPIC33_ADC_COUNT, 0u, 0u)");
    expect(state, !dspic33_pwm_dead_time(cpu, DSPIC33_PWM_MAX_COUNT, false, 0u),
           "!dspic33_pwm_dead_time(cpu, DSPIC33_PWM_MAX_COUNT, false, 0u)");
    expect(state, !dspic33_pwm_sync_output(cpu, 2u), "!dspic33_pwm_sync_output(cpu, 2u)");
    expect(state, !dspic33_can_receive(cpu, DSPIC33_CAN_COUNT, &can, 0u),
           "!dspic33_can_receive(cpu, DSPIC33_CAN_COUNT, &can, 0u)");
    can.length = 9u;
    expect(state, !dspic33_can_receive(cpu, 0u, &can, 0u),
           "!dspic33_can_receive(cpu, 0u, &can, 0u)");
    expect(state, !dspic33_can_error(cpu, DSPIC33_CAN_COUNT, false, 1u, 0u),
           "!dspic33_can_error(cpu, DSPIC33_CAN_COUNT, false, 1u, 0u)");
    expect(state, !dspic33_can_error(cpu, 0u, false, 0u, 0u),
           "!dspic33_can_error(cpu, 0u, false, 0u, 0u)");
    expect(state, !dspic33_usb_receive(cpu, DSPIC33_USB_ENDPOINT_COUNT, NULL, 0u, 0u),
           "!dspic33_usb_receive(cpu, DSPIC33_USB_ENDPOINT_COUNT, NULL, 0u, 0u)");
    expect(state, !dspic33_usb_receive(cpu, 0u, NULL, 1u, 0u),
           "!dspic33_usb_receive(cpu, 0u, NULL, 1u, 0u)");
    expect(state, !dspic33_usb_bus(cpu, (Dspic33UsbBusEvent)UINT8_MAX, 0u, 0u),
           "!dspic33_usb_bus(cpu, (Dspic33UsbBusEvent)UINT8_MAX, 0u, 0u)");
    expect(state, !dspic33_gpio_drive(cpu, DSPIC33_GPIO_PORT_COUNT, 0u, 0u),
           "!dspic33_gpio_drive(cpu, DSPIC33_GPIO_PORT_COUNT, 0u, 0u)");
    expect(state, !dspic33_gpio_pin(cpu, 0u, 16u, &high), "!dspic33_gpio_pin(cpu, 0u, 16u, &high)");
    expect(state, !dspic33_reference_clock_pin(cpu, UINT8_MAX, 1u, &edges),
           "!dspic33_reference_clock_pin(cpu, UINT8_MAX, 1u, &edges)");
    dspic33_destroy(cpu);
}

int main(void) {
    TestState state = {0};
    test_lifecycle(&state);
    test_execution(&state);
    test_trace(&state);
    test_execution_boundaries(&state);
    test_host_operations(&state);
    test_null_getters(&state);
    test_memory_guards(&state);
    test_peripheral_guards(&state);
    return test_finish(&state);
}
