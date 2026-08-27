#include "device/dspic33ep_mu/internal.h"

static uint8_t pmp_transfer_width(uint16_t mode_word) {
    return (mode_word & PMP_DATA_16_BIT) != 0u ? 2u : 1u;
}

static uint64_t pmp_transfer_cycles(uint16_t control_word, uint16_t mode_word) {
    uint8_t address_phase_count = (uint8_t)((control_word & PMP_ADDRESS_MUX_MASK) >> 11u);
    uint8_t data_phase_count = pmp_transfer_width(mode_word);
    uint8_t wait_begin_count = (uint8_t)((mode_word & PMP_WAIT_BEGIN_MASK) >> 6u);
    uint8_t wait_middle_count = (uint8_t)((mode_word & PMP_WAIT_MIDDLE_MASK) >> 2u);

    if (wait_middle_count == 0u) {
        return (uint64_t)address_phase_count + data_phase_count;
    }
    return (uint64_t)address_phase_count * (wait_begin_count + 1u) +
           (uint64_t)data_phase_count *
               (wait_begin_count + 1u + wait_middle_count + (mode_word & PMP_WAIT_END_MASK) + 1u);
}

static bool pmp_master_enabled(const Dspic33* cpu) {
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, PMP_CONTROL);
    uint16_t mode_word = dspic33_device_internal_raw_word(cpu, PMP_MODE);
    uint16_t master_mode = mode_word & PMP_MASTER_MODE_MASK;

    return !cpu->io.pmp.pmd_disabled && cpu->power_state != DSPIC33_POWER_SLEEP &&
           (cpu->power_state != DSPIC33_POWER_IDLE || (control_word & PMP_STOP_IDLE) == 0u) &&
           (control_word & PMP_ENABLE) != 0u &&
           (master_mode == PMP_MASTER_MODE_2 || master_mode == PMP_MASTER_MODE_1) &&
           (control_word & PMP_ADDRESS_MUX_MASK) != PMP_ADDRESS_MUX_MASK &&
           (control_word & PMP_CHIP_SELECT_FUNCTION_MASK) != PMP_CHIP_SELECT_FUNCTION_MASK;
}

static bool pmp_slave_configured(const Dspic33* cpu) {
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, PMP_CONTROL);
    uint16_t mode_word = dspic33_device_internal_raw_word(cpu, PMP_MODE);
    uint16_t slave_mode = mode_word & PMP_MASTER_MODE_MASK;
    uint16_t increment_mode = mode_word & PMP_INCREMENT_MODE_MASK;

    return !cpu->io.pmp.pmd_disabled && (control_word & PMP_ENABLE) != 0u &&
           ((slave_mode == 0u &&
             (increment_mode == 0u || increment_mode == PMP_INCREMENT_MODE_MASK)) ||
            (slave_mode == PMP_SLAVE_ADDRESSABLE && increment_mode == 0u));
}

static bool pmp_slave_enabled(const Dspic33* cpu, bool is_read) {
    uint16_t mode_word = dspic33_device_internal_raw_word(cpu, PMP_MODE);
    uint16_t slave_mode = mode_word & PMP_MASTER_MODE_MASK;
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, PMP_CONTROL);
    uint16_t enabled_strobe = is_read ? PMP_READ_STROBE_ENABLE : PMP_WRITE_STROBE_ENABLE;

    if (!pmp_slave_configured(cpu) || (control_word & enabled_strobe) == 0u ||
        (dspic33_device_internal_raw_word(cpu, PMP_ADDRESS_ENABLE_REGISTER) &
         PMP_CHIP_SELECT_ENABLE) == 0u) {
        return false;
    }
    return slave_mode != PMP_SLAVE_ADDRESSABLE ||
           (dspic33_device_internal_raw_word(cpu, PMP_ADDRESS_ENABLE_REGISTER) &
            PMP_ADDRESS_ENABLE) == PMP_ADDRESS_ENABLE;
}

static bool pmp_master_clock_available(const Dspic33* cpu) {
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, PMP_CONTROL);

    return !cpu->io.pmp.pmd_disabled && cpu->power_state != DSPIC33_POWER_SLEEP &&
           (cpu->power_state != DSPIC33_POWER_IDLE || (control_word & PMP_STOP_IDLE) == 0u);
}

static bool pmp_master_event(const Dspic33Event* event) {
    return event->type == DSPIC33_EVENT_PMP && event->source <= PMP_EVENT_CLEAR_BUSY;
}

static void pmp_pause_master_events(Dspic33* cpu) {
    bool changed = false;
    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        Dspic33Event* event = &cpu->events.items[event_index];
        if (!pmp_master_event(event) || event->paused) {
            continue;
        }
        event->paused_remaining = event->cycle - cpu->device_cycles;
        event->paused = true;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void pmp_resume_master_events(Dspic33* cpu) {
    bool changed = false;
    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        Dspic33Event* event = &cpu->events.items[event_index];
        if (!pmp_master_event(event) || !event->paused) {
            continue;
        }
        if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            continue;
        }
        event->cycle = cpu->device_cycles + event->paused_remaining;
        event->paused_remaining = 0u;
        event->paused = false;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void pmp_discard_master_events(Dspic33* cpu) {
    size_t retained_event_count = 0u;

    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        if (!pmp_master_event(&cpu->events.items[event_index])) {
            cpu->events.items[retained_event_count++] = cpu->events.items[event_index];
        }
    }
    cpu->events.count = retained_event_count;
    dspic33_reorder_events(cpu);
}

void dspic33_device_power_state_changed(Dspic33* cpu) {
    if (pmp_master_clock_available(cpu)) {
        pmp_resume_master_events(cpu);
    } else {
        pmp_pause_master_events(cpu);
    }
    dspic33_device_internal_output_compare_update_power_state(cpu);
    dspic33_device_internal_dci_update_power_state(cpu);
    dspic33_device_internal_dma_update_power_state(cpu);
    dspic33_device_internal_comparator_update_filter_power(cpu);
    dspic33_device_internal_comparator_evaluate_all(cpu);
    dspic33_device_internal_adc_update_power_state(cpu);
    dspic33_device_internal_uart_update_power_state(cpu);
    dspic33_device_internal_spi_update_power_state(cpu);
    dspic33_device_internal_usb_update_power_state(cpu);
    dspic33_device_internal_refresh_pwm_pins(cpu);
    dspic33_i2c_refresh_pins(cpu);
}

static void pmp_update_address(Dspic33* cpu, uint16_t control, uint16_t mode) {
    uint16_t increment = mode & PMP_INCREMENT_MODE_MASK;
    uint16_t counter_mask;
    uint16_t address;
    if (increment != PMP_INCREMENT_ADDRESS && increment != PMP_DECREMENT_ADDRESS) {
        return;
    }
    if ((control & 0x0080u) != 0u) {
        counter_mask = 0x3fffu;
    } else if ((control & 0x0040u) != 0u) {
        counter_mask = 0x7fffu;
    } else {
        counter_mask = 0xffffu;
    }
    address = dspic33_device_internal_raw_word(cpu, PMP_ADDRESS);
    dspic33_device_internal_raw_write_word(
        cpu, PMP_ADDRESS,
        (uint16_t)((address & ~counter_mask) |
                   ((increment == PMP_INCREMENT_ADDRESS ? address + 1u : address - 1u) &
                    counter_mask)));
}

static void pmp_abort(Dspic33* cpu) {
    cpu->io.pmp.generation++;
    pmp_discard_master_events(cpu);
    cpu->io.pmp.active = false;
    cpu->io.pmp.completing_active = false;
    cpu->io.pmp.reading = false;
    cpu->io.pmp.completing_reading = false;
    dspic33_device_internal_raw_write_word(
        cpu, PMP_MODE, (uint16_t)(dspic33_device_internal_raw_word(cpu, PMP_MODE) & ~PMP_BUSY));
}

static bool pmp_output_push(Dspic33PmpQueue* queue, const Dspic33PmpTransfer* transfer) {
    uint16_t index;
    if (queue->count == DSPIC33_PMP_QUEUE_SIZE) {
        return false;
    }
    index = (uint16_t)((queue->head + queue->count) % DSPIC33_PMP_QUEUE_SIZE);
    queue->transfers[index] = *transfer;
    queue->count++;
    return true;
}

bool dspic33_device_internal_pmp_output_pop(Dspic33PmpQueue* queue, Dspic33PmpTransfer* transfer) {
    if (queue->count == 0u) {
        return false;
    }
    *transfer = queue->transfers[queue->head];
    queue->head = (uint16_t)((queue->head + 1u) % DSPIC33_PMP_QUEUE_SIZE);
    queue->count--;
    return true;
}

bool dspic33_device_internal_pmp_response_push(Dspic33PmpResponseQueue* queue,
                                               const Dspic33PmpResponse* response) {
    uint16_t index;
    uint16_t logical;
    if (queue->count == DSPIC33_PMP_QUEUE_SIZE) {
        return false;
    }
    index = (uint16_t)((queue->head + queue->count) % DSPIC33_PMP_QUEUE_SIZE);
    queue->responses[index] = *response;
    queue->count++;
    logical = (uint16_t)(queue->count - 1u);
    while (logical != 0u) {
        uint16_t current = (uint16_t)((queue->head + logical) % DSPIC33_PMP_QUEUE_SIZE);
        uint16_t prior = (uint16_t)((queue->head + logical - 1u) % DSPIC33_PMP_QUEUE_SIZE);
        Dspic33PmpResponse temporary;
        if (queue->responses[prior].cycle <= queue->responses[current].cycle) {
            break;
        }
        temporary = queue->responses[prior];
        queue->responses[prior] = queue->responses[current];
        queue->responses[current] = temporary;
        logical--;
    }
    return true;
}

static bool pmp_response_pop(Dspic33PmpResponseQueue* queue, uint64_t response_cycle,
                             Dspic33PmpResponse* response) {
    if (queue->count == 0u || queue->responses[queue->head].cycle > response_cycle) {
        return false;
    }
    *response = queue->responses[queue->head];
    queue->head = (uint16_t)((queue->head + 1u) % DSPIC33_PMP_QUEUE_SIZE);
    queue->count--;
    return true;
}

static bool pmp_buffered_slave(const Dspic33* cpu) {
    uint16_t mode_word = dspic33_device_internal_raw_word(cpu, PMP_MODE);

    return (mode_word & PMP_MASTER_MODE_MASK) == PMP_SLAVE_ADDRESSABLE ||
           (mode_word & (PMP_MASTER_MODE_MASK | PMP_INCREMENT_MODE_MASK)) == PMP_BUFFERED_SLAVE;
}

static void pmp_refresh_slave_status(Dspic33* cpu) {
    uint16_t status_word = dspic33_device_internal_raw_word(cpu, PMP_STATUS);

    if (!pmp_buffered_slave(cpu)) {
        return;
    }
    status_word &= (uint16_t)~(PMP_INPUT_FULL | PMP_OUTPUT_EMPTY);
    if ((status_word & PMP_INPUT_BUFFER_MASK) == PMP_INPUT_BUFFER_MASK) {
        status_word |= PMP_INPUT_FULL;
    }
    if ((status_word & PMP_OUTPUT_BUFFER_MASK) == PMP_OUTPUT_BUFFER_MASK) {
        status_word |= PMP_OUTPUT_EMPTY;
    }
    dspic33_device_internal_raw_write_word(cpu, PMP_STATUS, status_word);
}

static uint8_t pmp_slave_buffer(Dspic33* cpu, uint8_t address, bool is_read) {
    uint16_t mode_word = dspic33_device_internal_raw_word(cpu, PMP_MODE);

    if ((mode_word & PMP_MASTER_MODE_MASK) == PMP_SLAVE_ADDRESSABLE) {
        return (uint8_t)(address & 3u);
    }
    if (!pmp_buffered_slave(cpu)) {
        return 0u;
    }
    uint8_t buffer_index = is_read ? cpu->io.pmp.slave_read_index : cpu->io.pmp.slave_write_index;

    if (is_read) {
        cpu->io.pmp.slave_read_index = (uint8_t)((buffer_index + 1u) & 3u);
    } else {
        cpu->io.pmp.slave_write_index = (uint8_t)((buffer_index + 1u) & 3u);
    }
    return buffer_index;
}

static void pmp_slave_interrupt(Dspic33* cpu, uint8_t buffer_index) {
    uint16_t mode_word = dspic33_device_internal_raw_word(cpu, PMP_MODE);
    uint16_t interrupt_mode = mode_word & PMP_INTERRUPT_MODE_MASK;

    if (interrupt_mode == PMP_INTERRUPT_EACH ||
        (interrupt_mode == PMP_INTERRUPT_LAST && pmp_buffered_slave(cpu) && buffer_index == 3u)) {
        dspic33_raise_interrupt(cpu, PMP_IRQ);
    }
}

void dspic33_device_internal_pmp_slave_write_event(Dspic33* cpu, uint8_t address, uint8_t value) {
    uint16_t status_word;
    uint16_t full_mask;

    if (!pmp_slave_enabled(cpu, false)) {
        return;
    }
    uint8_t buffer_index = pmp_slave_buffer(cpu, address, false);
    status_word = dspic33_device_internal_raw_word(cpu, PMP_STATUS);
    if (pmp_buffered_slave(cpu)) {
        full_mask = (uint16_t)(1u << (8u + buffer_index));
    } else {
        full_mask = PMP_INPUT_FULL;
    }
    if ((status_word & full_mask) != 0u) {
        status_word |= PMP_INPUT_OVERFLOW;
    } else {
        cpu->data[PMP_DATA + buffer_index] = value;
        status_word |= full_mask;
    }
    dspic33_device_internal_raw_write_word(cpu, PMP_STATUS, status_word);
    pmp_refresh_slave_status(cpu);
    pmp_slave_interrupt(cpu, buffer_index);
}

void dspic33_device_internal_pmp_slave_read_event(Dspic33* cpu, uint8_t address) {
    Dspic33PmpTransfer transfer;
    uint16_t status_word;
    uint16_t empty_mask;

    if (!pmp_slave_enabled(cpu, true)) {
        return;
    }
    uint8_t buffer_index = pmp_slave_buffer(cpu, address, true);
    status_word = dspic33_device_internal_raw_word(cpu, PMP_STATUS);
    empty_mask = pmp_buffered_slave(cpu) ? (uint16_t)(1u << buffer_index) : PMP_OUTPUT_EMPTY;
    if ((status_word & empty_mask) != 0u) {
        status_word |= PMP_OUTPUT_UNDERFLOW;
    }
    status_word |= empty_mask;
    dspic33_device_internal_raw_write_word(cpu, PMP_STATUS, status_word);
    pmp_refresh_slave_status(cpu);
    transfer.cycle = cpu->device_cycles;
    transfer.address = buffer_index;
    transfer.control = dspic33_device_internal_raw_word(cpu, PMP_CONTROL);
    transfer.mode = dspic33_device_internal_raw_word(cpu, PMP_MODE);
    transfer.value = cpu->data[PMP_ADDRESS + buffer_index];
    transfer.width = 1u;
    if (!pmp_output_push(&cpu->io.pmp.output, &transfer)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    pmp_slave_interrupt(cpu, buffer_index);
}

static void pmp_read_slave_buffer(Dspic33* cpu, uint16_t register_address) {
    uint16_t status_word;
    uint16_t full_mask;
    uint8_t first_byte;
    uint8_t access_width;

    if (!pmp_slave_configured(cpu) || register_address < PMP_DATA ||
        register_address > PMP_INPUT_2 + 1u) {
        return;
    }
    first_byte = (uint8_t)(register_address - PMP_DATA);
    access_width = 1u;
    if (cpu->io.cpu_read_valid && cpu->io.cpu_read_width == 2u &&
        cpu->io.cpu_read_address >= PMP_DATA && cpu->io.cpu_read_address <= PMP_INPUT_2) {
        first_byte = (uint8_t)(cpu->io.cpu_read_address - PMP_DATA);
        access_width = 2u;
    }
    status_word = dspic33_device_internal_raw_word(cpu, PMP_STATUS);
    full_mask = pmp_buffered_slave(cpu)
                    ? (uint16_t)(((1u << access_width) - 1u) << (8u + first_byte))
                    : PMP_INPUT_FULL;
    if (pmp_buffered_slave(cpu) || first_byte == 0u) {
        dspic33_device_internal_raw_write_word(cpu, PMP_STATUS,
                                               (uint16_t)(status_word & ~full_mask));
        pmp_refresh_slave_status(cpu);
    }
}

static void pmp_write_slave_buffer(Dspic33* cpu, uint16_t register_address) {
    uint16_t status_word;
    uint16_t empty_mask;
    uint8_t first_byte;
    uint8_t access_width;

    if (!pmp_slave_configured(cpu)) {
        return;
    }
    first_byte = (uint8_t)(register_address - PMP_ADDRESS);
    access_width = 1u;
    if (cpu->io.cpu_write_valid && cpu->io.cpu_write_width == 2u &&
        cpu->io.cpu_write_address >= PMP_ADDRESS && cpu->io.cpu_write_address <= PMP_OUTPUT_2) {
        first_byte = (uint8_t)(cpu->io.cpu_write_address - PMP_ADDRESS);
        access_width = 2u;
    }
    status_word = dspic33_device_internal_raw_word(cpu, PMP_STATUS);
    empty_mask = pmp_buffered_slave(cpu) ? (uint16_t)(((1u << access_width) - 1u) << first_byte)
                                         : PMP_OUTPUT_EMPTY;
    if (pmp_buffered_slave(cpu) || first_byte == 0u) {
        dspic33_device_internal_raw_write_word(cpu, PMP_STATUS,
                                               (uint16_t)(status_word & ~empty_mask));
        pmp_refresh_slave_status(cpu);
    }
}

void dspic33_device_internal_run_pmp_pmd(Dspic33* cpu, uint32_t event_value) {
    uint16_t event_generation = (uint16_t)(event_value >> 1u);

    if (event_generation != cpu->io.pmp.pmd_generation) {
        return;
    }
    cpu->io.pmp.pmd_disabled = (event_value & 1u) != 0u;
    dspic33_device_power_state_changed(cpu);
}

void dspic33_device_internal_update_pmp_pmd(Dspic33* cpu, uint16_t previous_value) {
    bool pmd_is_disabled = (dspic33_device_internal_raw_word(cpu, PMP_PMD_ADDRESS) & PMP_PMD) != 0u;

    if (((previous_value & PMP_PMD) != 0u) == pmd_is_disabled) {
        return;
    }
    cpu->io.pmp.pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_PMD,
                          ((uint32_t)cpu->io.pmp.pmd_generation << 1u) |
                              (pmd_is_disabled ? 1u : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        dspic33_device_internal_raw_write_word(cpu, PMP_PMD_ADDRESS, previous_value);
        cpu->io.pmp.pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void pmp_start_transfer(Dspic33* cpu, bool is_read) {
    cpu->io.pmp.generation++;
    cpu->io.pmp.address = dspic33_device_internal_raw_word(cpu, PMP_ADDRESS);
    cpu->io.pmp.control = dspic33_device_internal_raw_word(cpu, PMP_CONTROL);
    cpu->io.pmp.mode = (uint16_t)(dspic33_device_internal_raw_word(cpu, PMP_MODE) & ~PMP_BUSY);
    cpu->io.pmp.width = pmp_transfer_width(cpu->io.pmp.mode);
    cpu->io.pmp.value = is_read ? 0u
                                : (uint16_t)(dspic33_device_internal_raw_word(cpu, PMP_DATA) &
                                             (cpu->io.pmp.width == 2u ? 0xffffu : 0x00ffu));
    cpu->io.pmp.reading = is_read;
    cpu->io.pmp.active = true;
    uint64_t transfer_delay = pmp_transfer_cycles(cpu->io.pmp.control, cpu->io.pmp.mode);

    if (transfer_delay > 1u) {
        dspic33_device_internal_raw_write_word(
            cpu, PMP_MODE, (uint16_t)(dspic33_device_internal_raw_word(cpu, PMP_MODE) | PMP_BUSY));
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_COMPLETE, cpu->io.pmp.generation,
                          transfer_delay) ||
        (transfer_delay > 1u && !dspic33_schedule(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_CLEAR_BUSY,
                                                  cpu->io.pmp.generation, transfer_delay - 1u))) {
        pmp_abort(cpu);
    }
}

void dspic33_device_internal_pmp_clear_busy(Dspic33* cpu, uint16_t event_generation) {
    if (!cpu->io.pmp.active || event_generation != cpu->io.pmp.generation) {
        return;
    }
    cpu->io.pmp.completing.cycle = 0u;
    cpu->io.pmp.completing.address = cpu->io.pmp.address;
    cpu->io.pmp.completing.control = cpu->io.pmp.control;
    cpu->io.pmp.completing.mode = cpu->io.pmp.mode;
    cpu->io.pmp.completing.value = cpu->io.pmp.value;
    cpu->io.pmp.completing.width = cpu->io.pmp.width;
    cpu->io.pmp.completing_generation = event_generation;
    cpu->io.pmp.completing_reading = cpu->io.pmp.reading;
    cpu->io.pmp.completing_active = true;
    cpu->io.pmp.active = false;
    cpu->io.pmp.reading = false;
    pmp_update_address(cpu, cpu->io.pmp.control, cpu->io.pmp.mode);
    dspic33_device_internal_raw_write_word(
        cpu, PMP_MODE, (uint16_t)(dspic33_device_internal_raw_word(cpu, PMP_MODE) & ~PMP_BUSY));
}

void dspic33_device_internal_run_pmp(Dspic33* cpu, uint16_t event_generation) {
    Dspic33PmpResponse response;
    Dspic33PmpTransfer transfer;
    bool is_read;

    if (cpu->io.pmp.completing_active && event_generation == cpu->io.pmp.completing_generation) {
        transfer = cpu->io.pmp.completing;
        is_read = cpu->io.pmp.completing_reading;
        cpu->io.pmp.completing_active = false;
        cpu->io.pmp.completing_reading = false;
    } else if (cpu->io.pmp.active && event_generation == cpu->io.pmp.generation) {
        transfer.address = cpu->io.pmp.address;
        transfer.control = cpu->io.pmp.control;
        transfer.mode = cpu->io.pmp.mode;
        transfer.value = cpu->io.pmp.value;
        transfer.width = cpu->io.pmp.width;
        is_read = cpu->io.pmp.reading;
        cpu->io.pmp.active = false;
        cpu->io.pmp.reading = false;
        pmp_update_address(cpu, cpu->io.pmp.control, cpu->io.pmp.mode);
        dspic33_device_internal_raw_write_word(
            cpu, PMP_MODE, (uint16_t)(dspic33_device_internal_raw_word(cpu, PMP_MODE) & ~PMP_BUSY));
    } else {
        return;
    }
    transfer.cycle = cpu->device_cycles;
    if (is_read) {
        transfer.value = 0u;
        if (pmp_response_pop(&cpu->io.pmp.input, cpu->device_cycles, &response)) {
            transfer.value = response.value;
        }
        transfer.value &= transfer.width == 2u ? 0xffffu : 0x00ffu;
        dspic33_device_internal_raw_write_word(
            cpu, PMP_DATA,
            transfer.width == 2u
                ? transfer.value
                : (uint16_t)((dspic33_device_internal_raw_word(cpu, PMP_DATA) & 0xff00u) |
                             transfer.value));
        cpu->io.pmp.last_read = transfer;
        cpu->io.pmp.last_read_valid = true;
    } else {
        if (!pmp_output_push(&cpu->io.pmp.output, &transfer)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
    }
    if ((transfer.mode & PMP_INTERRUPT_MODE_MASK) == PMP_INTERRUPT_EACH) {
        dspic33_raise_interrupt(cpu, PMP_IRQ);
        if (!dspic33_dma_request(cpu, PMP_DMA_REQUEST, 0u, 0u)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
}

static bool pmp_initiating_write(const Dspic33* cpu, uint16_t register_address) {
    return register_address == PMP_DATA ||
           (register_address == PMP_DATA + 1u && cpu->io.cpu_write_valid &&
            cpu->io.cpu_write_width == 2u && cpu->io.cpu_write_address == PMP_DATA);
}

void dspic33_device_internal_pmp_read_register(Dspic33* cpu, uint16_t register_address) {
    pmp_read_slave_buffer(cpu, register_address);
    if (register_address == PMP_DATA && !cpu->io.pmp.active && pmp_master_enabled(cpu)) {
        pmp_start_transfer(cpu, true);
    }
}

void dspic33_device_internal_update_pmp_register(Dspic33* cpu, uint16_t register_address,
                                                 uint16_t previous_value) {
    uint16_t register_base = (uint16_t)(register_address & 0xfffeu);

    if (register_base < PMP_CONTROL || register_base > PMP_STATUS) {
        return;
    }
    if (cpu->io.pmp.pmd_disabled) {
        dspic33_device_internal_raw_write_word(cpu, register_base, previous_value);
        return;
    }
    if (register_base == PMP_CONTROL) {
        uint16_t current_control = dspic33_device_internal_raw_word(cpu, PMP_CONTROL);
        bool was_enabled = (previous_value & PMP_ENABLE) != 0u;
        bool is_enabled = (current_control & PMP_ENABLE) != 0u;

        if (was_enabled && !is_enabled) {
            pmp_abort(cpu);
        } else if (!was_enabled && is_enabled) {
            dspic33_device_internal_raw_write_word(cpu, PMP_STATUS, 0x008fu);
            cpu->io.pmp.slave_read_index = 0u;
            cpu->io.pmp.slave_write_index = 0u;
        }
        if (((previous_value ^ current_control) & PMP_STOP_IDLE) != 0u &&
            cpu->power_state == DSPIC33_POWER_IDLE) {
            dspic33_device_power_state_changed(cpu);
        }
        return;
    }
    if (register_base == PMP_ADDRESS || register_base == PMP_OUTPUT_2) {
        pmp_write_slave_buffer(cpu, register_address);
        return;
    }
    if (register_base != PMP_DATA || !pmp_initiating_write(cpu, register_address)) {
        return;
    }
    if (cpu->io.pmp.active) {
        dspic33_device_internal_raw_write_word(cpu, PMP_DATA, previous_value);
        return;
    }
    if (pmp_master_enabled(cpu)) {
        pmp_start_transfer(cpu, false);
    }
}
