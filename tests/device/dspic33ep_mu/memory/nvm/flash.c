#include "device/dspic33ep_mu/memory/nvm/internal.h"

static void codeguard_configuration_cases(TestState* state, Dspic33* cpu) {
    uint8_t general_index;
    uint8_t auxiliary_index;

    for (general_index = 0u; general_index < 16u; general_index++) {
        for (auxiliary_index = 0u; auxiliary_index < 16u; auxiliary_index++) {
            uint8_t general = dspic33_nvm_test_codeguard_configuration_value(general_index);
            uint8_t auxiliary = dspic33_nvm_test_codeguard_configuration_value(auxiliary_index);
            bool allowed = general == 0x03u && auxiliary == 0x03u;
            bool started;
            bool completed;

            dspic33_reset(cpu, 0u);
            dspic33_nvm_test_load_codeguard_configuration(cpu, general, auxiliary);
            cpu->write_latches[0] = 0x31u;
            started = dspic33_nvm_test_start_operation(cpu, 0u, CODEGUARD_AUXILIARY_CONFIGURATION);
            completed = dspic33_nvm_test_finish_operation(cpu);
            expect(state,
                   started && completed &&
                       dspic33_read_configuration_byte(cpu, CODEGUARD_AUXILIARY_CONFIGURATION) ==
                           (allowed ? 0x31u : auxiliary),
                   "CodeGuard FAS programming requires both unprotected segments");
        }
    }
}

static void codeguard_programming_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t operations[] = {1u, 2u, 3u};
    size_t operation_index;
    uint8_t target_segment;
    uint8_t origin_segment;
    uint8_t configuration_index;

    for (operation_index = 0u; operation_index < sizeof(operations) / sizeof(operations[0]);
         operation_index++) {
        uint16_t operation = operations[operation_index];
        for (target_segment = 0u; target_segment < 2u; target_segment++) {
            uint32_t target =
                target_segment == 0u ? 0x3000u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1000u;
            for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
                uint32_t origin = origin_segment == 0u ? NVM_SEQUENCE_BASE
                                                       : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
                for (configuration_index = 0u; configuration_index < 16u; configuration_index++) {
                    uint8_t configuration =
                        dspic33_nvm_test_codeguard_configuration_value(configuration_index);
                    bool write_protected = (configuration & 0x01u) == 0u;
                    bool high = dspic33_nvm_test_codeguard_configuration_high(configuration);
                    bool allowed = !write_protected && (!high || origin_segment == target_segment);
                    uint32_t initial = operation == 3u ? 0u : 0x00ffffffu;
                    uint32_t expected = operation == 3u ? allowed ? 0x00ffffffu : 0u
                                        : allowed       ? 0x00123456u
                                                        : 0x00ffffffu;
                    bool started;
                    bool completed;

                    dspic33_reset(cpu, 0u);
                    dspic33_nvm_test_load_codeguard_configuration(
                        cpu, target_segment == 0u ? configuration : 0x03u,
                        target_segment == 0u ? 0x03u : configuration);
                    dspic33_load_program_word(cpu, target, initial);
                    cpu->write_latches[0] = 0x00123456u;
                    started = dspic33_nvm_test_start_operation_from(cpu, operation, target, origin);
                    completed = dspic33_nvm_test_finish_operation(cpu);
                    expect(state,
                           started && completed &&
                               cpu->nvm.auxiliary_origin == (origin_segment != 0u) &&
                               dspic33_nvm_test_program_word(cpu, target) == expected,
                           "CodeGuard row and page programming matrix");
                }
            }
        }
    }

    for (operation_index = 0u; operation_index < sizeof(operations) / sizeof(operations[0]);
         operation_index++) {
        uint16_t operation = operations[operation_index];
        for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
            uint32_t origin =
                origin_segment == 0u ? NVM_SEQUENCE_BASE : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
            for (configuration_index = 0u; configuration_index < 16u; configuration_index++) {
                uint8_t configuration =
                    dspic33_nvm_test_codeguard_configuration_value(configuration_index);
                bool allowed = (configuration & 0x01u) != 0u &&
                               !dspic33_nvm_test_codeguard_configuration_high(configuration);
                uint32_t target = operation == 3u ? 0u : 0x0100u;
                uint32_t initial = operation == 3u ? 0u : 0x00ffffffu;
                uint32_t expected = operation == 3u ? allowed ? 0x00ffffffu : 0u
                                    : allowed       ? 0x00123456u
                                                    : 0x00ffffffu;
                bool started;
                bool completed;

                dspic33_reset(cpu, 0u);
                dspic33_nvm_test_load_codeguard_configuration(cpu, configuration, 0x03u);
                dspic33_load_program_word(cpu, target, initial);
                cpu->write_latches[0] = 0x00123456u;
                started = dspic33_nvm_test_start_operation_from(cpu, operation, target, origin);
                completed = dspic33_nvm_test_finish_operation(cpu);
                expect(state,
                       started && completed &&
                           dspic33_nvm_test_program_word(cpu, target) == expected,
                       "CodeGuard IVT programming matrix");
            }
        }
    }
}

static void codeguard_segment_erase_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t operations[] = {0x0au, 0x0du};
    size_t operation_index;
    uint8_t origin_segment;

    for (operation_index = 0u; operation_index < sizeof(operations) / sizeof(operations[0]);
         operation_index++) {
        uint16_t operation = operations[operation_index];
        bool auxiliary_target = operation == 0x0au;
        uint32_t target = auxiliary_target ? DSPIC33_AUXILIARY_PROGRAM_BASE : 0x2000u;
        for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
            uint32_t origin =
                origin_segment == 0u ? NVM_SEQUENCE_BASE : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
            bool allowed = auxiliary_target != (origin_segment != 0u);
            bool started;
            bool completed;

            dspic33_reset(cpu, 0u);
            dspic33_nvm_test_load_codeguard_configuration(cpu, 0x30u, 0x30u);
            dspic33_load_program_word(cpu, target, 0u);
            started = dspic33_nvm_test_start_operation_from(cpu, operation, 0u, origin);
            completed = dspic33_nvm_test_finish_operation(cpu);
            expect(state,
                   started && completed &&
                       dspic33_nvm_test_program_word(cpu, target) == (allowed ? 0x00ffffffu : 0u) &&
                       dspic33_read_configuration_byte(
                           cpu, auxiliary_target
                                    ? CODEGUARD_AUXILIARY_CONFIGURATION
                                    : CODEGUARD_GENERAL_CONFIGURATION) == (allowed ? 0xcfu : 0x30u),
                   "CodeGuard segment erase requires opposite-segment execution");
        }
    }
}

static void codeguard_origin_capture_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    uint32_t target = DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1000u;
    bool copy_initialized;

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, target, 0x00ffffffu);
    cpu->write_latches[0] = 0x00123456u;
    expect(state, dspic33_nvm_test_start_operation_from(cpu, 1u, target, NVM_SEQUENCE_BASE),
           "CodeGuard captures general NVM origin");
    cpu->pc = DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
    expect(state,
           dspic33_nvm_test_finish_operation(cpu) && !cpu->nvm.auxiliary_origin &&
               dspic33_nvm_test_program_word(cpu, target) == 0x00ffffffu,
           "CodeGuard preserves rejected cross-segment origin until completion");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, target, 0x00ffffffu);
    cpu->write_latches[0] = 0x00123456u;
    expect(state,
           dspic33_nvm_test_start_operation_from(cpu, 1u, target,
                                                 DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u),
           "CodeGuard captures auxiliary NVM origin");
    cpu->pc = NVM_SEQUENCE_BASE;
    expect(state,
           dspic33_nvm_test_finish_operation(cpu) && cpu->nvm.auxiliary_origin &&
               dspic33_nvm_test_program_word(cpu, target) == 0x00123456u,
           "CodeGuard preserves accepted self-segment origin until completion");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, target, 0x00ffffffu);
    cpu->write_latches[0] = 0x00654321u;
    expect(state,
           dspic33_nvm_test_start_operation_from(cpu, 1u, target,
                                                 DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u),
           "CodeGuard auxiliary-origin copy operation starts");
    copy_initialized = dspic33_initialize(&copy);
    expect(state,
           copy_initialized && dspic33_copy(&copy, cpu) && copy.nvm.active &&
               copy.nvm.auxiliary_origin,
           "CodeGuard copy retains active auxiliary origin");
    if (copy_initialized) {
        cpu->pc = NVM_SEQUENCE_BASE;
        copy.pc = NVM_SEQUENCE_BASE;
        expect(state,
               dspic33_nvm_test_finish_operation(cpu) && dspic33_nvm_test_finish_operation(&copy) &&
                   dspic33_nvm_test_program_word(cpu, target) == 0x00654321u &&
                   dspic33_nvm_test_program_word(&copy, target) == 0x00654321u &&
                   cpu->nvm.auxiliary_origin && copy.nvm.auxiliary_origin,
               "CodeGuard copied auxiliary origins complete independently");
        dspic33_release(&copy);
    }
}

static void codeguard_persistent_read_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t targets[] = {PERSISTENT_PROGRAM_BASE,
                                       PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE};
    static const uint8_t configurations[] = {0x03u, 0x31u};
    size_t target_index;
    size_t configuration_index;
    uint8_t origin_segment;

    for (target_index = 0u; target_index < sizeof(targets) / sizeof(targets[0]); target_index++) {
        for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
            uint32_t origin =
                origin_segment == 0u ? 0x1000u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
            for (configuration_index = 0u; configuration_index < sizeof(configurations);
                 configuration_index++) {
                uint8_t configuration = configurations[configuration_index];
                bool allowed = origin_segment == 0u || configuration == 0x03u;

                dspic33_reset(cpu, 0u);
                dspic33_nvm_test_load_codeguard_configuration(cpu, configuration, 0x03u);
                dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE, 0x00ab1357u);
                expect(state,
                       dspic33_nvm_test_execute_codeguard_table_read(
                           cpu, origin, targets[target_index]) == (allowed ? 0x1357u : 0u) &&
                           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_BASE) ==
                               0x00ab1357u &&
                           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG +
                                                                  PERSISTENT_PROGRAM_BASE) ==
                               0x00ab1357u,
                       "CodeGuard physical and tagged persistent table reads");
            }
        }
    }
}

static void codeguard_read_cases(TestState* state, Dspic33* cpu) {
    uint8_t target_segment;
    uint8_t origin_segment;
    uint8_t configuration_index;

    for (target_segment = 0u; target_segment < 2u; target_segment++) {
        uint32_t target = target_segment == 0u ? 0x4000u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0100u;
        for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
            uint32_t origin =
                origin_segment == 0u ? 0x1000u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
            for (configuration_index = 0u; configuration_index < 16u; configuration_index++) {
                uint8_t configuration =
                    dspic33_nvm_test_codeguard_configuration_value(configuration_index);
                bool allowed = origin_segment == target_segment ||
                               !dspic33_nvm_test_codeguard_configuration_high(configuration);

                dspic33_reset(cpu, 0u);
                dspic33_nvm_test_load_codeguard_configuration(
                    cpu, target_segment == 0u ? configuration : 0x03u,
                    target_segment == 0u ? 0x03u : configuration);
                dspic33_load_program_word(cpu, target, 0x00ab1357u);
                expect(state,
                       dspic33_nvm_test_execute_codeguard_table_read(cpu, origin, target) ==
                               (allowed ? 0x1357u : 0u) &&
                           dspic33_nvm_test_program_word(cpu, target) == 0x00ab1357u,
                       "CodeGuard cross-segment table-read matrix");
            }
        }
    }

    for (target_segment = 0u; target_segment < 2u; target_segment++) {
        uint32_t target = target_segment == 0u ? 0x4000u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0100u;
        for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
            uint32_t origin =
                origin_segment == 0u ? 0x1000u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
            static const uint8_t configurations[] = {0x03u, 0x31u};
            size_t index;
            for (index = 0u; index < sizeof(configurations); index++) {
                uint8_t configuration = configurations[index];
                bool allowed = origin_segment == target_segment ||
                               !dspic33_nvm_test_codeguard_configuration_high(configuration);

                dspic33_reset(cpu, 0u);
                dspic33_nvm_test_load_codeguard_configuration(
                    cpu, target_segment == 0u ? configuration : 0x03u,
                    target_segment == 0u ? 0x03u : configuration);
                dspic33_load_program_word(cpu, target, 0x00ab1357u);
                expect(state,
                       dspic33_nvm_test_execute_codeguard_psv_read(cpu, origin, target) ==
                               (allowed ? 0x1357u : 0u) &&
                           dspic33_nvm_test_program_word(cpu, target) == 0x00ab1357u,
                       "CodeGuard cross-segment PSV matrix");
            }
        }
    }
}

static void codeguard_program_flow_configuration_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t targets[] = {DSPIC33_AUXILIARY_PROGRAM_LIMIT - 66u,
                                       DSPIC33_AUXILIARY_PROGRAM_LIMIT - 64u};
    const uint32_t origin = 0x4000u;
    uint8_t configuration_index;
    size_t target_index;
    for (configuration_index = 0u; configuration_index < 16u; configuration_index++) {
        uint8_t configuration = dspic33_nvm_test_codeguard_configuration_value(configuration_index);
        bool high = dspic33_nvm_test_codeguard_configuration_high(configuration);
        for (target_index = 0u; target_index < sizeof(targets) / sizeof(targets[0]);
             target_index++) {
            uint32_t target = targets[target_index];
            uint64_t reset_count;
            bool allowed = !high || target_index != 0u;
            dspic33_reset(cpu, origin);
            dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, configuration);
            dspic33_nvm_test_load_long_program_flow(cpu, origin, target, false);
            reset_count = cpu->illegal_reset_count;
            expect(state,
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                       (allowed ? !cpu->illegal_reset && cpu->illegal_reset_count == reset_count &&
                                      cpu->pc == target
                                : dspic33_nvm_test_codeguard_security_reset(cpu, reset_count)),
                   "CodeGuard PFC configuration matrix");
        }
    }
}

static void codeguard_program_flow_instruction_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t targets[] = {DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u,
                                       DSPIC33_AUXILIARY_PROGRAM_LIMIT - 64u};
    const uint32_t origin = 0x4200u;
    uint64_t cycles;
    uint64_t device_cycles;
    uint64_t reset_count;
    uint64_t trap_count;
    size_t target_index;
    for (target_index = 0u; target_index < sizeof(targets) / sizeof(targets[0]); target_index++) {
        uint32_t target = targets[target_index];
        bool allowed = target_index != 0u;

        dspic33_reset(cpu, origin);
        dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_nvm_test_load_long_program_flow(cpu, origin, target, true);
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target && cpu->call_depth == 1u
                            : dspic33_nvm_test_codeguard_security_reset(cpu, reset_count)),
               "CodeGuard long CALL PFC");

        dspic33_reset(cpu, origin);
        dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_load_program_word(cpu, origin, OPCODE_COMPUTED_GOTO_W0);
        dspic33_set_working_register(cpu, 0u, (uint16_t)target);
        dspic33_set_working_register(cpu, 1u, (uint16_t)(target >> 16u));
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target
                            : dspic33_nvm_test_codeguard_security_reset(cpu, reset_count)),
               "CodeGuard computed GOTO PFC");

        dspic33_reset(cpu, origin);
        dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_load_program_word(cpu, origin, OPCODE_COMPUTED_CALL_W0);
        dspic33_set_working_register(cpu, 0u, (uint16_t)target);
        dspic33_set_working_register(cpu, 1u, (uint16_t)(target >> 16u));
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target && cpu->call_depth == 1u
                            : dspic33_nvm_test_codeguard_security_reset(cpu, reset_count)),
               "CodeGuard computed CALL PFC");

        dspic33_reset(cpu, origin);
        dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_nvm_test_load_program_return(cpu, origin, target, OPCODE_RETURN);
        cpu->call_depth = 1u;
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target && cpu->call_depth == 0u
                            : dspic33_nvm_test_codeguard_security_reset(cpu, reset_count)),
               "CodeGuard RETURN PFC");

        dspic33_reset(cpu, origin);
        dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_nvm_test_load_program_return(cpu, origin, target, OPCODE_RETFIE);
        cpu->interrupt_depth = 1u;
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target && cpu->interrupt_depth == 0u
                            : dspic33_nvm_test_codeguard_security_reset(cpu, reset_count)),
               "CodeGuard RETFIE PFC");
    }

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x200u);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_nvm_test_load_long_program_flow(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x200u,
                                            DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u, false);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u && !cpu->illegal_reset,
           "CodeGuard same Auxiliary Segment PFC");

    dspic33_reset(cpu, origin);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x03u);
    dspic33_nvm_test_load_long_program_flow(cpu, origin, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u,
                                            false);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u && !cpu->illegal_reset,
           "CodeGuard unprotected Auxiliary Segment PFC");

    dspic33_reset(cpu, origin);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x31u, 0x31u);
    dspic33_nvm_test_load_long_program_flow(cpu, origin, 0x4400u, false);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x4400u && !cpu->illegal_reset,
           "CodeGuard General Segment PFC");

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_nvm_test_load_long_program_flow(cpu, origin, 0x060000u, false);
    dspic33_load_program_word(cpu, 0x0006u, 0x004400u);
    dspic33_load_program_word(cpu, 0x4400u, OPCODE_NOP);
    reset_count = cpu->illegal_reset_count;
    trap_count = cpu->trap_count;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 1u &&
               cpu->trap_count == trap_count + 1u && cpu->illegal_reset_count == reset_count &&
               cpu->pc == 0x4400u,
           "unimplemented PFC takes Address Error before CodeGuard");

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_nvm_test_load_long_program_flow(cpu, origin, 0x060000u, false);
    dspic33_load_program_word(cpu, 0x0006u, DSPIC33_AUXILIARY_PROGRAM_LIMIT - 66u);
    cpu->corcon |= 0x0004u;
    cycles = cpu->cycles;
    device_cycles = cpu->device_cycles;
    reset_count = cpu->illegal_reset_count;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_nvm_test_codeguard_security_reset(cpu, reset_count) &&
               cpu->corcon == 0x0020u && cpu->cycles == cycles &&
               cpu->device_cycles == device_cycles,
           "restricted Address Error VFC retains reset CORCON");
}

static void codeguard_vector_flow_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t targets[] = {DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u,
                                       DSPIC33_AUXILIARY_PROGRAM_LIMIT - 64u};
    const uint32_t origin = 0x4600u;
    size_t target_index;
    for (target_index = 0u; target_index < sizeof(targets) / sizeof(targets[0]); target_index++) {
        uint32_t target = targets[target_index];
        bool allowed = target_index != 0u;
        uint64_t reset_count;

        dspic33_reset(cpu, origin);
        cpu->stop_on_trap = false;
        dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_load_program_word(cpu, 0x0014u, target);
        dspic33_load_program_word(cpu, target, OPCODE_NOP);
        dspic33_write_word(cpu, 0x0820u, 0x0001u);
        dspic33_write_word(cpu, 0x0840u, 0x0001u);
        dspic33_raise_interrupt(cpu, 0u);
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target + 2u && cpu->last_interrupt == 0u
                            : dspic33_nvm_test_codeguard_security_reset(cpu, reset_count)),
               "CodeGuard interrupt VFC");

        dspic33_reset(cpu, origin);
        dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_load_program_word(cpu, 0x0004u, target);
        dspic33_load_program_word(cpu, target, OPCODE_NOP);
        dspic33_raise_oscillator_fail_trap(cpu);
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target + 2u && cpu->last_trap == 0u
                            : dspic33_nvm_test_codeguard_security_reset(cpu, reset_count)),
               "CodeGuard trap VFC");
    }

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, 0x0014u, 0x060000u);
    dspic33_load_program_word(cpu, 0x0006u, 0x004400u);
    dspic33_load_program_word(cpu, 0x4400u, OPCODE_NOP);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    dspic33_raise_interrupt(cpu, 0u);
    uint64_t trap_count = cpu->trap_count;
    uint64_t reset_count = cpu->illegal_reset_count;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 1u &&
               cpu->trap_count == trap_count + 1u && cpu->pc == 0x4402u &&
               cpu->illegal_reset_count == reset_count,
           "unimplemented interrupt VFC dispatches Address Error");

    dspic33_reset(cpu, origin);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, 0x000eu, 0x060000u);
    dspic33_load_program_word(cpu, 0x0006u, 0x004400u);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    trap_count = cpu->trap_count;
    reset_count = cpu->illegal_reset_count;
    dspic33_raise_dma_collision_trap(cpu);
    expect(state,
           cpu->last_trap == 1u && cpu->trap_count == trap_count + 1u && cpu->pc == 0x4400u &&
               cpu->illegal_reset_count == reset_count &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x0028u) == 0x0028u,
           "unimplemented synchronous trap VFC dispatches Address Error");

    dspic33_reset(cpu, origin);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_LITERAL_0X1234_W2);
    dspic33_load_program_word(cpu, 0x0014u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_raise_interrupt(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    uint64_t instructions = cpu->instructions;
    reset_count = cpu->illegal_reset_count;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_nvm_test_codeguard_security_reset(cpu, reset_count) &&
               cpu->instructions == instructions && cpu->w[2] == 0u,
           "sleeping restricted interrupt VFC stops at security reset");
}

static void vector_segment_execution_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t vector_addresses[] = {0x000002u, 0x000004u, 0x000100u, 0x0001feu};
    static const uint32_t vector_opcodes[] = {OPCODE_NOP, OPCODE_SLEEP};
    const uint32_t handler = 0x000400u;
    const uint32_t origin = 0x004600u;
    size_t index;

    for (index = 0u; index < sizeof(vector_addresses) / sizeof(vector_addresses[0]); index++) {
        uint32_t address = vector_addresses[index];
        uint64_t cycles;
        uint64_t instructions;
        uint64_t trap_count;

        dspic33_reset(cpu, address);
        cpu->stop_on_trap = true;
        dspic33_load_program_word(cpu, address, OPCODE_MOV_LITERAL_0X1234_W2);
        dspic33_load_program_word(cpu, 0x000006u, handler);
        dspic33_load_program_word(cpu, handler, OPCODE_NOP);
        dspic33_set_working_register(cpu, 15u, 0x1000u);
        dspic33_set_working_register(cpu, 2u, 0xa5a5u);
        cycles = cpu->cycles;
        instructions = cpu->instructions;
        trap_count = cpu->trap_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 1u &&
                   cpu->trap_count == trap_count + 1u && cpu->pc == handler &&
                   cpu->last_trap_return == address + 2u && cpu->w[2] == 0xa5a5u &&
                   cpu->w[15] == 0x1004u && dspic33_read_word(cpu, 0x1000u) == address + 2u &&
                   cpu->instructions == instructions && cpu->cycles == cycles + 1u,
               "vector-segment instruction fetch raises Address Error");
    }

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_load_long_program_flow(cpu, 0u, 0x000600u, false);
    dspic33_load_program_word(cpu, 0x000600u, OPCODE_MOV_LITERAL_0X1234_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000600u &&
               cpu->instructions == 1u && cpu->cycles == 4u,
           "primary Reset GOTO reads its address extension");

    dspic33_reset(cpu, 0x000200u);
    dspic33_load_program_word(cpu, 0x000200u, OPCODE_MOV_LITERAL_0X1234_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000202u && cpu->w[2] == 0x1234u,
           "General Segment code after IVT remains executable");

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_nvm_test_load_long_program_flow(cpu, origin, 0x000100u, false);
    dspic33_load_program_word(cpu, 0x000100u, OPCODE_MOV_LITERAL_0X1234_W2);
    dspic33_load_program_word(cpu, 0x000006u, handler);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    dspic33_set_working_register(cpu, 2u, 0xa5a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000100u &&
               cpu->last_trap == UINT16_MAX && !cpu->illegal_reset,
           "PFC can target the vector segment");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 1u && cpu->pc == handler &&
               cpu->last_trap_return == 0x000102u && cpu->w[2] == 0xa5a5u &&
               cpu->instructions == 1u && cpu->cycles == 5u,
           "execution after vector-segment PFC raises Address Error");

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    dspic33_load_program_word(cpu, 0x000014u, 0x000100u);
    dspic33_load_program_word(cpu, 0x000100u, OPCODE_MOV_LITERAL_0X1234_W2);
    dspic33_load_program_word(cpu, 0x000006u, handler);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_interrupt == 0u &&
               cpu->interrupt_count == 1u && cpu->last_trap == 1u && cpu->trap_count == 1u &&
               cpu->pc == handler && cpu->last_trap_return == 0x000102u && cpu->w[15] == 0x1008u &&
               cpu->instructions == 0u && cpu->cycles == 10u,
           "interrupt VFC into vector segment traps before execution");

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    dspic33_load_program_word(cpu, 0x000004u, 0x000100u);
    dspic33_load_program_word(cpu, 0x000100u, OPCODE_MOV_LITERAL_0X1234_W2);
    dspic33_load_program_word(cpu, 0x000006u, handler);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    dspic33_raise_oscillator_fail_trap(cpu);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0u && cpu->trap_count == 1u &&
               cpu->last_trap == UINT16_MAX && cpu->w[15] == 0x1000u && cpu->instructions == 0u &&
               cpu->cycles == 0u && cpu->corcon == 0x0020u &&
               (dspic33_read_word(cpu, 0x0740u) & 0x8000u) != 0u,
           "lower-priority Address Error during hard-trap VFC causes conflict reset");

    for (index = 0u; index < sizeof(vector_opcodes) / sizeof(vector_opcodes[0]); index++) {
        dspic33_reset(cpu, 0x000100u);
        cpu->stop_on_trap = false;
        dspic33_load_program_word(cpu, 0x000100u, vector_opcodes[index]);
        dspic33_load_program_word(cpu, 0x000014u, 0x000300u);
        dspic33_load_program_word(cpu, 0x000300u, OPCODE_NOP);
        dspic33_set_working_register(cpu, 15u, 0x1000u);
        dspic33_write_word(cpu, 0x0820u, 0x0001u);
        dspic33_write_word(cpu, 0x0840u, 0x0001u);
        dspic33_raise_interrupt(cpu, 0u);
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->interrupt_count == 1u &&
                   cpu->last_interrupt == 0u && cpu->pc == 0x000302u && cpu->trap_count == 0u &&
                   cpu->last_trap == UINT16_MAX && cpu->instructions == 1u && cpu->w[15] == 0x1004u,
               "vector contents do not alter pending interrupt predispatch");
    }

    dspic33_reset(cpu, origin);
    dspic33_load_program_word(cpu, 0x000100u, 0x00ab1357u);
    expect(state,
           dspic33_nvm_test_execute_codeguard_table_read(cpu, origin, 0x000100u) == 0x1357u &&
               dspic33_nvm_test_program_word(cpu, 0x000100u) == 0x00ab1357u,
           "vector-segment data reads remain permitted");

    dspic33_reset(cpu, 0x000100u);
    cpu->stop_on_trap = false;
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, 0x000006u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u);
    uint64_t reset_count = cpu->illegal_reset_count;
    uint64_t cycles = cpu->cycles;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_nvm_test_codeguard_security_reset(cpu, reset_count) && cpu->cycles == cycles,
           "restricted Address Error vector retains security-reset precedence");
}

void dspic33_nvm_test_codeguard_cases(TestState* state, Dspic33* cpu) {
    codeguard_configuration_cases(state, cpu);
    codeguard_programming_cases(state, cpu);
    codeguard_segment_erase_cases(state, cpu);
    codeguard_origin_capture_cases(state, cpu);
    codeguard_read_cases(state, cpu);
    codeguard_persistent_read_cases(state, cpu);
    codeguard_program_flow_configuration_cases(state, cpu);
    codeguard_program_flow_instruction_cases(state, cpu);
    codeguard_vector_flow_cases(state, cpu);
    vector_segment_execution_cases(state, cpu);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE, 0x00ffffffu);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x03u);
}
