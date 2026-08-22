#include <stdint.h>
#include <string.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

static uint32_t next_value(uint32_t* state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static uint64_t mix(uint64_t fingerprint, uint64_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static bool communication_operation(Dspic33* cpu, uint8_t operation, uint32_t value) {
    uint8_t channel = (uint8_t)((value >> 8u) & 7u);
    uint64_t delay = (value >> 16u) & 3u;
    switch (operation) {
        case 0u:
            return dspic33_uart_receive(cpu, channel, (uint8_t)value, delay);
        case 1u:
            return dspic33_uart_set_cts(cpu, channel, (value & 1u) != 0u, delay);
        case 2u:
            return dspic33_spi_receive(cpu, channel, (uint16_t)value, delay);
        case 3u:
            return dspic33_spi_select(cpu, channel, (value & 1u) != 0u, delay);
        case 4u:
            return dspic33_spi_pin_input(cpu, channel, (value & 1u) != 0u,
                                         (value & 2u) != 0u, (value & 4u) != 0u);
        case 5u:
            return dspic33_i2c_respond(cpu, channel, (uint8_t)value, (value & 1u) != 0u, delay);
        case 6u:
            return dspic33_i2c_slave_start(cpu, channel, (uint16_t)(value & 0x3ffu),
                                            (value & 1u) != 0u, (value & 2u) != 0u, delay);
        case 7u:
            return dspic33_i2c_slave_write(cpu, channel, (uint8_t)value, delay);
        case 8u:
            return dspic33_i2c_slave_read(cpu, channel, (value & 1u) != 0u, delay);
        case 9u:
            return dspic33_i2c_slave_stop(cpu, channel, delay);
        case 10u:
            return dspic33_i2c_collision(cpu, channel, delay);
        case 11u:
            return dspic33_dci_clock(cpu, (uint16_t)value, (value & 1u) != 0u, delay);
        default:
            return dspic33_can_error(cpu, channel, (value & 1u) != 0u, (uint8_t)value, delay);
    }
}

static bool peripheral_operation(Dspic33* cpu, uint8_t operation, uint32_t value) {
    uint8_t channel = (uint8_t)((value >> 8u) & 0x1fu);
    uint64_t delay = (value >> 16u) & 3u;
    switch (operation) {
        case 0u:
            return dspic33_dma_request(cpu, (uint8_t)value, (uint16_t)value, delay);
        case 1u:
            return dspic33_pmp_respond(cpu, (uint16_t)value, delay);
        case 2u:
            return dspic33_pmp_slave_read(cpu, (uint8_t)value, delay);
        case 3u:
            return dspic33_pmp_slave_write(cpu, (uint8_t)(value >> 8u), (uint8_t)value, delay);
        case 4u:
            return dspic33_input_capture_input(cpu, channel, (value & 1u) != 0u, delay);
        case 5u:
            return dspic33_output_compare_fault(cpu, channel, (value & 1u) != 0u, delay);
        case 6u:
            return dspic33_comparator_input(cpu, channel, (Dspic33ComparatorInput)(value & 7u),
                                             (uint16_t)value, delay);
        case 7u:
            return dspic33_comparator_reference(
                cpu, (Dspic33ComparatorReference)(value & 7u), (uint16_t)value, delay);
        case 8u:
            return dspic33_rtcc_clock(cpu, value & 0xffu, delay);
        case 9u:
            return dspic33_qei_input(cpu, channel, (Dspic33QeiInput)(value & 7u),
                                     (value & 8u) != 0u, delay);
        case 10u:
            return dspic33_timer_pulse(cpu, channel, value & 0xffu, delay);
        case 11u:
            return dspic33_timer_gate(cpu, channel, (value & 1u) != 0u, delay);
        case 12u:
            return dspic33_adc_trigger(cpu, channel, (uint8_t)value, delay);
        case 13u:
            return dspic33_pwm_fault(cpu, channel, (value & 1u) != 0u, delay);
        case 14u:
            return dspic33_pwm_current_limit(cpu, channel, (value & 1u) != 0u, delay);
        default:
            return dspic33_pwm_sync(cpu, channel, (value & 1u) != 0u, delay);
    }
}

static bool usb_operation(Dspic33* cpu, uint8_t operation, uint32_t value) {
    uint8_t data[8];
    uint8_t endpoint = (uint8_t)((value >> 8u) & 0x1fu);
    uint16_t size = (uint16_t)(value & 0x0fu);
    uint64_t delay = (value >> 16u) & 3u;
    for (uint8_t index = 0u; index < sizeof(data); index++) {
        data[index] = (uint8_t)(value >> (index & 3u) * 8u);
    }
    if (size > sizeof(data)) {
        size = sizeof(data);
    }
    switch (operation) {
        case 0u:
            return dspic33_usb_receive(cpu, endpoint, data, size, delay);
        case 1u:
            return dspic33_usb_receive_toggle(cpu, endpoint, data, size, (value & 1u) != 0u, delay);
        case 2u:
            return dspic33_usb_setup(cpu, endpoint, data, size, delay);
        case 3u:
            return dspic33_usb_request(cpu, endpoint, delay);
        case 4u:
            return dspic33_usb_token(cpu, (uint8_t)value, endpoint,
                                     (Dspic33UsbPid)((value >> 16u) & 0x0fu), data, size,
                                     (value & 1u) != 0u, delay);
        case 5u:
            return dspic33_usb_host_response(cpu,
                                              (Dspic33UsbHandshake)((value >> 8u) & 7u), data,
                                              size, (value & 1u) != 0u, delay);
        default:
            return dspic33_usb_bus(cpu, (Dspic33UsbBusEvent)(value & 7u), (uint16_t)value, delay);
    }
}

static uint64_t census(Dspic33* cpu) {
    uint32_t random = UINT32_C(0x91e10da5);
    uint64_t fingerprint = UINT64_C(14695981039346656037);
    for (uint32_t scenario = 0u; scenario < 65536u; scenario++) {
        uint32_t value = next_value(&random);
        uint8_t operation = (uint8_t)(value >> 27u);
        bool accepted = false;
        if ((scenario & 63u) == 0u || cpu->stop_reason != DSPIC33_RUNNING) {
            dspic33_reset(cpu, 0u);
        }
        if (operation < 4u) {
            uint32_t address = (value >> 8u) & 0x0fffu;
            if (operation == 0u) {
                dspic33_write_word(cpu, address & ~1u, (uint16_t)value);
            } else if (operation == 1u) {
                dspic33_write_byte(cpu, address, (uint8_t)value);
            } else if (operation == 2u) {
                fingerprint = mix(fingerprint, dspic33_read_word(cpu, address & ~1u));
            } else {
                fingerprint = mix(fingerprint, dspic33_read_byte(cpu, address));
            }
            accepted = true;
        } else if (operation == 4u) {
            cpu->power_state = (Dspic33PowerState)(value % 3u);
            dspic33_device_power_state_changed(cpu);
            accepted = true;
        } else if (operation == 5u) {
            accepted = dspic33_device_advance(cpu, value & 7u);
        } else if (operation < 19u) {
            accepted = communication_operation(cpu, (uint8_t)(operation - 6u), value);
        } else if (operation < 28u) {
            accepted = peripheral_operation(cpu, (uint8_t)(operation - 19u), value);
        } else {
            accepted = usb_operation(cpu, (uint8_t)(operation - 28u), value);
        }
        fingerprint = mix(fingerprint, value);
        fingerprint = mix(fingerprint, accepted);
        fingerprint = mix(fingerprint, cpu->device_cycles);
        fingerprint = mix(fingerprint, cpu->events.count);
        fingerprint = mix(fingerprint, cpu->stop_reason);
    }
    return fingerprint;
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize peripheral state census");
    if (initialized) {
        uint64_t fingerprint = census(&cpu);
        expect(&state, fingerprint == UINT64_C(18345084362457565972),
               "peripheral state census matches");
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
