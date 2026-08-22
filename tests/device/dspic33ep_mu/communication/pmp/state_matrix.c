#include "device/dspic33ep_mu/communication/pmp/internal.h"

uint16_t dspic33_device_internal_raw_word(const Dspic33* cpu, uint16_t address);
void dspic33_device_internal_pmp_clear_busy(Dspic33* cpu, uint16_t generation);
void dspic33_device_internal_pmp_read_register(Dspic33* cpu, uint16_t address);
void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_device_internal_run_pmp(Dspic33* cpu, uint16_t generation);
void dspic33_device_internal_update_pmp_register(Dspic33* cpu, uint16_t address,
                                                 uint16_t previous);

static void configure(Dspic33* cpu, uint16_t control, uint16_t mode,
                      Dspic33PowerState power, bool pmd_disabled) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, PMP_CONTROL, control);
    dspic33_device_internal_raw_write_word(cpu, PMP_MODE, mode);
    cpu->power_state = power;
    cpu->io.pmp.pmd_disabled = pmd_disabled;
}

static void power_admission_matrix(TestState* state, Dspic33* cpu) {
    static const struct {
        uint16_t control;
        uint16_t mode;
        Dspic33PowerState power;
        bool pmd_disabled;
    } cases[] = {
        {PMP_ENABLE, PMP_MASTER_MODE_2, DSPIC33_POWER_ACTIVE, true},
        {PMP_ENABLE, PMP_MASTER_MODE_2, DSPIC33_POWER_SLEEP, false},
        {(uint16_t)(PMP_ENABLE | PMP_STOP_IDLE), PMP_MASTER_MODE_2, DSPIC33_POWER_IDLE, false},
        {(uint16_t)(PMP_ENABLE | 0x1800u), PMP_MASTER_MODE_2, DSPIC33_POWER_ACTIVE, false},
        {PMP_ENABLE, 0u, DSPIC33_POWER_ACTIVE, false},
        {PMP_ENABLE, 0x0100u, DSPIC33_POWER_ACTIVE, false},
        {PMP_ENABLE, 0x0400u, DSPIC33_POWER_ACTIVE, false},
    };
    for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        configure(cpu, cases[index].control, cases[index].mode, cases[index].power,
                  cases[index].pmd_disabled);
        dspic33_device_internal_pmp_read_register(cpu, PMP_DATA);
        expect(state, cpu->events.count == 0u, "PMP power admission rejects an inactive clock");
    }
}

static void generation_admission_matrix(TestState* state, Dspic33* cpu) {
    configure(cpu, PMP_ENABLE, PMP_MASTER_MODE_2, DSPIC33_POWER_ACTIVE, false);
    cpu->io.pmp.active = true;
    cpu->io.pmp.generation = 2u;
    dspic33_device_internal_run_pmp(cpu, 1u);
    expect(state, cpu->io.pmp.active, "PMP rejects a stale active transfer");

    cpu->io.pmp.active = false;
    dspic33_device_internal_run_pmp(cpu, 2u);
    expect(state, !cpu->io.pmp.active, "PMP ignores completion without an active transfer");

    cpu->io.pmp.completing_active = true;
    cpu->io.pmp.completing_generation = 3u;
    dspic33_device_internal_pmp_clear_busy(cpu, 2u);
    expect(state, cpu->io.pmp.completing_active, "PMP rejects a stale BUSY completion");
}

static void slave_access_matrix(TestState* state, Dspic33* cpu) {
    configure(cpu, PMP_ENABLE, PMP_BUFFERED_SLAVE, DSPIC33_POWER_ACTIVE, false);
    cpu->io.cpu_read_valid = true;
    cpu->io.cpu_read_width = 2u;
    cpu->io.cpu_read_address = PMP_DATA;
    dspic33_device_internal_pmp_read_register(cpu, PMP_DATA + 1u);
    expect(state, (dspic33_device_internal_raw_word(cpu, PMP_STATUS) & PMP_INPUT_OVERFLOW) == 0u,
           "PMP word read consumes a buffered slave lane");

    cpu->io.cpu_read_valid = false;
    dspic33_device_internal_pmp_read_register(cpu, PMP_DATA);
    cpu->io.cpu_read_valid = true;
    cpu->io.cpu_read_address = PMP_DATA - 1u;
    dspic33_device_internal_pmp_read_register(cpu, PMP_DATA);
    cpu->io.cpu_read_address = PMP_INPUT_2 + 1u;
    dspic33_device_internal_pmp_read_register(cpu, PMP_DATA);

    cpu->io.cpu_write_valid = true;
    cpu->io.cpu_write_width = 2u;
    cpu->io.cpu_write_address = PMP_ADDRESS;
    dspic33_device_internal_update_pmp_register(cpu, PMP_ADDRESS + 1u, 0u);
    expect(state, (dspic33_device_internal_raw_word(cpu, PMP_STATUS) & PMP_OUTPUT_EMPTY) == 0u,
           "PMP word write fills buffered slave lanes");

    cpu->io.cpu_write_valid = false;
    dspic33_device_internal_update_pmp_register(cpu, PMP_ADDRESS, 0u);
    cpu->io.cpu_write_valid = true;
    cpu->io.cpu_write_address = PMP_ADDRESS - 1u;
    dspic33_device_internal_update_pmp_register(cpu, PMP_ADDRESS, 0u);
    cpu->io.cpu_write_address = PMP_OUTPUT_2 + 1u;
    dspic33_device_internal_update_pmp_register(cpu, PMP_ADDRESS, 0u);

    dspic33_device_internal_pmp_read_register(cpu, PMP_INPUT_2 + 2u);
    dspic33_device_internal_update_pmp_register(cpu, PMP_ADDRESS - 1u, 0u);
    expect(state, cpu->stop_reason == DSPIC33_RUNNING, "PMP ignores addresses outside slave lanes");
}

void dspic33_pmp_test_state_matrix_cases(TestState* state, Dspic33* cpu) {
    power_admission_matrix(state, cpu);
    generation_admission_matrix(state, cpu);
    slave_access_matrix(state, cpu);
}
