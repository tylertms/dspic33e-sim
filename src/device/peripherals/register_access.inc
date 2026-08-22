
static void abort_oscillator_switch(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    if (cpu->oscillator.active) {
        cpu->oscillator.generation++;
        cpu->oscillator.active = false;
        cpu->oscillator.automatic = false;
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
    }
    raw_write_word(cpu, OSCILLATOR_CONTROL, (uint16_t)(control & ~OSCILLATOR_SWITCH_ENABLE));
}

void dspic33_device_abort_oscillator_switch(Dspic33* cpu) {
    if (!cpu->oscillator.active && cpu->oscillator.lock_pending) {
        cpu->oscillator.generation++;
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
    }
    abort_oscillator_switch(cpu);
}

static bool oscillator_pll_mode(uint16_t control) {
    uint16_t source = (uint16_t)((control & OSCILLATOR_REQUEST_MASK) >> 8u);
    return source == 1u || source == 3u;
}

static uint8_t oscillator_current_source(uint16_t control) {
    return (uint8_t)((control & OSCILLATOR_CURRENT_MASK) >> 12u);
}

static uint8_t oscillator_requested_source(uint16_t control) {
    return (uint8_t)((control & OSCILLATOR_REQUEST_MASK) >> 8u);
}

static bool oscillator_source_available(const Dspic33* cpu, uint8_t source) {
    return (source != 2u && source != 3u) || (cpu->configuration[8u] & 0x03u) != 0x03u;
}

static bool oscillator_source_immediately_ready(uint8_t source) {
    return source == 0u || source >= 5u;
}

static bool oscillator_configuration_locked(const Dspic33* cpu, uint16_t control) {
    return (control & OSCILLATOR_CLOCK_LOCK) != 0u &&
           (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_CLOCK_LOCK) != 0u;
}

static bool oscillator_direct_pll_transition(uint16_t control) {
    uint8_t current = oscillator_current_source(control);
    uint8_t requested = oscillator_requested_source(control);
    return (current == 1u && requested == 3u) || (current == 3u && requested == 1u);
}

static bool oscillator_pll_lock_enabled(const Dspic33* cpu) {
    return (cpu->configuration[10u] & OSCILLATOR_CONFIGURATION_PLL_LOCK) != 0u;
}

static bool schedule_oscillator_event(Dspic33* cpu, uint16_t phase, uint64_t delay) {
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_OSCILLATOR, phase, cpu->oscillator.generation,
                          delay)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    return true;
}

static void schedule_oscillator_readiness(Dspic33* cpu, uint16_t control) {
    if (!oscillator_source_available(cpu, oscillator_requested_source(control))) {
        return;
    }
    schedule_oscillator_event(cpu, OSCILLATOR_EVENT_SWITCH,
                              oscillator_pll_mode(control) ? OSCILLATOR_SOURCE_DELAY
                                                           : OSCILLATOR_SWITCH_DELAY);
}

static uint8_t configured_main_pll_source(const Dspic33* cpu, uint16_t control) {
    if (cpu->oscillator.active) {
        if (oscillator_direct_pll_transition(control)) {
            return UINT8_MAX;
        }
        return oscillator_requested_source(control);
    }
    return oscillator_current_source(control);
}

static bool main_pll_relock_required(const Dspic33* cpu, uint16_t address, uint16_t previous,
                                     uint16_t current) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    uint8_t source = configured_main_pll_source(cpu, control);
    uint16_t changed = (uint16_t)(previous ^ current);
    if (source != 1u && source != 3u) {
        return false;
    }
    if (address == MAIN_PLL_FEEDBACK) {
        return (changed & MAIN_PLL_FEEDBACK_DIVISOR) != 0u;
    }
    if (address == MAIN_OSCILLATOR_TUNING) {
        return source == 1u && (changed & MAIN_FRC_TUNING) != 0u;
    }
    if (address != MAIN_CLOCK_DIVISOR) {
        return false;
    }
    if ((changed & MAIN_PLL_PRESCALER) != 0u) {
        return true;
    }
    return source == 1u && (changed & MAIN_FRC_DIVISOR) != 0u;
}

static void restart_main_pll_lock(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    bool source_ready = cpu->oscillator.active && cpu->oscillator.source_ready;
    cpu->oscillator.generation++;
    cpu->oscillator.lock_pending = source_ready || !cpu->oscillator.active;
    cpu->oscillator.source_ready = source_ready;
    raw_write_word(cpu, OSCILLATOR_CONTROL, (uint16_t)(control & ~OSCILLATOR_PLL_LOCK));
    if (cpu->oscillator.lock_pending) {
        schedule_oscillator_event(cpu, OSCILLATOR_EVENT_LOCK, OSCILLATOR_SWITCH_DELAY);
    } else {
        schedule_oscillator_readiness(cpu, control);
    }
}

static void update_main_clock_configuration(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    uint16_t current = raw_word(cpu, address);
    if (address != MAIN_CLOCK_DIVISOR && address != MAIN_PLL_FEEDBACK &&
        address != MAIN_OSCILLATOR_TUNING) {
        return;
    }
    if (oscillator_configuration_locked(cpu, control)) {
        raw_write_word(cpu, address, previous);
        return;
    }
    if (address == MAIN_CLOCK_DIVISOR) {
        if ((previous & 0x0800u) != 0u) {
            current = (uint16_t)((current & ~0x7000u) | (previous & 0x7000u));
        }
        if ((current & 0x7000u) == 0u) {
            current &= 0xf7ffu;
        }
        raw_write_word(cpu, address, current);
    }
    if (main_pll_relock_required(cpu, address, previous, current)) {
        restart_main_pll_lock(cpu);
    }
}

static void oscillator_configuration_changed(Dspic33* cpu, uint8_t previous) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    uint8_t requested = oscillator_requested_source(control);
    if (cpu->oscillator.active && !cpu->oscillator.automatic &&
        (previous & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) == 0u &&
        (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) != 0u) {
        abort_oscillator_switch(cpu);
        return;
    }
    if (!cpu->oscillator.active || oscillator_direct_pll_transition(control) ||
        (requested != 2u && requested != 3u) ||
        ((previous ^ cpu->configuration[8u]) & 0x03u) == 0u) {
        return;
    }
    cpu->oscillator.generation++;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    schedule_oscillator_readiness(cpu, control);
}

static void oscillator_pll_configuration_changed(Dspic33* cpu, uint8_t previous) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    if (!cpu->oscillator.active || !cpu->oscillator.source_ready ||
        oscillator_direct_pll_transition(control) || !oscillator_pll_mode(control) ||
        ((previous ^ cpu->configuration[10u]) & OSCILLATOR_CONFIGURATION_PLL_LOCK) == 0u) {
        return;
    }
    if (!oscillator_pll_lock_enabled(cpu)) {
        control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_SWITCH_ENABLE) |
                             ((control & OSCILLATOR_REQUEST_MASK) << 4u));
        cpu->oscillator.active = false;
        cpu->oscillator.automatic = false;
        cpu->watchdog.ticks = 0u;
        raw_write_word(cpu, OSCILLATOR_CONTROL, control);
    }
}

static void oscillator_startup_configuration_changed(Dspic33* cpu, uint8_t previous) {
    uint8_t source = (uint8_t)(cpu->configuration[6u] & 0x07u);
    if (((previous ^ cpu->configuration[6u]) & 0x07u) == 0u ||
        (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) == 0u) {
        return;
    }
    dspic33_device_abort_oscillator_switch(cpu);
    start_automatic_oscillator_switch(cpu, source);
}

static void start_oscillator_switch(Dspic33* cpu, uint16_t control) {
    cpu->oscillator.generation++;
    cpu->oscillator.active = true;
    cpu->oscillator.automatic = false;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    raw_write_word(cpu, OSCILLATOR_CONTROL,
                   (uint16_t)((control | OSCILLATOR_SWITCH_ENABLE) & ~OSCILLATOR_PLL_LOCK &
                              ~OSCILLATOR_CLOCK_FAIL));
    schedule_oscillator_readiness(cpu, control);
}

static void start_automatic_oscillator_switch(Dspic33* cpu, uint8_t source) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    control = (uint16_t)((control & ~OSCILLATOR_REQUEST_MASK & ~OSCILLATOR_PLL_LOCK &
                          ~OSCILLATOR_CLOCK_FAIL & ~OSCILLATOR_SWITCH_ENABLE) |
                         ((uint16_t)source << 8u));
    raw_write_word(cpu, OSCILLATOR_CONTROL, control);
    if (oscillator_source_immediately_ready(source)) {
        control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK) | ((uint16_t)source << 12u));
        raw_write_word(cpu, OSCILLATOR_CONTROL, control);
        return;
    }
    cpu->oscillator.generation++;
    cpu->oscillator.active = true;
    cpu->oscillator.automatic = true;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    schedule_oscillator_readiness(cpu, control);
}

static void reset_main_oscillator(Dspic33* cpu) {
    uint8_t source = (uint8_t)(cpu->configuration[6u] & 0x07u);
    uint16_t control = (uint16_t)(source << 8u);
    if ((cpu->configuration[6u] & 0x80u) == 0u) {
        control |= (uint16_t)(source << 12u);
        if (source == 1u || source == 3u) {
            control |= OSCILLATOR_PLL_LOCK;
        }
    }
    raw_write_word(cpu, OSCILLATOR_CONTROL, control);
}

void dspic33_device_power_on_reset(Dspic33* cpu) {
    uint8_t source = (uint8_t)(cpu->configuration[6u] & 0x07u);
    if ((cpu->configuration[6u] & 0x80u) != 0u && source != 0u) {
        start_automatic_oscillator_switch(cpu, source);
    }
}

void dspic33_device_reset_restored(Dspic33* cpu) {
    pps_capture_shadow(cpu);
    refresh_physical_pin_inputs(cpu);
}

void dspic33_device_brown_out_reset(Dspic33* cpu) {
    size_t destination = 0u;
    size_t source;
    for (source = 0u; source < cpu->events.count; source++) {
        if (cpu->events.items[source].type != DSPIC33_EVENT_OSCILLATOR) {
            cpu->events.items[destination++] = cpu->events.items[source];
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
    memset(&cpu->oscillator, 0, sizeof(cpu->oscillator));
    reset_main_oscillator(cpu);
    dspic33_device_power_on_reset(cpu);
    dspic33_device_reset_restored(cpu);
}

static void complete_oscillator_event(Dspic33* cpu, uint16_t phase, uint32_t generation) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    if (generation != cpu->oscillator.generation) {
        return;
    }
    if (phase == OSCILLATOR_EVENT_LOCK) {
        if (!cpu->oscillator.lock_pending) {
            return;
        }
        if (cpu->oscillator.active && cpu->oscillator.source_ready) {
            control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_SWITCH_ENABLE) |
                                 ((control & OSCILLATOR_REQUEST_MASK) << 4u) | OSCILLATOR_PLL_LOCK);
            cpu->oscillator.active = false;
            cpu->oscillator.automatic = false;
            cpu->watchdog.ticks = 0u;
            raw_write_word(cpu, OSCILLATOR_CONTROL, control);
        } else if (oscillator_current_source(control) == 1u ||
                   oscillator_current_source(control) == 3u) {
            raw_write_word(cpu, OSCILLATOR_CONTROL, (uint16_t)(control | OSCILLATOR_PLL_LOCK));
        }
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
        return;
    }
    if (!cpu->oscillator.active ||
        (!cpu->oscillator.automatic && (control & OSCILLATOR_SWITCH_ENABLE) == 0u)) {
        return;
    }
    if (!oscillator_source_available(cpu, oscillator_requested_source(control))) {
        return;
    }
    if (oscillator_pll_mode(control)) {
        cpu->oscillator.source_ready = true;
        cpu->oscillator.lock_pending = true;
        schedule_oscillator_event(cpu, OSCILLATOR_EVENT_LOCK,
                                  OSCILLATOR_SWITCH_DELAY - OSCILLATOR_SOURCE_DELAY);
        if (oscillator_pll_lock_enabled(cpu)) {
            return;
        }
    }
    control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_SWITCH_ENABLE) |
                         ((control & OSCILLATOR_REQUEST_MASK) << 4u));
    cpu->oscillator.active = false;
    cpu->oscillator.automatic = false;
    cpu->watchdog.ticks = 0u;
    raw_write_word(cpu, OSCILLATOR_CONTROL, control);
}

static bool oscillator_key_authorized(const Dspic33* cpu, uint8_t lane, uint8_t stage) {
    return cpu->oscillator.key_stage == stage && cpu->oscillator.key_lane == lane &&
           cpu->instructions == cpu->oscillator.key_instruction + 1u &&
           cpu->interrupt_count == cpu->oscillator.key_interrupt_count &&
           cpu->trap_count == cpu->oscillator.key_trap_count;
}

static void oscillator_key_start(Dspic33* cpu, uint8_t lane) {
    cpu->oscillator.key_stage = 1u;
    cpu->oscillator.key_lane = lane;
    cpu->oscillator.key_instruction = cpu->instructions;
    cpu->oscillator.key_interrupt_count = cpu->interrupt_count;
    cpu->oscillator.key_trap_count = cpu->trap_count;
}

static void apply_oscillator_high(Dspic33* cpu, uint16_t previous, uint16_t requested) {
    uint16_t control;
    if (oscillator_configuration_locked(cpu, previous)) {
        raw_write_word(cpu, OSCILLATOR_CONTROL, previous);
        return;
    }
    control =
        (uint16_t)((previous & ~OSCILLATOR_REQUEST_MASK) | (requested & OSCILLATOR_REQUEST_MASK));
    if (cpu->oscillator.active) {
        abort_oscillator_switch(cpu);
        control &= (uint16_t)~OSCILLATOR_SWITCH_ENABLE;
    }
    raw_write_word(cpu, OSCILLATOR_CONTROL, control);
}

static void apply_oscillator_low(Dspic33* cpu, uint16_t previous, uint16_t requested) {
    uint16_t control = previous;
    uint16_t writable = OSCILLATOR_IO_LOCK;
    bool was_io_locked = (previous & OSCILLATOR_IO_LOCK) != 0u;
    bool requests_io_lock = (requested & OSCILLATOR_IO_LOCK) != 0u;
    if (was_io_locked && !requests_io_lock && (cpu->configuration[8u] & 0x20u) != 0u &&
        cpu->io.pps.one_way_committed) {
        requested |= OSCILLATOR_IO_LOCK;
    }
    if (!oscillator_configuration_locked(cpu, previous)) {
        writable |= OSCILLATOR_LP_ENABLE;
    }
    control = (uint16_t)((control & ~writable) | (requested & writable));
    if (!was_io_locked && (control & OSCILLATOR_IO_LOCK) != 0u) {
        cpu->io.pps.one_way_committed = true;
    }
    control |= (uint16_t)(requested & OSCILLATOR_CLOCK_LOCK);
    if ((requested & OSCILLATOR_CLOCK_FAIL) == 0u) {
        control &= (uint16_t)~OSCILLATOR_CLOCK_FAIL;
    }
    if ((requested & OSCILLATOR_SWITCH_ENABLE) == 0u) {
        raw_write_word(cpu, OSCILLATOR_CONTROL, control);
        if (!cpu->oscillator.automatic) {
            abort_oscillator_switch(cpu);
        }
        return;
    }
    if ((cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) != 0u ||
        oscillator_configuration_locked(cpu, control) ||
        ((control & OSCILLATOR_CURRENT_MASK) >> 4u) == (control & OSCILLATOR_REQUEST_MASK)) {
        raw_write_word(cpu, OSCILLATOR_CONTROL, (uint16_t)(control & ~OSCILLATOR_SWITCH_ENABLE));
        if (!cpu->oscillator.automatic) {
            abort_oscillator_switch(cpu);
        }
        return;
    }
    if (oscillator_direct_pll_transition(control)) {
        cpu->oscillator.generation++;
        cpu->oscillator.active = true;
        raw_write_word(cpu, OSCILLATOR_CONTROL, (uint16_t)(control | OSCILLATOR_SWITCH_ENABLE));
        return;
    }
    if (cpu->oscillator.active) {
        abort_oscillator_switch(cpu);
    }
    start_oscillator_switch(cpu, control);
}

bool dspic33_oscillator_failure_detected(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    uint8_t current = oscillator_current_source(control);
    if ((cpu->configuration[8u] & 0xc0u) != 0u || cpu->power_state == DSPIC33_POWER_SLEEP ||
        (current != 2u && current != 3u && current != 4u)) {
        return false;
    }
    dspic33_device_abort_oscillator_switch(cpu);
    control = raw_word(cpu, OSCILLATOR_CONTROL);
    control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_PLL_LOCK) |
                         OSCILLATOR_CLOCK_FAIL);
    raw_write_word(cpu, OSCILLATOR_CONTROL, control);
    dspic33_raise_oscillator_fail_trap(cpu);
    return true;
}

static bool protect_oscillator_write(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t requested;
    uint8_t lane;
    uint8_t value;
    uint8_t first;
    uint8_t second;
    if (address != OSCILLATOR_CONTROL && address != OSCILLATOR_CONTROL + 1u) {
        return false;
    }
    requested = raw_word(cpu, OSCILLATOR_CONTROL);
    lane = (uint8_t)(address - OSCILLATOR_CONTROL);
    value = cpu->data[address];
    first = lane == 0u ? 0x46u : 0x78u;
    second = lane == 0u ? 0x57u : 0x9au;
    raw_write_word(cpu, OSCILLATOR_CONTROL, previous);
    if (!cpu->instruction_active || cpu->io.cpu_write_width != 1u) {
        cpu->oscillator.key_stage = 0u;
        return true;
    }
    if (oscillator_key_authorized(cpu, lane, 2u)) {
        cpu->oscillator.key_stage = 0u;
        if (lane == 0u) {
            apply_oscillator_low(cpu, previous, requested);
        } else {
            apply_oscillator_high(cpu, previous, requested);
        }
        return true;
    }
    if (oscillator_key_authorized(cpu, lane, 1u) && value == second) {
        cpu->oscillator.key_stage = 2u;
        cpu->oscillator.key_instruction = cpu->instructions;
        return true;
    }
    if (value == first) {
        oscillator_key_start(cpu, lane);
        return true;
    }
    cpu->oscillator.key_stage = 0u;
    return true;
}

static bool dma_register_write_mask(uint16_t address, uint16_t* writable) {
    static const uint16_t channel_masks[] = {0xf833u, 0x80ffu, 0xffffu, 0x00ffu,
                                             0xffffu, 0x00ffu, 0xffffu, 0x3fffu};
    if (address >= DMA_CHANNEL_BASE &&
        address < DMA_CHANNEL_BASE + DSPIC33_DMA_COUNT * DMA_CHANNEL_STRIDE) {
        *writable = channel_masks[((address - DMA_CHANNEL_BASE) & 0x000fu) / 2u];
        return true;
    }
    if (address >= DMA_PWC && address <= DMA_SADRH) {
        *writable = 0u;
        return true;
    }
    return false;
}

static bool interrupt_control_write(Dspic33* cpu, uint16_t base, uint16_t previous,
                                    uint16_t requested) {
    uint16_t current;
    if (base == 0x08c0u) {
        current = (uint16_t)(requested & 0xfffeu);
        raw_write_word(cpu, base, current);
        if ((previous & 0x0020u) != 0u && (current & 0x0020u) == 0u) {
            raw_write_word(cpu, DMA_PWC, 0u);
            raw_write_word(cpu, DMA_RQC, 0u);
        }
        if ((current & 0x0010u) == 0u) {
            dspic33_set_math_error_source(cpu, false);
        }
        return true;
    }
    if (base == 0x08c2u) {
        current = (uint16_t)((previous & 0x4000u) | (requested & 0xa01fu));
        raw_write_word(cpu, base, current);
        if ((current & 0x2000u) != 0u) {
            raw_write_word(cpu, 0x08c6u, (uint16_t)(raw_word(cpu, 0x08c6u) | 0x0001u));
        }
        dspic33_set_generic_hard_trap_source(cpu, (current & 0x2000u) != 0u ||
                                                      (raw_word(cpu, 0x08c6u) & 0x0001u) != 0u);
        if ((current & 0x8000u) != 0u) {
            cpu->gie_disable_deferred = 0u;
            cpu->gie_disable_deferred_next = 0u;
        } else if ((previous & 0x8000u) != 0u) {
            cpu->gie_disable_deferred_next = 1u;
        }
        return true;
    }
    if (base == 0x08c4u) {
        current = (uint16_t)(requested & 0x0070u);
        raw_write_word(cpu, base, current);
        dspic33_set_generic_soft_trap_source(cpu, current != 0u);
        return true;
    }
    if (base == 0x08c6u) {
        current = (uint16_t)(requested & 0x0001u);
        raw_write_word(cpu, base, current);
        dspic33_set_generic_hard_trap_source(cpu, current != 0u ||
                                                      (raw_word(cpu, 0x08c2u) & 0x2000u) != 0u);
        return true;
    }
    if (base == 0x08c8u) {
        raw_write_word(cpu, base, previous);
        return true;
    }
    return false;
}

static uint32_t dma_register_address(const Dspic33* cpu, uint16_t base, uint16_t low_offset) {
    return ((uint32_t)(raw_word(cpu, (uint16_t)(base + low_offset + 2u)) & 0x00ffu) << 16u) |
           raw_word(cpu, (uint16_t)(base + low_offset));
}

static void initialize_dma_channel(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dma_channel_base(channel);
    uint16_t bit = dma_channel_bit(channel);
    cpu->io.dma_index[channel] = 0u;
    cpu->io.dma_start_a[channel] = dma_register_address(cpu, base, 4u);
    cpu->io.dma_start_b[channel] = dma_register_address(cpu, base, 8u);
    cpu->io.dma_address[channel] = cpu->io.dma_start_a[channel];
    cpu->io.dma_bank &= (uint16_t)~bit;
    cpu->io.dma_half_raised &= (uint16_t)~bit;
    cpu->io.dma_forced_pending &= (uint16_t)~bit;
    cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
    cpu->io.dma_active &= (uint16_t)~bit;
    cpu->io.dma_enabled |= bit;
    dma_advance_generation(cpu, channel);
    raw_write_word(cpu, DMA_PPS, (uint16_t)(raw_word(cpu, DMA_PPS) & ~bit));
}

static void update_dma_control(Dspic33* cpu, uint8_t channel, uint16_t previous) {
    uint16_t base = dma_channel_base(channel);
    uint16_t current = raw_word(cpu, base);
    uint16_t bit = dma_channel_bit(channel);
    bool was_enabled = (previous & DMA_CON_CHEN) != 0u;
    bool enabled = (current & DMA_CON_CHEN) != 0u;
    if (enabled && !was_enabled) {
        initialize_dma_channel(cpu, channel);
    } else if (!enabled && was_enabled) {
        cpu->io.dma_enabled &= (uint16_t)~bit;
        cpu->io.dma_forced_pending &= (uint16_t)~bit;
        cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
        cpu->io.dma_active &= (uint16_t)~bit;
        cpu->io.dma_arbiter_waiting &= (uint16_t)~bit;
        dma_advance_generation(cpu, channel);
        raw_write_word(cpu, (uint16_t)(base + 2u),
                       (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
    }
}

static void update_dma_request(Dspic33* cpu, uint8_t channel, uint16_t previous) {
    uint16_t base = dma_channel_base(channel);
    uint16_t address = (uint16_t)(base + 2u);
    uint16_t request = raw_word(cpu, address);
    uint16_t bit = dma_channel_bit(channel);
    if ((raw_word(cpu, base) & DMA_CON_CHEN) == 0u) {
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((previous & DMA_REQ_FORCE) != 0u) {
        raw_write_word(cpu, address, (uint16_t)(request | DMA_REQ_FORCE));
        return;
    }
    if ((request & DMA_REQ_FORCE) == 0u) {
        return;
    }
    if ((cpu->io.dma_active & bit) != 0u) {
        dma_request_collision(cpu, channel);
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((cpu->io.dma_peripheral_pending & bit) != 0u) {
        dma_request_collision(cpu, channel);
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((cpu->io.dma_forced_pending & bit) == 0u &&
        !schedule_dma_channel(cpu, channel, 0u, true, 1u)) {
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
    }
}

static void can_remove_transmit_events(Dspic33* cpu, uint8_t channel) {
    size_t destination = 0u;
    for (size_t source = 0u; source < cpu->events.count; source++) {
        Dspic33Event* event = &cpu->events.items[source];
        uint32_t kind = event->value & CAN_EVENT_KIND_MASK;
        if (event->type != DSPIC33_EVENT_CAN || event->source != channel ||
            (kind != CAN_EVENT_TRANSMIT_START && kind != CAN_EVENT_TRANSMIT_WORD &&
             kind != CAN_EVENT_TRANSMIT_FINISH && kind != CAN_EVENT_TRANSMIT_BUS_FINISH &&
             kind != CAN_EVENT_TRANSMIT_RETRY && kind != CAN_EVENT_TRANSMIT_ERROR_START &&
             kind != CAN_EVENT_TRANSMIT_SAMPLE && kind != CAN_EVENT_TRANSMIT_SAMPLE_FIRST &&
             kind != CAN_EVENT_TRANSMIT_SAMPLE_SECOND)) {
            cpu->events.items[destination++] = *event;
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
}

static void can_abort_transmissions(Dspic33* cpu, uint8_t channel) {
    can_remove_transmit_events(cpu, channel);
    uint8_t buffer;
    for (buffer = 0u; buffer < 8u; buffer++) {
        uint16_t control = can_buffer_control(cpu, channel, buffer);
        if ((control & CAN_BUFFER_REQUEST) != 0u) {
            can_set_buffer_control(
                cpu, channel, buffer,
                (uint16_t)((control & ~CAN_BUFFER_REQUEST) | CAN_BUFFER_ABORTED));
            can_raise_event(cpu, channel, CAN_INTERRUPT_TRANSMIT, buffer, 0u);
        }
    }
    raw_write_word(cpu, can_bases[channel],
                   (uint16_t)(raw_word(cpu, can_bases[channel]) & ~CAN_ABORT_ALL));
    cpu->io.can_tx_on_bus &= (uint8_t)~(uint8_t)(1u << channel);
    cpu->io.can_tx_busy &= (uint8_t)~(uint8_t)(1u << channel);
    cpu->io.can_tx_retry_wait &= (uint8_t)~(uint8_t)(1u << channel);
    cpu->io.can_tx_error_active &= (uint8_t)~(uint8_t)(1u << channel);
}

static void can_clear_receive_flags(Dspic33* cpu, uint8_t channel, uint16_t address,
                                    uint16_t previous, uint16_t requested) {
    uint16_t cleared;
    uint16_t result = (uint16_t)(previous & requested);
    uint8_t high = address == can_bases[channel] + 0x22u ? 16u : 0u;
    uint8_t bit;
    raw_write_word(cpu, address, result);
    if (address != can_bases[channel] + 0x20u && address != can_bases[channel] + 0x22u) {
        return;
    }
    cleared = (uint16_t)(previous & ~result);
    for (bit = 0u; bit < 16u; bit++) {
        uint8_t buffer = (uint8_t)(high + bit);
        if ((cleared & (uint16_t)(1u << bit)) != 0u &&
            buffer >= (raw_word(cpu, (uint16_t)(can_bases[channel] + 6u)) & 0x001fu)) {
            uint8_t next = can_next_fifo_buffer(cpu, channel, buffer);
            uint16_t fifo = raw_word(cpu, (uint16_t)(can_bases[channel] + 8u));
            raw_write_word(cpu, (uint16_t)(can_bases[channel] + 8u),
                           (uint16_t)((fifo & 0x3f00u) | next));
        }
    }
}

static void can_update_transmit_control(Dspic33* cpu, uint8_t channel, uint16_t address,
                                        uint16_t previous) {
    uint8_t first = (uint8_t)(((address - can_bases[channel]) - 0x30u));
    uint8_t half;
    for (half = 0u; half < 2u; half++) {
        uint8_t buffer = (uint8_t)(first + half);
        uint8_t shift = (uint8_t)(half * 8u);
        uint16_t before = (uint16_t)((previous >> shift) & 0xffu);
        uint16_t current = can_buffer_control(cpu, channel, buffer);
        if ((current & CAN_BUFFER_REQUEST) != 0u && (before & CAN_BUFFER_REQUEST) == 0u) {
            current &= (uint16_t)~(CAN_BUFFER_ABORTED | CAN_BUFFER_LOST | CAN_BUFFER_ERROR);
            can_set_buffer_control(cpu, channel, buffer, current);
            dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u);
        } else if ((current & CAN_BUFFER_REQUEST) == 0u && (before & CAN_BUFFER_REQUEST) != 0u) {
            bool active = (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) != 0u &&
                          cpu->io.can_tx_buffer[channel] == buffer;
            can_set_buffer_control(cpu, channel, buffer, (uint16_t)(current | CAN_BUFFER_ABORTED));
            can_raise_event(cpu, channel, CAN_INTERRUPT_TRANSMIT, buffer, 0u);
            if (active) {
                can_remove_transmit_events(cpu, channel);
                cpu->io.can_tx_on_bus &= (uint8_t)~(uint8_t)(1u << channel);
                cpu->io.can_tx_busy &= (uint8_t)~(uint8_t)(1u << channel);
                cpu->io.can_tx_retry_wait &= (uint8_t)~(uint8_t)(1u << channel);
                cpu->io.can_tx_error_active &= (uint8_t)~(uint8_t)(1u << channel);
                dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u);
            }
        }
    }
}

static void update_can_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = can_bases[channel];
        uint16_t offset = (uint16_t)(address - base);
        bool window;
        uint16_t writable;
        if (offset > 0x7eu || (offset & 1u) != 0u) {
            continue;
        }
        window = (raw_word(cpu, base) & CAN_WINDOW) != 0u;
        if (offset == 0u) {
            uint16_t control = raw_word(cpu, base);
            uint16_t mode = (uint16_t)((control & CAN_MODE_MASK) >> CAN_MODE_SHIFT);
            uint16_t active = (uint16_t)((previous >> 5u) & 7u);
            uint16_t prior_request = (uint16_t)((previous & CAN_MODE_MASK) >> CAN_MODE_SHIFT);
            control = (uint16_t)((control & ~0x00e0u) | (active << 5u));
            raw_write_word(cpu, base, control);
            if (mode != prior_request) {
                cpu->io.can_mode_generation[channel]++;
                if (mode != active && !can_schedule_mode_transition(cpu, channel, (uint8_t)mode)) {
                    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
                }
            }
            if ((control & CAN_ABORT_ALL) != 0u) {
                can_abort_transmissions(cpu, channel);
            }
            return;
        }
        if ((offset == 6u || offset == 0x10u || offset == 0x12u || offset == 0x14u ||
             offset == 0x18u || offset == 0x1au) &&
            can_mode(cpu, channel) != CAN_MODE_CONFIGURATION) {
            raw_write_word(cpu, address, previous);
            return;
        }
        if (offset == 0x0au) {
            raw_write_word(cpu, address,
                           (uint16_t)((previous & ~0x00efu) | (previous & requested & 0x00efu)));
            can_refresh_error_status(cpu, channel);
            can_update_vector(cpu, channel);
            return;
        }
        if (window && offset >= 0x20u) {
            uint16_t prior = can_filter_word(cpu, channel, offset);
            if (can_mode(cpu, channel) == CAN_MODE_CONFIGURATION &&
                can_register_write_mask(cpu, address, &writable)) {
                cpu->io.can_filter_window[channel][(offset - 0x20u) / 2u] =
                    (uint16_t)((prior & ~writable) | (requested & writable));
            }
            raw_write_word(cpu, address, previous);
            return;
        }
        if (!window && (offset == 0x20u || offset == 0x22u || offset == 0x28u || offset == 0x2au)) {
            can_clear_receive_flags(cpu, channel, address, previous, requested);
            return;
        }
        if (!window && offset >= 0x30u && offset <= 0x36u) {
            can_update_transmit_control(cpu, channel, address, previous);
            return;
        }
        if (offset == 6u) {
            uint8_t channel_bit = (uint8_t)(1u << channel);
            bool low_byte = cpu->io.cpu_write_valid && cpu->io.cpu_write_width == 1u &&
                            cpu->io.cpu_write_address == address;
            bool high_byte = cpu->io.cpu_write_valid && cpu->io.cpu_write_width == 1u &&
                             cpu->io.cpu_write_address == address + 1u;
            if (low_byte) {
                cpu->io.can_fctrl_fsa_ready |= channel_bit;
            } else if (high_byte && (cpu->io.can_fctrl_fsa_ready & channel_bit) != 0u) {
                cpu->io.can_fctrl_fsa_ready &= (uint8_t)~channel_bit;
            } else {
                raw_write_word(cpu, address, previous);
                cpu->io.can_fctrl_fsa_ready &= (uint8_t)~channel_bit;
                return;
            }
        }
        if (!window && offset == 0x42u && (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) != 0u &&
            cpu->io.can_tx_word[channel] != 0u) {
            cpu->io.can_tx_words[channel][cpu->io.can_tx_word[channel] - 1u] =
                raw_word(cpu, address);
            return;
        }
        if (offset == 6u) {
            uint8_t start = (uint8_t)(raw_word(cpu, address) & 0x001fu);
            cpu->io.can_fifo_write[channel] = start;
            raw_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)(((uint16_t)start << 8u) | start));
        }
        return;
    }
    if (address == 0x0760u) {
        for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
            if ((raw_word(cpu, address) & (uint16_t)(2u << channel)) != 0u) {
                cpu->io.can_rx_busy &= (uint8_t)~(uint8_t)(1u << channel);
                cpu->io.can_tx_busy &= (uint8_t)~(uint8_t)(1u << channel);
            }
        }
    }
}

static void usb_start_host_token(Dspic33* cpu) {
    Dspic33UsbPacket packet;
    uint16_t token = raw_word(cpu, USB_TOK);
    uint8_t direction;
    uint8_t bank;
    uint16_t words[4];
    uint32_t buffer;
    bool increment;
    memset(&packet, 0, sizeof(packet));
    packet.address = (uint8_t)(raw_word(cpu, USB_ADDR) & 0x007fu);
    packet.low_speed = (raw_word(cpu, USB_ADDR) & 0x0080u) != 0u;
    packet.endpoint = (uint8_t)(token & 0x0fu);
    packet.pid = (uint8_t)((token >> 4u) & 0x0fu);
    if (cpu->io.usb_pmd_disabled || (raw_word(cpu, USB_PWRC) & USB_POWER) == 0u ||
        (raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u ||
        (raw_word(cpu, USB_CON) & USB_TOKEN_BUSY) != 0u ||
        (packet.pid != DSPIC33_USB_PID_OUT && packet.pid != DSPIC33_USB_PID_IN &&
         packet.pid != DSPIC33_USB_PID_SETUP)) {
        return;
    }
    direction = packet.pid == DSPIC33_USB_PID_IN ? 0u : 1u;
    bank = (raw_word(cpu, USB_CON) & USB_PING_PONG_RESET) != 0u
               ? 0u
               : cpu->io.usb_next_bank[0][direction];
    if (!usb_descriptor(cpu, 0u, direction, bank, words)) {
        usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
        return;
    }
    if ((words[0] & USB_DESCRIPTOR_OWNED) == 0u) {
        return;
    }
    if (direction != 0u) {
        packet.size = words[1] & USB_DESCRIPTOR_COUNT_MASK;
        packet.data1 = (words[0] & USB_DESCRIPTOR_DATA1) != 0u;
        buffer = ((uint32_t)words[3] << 16u) | words[2];
        increment = (words[0] & USB_DESCRIPTOR_NO_INCREMENT) == 0u;
        if (!usb_read_memory(cpu, buffer, packet.data, packet.size, increment)) {
            usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
            return;
        }
    }
    if (!usb_queue_push(&cpu->io.usb_tx, &packet)) {
        usb_set_error(cpu, USB_ERROR_DMA);
        return;
    }
    cpu->io.usb_host_pid = packet.pid;
    cpu->io.usb_host_endpoint = packet.endpoint;
    cpu->io.usb_host_pending = true;
    raw_write_word(cpu, USB_CON, (uint16_t)(raw_word(cpu, USB_CON) | USB_TOKEN_BUSY));
}

static void update_usb_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint16_t current = raw_word(cpu, address);
    if (address == USB_OTGIR) {
        raw_write_word(cpu, address, (uint16_t)(previous & ~(requested & 0x00fdu)));
        usb_refresh_interrupt(cpu);
        return;
    }
    if (address == USB_EIR) {
        raw_write_word(cpu, address, (uint16_t)(previous & ~(requested & 0x00ffu)));
        usb_refresh_interrupt(cpu);
        return;
    }
    if (address == USB_IR) {
        uint16_t cleared = requested & 0x00fdu;
        uint16_t remaining = (uint16_t)(previous & ~(cleared & ~USB_TRANSACTION_INTERRUPT));
        if ((raw_word(cpu, USB_CON) & USB_HOST_ENABLE) != 0u) {
            if ((previous & USB_ATTACH_INTERRUPT) != 0u && cpu->io.usb_host_attached) {
                remaining |= USB_ATTACH_INTERRUPT;
            }
            if ((previous & USB_DETACH_INTERRUPT) != 0u && !cpu->io.usb_host_attached) {
                remaining |= USB_DETACH_INTERRUPT;
            }
        }
        raw_write_word(cpu, address, remaining);
        if ((cleared & USB_TRANSACTION_INTERRUPT) != 0u) {
            usb_pop_transaction_status(cpu);
        } else {
            usb_refresh_transaction_status(cpu);
        }
        return;
    }
    if (address == USB_PWRC) {
        if ((previous & USB_POWER) != 0u && (current & USB_POWER) == 0u) {
            usb_reset_registers(cpu);
        } else {
            usb_refresh_activity_pending(cpu);
            usb_update_power_state(cpu);
        }
        return;
    }
    if (address == USB_CON) {
        bool previous_host = (previous & USB_HOST_ENABLE) != 0u;
        bool current_host = (current & USB_HOST_ENABLE) != 0u;
        if ((current & USB_PING_PONG_RESET) != 0u) {
            usb_reset_ping_pong(cpu);
        }
        if (previous_host != current_host) {
            raw_write_word(
                cpu, USB_IR,
                (uint16_t)(raw_word(cpu, USB_IR) & ~(USB_ATTACH_INTERRUPT | USB_DETACH_INTERRUPT)));
            cpu->io.usb_host_attached = false;
            usb_refresh_interrupt(cpu);
        }
        if (previous_host && !current_host) {
            cpu->io.usb_host_pending = false;
        }
        if ((current & (USB_HOST_ENABLE | USB_ENABLE)) == (USB_HOST_ENABLE | USB_ENABLE) &&
            (previous & (USB_HOST_ENABLE | USB_ENABLE)) != (USB_HOST_ENABLE | USB_ENABLE)) {
            usb_schedule_bus_event(cpu, DSPIC33_USB_BUS_SOF, UINT16_MAX, USB_FRAME_CYCLES, false);
        }
        return;
    }
    if (address == USB_TOK) {
        usb_start_host_token(cpu);
        return;
    }
    if (address == USB_CNFG1) {
        usb_update_power_state(cpu);
        return;
    }
    if (address == USB_EIE || address == USB_IE || address == USB_OTGIE) {
        usb_refresh_interrupt(cpu);
        return;
    }
    if (address >= USB_EP0 && address < USB_EP0 + DSPIC33_USB_ENDPOINT_COUNT * 2u &&
        (requested & USB_ENDPOINT_STALL) == 0u) {
        raw_write_word(cpu, address, (uint16_t)(raw_word(cpu, address) & ~USB_ENDPOINT_STALL));
    }
}

static bool usb_register_address(uint16_t address) {
    return (address >= USB_OTGIR && address <= USB_PWRC) ||
           (address >= USB_IR && address < USB_EP0 + DSPIC33_USB_ENDPOINT_COUNT * 2u) ||
           address == USB_PWMRRS || address == USB_PWMCON;
}

static void update_usb_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    bool disabled;
    if (address != USB_PMD_ADDRESS || ((previous ^ raw_word(cpu, address)) & USB_PMD) == 0u) {
        return;
    }
    disabled = (raw_word(cpu, address) & USB_PMD) != 0u;
    cpu->io.usb_pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_USB_PMD, 0u,
                          ((uint32_t)cpu->io.usb_pmd_generation << USB_PMD_EVENT_GENERATION_SHIFT) |
                              (disabled ? USB_PMD_EVENT_DISABLED : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        raw_write_word(cpu, address, previous);
        cpu->io.usb_pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void update_nvm_key(Dspic33* cpu, uint16_t requested) {
    uint8_t key = (uint8_t)requested;
    if (key == 0x55u) {
        cpu->nvm.key_stage = 1u;
        cpu->nvm.key_instruction = cpu->instructions;
        cpu->nvm.key_interrupt_count = cpu->interrupt_count;
        cpu->nvm.key_trap_count = cpu->trap_count;
    } else if (key == 0xaau && cpu->nvm.key_stage == 1u &&
               cpu->interrupt_count == cpu->nvm.key_interrupt_count &&
               cpu->trap_count == cpu->nvm.key_trap_count) {
        cpu->nvm.key_stage = 2u;
        cpu->nvm.key_instruction = cpu->instructions;
    } else {
        cpu->nvm.key_stage = 0u;
    }
    raw_write_word(cpu, NVM_KEY, 0u);
}

static uint8_t crc_write_width(const Dspic33* cpu) {
    if (cpu->io.dma_transfer_active) {
        return cpu->io.dma_transfer_width;
    }
    return cpu->io.cpu_write_valid ? cpu->io.cpu_write_width : 1u;
}

static void update_crc_data(Dspic33* cpu, uint16_t address, uint16_t requested) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    uint8_t width = crc_data_width(cpu);
    uint8_t write_width = crc_write_width(cpu);
    bool high_byte = (address & 1u) != 0u;
    if ((raw_word(cpu, CRC_CONTROL) & CRC_ENABLE) == 0u) {
        raw_write_word(cpu, base, 0u);
        return;
    }
    if (base == CRC_DATA_LOW) {
        if (width <= 8u) {
            if (write_width == 1u) {
                crc_push(cpu, requested >> (high_byte ? 8u : 0u));
            } else {
                crc_push(cpu, requested);
            }
        } else if (width <= 16u) {
            if (write_width == 2u) {
                crc_push(cpu, requested);
                cpu->io.crc.data_latch = 0u;
            }
        } else if (write_width == 2u) {
            cpu->io.crc.data_latch = (cpu->io.crc.data_latch & 0xffff0000u) | requested;
        } else if (high_byte) {
            cpu->io.crc.data_latch = (cpu->io.crc.data_latch & 0xffff00ffu) | (requested & 0xff00u);
        } else {
            cpu->io.crc.data_latch = (cpu->io.crc.data_latch & 0xffffff00u) | (requested & 0x00ffu);
        }
    } else if (width > 16u) {
        if (write_width == 2u) {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0x0000ffffu) | ((uint32_t)requested << 16u);
            crc_push(cpu, cpu->io.crc.data_latch);
            cpu->io.crc.data_latch = 0u;
        } else if (high_byte) {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0x00ffffffu) | ((uint32_t)(requested & 0xff00u) << 16u);
            if (width > 24u) {
                crc_push(cpu, cpu->io.crc.data_latch);
                cpu->io.crc.data_latch = 0u;
            }
        } else {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0xff00ffffu) | ((uint32_t)(requested & 0x00ffu) << 16u);
            if (width <= 24u) {
                crc_push(cpu, cpu->io.crc.data_latch);
                cpu->io.crc.data_latch = 0u;
            }
        }
    }
    raw_write_word(cpu, base, 0u);
}

static void update_crc_control(Dspic33* cpu, uint16_t previous) {
    uint16_t control = raw_word(cpu, CRC_CONTROL);
    bool enabled = (control & CRC_ENABLE) != 0u;
    bool was_go = (previous & CRC_GO) != 0u;
    bool go = (control & CRC_GO) != 0u;
    if (!enabled) {
        crc_reset_runtime(cpu);
        return;
    }
    if (was_go && !go) {
        crc_abort(cpu);
    }
    crc_refresh_status(cpu);
    if (!was_go && go) {
        crc_start_if_ready(cpu);
    }
}

static void update_crc_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    if (base == CRC_PMD_ADDRESS) {
        update_crc_pmd(cpu, previous);
    } else if (base < CRC_CONTROL || base > CRC_SHIFT_HIGH) {
        return;
    } else if (cpu->io.crc.pmd_disabled) {
        raw_write_word(cpu, base, previous);
    } else if (base == CRC_CONTROL) {
        update_crc_control(cpu, previous);
    } else if (base == CRC_DATA_LOW || base == CRC_DATA_HIGH) {
        update_crc_data(cpu, address, requested);
    } else if ((base == CRC_SHIFT_LOW || base == CRC_SHIFT_HIGH) &&
               (raw_word(cpu, CRC_CONTROL) & CRC_GO) != 0u) {
        raw_write_word(cpu, base, previous);
    }
}

static void fail_nvm_write(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, NVM_CONTROL);
    cpu->nvm.key_stage = 0u;
    cpu->nvm.active = false;
    raw_write_word(cpu, NVM_CONTROL, (uint16_t)((control & ~NVM_WRITE) | NVM_WRITE_ERROR));
}

static bool nvm_target_valid(const Dspic33* cpu, uint16_t control, uint32_t address) {
    switch (control & 0x000fu) {
    case 0u:
        return address == 0xf80004u || address == 0xf80006u || address == 0xf80008u ||
               address == 0xf8000au || address == 0xf8000cu || address == 0xf8000eu ||
               address == 0xf80010u || address == 0xf80012u;
    case 1u:
        address &= 0x00fffffcu;
        return dspic33_device_program_range_implemented(cpu, address, 4u);
    case 2u:
        address &= 0x00ffff00u;
        return dspic33_device_program_range_implemented(cpu, address,
                                                        DSPIC33_WRITE_LATCH_WORDS * 2u);
    case 3u:
        address &= 0x00fff800u;
        return dspic33_device_program_range_implemented(cpu, address, 0x800u);
    case 0x0au:
    case 0x0du:
        return true;
    default:
        return false;
    }
}

static void update_nvm_control(Dspic33* cpu, uint16_t requested) {
    uint16_t control = raw_word(cpu, NVM_CONTROL);
    uint32_t target =
        ((uint32_t)raw_word(cpu, NVM_ADDRESS_HIGH) << 16u) | raw_word(cpu, NVM_ADDRESS);
    uint64_t completion_delay = cpu->non_cpu_sfr_read ? 3u : 2u;
    bool write_requested = (requested & NVM_WRITE) != 0u;
    if (cpu->nvm.active) {
        raw_write_word(cpu, NVM_CONTROL, (uint16_t)(control | NVM_WRITE | NVM_WRITE_ERROR));
        return;
    }
    if (!write_requested) {
        return;
    }
    if ((control & NVM_WRITE_ENABLE) == 0u || !nvm_key_authorized(cpu) ||
        cpu->cycles > UINT64_MAX - completion_delay || !nvm_target_valid(cpu, control, target)) {
        fail_nvm_write(cpu);
        return;
    }
    cpu->nvm.control = control;
    cpu->nvm.address = target;
    cpu->nvm.auxiliary_origin =
        cpu->instruction_active ? cpu->current_instruction_pc >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
                                      cpu->current_instruction_pc < DSPIC33_AUXILIARY_PROGRAM_LIMIT
                                : cpu->pc >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
                                      cpu->pc < DSPIC33_AUXILIARY_PROGRAM_LIMIT;
    cpu->nvm.stall_workaround = (raw_word(cpu, 0x08c2u) & 0x8000u) == 0u;
    memcpy(cpu->nvm.latches, cpu->write_latches, sizeof(cpu->nvm.latches));
    cpu->nvm.key_stage = 0u;
    cpu->nvm.active = true;
    cpu->nvm.completion_cycle = cpu->cycles + completion_delay;
    raw_write_word(cpu, NVM_CONTROL, (uint16_t)(control | NVM_WRITE | NVM_WRITE_ERROR));
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_NVM, 0u, 0u,
                          dspic33_device_instruction_cycles(cpu, completion_delay))) {
        fail_nvm_write(cpu);
    }
}
