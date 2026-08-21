#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"
#include "test.h"

enum {
    ADC1_BUFFER = 0x0300u,
    RTCC_ALARM = 0x0620u,
    RTCC_ALARM_CONTROL = 0x0622u,
    RTCC_VALUE = 0x0624u,
    RTCC_CONTROL = 0x0626u,
    NVM_CONTROL = 0x0728u,
    NVM_ADDRESS = 0x072au,
    NVM_ADDRESS_HIGH = 0x072cu,
    MAIN_OSCILLATOR_CONTROL = 0x0742u,
    MAIN_CLOCK_DIVISOR = 0x0744u,
    MAIN_PLL_FEEDBACK = 0x0746u,
    MAIN_OSCILLATOR_TUNING = 0x0748u,
    RESET_CONTROL = 0x0740u,
    INTERRUPT_FLAGS = 0x0800u,
    INTERRUPT_CONTROL_TWO = 0x08c2u,
    TIMER_ONE_CONTROL = 0x0104u,
    PORT_A_LATCH = 0x0e04u,
    NVM_SEQUENCE_BASE = 0x0400u,
    NVM_WRITE_ENABLE = 0x4000u,
    MOVE_KEY_55 = 0x200550u,
    MOVE_KEY_AA = 0x200aa0u,
    WRITE_NVM_KEY = 0x883970u,
    SET_NVM_WRITE = 0xa8e729u,
    OPCODE_NOP = 0x000000u,
    OPCODE_MOV_W1_POST_INCREMENT_W2 = 0x780131u
};

static uint16_t raw_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

static void raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[address + 1u] = (uint8_t)(value >> 8u);
}

static bool configure_primary_frc_reset(Dspic33* cpu) {
    return dspic33_load_configuration_word(cpu, 0xf80006u, 0u) &&
           dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdfu);
}

static bool start_nvm_pair(Dspic33* cpu, uint32_t address, uint32_t value) {
    static const uint32_t sequence[] = {MOVE_KEY_55, WRITE_NVM_KEY, MOVE_KEY_AA,
                                        WRITE_NVM_KEY, SET_NVM_WRITE};
    size_t index;
    dspic33_write_word(cpu, NVM_ADDRESS, (uint16_t)address);
    dspic33_write_word(cpu, NVM_ADDRESS_HIGH, (uint16_t)(address >> 16u));
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | 1u);
    cpu->write_latches[0] = value;
    for (index = 0u; index < sizeof(sequence) / sizeof(sequence[0]); index++) {
        dspic33_load_program_word(cpu, NVM_SEQUENCE_BASE + (uint32_t)index * 2u,
                                  sequence[index]);
    }
    cpu->pc = NVM_SEQUENCE_BASE;
    for (index = 0u; index < sizeof(sequence) / sizeof(sequence[0]); index++) {
        if (dspic33_step(cpu) != DSPIC33_RUNNING) {
            return false;
        }
    }
    return cpu->nvm.active;
}

static void master_clear_register_cases(TestState* state, Dspic33* cpu) {
    uint64_t instructions;
    uint64_t cycles;
    uint64_t device_cycles;
    bool pin_high = false;
    dspic33_reset(cpu, 0u);
    expect(state, configure_primary_frc_reset(cpu), "configure primary FRC reset");
    dspic33_reset(cpu, 0u);
    raw_write_word(cpu, MAIN_OSCILLATOR_CONTROL, 0x6660u);
    raw_write_word(cpu, MAIN_CLOCK_DIVISOR, 0x3740u);
    raw_write_word(cpu, MAIN_PLL_FEEDBACK, 0x0042u);
    raw_write_word(cpu, MAIN_OSCILLATOR_TUNING, 0x002au);
    raw_write_word(cpu, RTCC_ALARM, 0x1234u);
    raw_write_word(cpu, RTCC_ALARM_CONTROL, 0x91a5u);
    raw_write_word(cpu, RTCC_VALUE, 0x5678u);
    raw_write_word(cpu, RTCC_CONTROL, 0x8123u);
    raw_write_word(cpu, ADC1_BUFFER, 0xa55au);
    raw_write_word(cpu, PORT_A_LATCH, 0x8601u);
    dspic33_write_word(cpu, TIMER_ONE_CONTROL, 0xffffu);
    dspic33_write_word(cpu, INTERRUPT_CONTROL_TWO, 0xffffu);
    cpu->io.rtcc.calendar[0] = 0x5678u;
    cpu->io.rtcc.alarm[0] = 0x1234u;
    cpu->io.rtcc.prescaler = 321u;
    cpu->io.rtcc.alarm_output = true;
    cpu->data[0x1000u] = 0xa5u;
    cpu->data[0x2fffu] = 0x5au;
    dspic33_load_program_word(cpu, 0x0200u, 0x00123456u);
    expect(state, dspic33_gpio_drive(cpu, 0u, 0x0001u, 0x0001u),
           "drive external pin before MCLR");
    instructions = cpu->instructions;
    cycles = cpu->cycles;
    device_cycles = cpu->device_cycles;
    dspic33_mclr_reset(cpu);
    expect(state,
           cpu->pc == 0u && cpu->power_state == DSPIC33_POWER_ACTIVE &&
               cpu->stop_reason == DSPIC33_RUNNING,
           "MCLR enters primary reset vector");
    expect(state,
           cpu->instructions == instructions && cpu->cycles == cycles &&
               cpu->device_cycles == device_cycles,
           "MCLR preserves execution accounting");
    expect(state, cpu->data[0x1000u] == 0xa5u && cpu->data[0x2fffu] == 0x5au,
           "MCLR preserves data RAM");
    expect(state, dspic33_read_program_word(cpu, 0x0200u) == 0x00123456u,
           "MCLR preserves program memory");
    expect(state, dspic33_read_word(cpu, TIMER_ONE_CONTROL) == 0u,
           "MCLR resets ordinary peripheral controls");
    expect(state, (dspic33_read_word(cpu, INTERRUPT_CONTROL_TWO) & 0xc01fu) == 0u,
           "MCLR applies target-specific INTCON2 reset");
    expect(state,
           raw_word(cpu, PORT_A_LATCH) == 0x8601u &&
               raw_word(cpu, ADC1_BUFFER) == 0xa55au,
           "MCLR preserves device-pack unchanged SFRs");
    expect(state,
           raw_word(cpu, RTCC_ALARM) == 0x1234u &&
               raw_word(cpu, RTCC_ALARM_CONTROL) == 0x91a5u &&
               raw_word(cpu, RTCC_VALUE) == 0x5678u &&
               raw_word(cpu, RTCC_CONTROL) == 0x8123u,
           "MCLR preserves RTCC registers");
    expect(state,
           cpu->io.rtcc.calendar[0] == 0x5678u && cpu->io.rtcc.alarm[0] == 0x1234u &&
               cpu->io.rtcc.prescaler == 321u && cpu->io.rtcc.alarm_output,
           "MCLR preserves RTCC runtime phase");
    expect(state,
           raw_word(cpu, MAIN_OSCILLATOR_CONTROL) == 0x6660u &&
               raw_word(cpu, MAIN_CLOCK_DIVISOR) == 0x3740u &&
               raw_word(cpu, MAIN_PLL_FEEDBACK) == 0x0042u &&
               raw_word(cpu, MAIN_OSCILLATOR_TUNING) == 0x002au,
           "MCLR preserves oscillator registers");
    expect(state, dspic33_gpio_pin(cpu, 0u, 0u, &pin_high) && pin_high,
           "MCLR preserves external pin level");
}

static void reset_flag_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, RESET_CONTROL, 0xcadfu);
    dspic33_mclr_reset(cpu);
    expect(state,
           (dspic33_read_word(cpu, RESET_CONTROL) & 0xc2dfu) == 0xc2dfu &&
               (raw_word(cpu, RESET_CONTROL) & 0x0820u) == 0x0800u,
           "MCLR preserves causes, sets EXTR and resets controls");
    dspic33_brown_out_reset(cpu);
    expect(state, dspic33_read_word(cpu, RESET_CONTROL) == 0x0083u,
           "BOR clears warm causes and preserves EXTR and POR");
    dspic33_write_word(cpu, RESET_CONTROL, 0u);
    dspic33_brown_out_reset(cpu);
    expect(state, dspic33_read_word(cpu, RESET_CONTROL) == 0x0002u,
           "BOR sets only BOR after software-cleared flags");
    dspic33_mclr_reset(cpu);
    expect(state, dspic33_read_word(cpu, RESET_CONTROL) == 0x0082u,
           "MCLR after BOR preserves BOR and sets EXTR");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, RESET_CONTROL) == 0x0003u,
           "POR clears prior causes and sets POR and BOR");
}

static void brown_out_clock_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, configure_primary_frc_reset(cpu), "configure BOR clock source");
    dspic33_reset(cpu, 0u);
    raw_write_word(cpu, MAIN_OSCILLATOR_CONTROL, 0x7770u);
    raw_write_word(cpu, MAIN_CLOCK_DIVISOR, 0x3740u);
    raw_write_word(cpu, MAIN_PLL_FEEDBACK, 0x0042u);
    raw_write_word(cpu, MAIN_OSCILLATOR_TUNING, 0x002au);
    raw_write_word(cpu, RTCC_CONTROL, 0x8123u);
    cpu->oscillator.generation = 7u;
    cpu->oscillator.active = true;
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_OSCILLATOR, 0u, 7u, 20u),
           "schedule stale oscillator transition before BOR");
    dspic33_brown_out_reset(cpu);
    expect(state,
           cpu->pc == 0u && raw_word(cpu, MAIN_OSCILLATOR_CONTROL) == 0u &&
               !cpu->oscillator.active && cpu->oscillator.generation == 0u,
           "BOR cold-starts configured main oscillator");
    expect(state,
           raw_word(cpu, MAIN_CLOCK_DIVISOR) == 0x3740u &&
               raw_word(cpu, MAIN_PLL_FEEDBACK) == 0x0042u &&
               raw_word(cpu, MAIN_OSCILLATOR_TUNING) == 0x002au &&
               raw_word(cpu, RTCC_CONTROL) == 0x8123u,
           "BOR preserves POR-only oscillator and RTCC registers");
    expect(state, cpu->events.count == 0u,
           "BOR cancels stale main oscillator transition");
}

static void nvm_reset_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, configure_primary_frc_reset(cpu), "configure NVM reset vector");
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x3000u, 0x00ffffffu);
    expect(state, start_nvm_pair(cpu, 0x3000u, 0x00123456u),
           "start NVM operation before MCLR");
    dspic33_mclr_reset(cpu);
    expect(state, cpu->nvm.active && cpu->nvm.reset_pending && cpu->pc == 0x040au,
           "MCLR defers while RTSP is active");
    expect(state, dspic33_device_advance(cpu, 2u),
           "complete NVM operation before deferred MCLR");
    expect(state,
           !cpu->nvm.active && !cpu->nvm.reset_pending && cpu->pc == 0u &&
               dspic33_read_program_word(cpu, 0x3000u) == 0x00123456u &&
               (dspic33_read_word(cpu, RESET_CONTROL) & 0x0080u) != 0u,
           "NVM completion releases deferred MCLR");
    expect(state, (dspic33_read_word(cpu, INTERRUPT_FLAGS) & 0x8000u) == 0u,
           "deferred MCLR suppresses NVM completion interrupt");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x3200u, 0x00ffffffu);
    expect(state, start_nvm_pair(cpu, 0x3200u, 0x00654321u),
           "start NVM operation before BOR");
    dspic33_brown_out_reset(cpu);
    expect(state,
           !cpu->nvm.active && !cpu->nvm.reset_pending && cpu->events.count == 0u &&
               dspic33_read_program_word(cpu, 0x3200u) == 0x00ffffffu,
           "BOR aborts active NVM operation immediately");
    expect(state,
           (dspic33_read_word(cpu, RESET_CONTROL) & 0x0002u) != 0u &&
               (dspic33_read_word(cpu, INTERRUPT_FLAGS) & 0x8000u) == 0u,
           "BOR records cause without NVM completion interrupt");
}

static void copy_cases(TestState* state, Dspic33* source, Dspic33* copy) {
    dspic33_reset(source, 0u);
    expect(state, configure_primary_frc_reset(source), "configure copied reset state");
    dspic33_reset(source, 0u);
    raw_write_word(source, PORT_A_LATCH, 0x8201u);
    raw_write_word(source, ADC1_BUFFER, 0x5aa5u);
    source->data[0x1000u] = 0x42u;
    expect(state, dspic33_copy(copy, source), "copy pre-MCLR state");
    dspic33_mclr_reset(source);
    dspic33_mclr_reset(copy);
    expect(state,
           raw_word(source, PORT_A_LATCH) == raw_word(copy, PORT_A_LATCH) &&
               raw_word(source, ADC1_BUFFER) == raw_word(copy, ADC1_BUFFER) &&
               source->data[0x1000u] == copy->data[0x1000u] &&
               dspic33_read_word(source, RESET_CONTROL) ==
                   dspic33_read_word(copy, RESET_CONTROL),
           "copied state resets independently and identically");
}

static void reset_vector_cases(TestState* state, Dspic33* cpu, Dspic33* protected_cpu) {
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_load_configuration_word(cpu, 0xf80010u, 0x0003u) &&
               dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdbu),
           "configure unprotected auxiliary reset vector");
    dspic33_mclr_reset(cpu);
    expect(state, cpu->pc == 0x007ffffcu && !cpu->reset_locked,
           "MCLR honors auxiliary reset selection");
    dspic33_brown_out_reset(cpu);
    expect(state, cpu->pc == 0x007ffffcu && !cpu->reset_locked,
           "BOR honors auxiliary reset selection");

    dspic33_reset(protected_cpu, 0u);
    expect(state,
           dspic33_load_configuration_word(protected_cpu, 0xf80004u, 0xff03u) &&
               dspic33_load_configuration_word(protected_cpu, 0xf80010u, 0xff31u) &&
               dspic33_load_configuration_word(protected_cpu, 0xf8000eu, 0xffdbu),
           "configure protected auxiliary reset vector");
    dspic33_mclr_reset(protected_cpu);
    expect(state,
           protected_cpu->pc == 0u && protected_cpu->reset_locked &&
               protected_cpu->illegal_reset &&
               (dspic33_read_word(protected_cpu, RESET_CONTROL) & 0x4080u) == 0x4080u,
           "B1 protected auxiliary MCLR enters security reset");
    dspic33_brown_out_reset(protected_cpu);
    expect(state,
           protected_cpu->pc == 0u && protected_cpu->reset_locked &&
               (dspic33_read_word(protected_cpu, RESET_CONTROL) & 0x4002u) == 0x4002u,
           "B1 protected auxiliary BOR enters security reset");
}

static void trap_conflict_cases(TestState* state, Dspic33* cpu) {
    uint64_t instructions;
    uint64_t cycles;
    dspic33_reset(cpu, 0u);
    expect(state, configure_primary_frc_reset(cpu),
           "configure primary trap conflict reset vector");
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x0004u, 0x000300u);
    dspic33_load_program_word(cpu, 0x0300u, OPCODE_NOP);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    dspic33_raise_oscillator_fail_trap(cpu);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 0u &&
               cpu->pc == 0x0302u && (cpu->corcon & 0x0008u) != 0u &&
               (cpu->sr & 0x00e0u) == 0x00e0u,
           "oscillator hard trap establishes priority fifteen");
    instructions = cpu->instructions;
    cycles = cpu->cycles;
    dspic33_set_generic_hard_trap_source(cpu, true);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0u &&
               (dspic33_read_word(cpu, RESET_CONTROL) & 0x8000u) != 0u &&
               !cpu->illegal_reset && cpu->instructions == instructions &&
               cpu->cycles == cycles && cpu->w[15] == 0x1000u,
           "lower-priority hard trap causes conflict reset without continuation");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x0008u, 0x000300u);
    dspic33_load_program_word(cpu, 0x0004u, 0x000320u);
    dspic33_load_program_word(cpu, 0x0300u, OPCODE_NOP);
    dspic33_load_program_word(cpu, 0x0320u, OPCODE_NOP);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    dspic33_set_generic_hard_trap_source(cpu, true);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 2u &&
               cpu->pc == 0x0302u,
           "generic hard trap establishes priority thirteen");
    dspic33_raise_oscillator_fail_trap(cpu);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 0u &&
               cpu->pc == 0x0322u &&
               (dspic33_read_word(cpu, RESET_CONTROL) & 0x8000u) == 0u,
           "higher-priority hard trap nests without conflict reset");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u,
                              0x00ffffffu);
    expect(state,
           start_nvm_pair(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u, 0x00123456u),
           "start opposite-segment NVM operation before trap conflict");
    cpu->corcon |= 0x0008u;
    cpu->sr = (uint16_t)((cpu->sr & ~0x00e0u) | 0x00e0u);
    dspic33_set_generic_hard_trap_source(cpu, true);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
               !cpu->nvm.reset_pending && cpu->pc == 0u &&
               dspic33_read_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE +
                                                  0x3000u) == 0x00123456u &&
               (dspic33_read_word(cpu, RESET_CONTROL) & 0x8000u) != 0u,
           "trap conflict reset follows active RTSP completion");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x0200u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0x1001u);
    cpu->corcon |= 0x000cu;
    cpu->sr = (uint16_t)((cpu->sr & ~0x00e0u) | 0x00e0u);
    cpu->pc = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0u && cpu->cycles == 0u &&
               cpu->corcon == 0x0020u &&
               (dspic33_read_word(cpu, RESET_CONTROL) & 0x8000u) != 0u,
           "direct lower-priority hard trap resets without stale state or timing");
}

int main(void) {
    Dspic33 source;
    Dspic33 copy;
    TestState state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    master_clear_register_cases(&state, &source);
    reset_flag_cases(&state, &source);
    brown_out_clock_cases(&state, &source);
    nvm_reset_cases(&state, &source);
    copy_cases(&state, &source, &copy);
    reset_vector_cases(&state, &source, &copy);
    trap_conflict_cases(&state, &source);
    dspic33_release(&copy);
    dspic33_release(&source);
    return test_finish(&state);
}
