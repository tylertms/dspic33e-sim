#include <stdint.h>

#include "architecture/dspic33/internal.h"
#include "device/dspic33ep_mu/device.h"
#include "sfr_cases.h"
#include "test.h"

typedef struct {
    uint64_t accepted;
    uint64_t fingerprint;
} Census;

static uint32_t next_value(uint32_t* state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static uint64_t mix(uint64_t fingerprint, uint32_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static void record(Census* census, bool accepted) {
    census->accepted += accepted;
    census->fingerprint = mix(census->fingerprint, accepted);
}

static void configure_registers(Dspic33* cpu, uint32_t* random) {
    for (size_t index = 0u; index < DSPIC33_SFR_ACCESS_ADDRESS_COUNT; index++) {
        const Dspic33SfrAccessExpectation* expectation = &dspic33_sfr_access_expectations[index];
        const uint16_t value = (uint16_t)next_value(random) & expectation->normal;
        dspic33_write_word(cpu, expectation->address, value);
    }
}

static void exercise_serial(Census* census, Dspic33* cpu, uint32_t* random, uint32_t scenario) {
    for (uint8_t channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        const Dspic33UartFrame frame = {(uint16_t)next_value(random),
                                        16u,
                                        (uint8_t)(7u + (scenario & 1u)),
                                        (uint8_t)(1u + (scenario & 1u)),
                                        (Dspic33UartParity)(scenario % 3u),
                                        (scenario & 1u) != 0u,
                                        (scenario & 2u) != 0u,
                                        (scenario & 4u) != 0u,
                                        (scenario & 8u) != 0u,
                                        (scenario & 16u) != 0u};
        record(census,
               dspic33_uart_receive(cpu, channel, (uint8_t)next_value(random), channel + 1u));
        record(census, dspic33_uart_receive_frame(cpu, channel, &frame, channel + 2u));
        record(census, dspic33_uart_set_cts(cpu, channel, (channel & 1u) != 0u, channel + 2u));
    }
    for (uint8_t channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        record(census, dspic33_spi_receive(cpu, channel, (uint16_t)next_value(random), channel));
        record(census,
               dspic33_spi_select(cpu, channel, ((scenario + channel) & 1u) != 0u, channel + 1u));
        record(census, dspic33_spi_pin_input(cpu, channel, (scenario & 1u) != 0u,
                                             (scenario & 2u) != 0u, (scenario & 4u) != 0u));
    }
    for (uint8_t channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        record(census, dspic33_i2c_respond(cpu, channel, (uint8_t)next_value(random),
                                           (scenario & 1u) != 0u, channel));
        record(census, dspic33_i2c_status(cpu, channel, (uint16_t)next_value(random)));
        record(census,
               dspic33_i2c_slave_start(cpu, channel, (uint16_t)(0x52u + (scenario & 0x1ffu)),
                                       (scenario & 1u) != 0u, (scenario & 2u) != 0u, channel + 1u));
        record(census,
               dspic33_i2c_slave_write(cpu, channel, (uint8_t)next_value(random), channel + 2u));
        record(census, dspic33_i2c_slave_read(cpu, channel, (scenario & 4u) != 0u, channel + 3u));
        record(census, dspic33_i2c_slave_stop(cpu, channel, channel + 4u));
        record(census, dspic33_i2c_collision(cpu, channel, channel + 5u));
    }
}

static void exercise_control(Census* census, Dspic33* cpu, uint32_t* random, uint32_t scenario) {
    for (uint8_t channel = 0u; channel < DSPIC33_INPUT_CAPTURE_COUNT; channel++) {
        record(census, dspic33_input_capture_input(cpu, channel, (channel & 1u) != 0u, channel));
    }
    for (uint8_t source = 0u; source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT; source++) {
        record(census, dspic33_output_compare_fault(cpu, source, true, source));
    }
    for (uint8_t comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        for (uint8_t input = 0u; input <= DSPIC33_COMPARATOR_INPUT_NEGATIVE_3; input++) {
            record(census,
                   dspic33_comparator_input(cpu, comparator, (Dspic33ComparatorInput)input,
                                            (uint16_t)next_value(random), comparator + input));
        }
    }
    for (uint8_t reference = 0u; reference < DSPIC33_COMPARATOR_REFERENCE_COUNT; reference++) {
        record(census, dspic33_comparator_reference(cpu, (Dspic33ComparatorReference)reference,
                                                    (uint16_t)next_value(random), reference));
    }
    for (uint8_t channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        for (uint8_t input = 0u; input <= DSPIC33_QEI_HOME; input++) {
            record(census, dspic33_qei_input(cpu, channel, (Dspic33QeiInput)input,
                                             ((scenario + input) & 1u) != 0u, channel + input));
        }
    }
    for (uint8_t timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        record(census, dspic33_timer_pulse(cpu, timer, timer + 1u, timer));
        record(census, dspic33_timer_gate(cpu, timer, (timer & 1u) != 0u, timer + 1u));
    }
    for (uint8_t module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        record(census, dspic33_adc_trigger(cpu, module, module, module + 1u));
    }
    for (uint8_t generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        record(census, dspic33_pwm_dead_time(cpu, generator, (scenario & 1u) != 0u, generator));
    }
    record(census, dspic33_pwm_fault(cpu, scenario % 8u, (scenario & 1u) != 0u, 0u));
    record(census, dspic33_pwm_current_limit(cpu, scenario % 8u, (scenario & 2u) != 0u, 1u));
    record(census, dspic33_pwm_sync(cpu, scenario % 4u, (scenario & 4u) != 0u, 2u));
    record(census, dspic33_rtcc_clock(cpu, 3u, 1u));
    dspic33_dci_input(cpu, (uint16_t)next_value(random));
    record(census, dspic33_dci_clock(cpu, (uint16_t)next_value(random), true, 2u));
    record(census, dspic33_dci_clock(cpu, (uint16_t)next_value(random), false, 3u));
}

static void exercise_bus(Census* census, Dspic33* cpu, uint32_t* random, uint32_t scenario) {
    Dspic33CanFrame frame = {(scenario & 1u) != 0u ? next_value(random) & UINT32_C(0x1fffffff)
                                                   : next_value(random) & UINT32_C(0x7ff),
                             {0},
                             (uint8_t)(scenario % 9u),
                             (scenario & 1u) != 0u,
                             (scenario & 2u) != 0u};
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        record(census, dspic33_can_receive(cpu, channel, &frame, channel));
        record(census, dspic33_can_error(cpu, channel, (scenario & 4u) != 0u,
                                         (uint8_t)(1u + scenario % 8u), channel + 1u));
        record(census, dspic33_can_invalid(cpu, channel, channel + 2u));
    }
    uint8_t packet[8];
    for (uint8_t index = 0u; index < sizeof(packet); index++) {
        packet[index] = (uint8_t)next_value(random);
    }
    record(census, dspic33_usb_bus(cpu, (Dspic33UsbBusEvent)(scenario % 8u),
                                   (uint16_t)next_value(random), 0u));
    record(census, dspic33_usb_receive(cpu, 1u, packet, sizeof(packet), 1u));
    const Dspic33UsbPid pids[] = {DSPIC33_USB_PID_OUT, DSPIC33_USB_PID_SOF, DSPIC33_USB_PID_IN,
                                  DSPIC33_USB_PID_SETUP};
    record(census,
           dspic33_usb_token(cpu, (uint8_t)(scenario & 0x7fu), scenario % 16u, pids[scenario % 4u],
                             packet, sizeof(packet), (scenario & 1u) != 0u, 2u));
    record(census, dspic33_usb_receive_toggle(cpu, scenario % 16u, packet, sizeof(packet),
                                              (scenario & 2u) != 0u, 3u));
    record(census, dspic33_usb_setup(cpu, scenario % 16u, packet, sizeof(packet), 4u));
    record(census, dspic33_usb_host_response(cpu, (Dspic33UsbHandshake)(scenario % 6u), packet,
                                             sizeof(packet), (scenario & 4u) != 0u, 5u));
    record(census, dspic33_usb_request(cpu, 1u, 2u));
    record(census, dspic33_pmp_respond(cpu, (uint16_t)next_value(random), 1u));
    for (uint8_t port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        record(census, dspic33_gpio_drive(cpu, port, (uint16_t)next_value(random), UINT16_MAX));
        record(census, dspic33_gpio_release(cpu, port, (uint16_t)next_value(random)));
        dspic33_gpio_input(cpu, port, (uint16_t)next_value(random));
    }
}

static Census census_peripherals(Dspic33* cpu) {
    Census census = {0u, UINT64_C(14695981039346656037)};
    for (uint32_t scenario = 0u; scenario < 128u; scenario++) {
        uint32_t random = UINT32_C(0x9e3779b9) ^ scenario;
        dspic33_reset(cpu, 0u);
        configure_registers(cpu, &random);
        exercise_serial(&census, cpu, &random, scenario);
        exercise_control(&census, cpu, &random, scenario);
        exercise_bus(&census, cpu, &random, scenario);
        record(&census, dspic33_device_advance(cpu, 512u));
        census.fingerprint = mix(census.fingerprint, cpu->stop_reason);
        census.fingerprint = mix(census.fingerprint, (uint32_t)cpu->events.count);
        census.fingerprint = mix(census.fingerprint, cpu->io.dma_active);
        census.fingerprint = mix(census.fingerprint, dspic33_read_word(cpu, 0x0180u));
        census.fingerprint = mix(census.fingerprint, dspic33_read_word(cpu, 0x0286u));
    }
    return census;
}

int main(void) {
    TestState state = {0};
    Dspic33 cpu;
    const bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "cpu initialized");
    if (initialized) {
        const Census census = census_peripherals(&cpu);
        expect(&state,
               census.accepted == 16757u && census.fingerprint == UINT64_C(2239817818780952168),
               "peripheral census matches");
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
