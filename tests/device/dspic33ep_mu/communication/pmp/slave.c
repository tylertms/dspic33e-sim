#include "device/dspic33ep_mu/communication/pmp/internal.h"

static void pmp_power_wake_matrix_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33PmpTransfer transfer;
    uint16_t priority_address = (uint16_t)(0x0840u + (PMP_IRQ / 4u) * 2u);
    uint16_t priority_shift = (uint16_t)((PMP_IRQ % 4u) * 4u);
    bool initialized;

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x91u);
    expect(state,
           dspic33_device_advance(cpu, 20u) && !cpu->io.pmp.active &&
               cpu->io.pmp.completing_active && (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "master reaches final completion phase before Sleep");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->events.count == 1u && cpu->events.items[0].paused,
           "Sleep pauses final PMP completion phase");
    expect(state,
           dspic33_device_advance(cpu, 100u) && !dspic33_pmp_transmit(cpu, &transfer) &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) == 0u,
           "Sleep suppresses final PMP output and interrupt");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state, !cpu->events.items[0].paused, "Sleep exit resumes final PMP completion phase");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x91u,
           "resumed final PMP phase emits retained transfer");
    expect(state,
           !cpu->io.pmp.completing_active &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "resumed final PMP phase raises completion interrupt");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_program_word(cpu, 0u, OPCODE_POWER_SAVE_SLEEP),
           "load stepped PWRSAV instruction");
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x92u);
    expect(state, dspic33_step(cpu) == DSPIC33_SLEEPING && cpu->power_state == DSPIC33_POWER_SLEEP,
           "PWRSAV enters Sleep with active PMP transfer");
    expect(state,
           cpu->io.pmp.active && cpu->events.count == 2u && cpu->events.items[0].paused &&
               cpu->events.items[1].paused,
           "PWRSAV instruction pauses both PMP master phases");
    expect(state,
           dspic33_device_advance(cpu, 50u) && !dspic33_pmp_transmit(cpu, &transfer) &&
               cpu->io.pmp.active,
           "stepped PWRSAV keeps master transfer suspended");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 21u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x92u,
           "stepped PWRSAV transfer resumes after wake");

    dspic33_reset(cpu, 0u);
    cpu->configuration[10u] = 0x80u;
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x93u);
    expect(state, dspic33_device_advance(cpu, 5u), "advance PMP before WDT Sleep wake");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->io.pmp.active && cpu->events.items[0].paused,
           "Sleep pauses PMP before WDT wake");
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state,
           cpu->power_state == DSPIC33_POWER_ACTIVE &&
               (dspic33_read_word(cpu, 0x0740u) & 0x0010u) != 0u,
           "WDT timeout wakes sleeping processor");
    expect(state, !cpu->events.items[0].paused && !cpu->events.items[1].paused,
           "WDT wake resumes PMP master events");
    expect(state,
           dspic33_device_advance(cpu, 16u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x93u,
           "WDT wake completes retained PMP transfer");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_control(cpu, PMP_STOP_IDLE, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x94u);
    expect(state, dspic33_device_advance(cpu, 20u) && cpu->io.pmp.completing_active,
           "master reaches final completion phase before Idle");
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->events.count == 1u && cpu->events.items[0].paused,
           "PSIDL pauses final PMP completion phase");
    expect(state, dspic33_device_advance(cpu, 40u) && !dspic33_pmp_transmit(cpu, &transfer),
           "PSIDL suppresses final PMP completion while Idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state, !cpu->events.items[0].paused, "Idle exit resumes final PMP completion phase");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x94u,
           "Idle exit completes retained final PMP phase");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0xa1u, 1u) && dspic33_device_advance(cpu, 1u) &&
               cpu->data[PMP_DATA] == 0xa1u,
           "slave write continues in Idle with PSIDL clear");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "Idle slave write raises PMP interrupt flag");
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, PMP_STOP_IDLE, PMP_INTERRUPT_EACH);
    dspic33_write_byte(cpu, PMP_ADDRESS, 0xa2u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_pmp_slave_read(cpu, 0u, 1u) && dspic33_device_advance(cpu, 1u) &&
               dspic33_pmp_transmit(cpu, &transfer) && transfer.value == 0xa2u,
           "asynchronous slave read continues in Idle with PSIDL set");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "PSIDL slave read raises PMP interrupt flag");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0xb1u, 0u) && dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "sleeping slave latches interrupt with IEC disabled");
    expect(state, !dspic33_device_wake(cpu), "disabled PMP interrupt cannot wake processor");
    expect(state, cpu->power_state == DSPIC33_POWER_SLEEP && cpu->last_interrupt != PMP_IRQ,
           "IEC-disabled PMP event retains sleeping state");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    dspic33_write_word(cpu, 0x0824u, PMP_INTERRUPT_ENABLE);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0xb2u, 0u) && dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "sleeping slave latches interrupt at priority zero");
    expect(state, !dspic33_device_wake(cpu), "priority-zero PMP interrupt cannot wake processor");
    expect(state, cpu->power_state == DSPIC33_POWER_SLEEP && cpu->last_interrupt != PMP_IRQ,
           "priority-zero PMP event retains sleeping state");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    dspic33_write_word(cpu, 0x0824u, PMP_INTERRUPT_ENABLE);
    dspic33_write_word(cpu, priority_address, (uint16_t)(PMP_PRIORITY << priority_shift));
    cpu->program[(0x0014u + PMP_IRQ * 2u) / 2u] = PMP_VECTOR;
    cpu->w[15] = 0x1800u;
    cpu->sr = (uint16_t)(PMP_PRIORITY << 5u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, dspic33_pmp_slave_write(cpu, 0u, 0xb3u, 0u) && dspic33_device_advance(cpu, 0u),
           "sleeping slave raises equal-priority interrupt");
    expect(state, dspic33_device_wake(cpu), "equal-priority PMP interrupt wakes without vectoring");
    expect(state,
           cpu->pc == 0u && cpu->last_interrupt != PMP_IRQ &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "equal-priority wake retains pending PMP interrupt");
    cpu->sr = 0u;
    expect(state, dspic33_device_interrupt_pending(cpu),
           "lowered IPL exposes retained PMP interrupt");
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == PMP_IRQ &&
               cpu->pc == PMP_VECTOR,
           "lowered IPL vectors retained PMP interrupt");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0xc1u);
    expect(state, dspic33_device_advance(cpu, 19u) && cpu->io.pmp.active,
           "advance PMP to cycle before final BUSY phase");
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pmp.pmd_disabled && !cpu->io.pmp.active &&
               cpu->io.pmp.completing_active,
           "PMPMD transition pauses newly entered final phase");
    expect(state,
           dspic33_device_advance(cpu, 30u) && !dspic33_pmp_transmit(cpu, &transfer) &&
               cpu->io.pmp.completing_active,
           "PMPMD holds final completion phase indefinitely");
    dspic33_write_word(cpu, PMP_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled &&
               cpu->io.pmp.completing_active,
           "PMPMD clear resumes retained final completion phase");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0xc1u,
           "PMPMD resumed final phase emits transfer");
    expect(state, !cpu->io.pmp.completing_active,
           "PMPMD resumed final phase clears completion state");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.pmp.pmd_disabled,
           "establish PMP PMD disable before cold reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           (dspic33_read_word(cpu, PMP_PMD) & PMP_MODULE_DISABLE) == 0u &&
               !cpu->io.pmp.pmd_disabled,
           "cold reset clears PMP PMD state");
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0x008fu,
           "cold reset restores disabled PMP register reset state");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize PMP PMD transition copy");
    if (initialized) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
        expect(state, dspic33_copy(&copy, cpu), "copy pending PMP PMD transition");
        expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
               "advance copied PMP PMD transitions");
        expect(state, cpu->io.pmp.pmd_disabled && copy.io.pmp.pmd_disabled,
               "copied PMP PMD transitions complete equally");
        dspic33_write_word(cpu, PMP_PMD, 0u);
        expect(state, dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled,
               "source PMP PMD copy diverges after clear");
        expect(state,
               copy.io.pmp.pmd_disabled &&
                   (dspic33_read_word(&copy, PMP_PMD) & PMP_MODULE_DISABLE) != 0u,
               "copied PMP PMD state remains independent");
        dspic33_release(&copy);
    }

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    expect(state, dspic33_pmp_slave_write(cpu, 0u, 0xd1u, 5u),
           "schedule absolute slave event across PMP PMD window");
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state, dspic33_device_advance(cpu, 2u) && cpu->io.pmp.pmd_disabled,
           "disable PMP before absolute slave event deadline");
    dspic33_write_word(cpu, PMP_PMD, 0u);
    expect(state, dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled,
           "re-enable PMP before absolute slave event deadline");
    expect(state, dspic33_device_advance(cpu, 2u) && cpu->data[PMP_DATA] == 0xd1u,
           "absolute slave event retains deadline across PMP PMD window");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_read(cpu, 0u, PMP_FIRMWARE_MODE, 0u, 0x4455u);
    expect(state,
           dspic33_pmp_respond(cpu, 0x6677u, 0u) && dspic33_read_word(cpu, PMP_DATA) == 0x4455u &&
               dspic33_device_advance(cpu, 5u),
           "begin master read before Sleep");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->io.pmp.active && cpu->events.items[0].paused,
           "Sleep pauses active master read");
    expect(state,
           dspic33_device_advance(cpu, 40u) && cpu->io.pmp.active &&
               dspic33_pmp_test_raw_data_word(cpu, PMP_DATA) == 0x4455u,
           "Sleep preserves previous read result while suspended");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state, !cpu->events.items[0].paused && !cpu->events.items[1].paused,
           "wake resumes both master read phases");
    expect(state,
           dspic33_device_advance(cpu, 16u) &&
               dspic33_pmp_test_raw_data_word(cpu, PMP_DATA) == 0x4477u && !cpu->io.pmp.active &&
               !cpu->io.pmp.completing_active,
           "wake completes retained master read");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    dspic33_write_byte(cpu, PMP_ADDRESS, 0xe1u);
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.pmp.pmd_disabled,
           "disable PMP before external slave read");
    expect(state, dspic33_pmp_slave_read(cpu, 0u, 1u) && dspic33_device_advance(cpu, 1u),
           "external slave read deadline occurs while PMP disabled");
    expect(state,
           !dspic33_pmp_transmit(cpu, &transfer) &&
               dspic33_pmp_test_raw_data_word(cpu, PMP_STATUS) == 0x000fu,
           "disabled PMP drops external slave read without status change");
    dspic33_write_word(cpu, PMP_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled &&
               !dspic33_pmp_transmit(cpu, &transfer),
           "re-enabled PMP does not replay missed slave read");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize PMP processor");
    if (initialized) {
        dspic33_pmp_test_access_cases(&state, &cpu);
        dspic33_pmp_test_timing_cases(&state, &cpu);
        dspic33_pmp_test_access_lane_cases(&state, &cpu);
        dspic33_pmp_test_sixteen_bit_lane_cases(&state, &cpu);
        dspic33_pmp_test_master_write_matrix_cases(&state, &cpu);
        dspic33_pmp_test_wait_state_matrix_cases(&state, &cpu);
        dspic33_pmp_test_address_update_cases(&state, &cpu);
        dspic33_pmp_test_master_read_pipeline_cases(&state, &cpu);
        dspic33_pmp_test_master_read_matrix_cases(&state, &cpu);
        dspic33_pmp_test_read_wait_state_matrix_cases(&state, &cpu);
        dspic33_pmp_test_read_address_update_cases(&state, &cpu);
        dspic33_pmp_test_read_interrupt_dma_cases(&state, &cpu);
        dspic33_pmp_test_read_lifecycle_cases(&state, &cpu);
        dspic33_pmp_test_interrupt_cases(&state, &cpu);
        dspic33_pmp_test_dma_chain_cases(&state, &cpu);
        dspic33_pmp_test_dma_negative_cases(&state, &cpu);
        dspic33_pmp_test_lifecycle_cases(&state, &cpu);
        dspic33_pmp_test_legacy_slave_cases(&state, &cpu);
        dspic33_pmp_test_buffered_slave_cases(&state, &cpu);
        dspic33_pmp_test_addressable_slave_cases(&state, &cpu);
        dspic33_pmp_test_power_management_cases(&state, &cpu);
        dspic33_pmp_test_slave_power_lifecycle_cases(&state, &cpu);
        dspic33_pmp_test_slave_mode_matrix_cases(&state, &cpu);
        dspic33_pmp_test_slave_dma_isolation_cases(&state, &cpu);
        dspic33_pmp_test_pmp_extended_lifecycle_cases(&state, &cpu);
        pmp_power_wake_matrix_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
