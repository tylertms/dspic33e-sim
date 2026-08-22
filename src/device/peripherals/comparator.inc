
static uint16_t comparator_base(uint8_t comparator) {
    return (uint16_t)(COMPARATOR_BASE + comparator * COMPARATOR_STRIDE);
}

static bool comparator_reference_level(const Dspic33* cpu, uint16_t control, bool positive,
                                       uint16_t* level) {
    uint16_t reference = raw_word(cpu, COMPARATOR_REFERENCE);
    if (positive) {
        uint32_t source;
        uint8_t tap;
        if ((control & COMPARATOR_REFERENCE_INTERNAL) == 0u) {
            return false;
        }
        if ((reference & COMPARATOR_REFERENCE_EXTERNAL) != 0u) {
            *level = cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE];
            return true;
        }
        if ((reference & COMPARATOR_REFERENCE_ENABLE) == 0u) {
            return false;
        }
        if ((reference & COMPARATOR_REFERENCE_SOURCE_EXTERNAL) != 0u) {
            uint16_t positive_reference =
                cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE];
            uint16_t negative_reference =
                cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_NEGATIVE];
            if (positive_reference < negative_reference) {
                return false;
            }
            source = (uint32_t)(positive_reference - negative_reference);
        } else {
            source = cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_AVDD];
        }
        tap = (uint8_t)(reference & 0x000fu);
        *level = (uint16_t)((reference & COMPARATOR_REFERENCE_LOW_RANGE) != 0u
                                ? source * tap / 24u
                                : source * (8u + tap) / 32u);
        return true;
    }
    if ((control & COMPARATOR_CHANNEL_MASK) != COMPARATOR_CHANNEL_MASK) {
        return false;
    }
    switch (reference & COMPARATOR_REFERENCE_BAND_GAP_MASK) {
    case 0x0000u:
        *level = 1200u;
        return true;
    case 0x0100u:
        *level = 600u;
        return true;
    case 0x0200u:
        *level = 200u;
        return true;
    default:
        if ((reference & COMPARATOR_REFERENCE_SOURCE_EXTERNAL) != 0u) {
            return false;
        }
        *level = cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE];
        return true;
    }
}

static bool comparator_configuration_supported(const Dspic33* cpu, uint8_t comparator) {
    uint16_t control = raw_word(cpu, comparator_base(comparator));
    uint16_t level;
    if ((control & COMPARATOR_ENABLE) == 0u) {
        return false;
    }
    if ((control & COMPARATOR_REFERENCE_INTERNAL) != 0u &&
        !comparator_reference_level(cpu, control, true, &level)) {
        return false;
    }
    return (control & COMPARATOR_CHANNEL_MASK) != COMPARATOR_CHANNEL_MASK ||
           comparator_reference_level(cpu, control, false, &level);
}

static bool comparator_operating(const Dspic33* cpu, uint8_t comparator) {
    if (cpu->io.comparator.pmd_disabled || !comparator_configuration_supported(cpu, comparator)) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE ||
           (raw_word(cpu, COMPARATOR_STATUS) & COMPARATOR_STOP_IDLE) == 0u;
}

static void comparator_refresh_status(Dspic33* cpu) {
    uint16_t status = raw_word(cpu, COMPARATOR_STATUS) & COMPARATOR_STOP_IDLE;
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint16_t control = raw_word(cpu, comparator_base(comparator));
        if ((control & COMPARATOR_EVENT) != 0u) {
            status |= (uint16_t)(0x0100u << comparator);
        }
        if ((control & COMPARATOR_OUTPUT) != 0u) {
            status |= (uint16_t)(1u << comparator);
        }
    }
    raw_write_word(cpu, COMPARATOR_STATUS, status);
}

static void comparator_set_output(Dspic33* cpu, uint8_t comparator, bool high) {
    uint16_t base = comparator_base(comparator);
    uint16_t control = raw_word(cpu, base);
    uint8_t bit = (uint8_t)(1u << comparator);
    bool rising = (control & COMPARATOR_OUTPUT) == 0u && high;
    if (high) {
        control |= COMPARATOR_OUTPUT;
        cpu->io.comparator.output_high |= bit;
    } else {
        control &= (uint16_t)~COMPARATOR_OUTPUT;
        cpu->io.comparator.output_high &= (uint8_t)~bit;
    }
    raw_write_word(cpu, base, control);
    comparator_refresh_status(cpu);
    refresh_external_interrupts(cpu);
    refresh_timer_inputs(cpu);
    output_compare_refresh_fault_pps_inputs(cpu);
    refresh_pwm_inputs(cpu);
    if (rising) {
        input_capture_pulse_source(cpu,
                                   (uint8_t)(INPUT_CAPTURE_SYNC_COMPARATOR_FIRST + comparator));
    }
}

static void comparator_raise_event(Dspic33* cpu, uint8_t comparator) {
    uint16_t base = comparator_base(comparator);
    raw_write_word(cpu, base, (uint16_t)(raw_word(cpu, base) | COMPARATOR_EVENT));
    comparator_refresh_status(cpu);
    dspic33_raise_interrupt(cpu, COMPARATOR_IRQ);
    output_compare_pulse_source(cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_COMPARATOR_FIRST + comparator));
}

static bool comparator_transition_matches(uint16_t control, bool previous, bool current) {
    uint16_t polarity = control & COMPARATOR_EVENT_POLARITY_MASK;
    bool rising = !previous && current;
    bool falling = previous && !current;
    if (polarity == COMPARATOR_EVENT_POLARITY_MASK) {
        return rising || falling;
    }
    if (polarity == 0x0040u) {
        return rising;
    }
    if (polarity == 0x0080u) {
        return falling;
    }
    return false;
}

static bool comparator_mask_source(const Dspic33* cpu, uint8_t selection) {
    if (selection < 14u) {
        return dspic33_pwm_output(cpu, (uint8_t)(selection / 2u), (selection & 1u) != 0u);
    }
    return (cpu->io.pwm_fault_inputs & ((uint32_t)1u << (selection == 14u ? 1u : 3u))) != 0u;
}

static bool comparator_mask_gate(bool source, uint16_t control, uint16_t positive,
                                 uint16_t negative) {
    return ((control & positive) != 0u && source) || ((control & negative) != 0u && !source);
}

static bool comparator_mask_and(bool source, uint16_t control, uint16_t positive, uint16_t negative,
                                bool result) {
    if ((control & positive) != 0u) {
        result &= source;
    }
    if ((control & negative) != 0u) {
        result &= !source;
    }
    return result;
}

static bool comparator_mask_active(const Dspic33* cpu, uint8_t comparator) {
    uint16_t base = comparator_base(comparator);
    uint16_t selections = raw_word(cpu, (uint16_t)(base + 2u));
    uint16_t control = raw_word(cpu, (uint16_t)(base + 4u));
    bool source_a = comparator_mask_source(cpu, (uint8_t)(selections & 0x000fu));
    bool source_b = comparator_mask_source(cpu, (uint8_t)((selections >> 4u) & 0x000fu));
    bool source_c = comparator_mask_source(cpu, (uint8_t)((selections >> 8u) & 0x000fu));
    bool and_result = true;
    bool mask = comparator_mask_gate(source_a, control, 0x0200u, 0x0100u) ||
                comparator_mask_gate(source_b, control, 0x0800u, 0x0400u) ||
                comparator_mask_gate(source_c, control, 0x2000u, 0x1000u);
    and_result = comparator_mask_and(source_a, control, 0x0002u, 0x0001u, and_result);
    and_result = comparator_mask_and(source_b, control, 0x0008u, 0x0004u, and_result);
    and_result = comparator_mask_and(source_c, control, 0x0020u, 0x0010u, and_result);
    if ((control & 0x0040u) != 0u && and_result) {
        mask = true;
    }
    if ((control & 0x0080u) != 0u && !and_result) {
        mask = true;
    }
    return mask;
}

static bool comparator_filter_enabled(const Dspic33* cpu, uint8_t comparator) {
    return (raw_word(cpu, (uint16_t)(comparator_base(comparator) + 6u)) &
            COMPARATOR_FILTER_ENABLE) != 0u;
}

static uint8_t comparator_filter_source(const Dspic33* cpu, uint8_t comparator) {
    return (uint8_t)((raw_word(cpu, (uint16_t)(comparator_base(comparator) + 6u)) &
                      COMPARATOR_FILTER_SOURCE_MASK) >>
                     4u);
}

static uint16_t comparator_filter_divider(const Dspic33* cpu, uint8_t comparator) {
    uint16_t control = raw_word(cpu, (uint16_t)(comparator_base(comparator) + 6u));
    return (uint16_t)(1u << (control & COMPARATOR_FILTER_DIVIDER_MASK));
}

static void comparator_publish_output(Dspic33* cpu, uint8_t comparator, bool current) {
    uint16_t control = raw_word(cpu, comparator_base(comparator));
    bool previous = (cpu->io.comparator.last_read_cout & (uint8_t)(1u << comparator)) != 0u;
    comparator_set_output(cpu, comparator, current);
    if ((control & COMPARATOR_EVENT) == 0u &&
        cpu->device_cycles >= cpu->io.comparator.rearm_cycle[comparator] &&
        comparator_transition_matches(control, previous, current)) {
        comparator_raise_event(cpu, comparator);
    }
}

static void comparator_filter_sample(Dspic33* cpu, uint8_t comparator) {
    uint8_t bit = (uint8_t)(1u << comparator);
    bool sample = (cpu->io.comparator.raw_high & bit) != 0u;
    bool output = (cpu->io.comparator.output_high & bit) != 0u;
    bool candidate = (cpu->io.comparator.filter_candidate_high & bit) != 0u;
    if (sample == output) {
        cpu->io.comparator.filter_count[comparator] = 0u;
        comparator_publish_output(cpu, comparator, output);
        return;
    }
    if (cpu->io.comparator.filter_count[comparator] == 0u || sample != candidate) {
        cpu->io.comparator.filter_count[comparator] = 1u;
        if (sample) {
            cpu->io.comparator.filter_candidate_high |= bit;
        } else {
            cpu->io.comparator.filter_candidate_high &= (uint8_t)~bit;
        }
        return;
    }
    cpu->io.comparator.filter_count[comparator]++;
    if (cpu->io.comparator.filter_count[comparator] >= 3u) {
        cpu->io.comparator.filter_count[comparator] = 0u;
        comparator_publish_output(cpu, comparator, sample);
    }
}

static void comparator_filter_samples(Dspic33* cpu, uint8_t comparator, uint64_t samples) {
    while (samples-- != 0u) {
        comparator_filter_sample(cpu, comparator);
        if (cpu->io.comparator.filter_count[comparator] == 0u &&
            ((cpu->io.comparator.raw_high ^ cpu->io.comparator.output_high) &
             (uint8_t)(1u << comparator)) == 0u) {
            break;
        }
    }
}

static bool comparator_internal_filter_clock_available(const Dspic33* cpu, uint8_t comparator) {
    return comparator_operating(cpu, comparator) && comparator_filter_enabled(cpu, comparator) &&
           comparator_filter_source(cpu, comparator) < 2u &&
           cpu->power_state != DSPIC33_POWER_SLEEP;
}

static uint64_t comparator_internal_filter_period(const Dspic33* cpu, uint8_t comparator) {
    uint16_t divider = comparator_filter_divider(cpu, comparator);
    return comparator_filter_source(cpu, comparator) == 0u
               ? divider
               : (uint64_t)(divider > 1u ? divider / 2u : 1u);
}

static bool comparator_schedule_filter(Dspic33* cpu, uint8_t comparator, uint32_t generation) {
    return !comparator_internal_filter_clock_available(cpu, comparator) ||
           dspic33_schedule(cpu, DSPIC33_EVENT_COMPARATOR,
                            (uint16_t)(COMPARATOR_EVENT_FILTER_FIRST + comparator), generation,
                            comparator_internal_filter_period(cpu, comparator));
}

static void comparator_remove_filter_events(Dspic33* cpu, uint8_t comparator, uint32_t generation,
                                            bool retain) {
    size_t source;
    size_t destination = 0u;
    uint16_t event_source = (uint16_t)(COMPARATOR_EVENT_FILTER_FIRST + comparator);
    for (source = 0u; source < cpu->events.count; source++) {
        Dspic33Event* event = &cpu->events.items[source];
        if (event->type == DSPIC33_EVENT_COMPARATOR && event->source == event_source &&
            (!retain || event->value != generation)) {
            continue;
        }
        cpu->events.items[destination++] = *event;
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
}

static bool comparator_reconfigure_filter(Dspic33* cpu, uint8_t comparator) {
    uint32_t generation = cpu->io.comparator.filter_generation[comparator] + 1u;
    if (!comparator_schedule_filter(cpu, comparator, generation)) {
        return false;
    }
    cpu->io.comparator.filter_generation[comparator] = generation;
    cpu->io.comparator.filter_fraction[comparator] = 0u;
    cpu->io.comparator.filter_count[comparator] = 0u;
    comparator_remove_filter_events(cpu, comparator, generation,
                                    comparator_internal_filter_clock_available(cpu, comparator));
    return true;
}

static bool comparator_filter_event(const Dspic33* cpu, const Dspic33Event* event,
                                    uint8_t comparator) {
    return event->type == DSPIC33_EVENT_COMPARATOR &&
           event->source == COMPARATOR_EVENT_FILTER_FIRST + comparator &&
           event->value == cpu->io.comparator.filter_generation[comparator];
}

static void comparator_update_filter_power(Dspic33* cpu) {
    uint8_t comparator;
    bool changed = false;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        bool available = comparator_internal_filter_clock_available(cpu, comparator);
        bool found = false;
        size_t index;
        for (index = 0u; index < cpu->events.count; index++) {
            Dspic33Event* event = &cpu->events.items[index];
            if (!comparator_filter_event(cpu, event, comparator)) {
                continue;
            }
            found = true;
            if (!available && !event->paused) {
                event->paused_remaining = event->cycle - cpu->device_cycles;
                event->paused = true;
                changed = true;
            } else if (available && event->paused) {
                if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
                    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
                    continue;
                }
                event->cycle = cpu->device_cycles + event->paused_remaining;
                event->paused_remaining = 0u;
                event->paused = false;
                changed = true;
            }
        }
        if (available && !found &&
            !comparator_schedule_filter(cpu, comparator,
                                        cpu->io.comparator.filter_generation[comparator])) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void comparator_filter_clock(Dspic33* cpu, uint8_t source, uint64_t clocks) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint64_t accumulated;
        uint16_t divider;
        if (!comparator_operating(cpu, comparator) || !comparator_filter_enabled(cpu, comparator) ||
            comparator_filter_source(cpu, comparator) != source) {
            continue;
        }
        divider = comparator_filter_divider(cpu, comparator);
        accumulated = cpu->io.comparator.filter_fraction[comparator] + clocks;
        cpu->io.comparator.filter_fraction[comparator] = (uint16_t)(accumulated % divider);
        comparator_filter_samples(cpu, comparator, accumulated / divider);
    }
}

static void comparator_evaluate(Dspic33* cpu, uint8_t comparator) {
    uint16_t base = comparator_base(comparator);
    uint16_t control = raw_word(cpu, base);
    uint8_t bit = (uint8_t)(1u << comparator);
    uint16_t positive;
    uint16_t negative;
    bool current;
    if (!comparator_operating(cpu, comparator)) {
        if (!comparator_configuration_supported(cpu, comparator) ||
            cpu->io.comparator.pmd_disabled) {
            comparator_set_output(cpu, comparator, false);
            cpu->io.comparator.raw_high &= (uint8_t)~bit;
            cpu->io.comparator.filter_count[comparator] = 0u;
        }
        return;
    }
    if (!comparator_reference_level(cpu, control, true, &positive)) {
        positive = cpu->io.comparator.input[comparator][DSPIC33_COMPARATOR_INPUT_POSITIVE];
    }
    if (!comparator_reference_level(cpu, control, false, &negative)) {
        negative = cpu->io.comparator.input[comparator][(control & COMPARATOR_CHANNEL_MASK) + 1u];
    }
    current = positive > negative;
    if ((control & COMPARATOR_POLARITY) != 0u) {
        current = !current;
    }
    if (comparator_mask_active(cpu, comparator)) {
        current = (raw_word(cpu, (uint16_t)(base + 4u)) & 0x8000u) != 0u;
    }
    if (current) {
        cpu->io.comparator.raw_high |= bit;
    } else {
        cpu->io.comparator.raw_high &= (uint8_t)~bit;
    }
    if (!comparator_filter_enabled(cpu, comparator)) {
        cpu->io.comparator.filter_count[comparator] = 0u;
        comparator_publish_output(cpu, comparator, current);
    }
}

static void comparator_evaluate_all(Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        comparator_evaluate(cpu, comparator);
    }
}

static void run_comparator(Dspic33* cpu, uint16_t source, uint32_t value) {
    if (source == COMPARATOR_EVENT_PMD_SOURCE) {
        uint16_t generation = (uint16_t)(value >> 1u);
        if (generation != cpu->io.comparator.pmd_generation) {
            return;
        }
        cpu->io.comparator.pmd_disabled = (value & 1u) != 0u;
        comparator_evaluate_all(cpu);
        comparator_update_filter_power(cpu);
        return;
    }
    if (source < COMPARATOR_EVENT_INPUT_COUNT) {
        uint8_t comparator = (uint8_t)(source / DSPIC33_COMPARATOR_INPUT_COUNT);
        uint8_t input = (uint8_t)(source % DSPIC33_COMPARATOR_INPUT_COUNT);
        cpu->io.comparator.input[comparator][input] = (uint16_t)value;
        comparator_evaluate(cpu, comparator);
        return;
    }
    if (source >= COMPARATOR_EVENT_REFERENCE_FIRST &&
        source < COMPARATOR_EVENT_REFERENCE_FIRST + DSPIC33_COMPARATOR_REFERENCE_COUNT) {
        cpu->io.comparator.reference[source - COMPARATOR_EVENT_REFERENCE_FIRST] = (uint16_t)value;
        comparator_evaluate_all(cpu);
        return;
    }
    if (source >= COMPARATOR_EVENT_FILTER_FIRST &&
        source < COMPARATOR_EVENT_FILTER_FIRST + DSPIC33_COMPARATOR_COUNT) {
        uint8_t comparator = (uint8_t)(source - COMPARATOR_EVENT_FILTER_FIRST);
        uint64_t samples = comparator_filter_source(cpu, comparator) == 1u &&
                                   comparator_filter_divider(cpu, comparator) == 1u
                               ? 2u
                               : 1u;
        if (value != cpu->io.comparator.filter_generation[comparator] ||
            !comparator_internal_filter_clock_available(cpu, comparator)) {
            return;
        }
        comparator_filter_samples(cpu, comparator, samples);
        if (!comparator_schedule_filter(cpu, comparator, value)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
}

static void update_comparator_pmd(Dspic33* cpu, uint16_t previous) {
    bool disabled = (raw_word(cpu, COMPARATOR_PMD_ADDRESS) & COMPARATOR_PMD) != 0u;
    if (((previous & COMPARATOR_PMD) != 0u) == disabled) {
        return;
    }
    cpu->io.comparator.pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_COMPARATOR, COMPARATOR_EVENT_PMD_SOURCE,
                          ((uint32_t)cpu->io.comparator.pmd_generation << 1u) |
                              (disabled ? 1u : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        raw_write_word(cpu, COMPARATOR_PMD_ADDRESS, previous);
        cpu->io.comparator.pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void update_comparator_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                       uint16_t requested) {
    uint16_t offset;
    uint8_t comparator;
    if (address == COMPARATOR_PMD_ADDRESS) {
        update_comparator_pmd(cpu, previous);
        return;
    }
    if (!comparator_register_write_mask(address, &offset)) {
        return;
    }
    if (cpu->io.comparator.pmd_disabled) {
        raw_write_word(cpu, address, previous);
        return;
    }
    if (address == COMPARATOR_STATUS) {
        comparator_evaluate_all(cpu);
        comparator_update_filter_power(cpu);
        return;
    }
    if (address == COMPARATOR_REFERENCE) {
        comparator_evaluate_all(cpu);
        return;
    }
    comparator = (uint8_t)((address - COMPARATOR_BASE) / COMPARATOR_STRIDE);
    offset = (uint16_t)((address - COMPARATOR_BASE) % COMPARATOR_STRIDE);
    if (((offset == 0u && ((previous ^ raw_word(cpu, address)) & COMPARATOR_ENABLE) != 0u) ||
         (offset == 6u && previous != raw_word(cpu, address))) &&
        !comparator_reconfigure_filter(cpu, comparator)) {
        raw_write_word(cpu, address, previous);
        comparator_evaluate(cpu, comparator);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    if (offset == 0u) {
        bool previous_event = (previous & COMPARATOR_EVENT) != 0u;
        bool requested_event = (requested & COMPARATOR_EVENT) != 0u;
        if (!previous_event && requested_event) {
            comparator_raise_event(cpu, comparator);
        } else if (previous_event && !requested_event) {
            cpu->io.comparator.rearm_cycle[comparator] = cpu->device_cycles + 1u;
            comparator_refresh_status(cpu);
        }
    }
    comparator_evaluate(cpu, comparator);
}

static const uint16_t rtcc_calendar_masks[4] = {0x7f7fu, 0x073fu, 0x1f3fu, 0x00ffu};
static const uint16_t rtcc_alarm_masks[3] = {0x7f7fu, 0x073fu, 0x1f3fu};
