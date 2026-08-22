
static void apply_master_clear_sfr_reset(Dspic33* cpu, const uint8_t* previous) {
    size_t reset_count;
    const Dspic33SfrMasterClearReset* resets =
        dspic33ep_mu_master_clear_resets(cpu->device, &reset_count);
    size_t index;
    for (index = 0u; index < reset_count; index++) {
        const Dspic33SfrMasterClearReset* reset = &resets[index];
        uint16_t address = reset->address;
        uint16_t current =
            (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
        uint16_t prior = (uint16_t)(previous[address] | ((uint16_t)previous[address + 1u] << 8u));
        uint16_t mask = (uint16_t)(reset->known_mask | reset->unchanged);
        uint16_t value = (uint16_t)((current & ~mask) | reset->value | (prior & reset->unchanged));
        cpu->data[address] = (uint8_t)value;
        cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    }
}

typedef struct {
    Dspic33PmpResponseQueue pmp;
    Dspic33I2cResponseQueue i2c[DSPIC33_I2C_COUNT];
    Dspic33CanQueue can[DSPIC33_CAN_COUNT];
    Dspic33UsbPending usb[DSPIC33_USB_PENDING_COUNT];
} Dspic33ExternalQueues;

static bool external_queues_pending(const Dspic33* cpu) {
    size_t index;
    if (cpu->io.pmp.input.count != 0u) {
        return true;
    }
    for (index = 0u; index < DSPIC33_I2C_COUNT; index++) {
        if (cpu->io.i2c_response[index].count != 0u) {
            return true;
        }
    }
    for (index = 0u; index < DSPIC33_CAN_COUNT; index++) {
        if (cpu->io.can_rx[index].count != 0u) {
            return true;
        }
    }
    for (index = 0u; index < cpu->events.count; index++) {
        const Dspic33Event* event = &cpu->events.items[index];
        if (event->external && event->type == DSPIC33_EVENT_USB &&
            event->source < DSPIC33_USB_PENDING_COUNT) {
            return true;
        }
    }
    return false;
}

static bool capture_external_queues(const Dspic33* cpu, Dspic33ExternalQueues** queues) {
    size_t index;
    *queues = NULL;
    if (!external_queues_pending(cpu)) {
        return true;
    }
    *queues = calloc(1u, sizeof(**queues));
    if (*queues == NULL) {
        return false;
    }
    (*queues)->pmp = cpu->io.pmp.input;
    memcpy((*queues)->i2c, cpu->io.i2c_response, sizeof((*queues)->i2c));
    memcpy((*queues)->can, cpu->io.can_rx, sizeof((*queues)->can));
    for (index = 0u; index < cpu->events.count; index++) {
        const Dspic33Event* event = &cpu->events.items[index];
        if (event->external && event->type == DSPIC33_EVENT_USB &&
            event->source < DSPIC33_USB_PENDING_COUNT) {
            (*queues)->usb[event->source] = cpu->io.usb_pending[event->source];
        }
    }
    return true;
}

static size_t retain_external_events(Dspic33* cpu) {
    size_t input;
    size_t output = 0u;
    for (input = 0u; input < cpu->events.count; input++) {
        if (cpu->events.items[input].external) {
            cpu->events.items[output++] = cpu->events.items[input];
        }
    }
    return output;
}

static void restore_external_queues(Dspic33* cpu, const Dspic33ExternalQueues* queues) {
    if (queues == NULL) {
        return;
    }
    cpu->io.pmp.input = queues->pmp;
    memcpy(cpu->io.i2c_response, queues->i2c, sizeof(queues->i2c));
    memcpy(cpu->io.can_rx, queues->can, sizeof(queues->can));
    memcpy(cpu->io.usb_pending, queues->usb, sizeof(queues->usb));
}

static void perform_warm_reset(Dspic33* cpu, uint16_t cause, Dspic33ResetKind kind) {
    static const uint16_t preserved_addresses[] = {
        0x0626u, 0x0728u, 0x0742u, 0x0744u, 0x0746u, 0x0748u, 0x0758u, 0x075au,
    };
    uint16_t preserved_values[sizeof(preserved_addresses) / sizeof(preserved_addresses[0])];
    uint8_t previous_sfr[0x1000u];
    uint16_t adc[DSPIC33_ADC_CHANNEL_COUNT];
    uint16_t gpio[DSPIC33_GPIO_PORT_COUNT];
    uint16_t comparator_input[DSPIC33_COMPARATOR_COUNT][DSPIC33_COMPARATOR_INPUT_COUNT];
    uint16_t comparator_reference[DSPIC33_COMPARATOR_REFERENCE_COUNT];
    uint16_t dci_receive[DSPIC33_DCI_BUFFER_COUNT];
    uint16_t dci_input = cpu->io.dci.input;
    Dspic33Rtcc rtcc = cpu->io.rtcc;
    uint64_t instructions = cpu->instructions;
    uint64_t cycles = cpu->cycles;
    uint64_t device_cycles = cpu->device_cycles;
    uint64_t interrupt_count = cpu->interrupt_count;
    uint64_t software_reset_count =
        cpu->software_reset_count + (kind == DSPIC33_RESET_SOFTWARE ? 1u : 0u);
    uint64_t trap_count = cpu->trap_count;
    uint32_t reset_entry = (cpu->configuration[0x0eu] & 0x04u) != 0u ? 0u : 0x007ffffcu;
    bool auxiliary_security_reset =
        reset_entry == 0x007ffffcu && codeguard_high_security(codeguard_configuration(cpu, true));
    uint64_t illegal_reset_count =
        cpu->illegal_reset_count +
        (kind == DSPIC33_RESET_ILLEGAL || auxiliary_security_reset ? 1u : 0u);
    uint64_t auxiliary_pll_remaining = 0u;
    uint64_t oscillator_remaining = 0u;
    uint16_t oscillator_phase = 0u;
    uint16_t reset_interrupt = cpu->last_interrupt;
    uint16_t rcon = (uint16_t)(cpu->data[0x0740u] | ((uint16_t)cpu->data[0x0741u] << 8u));
    uint8_t uart_cts = cpu->io.uart_cts;
    uint8_t spi_selected = cpu->io.spi_selected;
    uint8_t spi_pin_clock_high = cpu->io.spi_pin_clock_high;
    uint8_t spi_pin_data_high = cpu->io.spi_pin_data_high;
    uint8_t spi_pin_input_enabled = cpu->io.spi_pin_input_enabled;
    uint8_t spi_pin_select_high = cpu->io.spi_pin_select_high;
    uint16_t timer_gate = cpu->io.timer_gate;
    uint8_t pwm_dead_time_inputs = cpu->io.pwm_dead_time_inputs;
    uint8_t pwm_sync_inputs = cpu->io.pwm_sync_inputs;
    uint8_t pwm_dead_time_direct = cpu->io.pwm_dead_time_direct;
    uint8_t pwm_sync_direct = cpu->io.pwm_sync_direct;
    uint8_t output_compare_fault_inputs = cpu->io.output_compare.fault_inputs;
    uint8_t output_compare_fault_direct_mask = cpu->io.output_compare.fault_direct_mask;
    uint32_t pwm_fault_inputs = cpu->io.pwm_fault_inputs;
    uint32_t pwm_current_limit_inputs = cpu->io.pwm_current_limit_inputs;
    uint32_t pwm_fault_direct = cpu->io.pwm_fault_direct;
    uint32_t pwm_current_limit_direct = cpu->io.pwm_current_limit_direct;
    bool usb_host_attached = cpu->io.usb_host_attached;
    bool async_events_enabled = cpu->async_events_enabled;
    bool stop_on_trap = cpu->stop_on_trap;
    bool reset_instruction_timing = kind == DSPIC33_RESET_SOFTWARE && cpu->instruction_active;
    bool auxiliary_pll_pending = false;
    bool oscillator_pending = false;
    uint32_t auxiliary_pll_generation = cpu->io.auxiliary_pll_generation;
    Dspic33Oscillator oscillator = cpu->oscillator;
    Dspic33ExternalQueues* external_queues;
    size_t external_event_count;
    uint64_t event_sequence = cpu->events.sequence;
    size_t index;
    if (cpu->nvm.active) {
        if (!cpu->nvm.reset_pending) {
            cpu->nvm.reset_cause = cause;
            cpu->nvm.reset_kind = (uint8_t)kind;
            cpu->nvm.reset_pending = true;
        }
        if (kind == DSPIC33_RESET_ILLEGAL) {
            cpu->illegal_reset = true;
        }
        return;
    }
    if (!capture_external_queues(cpu, &external_queues)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    for (index = 0u; index < cpu->events.count; index++) {
        const Dspic33Event* event = &cpu->events.items[index];
        if (event->type == DSPIC33_EVENT_AUX_PLL && event->value == auxiliary_pll_generation) {
            auxiliary_pll_remaining =
                event->cycle > device_cycles ? event->cycle - device_cycles : 0u;
            auxiliary_pll_pending = true;
            break;
        }
    }
    for (index = 0u; index < cpu->events.count; index++) {
        const Dspic33Event* event = &cpu->events.items[index];
        if (event->type == DSPIC33_EVENT_OSCILLATOR && event->value == oscillator.generation) {
            oscillator_remaining = event->cycle > device_cycles ? event->cycle - device_cycles : 0u;
            oscillator_phase = event->source;
            oscillator_pending = true;
            break;
        }
    }
    for (index = 0u; index < sizeof(preserved_addresses) / sizeof(preserved_addresses[0]);
         index++) {
        uint16_t address = preserved_addresses[index];
        preserved_values[index] =
            (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
    }
    memcpy(previous_sfr, cpu->data, sizeof(previous_sfr));
    memcpy(adc, cpu->io.adc, sizeof(adc));
    memcpy(gpio, cpu->io.gpio, sizeof(gpio));
    memcpy(comparator_input, cpu->io.comparator.input, sizeof(comparator_input));
    memcpy(comparator_reference, cpu->io.comparator.reference, sizeof(comparator_reference));
    for (index = 0u; index < DSPIC33_DCI_BUFFER_COUNT; index++) {
        uint16_t address = (uint16_t)(0x0290u + index * 2u);
        dci_receive[index] =
            (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
    }
    external_event_count = retain_external_events(cpu);
    reset_processor(cpu, auxiliary_security_reset ? 0u : reset_entry, false);
    apply_master_clear_sfr_reset(cpu, previous_sfr);
    for (index = 0u; index < sizeof(preserved_addresses) / sizeof(preserved_addresses[0]);
         index++) {
        uint16_t address = preserved_addresses[index];
        cpu->data[address] = (uint8_t)preserved_values[index];
        cpu->data[address + 1u] = (uint8_t)(preserved_values[index] >> 8u);
    }
    memcpy(cpu->io.adc, adc, sizeof(adc));
    memcpy(cpu->io.gpio, gpio, sizeof(gpio));
    memcpy(cpu->io.comparator.input, comparator_input, sizeof(comparator_input));
    memcpy(cpu->io.comparator.reference, comparator_reference, sizeof(comparator_reference));
    for (index = 0u; index < DSPIC33_DCI_BUFFER_COUNT; index++) {
        uint16_t address = (uint16_t)(0x0290u + index * 2u);
        cpu->data[address] = (uint8_t)dci_receive[index];
        cpu->data[address + 1u] = (uint8_t)(dci_receive[index] >> 8u);
    }
    cpu->io.dci.input = dci_input;
    memcpy(cpu->io.rtcc.calendar, rtcc.calendar, sizeof(rtcc.calendar));
    memcpy(cpu->io.rtcc.alarm, rtcc.alarm, sizeof(rtcc.alarm));
    cpu->io.rtcc.prescaler = rtcc.prescaler;
    cpu->io.rtcc.alarm_output = rtcc.alarm_output;
    cpu->io.rtcc.calibration_pending = rtcc.calibration_pending;
    cpu->io.uart_cts = uart_cts;
    cpu->io.spi_selected = spi_selected;
    cpu->io.spi_pin_clock_high = spi_pin_clock_high;
    cpu->io.spi_pin_data_high = spi_pin_data_high;
    cpu->io.spi_pin_input_enabled = spi_pin_input_enabled;
    cpu->io.spi_pin_select_high = spi_pin_select_high;
    cpu->io.timer_gate = timer_gate;
    cpu->io.pwm_dead_time_direct = pwm_dead_time_direct;
    cpu->io.pwm_sync_direct = pwm_sync_direct;
    cpu->io.pwm_dead_time_inputs = (uint8_t)(pwm_dead_time_inputs & pwm_dead_time_direct);
    cpu->io.pwm_sync_inputs = (uint8_t)(pwm_sync_inputs & pwm_sync_direct);
    cpu->io.output_compare.fault_direct_mask = output_compare_fault_direct_mask;
    cpu->io.output_compare.fault_inputs =
        (uint8_t)(output_compare_fault_inputs & output_compare_fault_direct_mask);
    cpu->io.pwm_fault_direct = pwm_fault_direct;
    cpu->io.pwm_current_limit_direct = pwm_current_limit_direct;
    cpu->io.pwm_fault_inputs = pwm_fault_inputs & pwm_fault_direct;
    cpu->io.pwm_current_limit_inputs = pwm_current_limit_inputs & pwm_current_limit_direct;
    cpu->io.usb_host_attached = usb_host_attached;
    cpu->instructions = instructions;
    cpu->cycles = cycles;
    cpu->device_cycles = device_cycles;
    cpu->events.count = external_event_count;
    cpu->events.sequence = event_sequence;
    dspic33_reorder_events(cpu);
    restore_external_queues(cpu, external_queues);
    free(external_queues);
    cpu->io.auxiliary_pll_generation = auxiliary_pll_generation;
    if (auxiliary_pll_pending &&
        !dspic33_schedule(cpu, DSPIC33_EVENT_AUX_PLL, 0u, auxiliary_pll_generation,
                          auxiliary_pll_remaining)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
    oscillator.key_stage = 0u;
    cpu->oscillator = oscillator;
    if (oscillator_pending && !dspic33_schedule(cpu, DSPIC33_EVENT_OSCILLATOR, oscillator_phase,
                                                oscillator.generation, oscillator_remaining)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
    cpu->interrupt_count = interrupt_count;
    cpu->software_reset_count = software_reset_count;
    cpu->illegal_reset_count = illegal_reset_count;
    cpu->trap_count = trap_count;
    cpu->reset_interrupt = reset_interrupt;
    cpu->async_events_enabled = async_events_enabled;
    cpu->stop_on_trap = stop_on_trap;
    cpu->illegal_reset = kind == DSPIC33_RESET_ILLEGAL || auxiliary_security_reset;
    cpu->reset_occurred = true;
    cpu->reset_instruction_timing = reset_instruction_timing;
    cpu->reset_locked = auxiliary_security_reset;
    rcon = (uint16_t)((rcon | cause | (auxiliary_security_reset ? 0x4000u : 0u)) & 0xcadfu);
    cpu->data[0x0740u] = (uint8_t)rcon;
    cpu->data[0x0741u] = (uint8_t)(rcon >> 8u);
    dspic33_device_reset_restored(cpu);
}

static bool watchdog_enabled(const Dspic33* cpu) {
    return (cpu->configuration[10u] & 0x80u) != 0u || (cpu->data[0x0740u] & 0x20u) != 0u;
}

static uint32_t watchdog_period(const Dspic33* cpu) {
    uint8_t configuration = cpu->configuration[10u];
    uint32_t prescaler = (configuration & 0x10u) != 0u ? 128u : 32u;
    return prescaler << (configuration & 0x0fu);
}

static void watchdog_timeout(Dspic33* cpu) {
    cpu->watchdog.ticks = 0u;
    if (cpu->power_state != DSPIC33_POWER_ACTIVE) {
        cpu->data[0x0740u] |= 0x10u;
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        dspic33_device_power_state_changed(cpu);
        cpu->stop_reason = DSPIC33_RUNNING;
        return;
    }
    if (cpu->nvm.active) {
        cpu->watchdog.reset_pending = true;
        return;
    }
    perform_warm_reset(cpu, 0x0010u, DSPIC33_RESET_HARDWARE);
}

static void clear_watchdog(Dspic33* cpu) {
    uint32_t period = watchdog_period(cpu);
    if (watchdog_enabled(cpu) && (cpu->configuration[10u] & 0x40u) == 0u &&
        cpu->watchdog.ticks < period - period / 4u) {
        watchdog_timeout(cpu);
        return;
    }
    cpu->watchdog.ticks = 0u;
}

void dspic33_watchdog_advance_lprc(Dspic33* cpu, uint64_t ticks) {
    while (ticks != 0u && watchdog_enabled(cpu)) {
        uint32_t period = watchdog_period(cpu);
        uint32_t remaining = period - cpu->watchdog.ticks;
        if (ticks < remaining) {
            cpu->watchdog.ticks += (uint32_t)ticks;
            return;
        }
        ticks -= remaining;
        watchdog_timeout(cpu);
        if (cpu->watchdog.reset_pending) {
            return;
        }
    }
    if (!watchdog_enabled(cpu)) {
        cpu->watchdog.ticks = 0u;
    }
}

bool dspic33_watchdog_complete_nvm(Dspic33* cpu) {
    if (!cpu->watchdog.reset_pending) {
        return false;
    }
    cpu->watchdog.reset_pending = false;
    perform_warm_reset(cpu, 0x0010u, DSPIC33_RESET_HARDWARE);
    return true;
}

bool dspic33_complete_nvm_reset(Dspic33* cpu) {
    uint16_t cause;
    Dspic33ResetKind kind;
    if (!cpu->nvm.reset_pending) {
        return false;
    }
    cause = cpu->nvm.reset_cause;
    kind = (Dspic33ResetKind)cpu->nvm.reset_kind;
    cpu->nvm.reset_pending = false;
    perform_warm_reset(cpu, cause, kind);
    return true;
}

void dspic33_configuration_mismatch_reset(Dspic33* cpu) {
    perform_warm_reset(cpu, 0x0200u, DSPIC33_RESET_HARDWARE);
}

void dspic33_mclr_reset(Dspic33* cpu) { perform_warm_reset(cpu, 0x0080u, DSPIC33_RESET_HARDWARE); }

void dspic33_brown_out_reset(Dspic33* cpu) {
    uint16_t previous_rcon = dspic33_read_word(cpu, 0x0740u);
    cpu->nvm.active = false;
    perform_warm_reset(cpu, 0u, DSPIC33_RESET_HARDWARE);
    previous_rcon =
        (uint16_t)((previous_rcon & 0x0081u) | 0x0002u | (cpu->reset_locked ? 0x4000u : 0u));
    cpu->data[0x0740u] = (uint8_t)previous_rcon;
    cpu->data[0x0741u] = (uint8_t)(previous_rcon >> 8u);
    dspic33_device_brown_out_reset(cpu);
}

void dspic33_reset(Dspic33* cpu, uint32_t entry) {
    reset_processor(cpu, entry, true);
    dspic33_device_power_on_reset(cpu);
}

void dspic33_set_async_events(Dspic33* cpu, bool enabled) { cpu->async_events_enabled = enabled; }

bool dspic33_load_program_word(Dspic33* cpu, uint32_t address, uint32_t word) {
    uint32_t* destination;
    if (cpu == NULL || (address & 1u) != 0u) {
        return false;
    }
    destination = writable_program_word(cpu, address);
    if (destination == NULL) {
        return false;
    }
    *destination = word & 0x00ffffffu;
    return true;
}

static void erase_program_words(uint32_t* words, uint32_t count) {
    uint32_t index;
    for (index = 0u; index < count; index++) {
        words[index] = 0x00ffffffu;
    }
}

void dspic33_complete_nvm(Dspic33* cpu) {
    uint16_t operation = (uint16_t)(cpu->nvm.control & 0x000fu);
    uint32_t target = cpu->nvm.address;
    uint32_t count;
    uint32_t index;
    if (operation == 0u) {
        uint8_t configuration_index;
        uint32_t offset;
        uint8_t current;
        uint8_t latch;
        uint8_t mask;
        if (!configuration_register_index(target, &configuration_index)) {
            return;
        }
        if (target == DSPIC33_CONFIGURATION_BASE + CODEGUARD_AUXILIARY_CONFIGURATION_OFFSET &&
            ((cpu->configuration[CODEGUARD_GENERAL_CONFIGURATION_OFFSET] & 0x33u) != 0x03u ||
             (cpu->configuration[CODEGUARD_AUXILIARY_CONFIGURATION_OFFSET] & 0x33u) != 0x03u)) {
            return;
        }
        offset = target - DSPIC33_CONFIGURATION_BASE;
        mask = configuration_program_masks[configuration_index];
        current = cpu->configuration[offset];
        latch = (uint8_t)cpu->nvm.latches[0];
        if (configuration_index == 0u || configuration_index == 6u) {
            cpu->configuration[offset] =
                (uint8_t)((current & (uint8_t)~mask) | (latch & 0x30u) | (current & latch & 0x03u));
        } else {
            cpu->configuration[offset] = (uint8_t)((current & (uint8_t)~mask) | (latch & mask));
        }
        dspic33_device_configuration_changed(cpu, target, current);
        return;
    }
    if (operation == 0x0au) {
        if (cpu->nvm.auxiliary_origin) {
            return;
        }
        erase_program_words(cpu->auxiliary_program, DSPIC33_AUXILIARY_PROGRAM_WORDS);
        cpu->configuration[0x10u] = configuration_factory_defaults[6u];
        cpu->configuration[0x11u] = 0xffu;
        return;
    }
    if (operation == 0x0du) {
        if (!cpu->nvm.auxiliary_origin) {
            return;
        }
        erase_program_words(cpu->program, DSPIC33_PROGRAM_WORDS);
        erase_program_words(cpu->persistent_program + PERSISTENT_PROGRAM_PHYSICAL_BASE / 2u,
                            (PERSISTENT_PROGRAM_PHYSICAL_LIMIT - PERSISTENT_PROGRAM_PHYSICAL_BASE) /
                                2u);
        cpu->configuration[4u] = configuration_factory_defaults[0u];
        cpu->configuration[5u] = 0xffu;
        return;
    }
    if (operation == 1u) {
        target &= 0x00fffffcu;
        count = 2u;
    } else if (operation == 2u) {
        target &= 0x00ffff00u;
        count = DSPIC33_WRITE_LATCH_WORDS;
    } else if (operation == 3u) {
        target &= 0x00fff800u;
        if (!codeguard_programming_allowed(cpu, target)) {
            return;
        }
        for (index = 0u; index < 0x400u; index++) {
            uint32_t* destination = writable_program_word(cpu, target + index * 2u);
            if (destination != NULL) {
                *destination = 0x00ffffffu;
            }
        }
        return;
    } else {
        return;
    }
    if (!codeguard_programming_allowed(cpu, target)) {
        return;
    }
    for (index = 0u; index < count; index++) {
        uint32_t* destination = writable_program_word(cpu, target + index * 2u);
        if (destination != NULL) {
            *destination &= cpu->nvm.latches[index];
        }
    }
}

bool dspic33_load_configuration_word(Dspic33* cpu, uint32_t address, uint32_t word) {
    uint32_t offset;
    if (address < DSPIC33_CONFIGURATION_BASE ||
        address + 1u >= DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE) {
        return false;
    }
    offset = address - DSPIC33_CONFIGURATION_BASE;
    cpu->configuration[offset] = (uint8_t)word;
    cpu->configuration[offset + 1u] = (uint8_t)(word >> 8u);
    return true;
}

uint8_t dspic33_read_program_byte(const Dspic33* cpu, uint32_t address) {
    uint32_t word_address = address & ~1u;
    uint32_t word;
    word = dspic33_read_program_word(cpu, word_address);
    return (uint8_t)(word >> ((address & 1u) * 8u));
}

uint8_t dspic33_read_configuration_byte(const Dspic33* cpu, uint32_t address) {
    if (address < DSPIC33_CONFIGURATION_BASE ||
        address >= DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE) {
        return 0xffu;
    }
    return cpu->configuration[address - DSPIC33_CONFIGURATION_BASE];
}

void dspic33_write_byte(Dspic33* cpu, uint32_t address, uint8_t value) {
    uint16_t previous;
    uint8_t accumulator;
    uint8_t accumulator_byte;
    if ((cpu->address_error && !cpu->address_error_access_allowed) ||
        !check_data_implementation(cpu, address, 1u) || address >= DSPIC33_DATA_SIZE) {
        return;
    }
    if (!data_byte_is_implemented(cpu, address)) {
        return;
    }
    if (!cpu->io.dma_transfer_active) {
        if (cpu->instruction_active && address >= 0x1000u) {
            cpu->io.cpu_bus_cycle = cpu->cycles;
        }
        cpu->io.cpu_write_cycle = cpu->cycles;
        cpu->io.cpu_write_instruction = cpu->instructions;
        cpu->io.cpu_write_address = address;
        cpu->io.cpu_write_previous = address < 32u
                                         ? (uint8_t)(cpu->w[address / 2u] >> ((address & 1u) * 8u))
                                         : cpu->data[address];
        cpu->io.cpu_write_width = 1u;
        cpu->io.cpu_write_valid = true;
        cpu->io.cpu_write_rmw = instruction_rmw_write_matches(cpu, address, 1u);
    }
    record_var_write(cpu, address, 1u);
    if (address < 32u) {
        uint8_t reg = (uint8_t)(address / 2u);
        write_working_register_byte(cpu, reg, (address & 1u) != 0u, value);
        return;
    }
    if (accumulator_byte_location(address, &accumulator, &accumulator_byte)) {
        write_accumulator_byte(cpu, accumulator, accumulator_byte, value);
        return;
    }
    if (address >= 0x0020u && address <= 0x0055u) {
        uint16_t* reg = NULL;
        uint16_t word_address = (uint16_t)(address & 0xfffeu);
        if (word_address == 0x002eu || word_address == 0x0030u) {
            return;
        }
        if (word_address == 0x0042u) {
            write_status_byte(cpu, (address & 1u) != 0u, value);
            return;
        }
        if (word_address == 0x0044u) {
            if ((address & 1u) == 0u) {
                uint8_t low = (uint8_t)cpu->corcon;
                low = (uint8_t)((low & 0x04u) | (value & 0xf3u) | (low & value & 0x08u));
                cpu->corcon = (uint16_t)((cpu->corcon & 0xff00u) | low);
            } else {
                uint8_t high = (uint8_t)(cpu->corcon >> 8u);
                if ((value & 0x08u) != 0u && cpu->do_depth != 0u) {
                    uint8_t depth = (uint8_t)(cpu->do_depth - 1u);
                    bool late =
                        cpu->instruction_active &&
                        (cpu->current_instruction_pc == cpu->do_end[depth] ||
                         program_address_add(cpu->current_instruction_pc, 2) == cpu->do_end[depth]);
                    cpu->do_terminate[depth] = late ? 2u : 1u;
                }
                high = (uint8_t)((high & 0x07u) | (value & 0xb0u));
                cpu->corcon = (uint16_t)((cpu->corcon & 0x00ffu) | ((uint16_t)high << 8u));
            }
            return;
        }
        if (word_address == 0x0054u) {
            if ((address & 1u) == 0u) {
                cpu->tblpag = value;
            }
            return;
        }
        if (word_address == 0x0052u) {
            write_disicnt_byte(cpu, (address & 1u) != 0u, value);
            return;
        }
        switch (word_address) {
        case 0x0020u:
            reg = &cpu->splim;
            break;
        case 0x0032u:
            reg = &cpu->dsrpag;
            break;
        case 0x0034u:
            reg = &cpu->dswpag;
            break;
        case 0x0036u:
            reg = &cpu->rcount;
            break;
        case 0x0038u:
            reg = &cpu->dcount;
            break;
        default:
            break;
        }
        if (reg != NULL) {
            if ((address & 1u) == 0u) {
                *reg = (uint16_t)((*reg & 0xff00u) | value);
            } else {
                *reg = (uint16_t)((*reg & 0x00ffu) | ((uint16_t)value << 8u));
            }
            if (word_address == 0x0020u) {
                *reg &= 0xfffeu;
            } else if (word_address == 0x0032u) {
                *reg &= 0x03ffu;
            } else if (word_address == 0x0034u) {
                *reg &= 0x01ffu;
            } else if (word_address == 0x0038u && cpu->do_depth != 0u) {
                cpu->do_count[cpu->do_depth - 1u] = cpu->dcount;
            }
            return;
        }
        if (word_address == 0x003au || word_address == 0x003cu) {
            return;
        }
        if (word_address == 0x003eu || word_address == 0x0040u) {
            uint8_t shift = (uint8_t)((address - 0x003eu) * 8u);
            cpu->doend = (cpu->doend & ~((uint32_t)0xffu << shift)) | ((uint32_t)value << shift);
            cpu->doend &= 0x003ffffeu;
            if (cpu->do_depth != 0u) {
                cpu->do_end[cpu->do_depth - 1u] = cpu->doend;
            }
            return;
        }
    }
    previous = (uint16_t)(cpu->data[address & ~1u] | ((uint16_t)cpu->data[address | 1u] << 8u));
    cpu->data[address] = value;
    if (address < 0x10000u) {
        dspic33_device_write_byte(cpu, (uint16_t)address, previous);
    }
}

void dspic33_write_word(Dspic33* cpu, uint32_t address, uint16_t value) {
    bool low_implemented;
    bool high_implemented;
    uint16_t previous;
    if (!check_data_alignment(cpu, address) || !check_data_implementation(cpu, address, 2u) ||
        (cpu->address_error && !cpu->address_error_access_allowed) ||
        address + 1u >= DSPIC33_DATA_SIZE) {
        return;
    }
    low_implemented = data_byte_is_implemented(cpu, address);
    high_implemented = data_byte_is_implemented(cpu, address + 1u);
    if (!low_implemented || !high_implemented) {
        if (low_implemented) {
            dspic33_write_byte(cpu, address, (uint8_t)value);
        }
        if (high_implemented) {
            dspic33_write_byte(cpu, address + 1u, (uint8_t)(value >> 8u));
        }
        return;
    }
    if (!cpu->io.dma_transfer_active) {
        if (cpu->instruction_active && address >= 0x1000u) {
            cpu->io.cpu_bus_cycle = cpu->cycles;
        }
        cpu->io.cpu_write_cycle = cpu->cycles;
        cpu->io.cpu_write_instruction = cpu->instructions;
        cpu->io.cpu_write_address = address;
        cpu->io.cpu_write_previous =
            address < 32u
                ? cpu->w[address / 2u]
                : (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
        cpu->io.cpu_write_width = 2u;
        cpu->io.cpu_write_valid = true;
        cpu->io.cpu_write_rmw = instruction_rmw_write_matches(cpu, address, 2u);
    }
    record_var_write(cpu, address, 2u);
    if (address < 32u) {
        write_working_register(cpu, (uint8_t)(address / 2u), value);
        return;
    }
    if (address >= 0x0020u && address <= 0x0054u) {
        if (address == 0x0052u) {
            if (cpu->disicnt != 0u) {
                cpu->disicnt = (uint16_t)(value & 0x3fffu);
            }
            return;
        }
        dspic33_write_byte(cpu, address, (uint8_t)value);
        dspic33_write_byte(cpu, address + 1u, (uint8_t)(value >> 8u));
        if (address == 0x0020u) {
            cpu->splim_enabled = true;
        }
        return;
    }
    previous = (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
    cpu->data[address] = (uint8_t)value;
    cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    if (address < 0xffffu) {
        dspic33_device_write_byte(cpu, (uint16_t)(address + 1u), previous);
    }
}

bool dspic33_cpu_rmw_matches(const Dspic33* cpu, uint32_t address, uint8_t width) {
    uint32_t write_end;
    uint32_t target_end;
    if (!cpu->io.cpu_write_valid || !cpu->io.cpu_write_rmw || !cpu->instruction_advancing ||
        cpu->io.cpu_write_instruction != cpu->instructions) {
        return false;
    }
    write_end = cpu->io.cpu_write_address + cpu->io.cpu_write_width;
    target_end = address + width;
    return cpu->io.cpu_write_address < target_end && address < write_end;
}

static uint8_t read_byte_value(Dspic33* cpu, uint32_t address) {
    uint16_t value;
    uint8_t accumulator;
    uint8_t accumulator_byte;
    if ((cpu->address_error && !cpu->address_error_access_allowed) ||
        !check_data_implementation(cpu, address, 1u)) {
        return 0u;
    }
    if ((address & PSV_ADDRESS) != 0u) {
        uint32_t program_address = address & PSV_ADDRESS_MASK;
        uint32_t word;
        if (!dspic33_device_program_range_implemented(cpu, program_address & ~1u, 2u)) {
            if (cpu->instruction_active &&
                program_target_requires_address_error(cpu, program_address & ~1u)) {
                raise_program_read_error(cpu);
            }
            return 0u;
        }
        word = read_cpu_program_word(cpu, program_address & ~1u);
        if ((address & PSV_HIGH_BYTE) != 0u) {
            return (program_address & 1u) == 0u ? (uint8_t)(word >> 16u) : 0u;
        }
        return (uint8_t)(word >> ((program_address & 1u) * 8u));
    }
    if (address >= DSPIC33_DATA_SIZE) {
        return 0u;
    }
    if (!data_byte_is_implemented(cpu, address)) {
        return 0u;
    }
    if (address < 32u) {
        value = cpu->w[address / 2u];
        return (uint8_t)(value >> ((address & 1u) * 8u));
    }
    if (accumulator_byte_location(address, &accumulator, &accumulator_byte)) {
        return read_accumulator_byte(cpu, accumulator, accumulator_byte);
    }
    switch (address & 0xfffeu) {
    case 0x0020u:
        value = cpu->splim;
        break;
    case 0x0032u:
        value = cpu->dsrpag;
        break;
    case 0x0034u:
        value = cpu->dswpag;
        break;
    case 0x0036u:
        value = cpu->rcount;
        break;
    case 0x0038u:
        value = cpu->dcount;
        break;
    case 0x003au:
        value = (uint16_t)cpu->dostart;
        break;
    case 0x003cu:
        value = (uint16_t)(cpu->dostart >> 16u);
        break;
    case 0x003eu:
        value = (uint16_t)cpu->doend;
        break;
    case 0x0040u:
        value = (uint16_t)(cpu->doend >> 16u);
        break;
    case 0x002eu:
        value = (uint16_t)cpu->pc;
        break;
    case 0x0030u:
        value = (uint16_t)((cpu->pc >> 16u) & 0x007fu);
        break;
    case 0x0042u:
        value = cpu->sr;
        break;
    case 0x0044u:
        value = (uint16_t)(cpu->corcon & ~0x0800u);
        break;
    case 0x0052u:
        value = cpu->disicnt;
        break;
    case 0x0054u:
        value = cpu->tblpag;
        break;
    default:
        if (address < 0x10000u) {
            return dspic33_device_read_byte(cpu, (uint16_t)address, cpu->data[address]);
        }
        return cpu->data[address];
    }
    return (uint8_t)(value >> ((address & 1u) * 8u));
}
