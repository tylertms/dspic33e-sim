#ifndef DSPIC33_PWM_TEST_INTERNAL_H
#define DSPIC33_PWM_TEST_INTERNAL_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

static const uint8_t irqs[DSPIC33_PWM_COUNT] = {94u, 95u, 96u, 97u, 98u, 99u};

bool dspic33_pwm_test_gpio_pin_is(const Dspic33* cpu, uint8_t port, uint8_t bit, bool expected);
bool dspic33_pwm_test_interrupt_flag(Dspic33* cpu, uint8_t irq);
uint16_t dspic33_pwm_test_base(uint8_t generator);
void dspic33_pwm_test_b1_dead_time_cases(TestState* state, Dspic33* cpu);
void dspic33_pwm_test_b1_update_cases(TestState* state, Dspic33* cpu);
void dspic33_pwm_test_clock_cases(TestState* state, Dspic33* cpu);
void dspic33_pwm_test_configure_generator(Dspic33* cpu, uint8_t generator, uint16_t mode,
                                          uint16_t period, uint16_t duty, uint16_t control);
void dspic33_pwm_test_configure_interrupt(Dspic33* cpu, uint8_t generator, uint16_t vector);
void dspic33_pwm_test_dead_time_cases(TestState* state, Dspic33* cpu);
void dspic33_pwm_test_duty_cases(TestState* state, Dspic33* cpu);
void dspic33_pwm_test_enable_pwm(Dspic33* cpu, uint16_t control);
void dspic33_pwm_test_mode_cases(TestState* state, Dspic33* cpu);
void dspic33_pwm_test_register_cases(TestState* state, Dspic33* cpu);
void dspic33_pwm_test_trigger_cases(TestState* state, Dspic33* cpu);
void dspic33_pwm_test_update_cases(TestState* state, Dspic33* cpu);

#endif
