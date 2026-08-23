#include "device/dspic33ep_mu/communication/dci/internal.h"

enum {
    DCI_MATRIX_EVENT_START,
    DCI_MATRIX_EVENT_INTERNAL,
    DCI_MATRIX_EVENT_EXTERNAL,
    DCI_MATRIX_EVENT_EXTERNAL_FRAME,
    DCI_MATRIX_EVENT_SAMPLE,
    DCI_MATRIX_EVENT_FRAME_START,
    DCI_MATRIX_EVENT_PMD = UINT16_MAX,
    DCI_MATRIX_EVENT_GENERATION_SHIFT = 1u,
    DCI_MATRIX_EVENT_DISABLED = 1u
};

void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_device_internal_run_dci(Dspic33* cpu, uint16_t event_source, uint32_t value);
bool dspic33_device_internal_dci_read_register(Dspic33* cpu, uint16_t address, uint8_t* value);

static void configure_dci(Dspic33* cpu, uint16_t control_word) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, control_word);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    cpu->io.dci.generation = 2u;
}

static void pmd_event_matrix(TestState* state, Dspic33* cpu) {
    configure_dci(cpu, DCI_ENABLE);
    cpu->io.dci.pmd_generation = 2u;
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_PMD,
                                    (UINT32_C(1) << DCI_MATRIX_EVENT_GENERATION_SHIFT) |
                                        DCI_MATRIX_EVENT_DISABLED);
    expect(state, !cpu->io.dci.pmd_disabled, "DCI rejects a stale PMD event");
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_PMD,
                                    (UINT32_C(2) << DCI_MATRIX_EVENT_GENERATION_SHIFT) |
                                        DCI_MATRIX_EVENT_DISABLED);
    expect(state, cpu->io.dci.pmd_disabled, "DCI accepts a current PMD disable event");
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_PMD,
                                    UINT32_C(2) << DCI_MATRIX_EVENT_GENERATION_SHIFT);
    expect(state, !cpu->io.dci.pmd_disabled, "DCI accepts a current PMD enable event");
}

static void start_admission_matrix(TestState* state, Dspic33* cpu) {
    configure_dci(cpu, DCI_ENABLE);
    cpu->io.dci.pmd_disabled = true;
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_START, 2u);
    expect(state, !cpu->io.dci.initialized, "PMD-disabled DCI rejects a start event");

    configure_dci(cpu, 0u);
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_START, 2u);
    expect(state, !cpu->io.dci.initialized, "disabled DCI rejects a start event");

    configure_dci(cpu, DCI_ENABLE);
    cpu->io.dci.internal_scheduled = true;
    cpu->io.dci.initialized = true;
    cpu->io.dci.started = true;
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_INTERNAL, 2u);
    expect(state, cpu->stop_reason == DSPIC33_RUNNING,
           "DCI reuses an already scheduled internal phase");
}

static void frame_admission_matrix(TestState* state, Dspic33* cpu) {
    configure_dci(cpu, DCI_ENABLE | DCI_EXTERNAL_FRAME);
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_FRAME_START, 2u);
    expect(state, !cpu->io.dci.started, "DCI frame start requires a pending frame");

    cpu->io.dci.pps_frame_pending = true;
    cpu->io.dci.started = true;
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_FRAME_START, 2u);
    expect(state, cpu->io.dci.pps_frame_pending, "started DCI rejects a duplicate frame");

    cpu->io.dci.started = false;
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, UINT16_MAX);
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_FRAME_START, 2u);
    expect(state, cpu->io.dci.pps_frame_pending, "unsupported DCI rejects a pending frame");

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1,
                                           DCI_ENABLE | DCI_EXTERNAL_FRAME | DCI_STOP_IDLE);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_FRAME_START, 2u);
    expect(state, cpu->io.dci.pps_frame_pending, "stopped idle DCI retains a pending frame");

    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_internal_run_dci(cpu, DCI_MATRIX_EVENT_FRAME_START, 2u);
    expect(state, cpu->io.dci.started && !cpu->io.dci.pps_frame_pending,
           "operating DCI accepts a pending frame");
}

static void read_lane_matrix(TestState* state, Dspic33* cpu) {
    uint8_t register_value;

    configure_dci(cpu, DCI_ENABLE);
    cpu->io.dci.receive_unread = 1u;
    expect(state, dspic33_device_internal_dci_read_register(cpu, DCI_RECEIVE_BASE, &register_value),
           "DCI byte read without CPU metadata is accepted");

    cpu->io.dci.receive_unread = 1u;
    cpu->io.dma_transfer_active = true;
    cpu->io.dma_transfer_width = 1u;
    expect(state, dspic33_device_internal_dci_read_register(cpu, DCI_RECEIVE_BASE, &register_value),
           "DCI byte DMA read is accepted");

    cpu->io.dci.receive_unread = 1u;
    cpu->io.dma_transfer_width = 2u;
    expect(state, dspic33_device_internal_dci_read_register(cpu, DCI_RECEIVE_BASE, &register_value),
           "DCI unrelated DMA word read preserves lane state");
    expect(state,
           dspic33_device_internal_dci_read_register(cpu, DCI_RECEIVE_BASE + 1u, &register_value),
           "DCI DMA word high byte completes the lane read");

    cpu->io.dma_transfer_active = false;
    cpu->io.cpu_read_valid = true;
    cpu->io.cpu_read_width = 2u;
    cpu->io.cpu_read_address = DCI_RECEIVE_BASE;
    expect(state,
           dspic33_device_internal_dci_read_register(cpu, DCI_RECEIVE_BASE + 1u, &register_value),
           "DCI CPU word read consumes both lanes");
}

void dspic33_dci_test_state_matrix_cases(TestState* state, Dspic33* cpu) {
    pmd_event_matrix(state, cpu);
    start_admission_matrix(state, cpu);
    frame_admission_matrix(state, cpu);
    read_lane_matrix(state, cpu);
}
