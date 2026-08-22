#include "../../../../../src/device/dspic33ep_mu/communication/i2c/internal.h"
#include "device/dspic33ep_mu/communication/i2c/internal.h"
#include "device/dspic33ep_mu/internal.h"

typedef struct {
    uint64_t fingerprint;
    uint32_t cases;
} PinMatrixResult;

static uint64_t mix(uint64_t fingerprint, uint32_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static uint32_t event_value(uint8_t kind, uint8_t generation, uint16_t payload) {
    return ((uint32_t)kind << I2C_EVENT_KIND_SHIFT) |
           ((uint32_t)generation << I2C_EVENT_GENERATION_SHIFT) | payload;
}

static void event_admission_matrix(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_i2c_internal_pause_events(cpu, 0u);
    dspic33_i2c_internal_resume_events(cpu, 0u);
    expect(state, cpu->events.count == 0u, "empty I2C event filters are stable");

    dspic33_i2c_process_event(cpu, DSPIC33_I2C_COUNT, 0u, false);
    cpu->io.i2c_pmd_generation[0] = 2u;
    dspic33_i2c_process_event(cpu, 0u, event_value(I2C_EVENT_PMD, 1u, 1u), false);
    expect(state, cpu->io.i2c_pmd_disabled == 0u, "I2C rejects a stale PMD event");
    dspic33_i2c_process_event(cpu, 0u, event_value(I2C_EVENT_PMD, 2u, 1u), false);
    expect(state, cpu->io.i2c_pmd_disabled == 1u, "I2C accepts a current disable event");
    dspic33_i2c_process_event(cpu, 0u, event_value(I2C_EVENT_PMD, 2u, 0u), false);
    expect(state, cpu->io.i2c_pmd_disabled == 0u, "I2C accepts a current enable event");

    cpu->io.i2c_generation[0] = 2u;
    dspic33_i2c_process_event(cpu, 0u, event_value(I2C_EVENT_CONTROL, 1u, I2C_SEN), false);
    dspic33_i2c_process_event(cpu, 0u, event_value(I2C_EVENT_PIN, 1u, 0u), false);
    expect(state, cpu->events.count == 0u, "I2C rejects stale controller and pin events");

    dspic33_i2c_test_enable(cpu, 0u, 0x2000u, 0u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_i2c_process_event(cpu, 0u, event_value(I2C_EVENT_COLLISION, 2u, 0u), true);
    expect(state, cpu->events.count == 1u, "I2C stop-in-idle defers an external event");
}

static void ten_bit_restart_matrix(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, !dspic33_i2c_slave_start(cpu, 0u, 0x155u, true, true, 0u),
           "I2C ten-bit restart requires an active slave");
    cpu->io.i2c_slave_active = 1u;
    cpu->io.i2c_slave_address[0] = 0x154u;
    expect(state, !dspic33_i2c_slave_start(cpu, 0u, 0x155u, true, true, 0u),
           "I2C ten-bit restart requires the matching address");
    cpu->io.i2c_slave_address[0] = 0x155u;
    expect(state, !dspic33_i2c_slave_start(cpu, 0u, 0x155u, true, true, 0u),
           "I2C ten-bit restart requires ten-bit status");
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_STAT), I2C_TEN_BIT);
    expect(state, dspic33_i2c_slave_start(cpu, 0u, 0x155u, true, true, 0u),
           "I2C ten-bit restart accepts a matching active transfer");
}

static void pin_admission_matrix(TestState* state, Dspic33* cpu) {
    bool high;
    dspic33_reset(cpu, 0u);
    cpu->configuration[12u] &= (uint8_t)~0x10u;
    dspic33_i2c_test_enable(cpu, 0u, I2C_SCLREL, 0u);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_CON),
                                        (uint16_t)(I2C_ENABLE | I2C_SEN));
    expect(state, !dspic33_i2c_pin(cpu, 3u, 10u, &high),
           "I2C master control hides pin output");

    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_CON), I2C_ENABLE);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_STAT),
                                        I2C_TRANSMIT_ACTIVE);
    expect(state, !dspic33_i2c_pin(cpu, 3u, 10u, &high),
           "I2C active transmission hides pin output");

    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_STAT), 0u);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_CON),
                                        (uint16_t)(I2C_ENABLE | I2C_SCLREL));
    cpu->io.i2c_slave_active = 1u;
    cpu->io.i2c_slave_read = 1u;
    expect(state, !dspic33_i2c_pin(cpu, 3u, 10u, &high),
           "released reading slave hides the clock output");

    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_CON), I2C_ENABLE);
    cpu->io.i2c_slave_read = 0u;
    expect(state, dspic33_i2c_pin(cpu, 3u, 10u, &high) && !high,
           "active slave holds an unreleased clock low");

    cpu->io.i2c_slave_active = 0u;
    cpu->io.i2c_master_active = 1u;
    expect(state, dspic33_i2c_pin(cpu, 3u, 10u, &high) && !high,
           "active master holds the clock low");
    expect(state, !dspic33_i2c_pin(cpu, 3u, 9u, &high),
           "active master owns the data output");
}

static PinMatrixResult run_pin_transition_matrix(Dspic33* cpu) {
    PinMatrixResult result = {UINT64_C(14695981039346656037), 0u};
    for (uint8_t operation = I2C_PIN_START; operation <= I2C_PIN_ACKNOWLEDGE; operation++) {
        for (uint8_t phase = 0u; phase <= 18u; phase++) {
            for (uint8_t levels = 0u; levels < 4u; levels++) {
                for (uint8_t queue_case = 0u; queue_case < 2u; queue_case++) {
                    dspic33_reset(cpu, 0u);
                    cpu->configuration[12u] &= (uint8_t)~0x10u;
                    dspic33_i2c_internal_raw_write_word(
                        cpu, (uint16_t)(bases[0] + I2C_CON),
                        (uint16_t)(I2C_ENABLE | ((levels & 1u) != 0u ? I2C_ACKDT : 0u)));
                    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_TRN),
                                                        0xa5u);
                    cpu->io.gpio_driven[3] |= 0x0600u;
                    cpu->io.gpio[3] =
                        (uint16_t)((cpu->io.gpio[3] & ~0x0600u) |
                                   ((levels & 1u) != 0u ? 0x0400u : 0u) |
                                   ((levels & 2u) != 0u ? 0x0200u : 0u));
                    cpu->io.i2c_pin_active = 1u;
                    cpu->io.i2c_pin_physical = 1u;
                    cpu->io.i2c_pin_operation[0] = operation;
                    cpu->io.i2c_pin_phase[0] = phase;
                    cpu->io.i2c_response[0].count =
                        queue_case != 0u ? DSPIC33_I2C_QUEUE_SIZE : 0u;
                    dspic33_i2c_internal_pin_run(cpu, 0u);
                    result.fingerprint = mix(result.fingerprint, cpu->stop_reason);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_pin_active);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_pin_clock_low);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_pin_data_low);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_pin_phase[0]);
                    result.fingerprint = mix(result.fingerprint, (uint32_t)cpu->events.count);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_response[0].count);
                    result.cases++;
                }
            }
        }
    }
    return result;
}

static void pin_transition_matrix(TestState* state, Dspic33* cpu) {
    const PinMatrixResult result = run_pin_transition_matrix(cpu);
    expect(state,
           result.cases == 912u && result.fingerprint == UINT64_C(8281854284848555637),
           "I2C pin transition matrix matches");
}

static PinMatrixResult run_slave_pin_matrix(Dspic33* cpu) {
    PinMatrixResult result = {UINT64_C(14695981039346656037), 0u};
    for (uint8_t pin_state = I2C_SLAVE_PIN_IDLE; pin_state <= I2C_SLAVE_PIN_RECEIVED;
         pin_state++) {
        for (uint8_t bits = 0u; bits <= 9u; bits++) {
            for (uint8_t edge = 0u; edge < 4u; edge++) {
                for (uint8_t flags = 0u; flags < 4u; flags++) {
                    dspic33_reset(cpu, 0u);
                    cpu->configuration[12u] &= (uint8_t)~0x10u;
                    dspic33_i2c_internal_raw_write_word(
                        cpu, (uint16_t)(bases[0] + I2C_CON),
                        (uint16_t)(I2C_ENABLE | ((flags & 1u) != 0u ? I2C_SCLREL : 0u)));
                    dspic33_i2c_internal_raw_write_word(
                        cpu, (uint16_t)(bases[0] + I2C_STAT),
                        (uint16_t)(((flags & 1u) != 0u ? I2C_TBF : 0u) |
                                   ((flags & 2u) != 0u ? I2C_RBF : 0u)));
                    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_TRN),
                                                        0xa5u);
                    cpu->io.gpio_driven[3] |= 0x0600u;
                    const bool current_clock = edge != 3u;
                    const bool current_data = edge == 1u || (edge >= 2u && (flags & 1u) != 0u);
                    const bool previous_clock = edge != 2u;
                    const bool previous_data = edge == 0u || (edge >= 2u && current_data);
                    cpu->io.gpio[3] =
                        (uint16_t)((cpu->io.gpio[3] & ~0x0600u) |
                                   (current_clock ? 0x0400u : 0u) |
                                   (current_data ? 0x0200u : 0u));
                    cpu->io.i2c_pin_clock_high = previous_clock ? 1u : 0u;
                    cpu->io.i2c_pin_data_high = previous_data ? 1u : 0u;
                    cpu->io.i2c_slave_pin_active = 1u;
                    cpu->io.i2c_slave_active = (flags & 1u) != 0u ? 1u : 0u;
                    cpu->io.i2c_slave_rejected = (flags & 2u) != 0u ? 1u : 0u;
                    cpu->io.i2c_slave_read = (flags & 2u) != 0u ? 1u : 0u;
                    cpu->io.i2c_slave_pin_acknowledge = (flags & 1u) != 0u ? 1u : 0u;
                    cpu->io.i2c_slave_pin_interrupt = (flags & 2u) != 0u ? 1u : 0u;
                    cpu->io.i2c_slave_pin_stretch = (flags & 1u) != 0u ? 1u : 0u;
                    cpu->io.i2c_slave_pin_state[0] = pin_state;
                    cpu->io.i2c_slave_pin_next[0] =
                        (uint8_t)((pin_state + 1u) % (I2C_SLAVE_PIN_RECEIVED + 1u));
                    cpu->io.i2c_slave_pin_bits[0] = bits;
                    cpu->io.i2c_slave_pin_shift[0] = (flags & 1u) != 0u ? 0xf1u : 0xa4u;
                    dspic33_i2c_refresh_pins(cpu);
                    result.fingerprint = mix(result.fingerprint, cpu->stop_reason);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_slave_pin_state[0]);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_slave_pin_bits[0]);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_slave_pin_shift[0]);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_slave_active);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_slave_rejected);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_pin_clock_low);
                    result.fingerprint = mix(result.fingerprint, cpu->io.i2c_pin_data_low);
                    result.cases++;
                }
            }
        }
    }
    return result;
}

static void slave_pin_transition_matrix(TestState* state, Dspic33* cpu) {
    const PinMatrixResult result = run_slave_pin_matrix(cpu);
    expect(state,
           result.cases == 1440u && result.fingerprint == UINT64_C(2689562037817619653),
           "I2C slave pin transition matrix matches");
}

void dspic33_i2c_test_state_matrix_cases(TestState* state, Dspic33* cpu) {
    event_admission_matrix(state, cpu);
    ten_bit_restart_matrix(state, cpu);
    pin_admission_matrix(state, cpu);
    pin_transition_matrix(state, cpu);
    slave_pin_transition_matrix(state, cpu);
}
