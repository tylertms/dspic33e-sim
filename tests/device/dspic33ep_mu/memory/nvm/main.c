#include "device/dspic33ep_mu/memory/nvm/internal.h"

static void async_suppression_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x3a00u),
           "suppressed-events operation starts");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "suppressed-events completion cycle");
    expect(state, !cpu->nvm.active && cpu->events.count == 0u,
           "suppressed-events completion removes NVM event");
    expect(state, dspic33_nvm_test_interrupt_flag(cpu),
           "suppressed-events completion raises NVMIF");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x3c00u),
           "disable-during operation starts");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 5u),
           "queue unrelated event during NVM");
    dspic33_set_async_events(cpu, false);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "disabled-during completion cycle");
    expect(state, !cpu->nvm.active && cpu->events.count == 1u,
           "disabled-during keeps unrelated event only");
    dspic33_write_word(cpu, 0x0800u, 0u);
    dspic33_set_async_events(cpu, true);
    expect(state, dspic33_device_advance(cpu, 5u), "advance retained unrelated event");
    expect(state, (dspic33_read_word(cpu, 0x0800u) & 0x0002u) != 0u,
           "retained unrelated event dispatched");
    expect(state, (dspic33_read_word(cpu, 0x0800u) & 0x8000u) == 0u,
           "stale NVM event does not redispatch");

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x3e00u), "reenabled operation starts");
    dspic33_set_async_events(cpu, true);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "reenabled operation completion cycle");
    expect(state, !cpu->nvm.active && cpu->events.count == 0u,
           "reenabled operation completes without stale event");
}

static void reset_copy_and_failure_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized;
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x3400u, 0x00ffffffu);
    cpu->write_latches[0] = 0x00112233u;
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x3400u),
           "reset-abort operation starts");
    dspic33_reset(cpu, 0u);
    expect(state, !cpu->nvm.active && cpu->events.count == 0u, "POR aborts operation and event");
    expect(state, dspic33_read_word(cpu, NVM_CONTROL) == 0u, "POR clears WR and WRERR");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x3400u) == 0x00ffffffu,
           "POR-aborted operation does not program");
    expect(state, cpu->write_latches[0] == 0x00ffffffu, "POR resets write latches");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize NVM copy");
    if (initialized) {
        dspic33_reset(cpu, 0u);
        dspic33_load_program_word(cpu, 0x3600u, 0x00ffffffu);
        cpu->write_latches[0] = 0x00010203u;
        cpu->write_latches[1] = 0x00040506u;
        expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x3600u), "copy operation starts");
        expect(state, dspic33_copy(&copy, cpu), "copy active NVM state");
        expect(state, copy.nvm.active && copy.events.count == 1u, "copy retains active event");
        expect(state,
               copy.nvm.address == cpu->nvm.address && copy.nvm.latches[1] == cpu->nvm.latches[1],
               "copy retains captured operation");
        expect(state, dspic33_nvm_test_finish_operation(&copy), "copied operation completes");
        expect(state,
               dspic33_nvm_test_program_word(&copy, 0x3600u) == 0x00010203u &&
                   dspic33_nvm_test_program_word(&copy, 0x3602u) == 0x00040506u,
               "copied event programs captured pair");
        expect(state, cpu->nvm.active && dspic33_nvm_test_program_word(cpu, 0x3600u) == 0x00ffffffu,
               "source operation remains independent");
        dspic33_release(&copy);
    }

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x3800u, true);
    cpu->cycles = 1u;
    cpu->device_cycles = UINT64_MAX;
    cpu->instructions = 1u;
    cpu->nvm.key_stage = 2u;
    cpu->nvm.key_instruction = 0u;
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | NVM_WRITE | 1u);
    expect(state, !cpu->nvm.active, "event overflow cancels operation");
    expect(state, cpu->events.count == 0u, "event overflow queues no completion");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) == 0u,
           "event overflow clears WR");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "event overflow sets WRERR");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | NVM_WRITE_ERROR | 2u);
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "software reset executes");
    expect(state, cpu->software_reset_count == 1u, "software reset counted");
    expect(state, dspic33_read_word(cpu, NVM_CONTROL) == (NVM_WRITE_ENABLE | NVM_WRITE_ERROR | 2u),
           "warm reset preserves NVMCON POR-only fields");
}

static void deferred_reset_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t targets[] = {DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3600u, 0x3e00u};
    static const uint32_t origins[] = {NVM_SEQUENCE_BASE + 10u,
                                       DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3800u};
    Dspic33 copy;
    bool copy_initialized;
    size_t index;

    for (index = 0u; index < sizeof(targets) / sizeof(targets[0]); index++) {
        uint64_t reset_count;
        dspic33_reset(cpu, 0u);
        dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x03u);
        cpu->write_latches[0] = 0x00112233u;
        cpu->write_latches[1] = 0x00445566u;
        expect(state,
               dspic33_nvm_test_start_operation_from(cpu, 1u, targets[index], origins[index]),
               "deferred software-reset operation starts");
        dspic33_load_program_word(cpu, cpu->pc, OPCODE_RESET);
        reset_count = cpu->software_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
                   !cpu->nvm.reset_pending && cpu->software_reset_count == reset_count + 1u &&
                   cpu->pc == 0u &&
                   dspic33_nvm_test_program_word(cpu, targets[index]) == 0x00112233u &&
                   dspic33_nvm_test_program_word(cpu, targets[index] + 2u) == 0x00445566u &&
                   (dspic33_read_word(cpu, 0x0740u) & 0x0040u) != 0u &&
                   (dspic33_read_word(cpu, NVM_CONTROL) & (NVM_WRITE | NVM_WRITE_ERROR)) == 0u &&
                   !dspic33_nvm_test_interrupt_flag(cpu),
               "software reset waits for opposite-segment programming");
    }

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x03u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x200u, 0x00123456u);
    expect(state, dspic33_nvm_test_start_operation(cpu, 0x0au, 0u),
           "deferred security-reset erase starts");
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, cpu->pc, OPCODE_COMPUTED_GOTO_W0);
    {
        const uint32_t auxiliary_address = DSPIC33_AUXILIARY_PROGRAM_BASE + UINT32_C(0x100);
        dspic33_set_working_register(cpu, 0u, (uint16_t)auxiliary_address);
        dspic33_set_working_register(cpu, 1u, (uint16_t)(auxiliary_address >> 16u));
    }
    {
        uint64_t reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
                   !cpu->nvm.reset_pending && cpu->illegal_reset_count == reset_count + 1u &&
                   cpu->illegal_reset && cpu->pc == 0u &&
                   dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x200u) ==
                       0x00ffffffu &&
                   (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
                   (dspic33_read_word(cpu, NVM_CONTROL) & (NVM_WRITE | NVM_WRITE_ERROR)) == 0u &&
                   !dspic33_nvm_test_interrupt_flag(cpu),
               "security reset waits for Auxiliary Segment erase");
    }

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x03u);
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state,
           dspic33_nvm_test_start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3e00u),
           "deferred illegal-source operation starts");
    dspic33_load_program_word(cpu, cpu->pc, OPCODE_ADD_W2_W4_POST_INCREMENT_W5_POST_DECREMENT);
    dspic33_set_working_register(cpu, 2u, 1u);
    cpu->w[4] = 0x1000u;
    dspic33_set_working_register(cpu, 5u, 0x5000u);
    dspic33_write_word(cpu, 0x1000u, 2u);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    {
        uint64_t reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
                   !cpu->nvm.reset_pending && cpu->illegal_reset &&
                   cpu->illegal_reset_count == reset_count + 1u && cpu->pc == 0u &&
                   dspic33_read_word(cpu, 0x5000u) == 0xaaaau &&
                   dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3e00u) ==
                       0x00123456u &&
                   dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3e02u) ==
                       0x00654321u &&
                   (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
                   !dspic33_nvm_test_interrupt_flag(cpu),
               "deferred illegal source inhibits RAM write before reset");
    }

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x03u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x400u, 0x00123456u);
    expect(state, dspic33_nvm_test_start_operation(cpu, 0x0au, 0u),
           "deferred restricted-vector erase starts");
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, 0x0014u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    dspic33_write_word(cpu, 0x5000u, 0xa55au);
    dspic33_write_word(cpu, 0x5002u, 0x5aa5u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    dspic33_raise_interrupt(cpu, 0u);
    {
        uint64_t reset_count = cpu->illegal_reset_count;
        uint64_t interrupt_count = cpu->interrupt_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
                   !cpu->nvm.reset_pending && cpu->illegal_reset &&
                   cpu->illegal_reset_count == reset_count + 1u && cpu->pc == 0u &&
                   cpu->interrupt_count == interrupt_count &&
                   dspic33_read_word(cpu, 0x5000u) == 0xa55au &&
                   dspic33_read_word(cpu, 0x5002u) == 0x5aa5u &&
                   dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x400u) ==
                       0x00ffffffu &&
                   (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
                   !dspic33_nvm_test_interrupt_flag(cpu),
               "restricted vector reset inhibits frame before NVM completion");
    }

    copy_initialized = dspic33_initialize(&copy);
    expect(state, copy_initialized, "initialize deferred-reset copy");
    if (copy_initialized) {
        dspic33_reset(cpu, 0u);
        dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x03u);
        cpu->write_latches[0] = 0x00010203u;
        cpu->write_latches[1] = 0x00040506u;
        expect(state,
               dspic33_nvm_test_start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3a00u),
               "copied deferred-reset operation starts");
        dspic33_configuration_mismatch_reset(cpu);
        expect(state, cpu->nvm.active && cpu->nvm.reset_pending,
               "configuration-mismatch reset remains pending");
        expect(state, dspic33_copy(&copy, cpu), "copy deferred-reset state");
        expect(state,
               dspic33_nvm_test_finish_operation(cpu) && dspic33_nvm_test_finish_operation(&copy),
               "copied deferred resets complete independently");
        expect(state,
               !cpu->nvm.active && !copy.nvm.active && !cpu->nvm.reset_pending &&
                   !copy.nvm.reset_pending && cpu->pc == 0u && copy.pc == 0u &&
                   (dspic33_read_word(cpu, 0x0740u) & 0x0200u) != 0u &&
                   (dspic33_read_word(&copy, 0x0740u) & 0x0200u) != 0u &&
                   dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3a00u) ==
                       0x00010203u &&
                   dspic33_nvm_test_program_word(&copy, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3a00u) ==
                       0x00010203u &&
                   !dspic33_nvm_test_interrupt_flag(cpu) && !dspic33_nvm_test_interrupt_flag(&copy),
               "copied configuration-mismatch resets follow NVM completion");
        dspic33_release(&copy);
    }

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x03u);
    cpu->write_latches[0] = 0x00654321u;
    expect(state,
           dspic33_nvm_test_start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3c00u),
           "POR pending-reset operation starts");
    dspic33_configuration_mismatch_reset(cpu);
    expect(state, cpu->nvm.reset_pending, "POR operation has pending warm reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           !cpu->nvm.active && !cpu->nvm.reset_pending && cpu->events.count == 0u &&
               dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3c00u) ==
                   0x00ffffffu,
           "POR aborts operation and pending warm reset");
}

static void doze_stall_cases(TestState* state, Dspic33* cpu) {
    uint64_t cpu_cycles;
    uint64_t device_cycles;

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, MAIN_CLOCK_DIVISOR, 0x3800u);
    dspic33_load_program_word(cpu, 0x3a00u, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x3a02u, 0x00ffffffu);
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x3a00u),
           "DOZE NVM operation remains active after WR instruction");
    expect(state,
           cpu->nvm.completion_cycle == cpu->cycles + 1u && cpu->events.count == 1u &&
               cpu->events.items[0].cycle - cpu->device_cycles == 8u &&
               dspic33_nvm_test_program_word(cpu, 0x3a00u) == 0x00ffffffu &&
               !dspic33_nvm_test_interrupt_flag(cpu),
           "DOZE NVM event cannot complete before the CPU deadline");
    cpu_cycles = cpu->cycles;
    device_cycles = cpu->device_cycles;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
               cpu->cycles - cpu_cycles == 1u && cpu->device_cycles - device_cycles == 8u &&
               dspic33_nvm_test_program_word(cpu, 0x3a00u) == 0x00123456u &&
               dspic33_nvm_test_program_word(cpu, 0x3a02u) == 0x00654321u &&
               dspic33_nvm_test_interrupt_flag(cpu),
           "DOZE NVM stall advances both domains and completes at the CPU deadline");
}

static void persistent_program_alias_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    uint32_t index;
    bool copy_initialized;

    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_BASE) == 0x00ffffffu &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG +
                                                      PERSISTENT_PROGRAM_BASE) == 0x00ffffffu,
           "persistent physical and tagged views reset erased");
    expect(state,
           dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE, 0x00112233u) &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG +
                                                      PERSISTENT_PROGRAM_BASE) == 0x00112233u &&
               dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE + 2u,
                                         0x00445566u) &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_BASE + 2u) == 0x00445566u,
           "persistent loader keeps physical and tagged views coherent");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE - 2u, 0x00010203u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE - 2u,
                              0x00654321u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE, 0x00ffffffu);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE + 2u, 0x00ffffffu);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE + 4u, 0x00040506u);
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(
        state,
        dspic33_nvm_test_start_operation(cpu, 1u, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE),
        "persistent pair operation starts");
    expect(state, cpu->nvm.address == PERSISTENT_PROGRAM_BASE,
           "persistent pair request normalizes through NVMADRU");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "persistent pair operation completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE) ==
                   0x00123456u &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE +
                                                      2u) == 0x00654321u,
           "persistent pair updates tagged firmware view");
    expect(state,
           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_BASE - 2u) == 0x00010203u &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_BASE + 4u) == 0x00040506u,
           "persistent pair preserves adjacent physical words");
    expect(state,
           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE -
                                                  2u) == 0x00654321u,
           "persistent lower boundary keeps physical and tagged storage separate");

    dspic33_reset(cpu, 0u);
    for (index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
        cpu->write_latches[index] = 0x00330000u | index;
    }
    dspic33_load_program_word(cpu, 0x22feu, 0x00010203u);
    dspic33_load_program_word(cpu, 0x2300u, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x23feu, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x2400u, 0x00040506u);
    expect(state, dspic33_nvm_test_start_operation(cpu, 2u, PERSISTENT_PROGRAM_TAG + 0x23feu),
           "persistent row operation starts");
    expect(state, cpu->nvm.address == 0x23feu, "persistent row request normalizes through NVMADRU");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "persistent row operation completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG + 0x2300u) == 0x00330000u &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG + 0x23feu) == 0x0033007fu,
           "persistent row updates exact tagged range");
    expect(state,
           dspic33_nvm_test_program_word(cpu, 0x22feu) == 0x00010203u &&
               dspic33_nvm_test_program_word(cpu, 0x2400u) == 0x00040506u,
           "persistent row preserves adjacent words");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x27feu, 0x00010203u);
    dspic33_load_program_word(cpu, 0x2800u, 0u);
    dspic33_load_program_word(cpu, 0x2ffeu, 0u);
    dspic33_load_program_word(cpu, 0x3000u, 0x00040506u);
    expect(state, dspic33_nvm_test_start_operation(cpu, 3u, PERSISTENT_PROGRAM_TAG + 0x2ffeu),
           "persistent page operation starts");
    expect(state, cpu->nvm.address == 0x2ffeu,
           "persistent page request normalizes through NVMADRU");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "persistent page operation completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG + 0x2800u) == 0x00ffffffu &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG + 0x2ffeu) == 0x00ffffffu,
           "persistent page erase updates tagged view");
    expect(state,
           dspic33_nvm_test_program_word(cpu, 0x27feu) == 0x00010203u &&
               dspic33_nvm_test_program_word(cpu, 0x3000u) == 0x00040506u,
           "persistent page erase preserves adjacent words");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x4800u, 0u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_LIMIT - 2u, 0u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_LIMIT, 0x00010203u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_LIMIT, 0x00040506u);
    expect(state,
           dspic33_nvm_test_start_operation(cpu, 3u,
                                            PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_LIMIT - 2u),
           "persistent upper page operation starts");
    expect(state, cpu->nvm.address == PERSISTENT_PROGRAM_LIMIT - 2u,
           "persistent upper page normalizes through NVMADRU");
    expect(state, dspic33_nvm_test_finish_operation(cpu),
           "persistent upper page operation completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG + 0x4800u) == 0x00ffffffu &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG +
                                                      PERSISTENT_PROGRAM_LIMIT - 2u) == 0x00ffffffu,
           "persistent upper page erases through physical limit");
    expect(state,
           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_LIMIT) == 0x00010203u &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG +
                                                      PERSISTENT_PROGRAM_LIMIT) == 0x00040506u,
           "program origin and tagged boundary remain independently routed");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00010203u);
    dspic33_load_configuration_word(cpu, DSPIC33_CONFIGURATION_BASE + 4u, 0xff30u);
    expect(state,
           dspic33_nvm_test_start_operation_from(cpu, 0x0du, 0u,
                                                 DSPIC33_AUXILIARY_PROGRAM_BASE + 0x200u),
           "general bulk erase with persistent data starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu),
           "general bulk erase with persistent data completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_BASE) == 0x00ffffffu &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG +
                                                      PERSISTENT_PROGRAM_BASE) == 0x00ffffffu &&
               dspic33_read_configuration_byte(cpu, DSPIC33_CONFIGURATION_BASE + 4u) == 0xcfu,
           "general bulk erase clears persistent data and FGS");
    expect(state, dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00010203u,
           "general bulk erase preserves auxiliary program");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE + 0x200u, 0x00112233u);
    copy_initialized = dspic33_initialize(&copy);
    expect(state, copy_initialized, "initialize persistent alias copy");
    if (copy_initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy persistent alias state");
        dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE + 0x200u,
                                  0x00445566u);
        expect(state,
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_BASE + 0x200u) ==
                       0x00445566u &&
                   dspic33_nvm_test_program_word(&copy, PERSISTENT_PROGRAM_BASE + 0x200u) ==
                       0x00112233u,
               "copied persistent aliases diverge independently");
        dspic33_release(&copy);
    }

    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE + 0x400u, 0x00123456u);
    dspic33_load_program_word(cpu, 0u, OPCODE_RESET);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE +
                                                      0x400u) == 0x00123456u,
           "warm reset preserves persistent program");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_nvm_test_program_word(cpu, PERSISTENT_PROGRAM_BASE + 0x400u) == 0x00123456u,
           "cold processor reset preserves persistent program");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize NVM processor");
    if (initialized) {
        dspic33_nvm_test_configuration_table_view_cases(&state, &cpu);
        dspic33_nvm_test_reset_and_access_cases(&state, &cpu);
        dspic33_nvm_test_key_sequence_cases(&state, &cpu);
        dspic33_nvm_test_key_byte_access_cases(&state, &cpu);
        dspic33_nvm_test_invalid_operation_cases(&state, &cpu);
        dspic33_nvm_test_invalid_target_cases(&state, &cpu);
        dspic33_nvm_test_program_range_cases(&state, &cpu);
        dspic33_nvm_test_configuration_operation_cases(&state, &cpu);
        dspic33_nvm_test_configuration_programming_matrix_cases(&state, &cpu);
        dspic33_nvm_test_pair_and_capture_cases(&state, &cpu);
        dspic33_nvm_test_row_operation_cases(&state, &cpu);
        dspic33_nvm_test_erase_operation_cases(&state, &cpu);
        dspic33_nvm_test_auxiliary_loader_cases(&state, &cpu);
        dspic33_nvm_test_auxiliary_access_and_execution_cases(&state, &cpu);
        dspic33_nvm_test_auxiliary_nvm_cases(&state, &cpu);
        dspic33_nvm_test_stall_and_interrupt_cases(&state, &cpu);
        dspic33_nvm_test_same_segment_stall_erratum_cases(&state, &cpu);
        dspic33_nvm_test_power_save_cases(&state, &cpu);
        dspic33_nvm_test_codeguard_cases(&state, &cpu);
        persistent_program_alias_cases(&state, &cpu);
        doze_stall_cases(&state, &cpu);
        async_suppression_cases(&state, &cpu);
        reset_copy_and_failure_cases(&state, &cpu);
        deferred_reset_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
