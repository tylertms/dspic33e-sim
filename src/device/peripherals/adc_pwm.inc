
static uint16_t adc_register(const Dspic33* cpu, uint8_t module, uint16_t offset) {
    return raw_word(cpu, (uint16_t)(adc_controls[module] + offset));
}

static bool adc_12_bit(const Dspic33* cpu, uint8_t module) {
    return module == 0u && (adc_register(cpu, module, 0u) & ADC_12_BIT) != 0u;
}

static bool adc_power_enabled(const Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_register(cpu, module, 0u);
    uint8_t bit = (uint8_t)(1u << module);
    if ((cpu->io.adc_pmd_disabled & bit) != 0u || (cpu->io.adc_sleep_disabled & bit) != 0u) {
        return false;
    }
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    if (cpu->power_state == DSPIC33_POWER_IDLE) {
        return (control & ADC_STOP_IDLE) == 0u;
    }
    return (adc_register(cpu, module, 4u) & 0x8000u) != 0u;
}

static uint8_t adc_channel_count(const Dspic33* cpu, uint8_t module) {
    uint16_t selection;
    if (adc_12_bit(cpu, module)) {
        return 1u;
    }
    selection = adc_register(cpu, module, 2u) & ADC_CHANNELS_MASK;
    if (selection == 0u) {
        return 1u;
    }
    return selection == 0x0100u ? 2u : 4u;
}

static uint8_t adc_next_scan_channel(Dspic33* cpu, uint8_t module) {
    uint8_t limit = module == 0u ? 32u : 16u;
    uint8_t offset;
    uint32_t selected = module == 0u
                            ? ((uint32_t)raw_word(cpu, 0x032eu) << 16u) | raw_word(cpu, 0x0330u)
                            : raw_word(cpu, 0x0370u);
    if (selected == 0u) {
        return (uint8_t)(adc_register(cpu, module, 8u) & 0x001fu);
    }
    for (offset = 0u; offset < limit; offset++) {
        uint8_t channel = (uint8_t)((cpu->io.adc_scan_index[module] + offset) % limit);
        if ((selected & ((uint32_t)1u << channel)) != 0u) {
            cpu->io.adc_scan_index[module] = (uint8_t)((channel + 1u) % limit);
            return channel;
        }
    }
    return 0u;
}

static uint8_t adc_positive_channel(Dspic33* cpu, uint8_t module, uint8_t lane, bool mux_b) {
    uint16_t channels;
    if (lane == 0u) {
        if (!mux_b && (adc_register(cpu, module, 2u) & ADC_SCAN) != 0u) {
            return adc_next_scan_channel(cpu, module);
        }
        channels = adc_register(cpu, module, 8u);
        return (uint8_t)((channels >> (mux_b ? 8u : 0u)) & 0x001fu);
    }
    channels = adc_register(cpu, module, 6u);
    return (uint8_t)(lane - 1u + (((channels >> (mux_b ? 8u : 0u)) & 1u) != 0u ? 3u : 0u));
}

static uint8_t adc_negative_channel(const Dspic33* cpu, uint8_t module, uint8_t lane, bool mux_b) {
    uint16_t channels;
    uint16_t negative;
    if (lane == 0u) {
        channels = adc_register(cpu, module, 8u);
        return (channels & (mux_b ? 0x8000u : 0x0080u)) != 0u ? 1u : UINT8_MAX;
    }
    channels = adc_register(cpu, module, 6u);
    negative = (channels >> (mux_b ? 9u : 1u)) & 3u;
    if (negative < 2u) {
        return UINT8_MAX;
    }
    return (uint8_t)((negative == 2u ? 6u : 9u) + lane - 1u);
}

static bool adc_channel_pin(uint8_t channel, uint8_t* port, uint8_t* bit) {
    if (channel < 16u) {
        *port = 1u;
        *bit = channel;
        return true;
    }
    if (channel < 20u) {
        *port = 2u;
        *bit = (uint8_t)(channel - 15u);
        return true;
    }
    if (channel < 22u) {
        *port = 4u;
        *bit = (uint8_t)(channel - 12u);
        return true;
    }
    if (channel < 24u) {
        *port = 0u;
        *bit = (uint8_t)(channel - 16u);
        return true;
    }
    if (channel < 32u) {
        *port = 4u;
        *bit = (uint8_t)(channel - 24u);
        return true;
    }
    return false;
}

static uint16_t adc_pin_input(const Dspic33* cpu, uint8_t channel) {
    uint8_t port;
    uint8_t bit;
    uint16_t mask;
    if (!adc_channel_pin(channel, &port, &bit)) {
        return 0u;
    }
    mask = (uint16_t)(1u << bit);
    if ((raw_word(cpu, gpio_analog_addresses[port]) & mask) == 0u ||
        (cpu->io.adc_pmd_disabled & 1u) != 0u ||
        (channel < 16u && (cpu->io.adc_pmd_disabled & 2u) != 0u)) {
        return 0u;
    }
    if ((raw_word(cpu, gpio_tris_addresses[port]) & mask) == 0u) {
        return (raw_word(cpu, gpio_latch_addresses[port]) & mask) != 0u ? 0x0fffu : 0u;
    }
    return cpu->io.adc[channel];
}

static uint16_t adc_input_code(const Dspic33* cpu, uint8_t module, uint8_t positive,
                               uint8_t negative) {
    uint16_t high = adc_pin_input(cpu, positive);
    uint16_t low = adc_pin_input(cpu, negative);
    uint16_t difference = high > low ? (uint16_t)(high - low) : 0u;
    return adc_12_bit(cpu, module) ? (uint16_t)(difference & 0x0fffu)
                                   : (uint16_t)((difference >> 2u) & 0x03ffu);
}

static uint16_t adc_format_code(const Dspic33* cpu, uint8_t module, uint16_t code) {
    uint8_t bits = adc_12_bit(cpu, module) ? 12u : 10u;
    uint8_t shift = (uint8_t)(16u - bits);
    uint16_t sign = (uint16_t)(1u << (bits - 1u));
    uint16_t mask = (uint16_t)((1u << bits) - 1u);
    uint16_t format = adc_register(cpu, module, 0u) & ADC_FORMAT_MASK;
    if (format == 0u) {
        return code;
    }
    if (format == 0x0200u) {
        return (uint16_t)(code << shift);
    }
    code ^= sign;
    if ((code & sign) != 0u) {
        code |= (uint16_t)~mask;
    }
    return format == 0x0100u ? code : (uint16_t)(code << shift);
}

static uint64_t adc_clock_cycles(const Dspic33* cpu, uint8_t module) {
    uint16_t timing = adc_register(cpu, module, 4u);
    return (timing & 0x8000u) != 0u ? 1u : (uint64_t)(timing & 0x00ffu) + 1u;
}

static uint64_t adc_sample_cycles(const Dspic33* cpu, uint8_t module) {
    return ((adc_register(cpu, module, 4u) >> 8u) & 0x001fu) * adc_clock_cycles(cpu, module);
}

static uint64_t adc_conversion_cycles(const Dspic33* cpu, uint8_t module) {
    return (adc_12_bit(cpu, module) ? 14u : 12u) * adc_clock_cycles(cpu, module);
}

static uint32_t adc_event_value(const Dspic33* cpu, uint8_t module, uint8_t source, bool complete) {
    uint32_t value = source;
    value |= (uint32_t)cpu->io.adc_generation[module] << ADC_EVENT_GENERATION_SHIFT;
    if (complete) {
        value |= ADC_EVENT_COMPLETE;
    }
    return value;
}

static uint8_t adc_increment_threshold(const Dspic33* cpu, uint8_t module) {
    uint8_t value = (uint8_t)((adc_register(cpu, module, 2u) >> 2u) & 0x001fu);
    uint16_t dma = module == 0u ? raw_word(cpu, 0x0332u) : raw_word(cpu, 0x0372u);
    if (module != 0u || (dma & ADC_DMA_ENABLE) == 0u) {
        value &= 0x0fu;
    }
    return (uint8_t)(value + 1u);
}

static uint16_t adc_dma_address(Dspic33* cpu, uint8_t module, uint8_t channel) {
    uint16_t control = adc_register(cpu, module, 0u);
    uint16_t dma = module == 0u ? raw_word(cpu, 0x0332u) : raw_word(cpu, 0x0372u);
    uint8_t length = (uint8_t)(dma & ADC_DMA_LENGTH_MASK);
    uint8_t slot = cpu->io.adc_dma_sample[module][channel];
    uint16_t address;
    if ((control & ADC_BUFFER_ORDER) != 0u) {
        return (uint16_t)(cpu->io.adc_buffer_index[module] * 2u);
    }
    address = (uint16_t)(channel * ((uint16_t)2u << length) + slot * 2u);
    cpu->io.adc_dma_sample[module][channel] = (uint8_t)((slot + 1u) & ((1u << length) - 1u));
    return address;
}

static void adc_increment_boundary(Dspic33* cpu, uint8_t module) {
    uint16_t control2 = adc_register(cpu, module, 2u);
    cpu->io.adc_sample_count[module] = 0u;
    cpu->io.adc_scan_index[module] = 0u;
    if ((control2 & ADC_BUFFER_SPLIT) != 0u) {
        control2 ^= ADC_SECOND_BUFFER;
        cpu->io.adc_buffer_index[module] = (control2 & ADC_SECOND_BUFFER) != 0u ? 8u : 0u;
        raw_write_word(cpu, (uint16_t)(adc_controls[module] + 2u), control2);
    } else {
        cpu->io.adc_buffer_index[module] = 0u;
    }
}

static void adc_store_result(Dspic33* cpu, uint8_t module, uint8_t channel, uint16_t result) {
    uint16_t dma = module == 0u ? raw_word(cpu, 0x0332u) : raw_word(cpu, 0x0372u);
    uint16_t indirect = 0u;
    bool increment_boundary;
    if ((dma & ADC_DMA_ENABLE) != 0u) {
        indirect = adc_dma_address(cpu, module, channel);
        raw_write_word(cpu, adc_buffers[module], result);
        dspic33_dma_request(cpu, adc_irqs[module], indirect, 0u);
    } else {
        raw_write_word(
            cpu, (uint16_t)(adc_buffers[module] + (cpu->io.adc_buffer_index[module] & 0x0fu) * 2u),
            result);
    }
    cpu->io.adc_buffer_index[module] = (uint8_t)((cpu->io.adc_buffer_index[module] + 1u) & 0x0fu);
    cpu->io.adc_sample_count[module]++;
    increment_boundary = cpu->io.adc_sample_count[module] >= adc_increment_threshold(cpu, module);
    if (increment_boundary) {
        adc_increment_boundary(cpu, module);
    }
    if ((dma & ADC_DMA_ENABLE) != 0u || increment_boundary) {
        dspic33_raise_interrupt(cpu, adc_irqs[module]);
        if (module == 0u) {
            output_compare_pulse_source(cpu, OUTPUT_COMPARE_SYNC_ADC1);
        }
    }
}

static void adc_abort(Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_register(cpu, module, 0u);
    cpu->io.adc_generation[module]++;
    cpu->io.adc_latched_count[module] = 0u;
    cpu->io.adc_conversion_index[module] = 0u;
    raw_write_word(cpu, adc_controls[module], (uint16_t)(control & ~(ADC_SAMPLE | ADC_DONE)));
}

static bool adc_schedule(Dspic33* cpu, uint8_t module, uint8_t source, bool complete,
                         uint64_t delay) {
    if (dspic33_schedule(cpu, DSPIC33_EVENT_ADC, module,
                         adc_event_value(cpu, module, source, complete), delay)) {
        return true;
    }
    adc_abort(cpu, module);
    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    return false;
}

static void adc_latch(Dspic33* cpu, uint8_t module, uint8_t index) {
    uint8_t positive = cpu->io.adc_latched_channel[module][index];
    uint8_t negative = cpu->io.adc_latched_negative[module][index];
    cpu->io.adc_latched[module][index] =
        adc_format_code(cpu, module, adc_input_code(cpu, module, positive, negative));
}

static void adc_complete_conversion(Dspic33* cpu, uint8_t module, uint8_t source) {
    uint16_t control;
    uint8_t index;
    if (!adc_power_enabled(cpu, module)) {
        return;
    }
    control = adc_register(cpu, module, 0u);
    if ((control & ADC_ON) == 0u) {
        return;
    }
    index = cpu->io.adc_conversion_index[module];
    if (index >= cpu->io.adc_latched_count[module]) {
        return;
    }
    adc_store_result(cpu, module, cpu->io.adc_latched_channel[module][index],
                     cpu->io.adc_latched[module][index]);
    index++;
    cpu->io.adc_conversion_index[module] = index;
    if (index < cpu->io.adc_latched_count[module]) {
        if ((control & ADC_SIMULTANEOUS) == 0u) {
            adc_latch(cpu, module, index);
        }
        adc_schedule(cpu, module, source, true, adc_conversion_cycles(cpu, module));
        return;
    }
    cpu->io.adc_latched_count[module] = 0u;
    raw_write_word(cpu, adc_controls[module],
                   source == 1u ? (uint16_t)(control & ~ADC_DONE) : (uint16_t)(control | ADC_DONE));
    if (cpu->power_state == DSPIC33_POWER_SLEEP && !interrupt_enabled(cpu, adc_irqs[module])) {
        cpu->io.adc_sleep_disabled |= (uint8_t)(1u << module);
        return;
    }
    if ((control & ADC_AUTO_SAMPLE) != 0u) {
        adc_begin_sampling(cpu, module);
    }
}

static void adc_start_conversion(Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_register(cpu, module, 0u);
    uint16_t control2 = adc_register(cpu, module, 2u);
    uint8_t count;
    uint8_t index;
    bool mux_b;
    if ((control & (ADC_ON | ADC_SAMPLE)) != (ADC_ON | ADC_SAMPLE) ||
        !adc_power_enabled(cpu, module)) {
        return;
    }
    count = adc_channel_count(cpu, module);
    mux_b = (cpu->io.adc_mux_b & (uint8_t)(1u << module)) != 0u;
    if ((control & (ADC_12_BIT | ADC_SIMULTANEOUS | ADC_AUTO_SAMPLE | ADC_TRIGGER_MASK)) ==
            (ADC_AUTO_SAMPLE | 0x0070u) &&
        (control2 & ADC_CHANNELS_MASK) == 0x0100u &&
        (adc_register(cpu, module, 4u) & 0x8000u) == 0u &&
        ((adc_register(cpu, module, 4u) >> 8u) & 0x001fu) < 12u &&
        (adc_register(cpu, module, 4u) & 0x00ffu) == 2u && count >= 2u) {
        uint8_t channel0 = adc_positive_channel(cpu, module, 0u, mux_b);
        uint8_t channel1 = adc_positive_channel(cpu, module, 1u, mux_b);
        if (channel0 == channel1 && (channel0 == 0u || channel0 == 3u)) {
            cpu->io.adc_latched_count[module] = 0u;
            cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
            return;
        }
    }
    for (index = 0u; index < count; index++) {
        uint8_t positive = adc_positive_channel(cpu, module, index, mux_b);
        uint8_t negative = adc_negative_channel(cpu, module, index, mux_b);
        cpu->io.adc_latched_channel[module][index] = positive;
        cpu->io.adc_latched_negative[module][index] = negative;
        if ((control & ADC_SIMULTANEOUS) != 0u || index == 0u) {
            adc_latch(cpu, module, index);
        }
    }
    cpu->io.adc_latched_count[module] = count;
    cpu->io.adc_conversion_index[module] = 0u;
    if ((control2 & ADC_ALTERNATE) != 0u) {
        cpu->io.adc_mux_b ^= (uint8_t)(1u << module);
    } else {
        cpu->io.adc_mux_b &= (uint8_t)~(1u << module);
    }
    raw_write_word(cpu, adc_controls[module], (uint16_t)(control & ~(ADC_SAMPLE | ADC_DONE)));
    if (module == 0u) {
        input_capture_pulse_source(cpu, INPUT_CAPTURE_SYNC_ADC1);
    }
    adc_schedule(cpu, module, (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u), true,
                 adc_conversion_cycles(cpu, module));
}

static void adc_begin_sampling(Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_register(cpu, module, 0u);
    uint8_t source;
    if ((control & ADC_ON) == 0u || !adc_power_enabled(cpu, module)) {
        return;
    }
    cpu->io.adc_generation[module]++;
    control |= ADC_SAMPLE;
    raw_write_word(cpu, adc_controls[module], control);
    source = (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u);
    if (source == 7u) {
        adc_schedule(cpu, module, source, false, adc_sample_cycles(cpu, module));
    }
}

static void adc_update_power_state(Dspic33* cpu) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        uint16_t control = adc_register(cpu, module, 0u);
        uint8_t bit = (uint8_t)(1u << module);
        if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
            cpu->io.adc_sleep_disabled &= (uint8_t)~bit;
            if ((control & (ADC_ON | ADC_AUTO_SAMPLE | ADC_SAMPLE)) == (ADC_ON | ADC_AUTO_SAMPLE) &&
                cpu->io.adc_latched_count[module] == 0u) {
                adc_begin_sampling(cpu, module);
            }
            continue;
        }
        if ((cpu->power_state == DSPIC33_POWER_IDLE && (control & ADC_STOP_IDLE) != 0u) ||
            (cpu->power_state == DSPIC33_POWER_SLEEP &&
             (adc_register(cpu, module, 4u) & 0x8000u) == 0u)) {
            if ((control & ADC_SAMPLE) != 0u || cpu->io.adc_latched_count[module] != 0u) {
                adc_abort(cpu, module);
            }
        }
    }
}

static void run_adc(Dspic33* cpu, uint8_t module, uint32_t event_value) {
    uint16_t generation;
    uint8_t source;
    if (module >= DSPIC33_ADC_COUNT) {
        return;
    }
    generation = (uint16_t)(event_value >> ADC_EVENT_GENERATION_SHIFT);
    source = (uint8_t)(event_value & ADC_EVENT_SOURCE_MASK);
    if ((event_value & ADC_EVENT_COMPLETE) != 0u) {
        if (generation == cpu->io.adc_generation[module]) {
            adc_complete_conversion(cpu, module, source);
        }
        return;
    }
    if (generation != UINT16_MAX && generation != cpu->io.adc_generation[module]) {
        return;
    }
    if (((adc_register(cpu, module, 0u) & ADC_TRIGGER_MASK) >> 4u) == source) {
        adc_start_conversion(cpu, module);
    }
}

static uint16_t pwm_generator_base(uint8_t generator) {
    return (uint16_t)(PWM_GENERATOR_BASE + generator * PWM_GENERATOR_STRIDE);
}

static uint16_t pwm_register(const Dspic33* cpu, uint8_t generator, uint16_t offset) {
    return raw_word(cpu, (uint16_t)(pwm_generator_base(generator) + offset));
}

static bool pwm_global_pmd_disabled(const Dspic33* cpu) {
    return (cpu->io.pwm_pmd_disabled & 1u) != 0u;
}

static bool pwm_generator_pmd_disabled(const Dspic33* cpu, uint8_t generator) {
    return pwm_global_pmd_disabled(cpu) ||
           (cpu->io.pwm_pmd_disabled & (uint8_t)(2u << generator)) != 0u;
}

static bool pwm_address_pmd_disabled(const Dspic33* cpu, uint16_t address) {
    if (address >= PWM_GLOBAL_BASE && address < PWM_GENERATOR_BASE) {
        return pwm_global_pmd_disabled(cpu);
    }
    if (address >= PWM_GENERATOR_BASE &&
        address < PWM_GENERATOR_BASE + pwm_generator_count(cpu) * PWM_GENERATOR_STRIDE) {
        uint8_t generator = (uint8_t)((address - PWM_GENERATOR_BASE) / PWM_GENERATOR_STRIDE);
        return pwm_generator_pmd_disabled(cpu, generator);
    }
    return false;
}

static bool pwm_address_inaccessible(const Dspic33* cpu, uint16_t address) {
    return pwm_address_pmd_disabled(cpu, address) ||
           (cpu->power_state == DSPIC33_POWER_IDLE &&
            (raw_word(cpu, PWM_GLOBAL_BASE) & PWM_STOP_IDLE) != 0u && address >= PWM_GLOBAL_BASE &&
            address < PWM_GENERATOR_BASE + pwm_generator_count(cpu) * PWM_GENERATOR_STRIDE);
}

static bool pwm_power_enabled(const Dspic33* cpu) {
    uint16_t control = raw_word(cpu, PWM_GLOBAL_BASE);
    if (pwm_global_pmd_disabled(cpu) || (control & PWM_ENABLE) == 0u ||
        cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE || (control & PWM_STOP_IDLE) == 0u;
}

static uint16_t pwm_divider(const Dspic33* cpu, uint8_t time_base) {
    uint16_t address = (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 2u : 0x10u));
    uint8_t selection = (uint8_t)(raw_word(cpu, address) & 7u);
    return selection < 7u ? (uint16_t)(1u << selection) : 64u;
}

static void pwm_latch_periods(Dspic33* cpu) {
    cpu->io.pwm_active_period[0] = raw_word(cpu, PWM_GLOBAL_BASE + 4u);
    cpu->io.pwm_active_period[1] = raw_word(cpu, PWM_GLOBAL_BASE + 0x12u);
}

static void pwm_latch_generator(Dspic33* cpu, uint8_t generator) {
    uint16_t control = pwm_register(cpu, generator, 0u);
    uint16_t master = raw_word(cpu, PWM_GLOBAL_BASE + 0x0au);
    cpu->io.pwm_active_duty[generator][0] =
        (control & PWM_MASTER_DUTY) != 0u ? master : pwm_register(cpu, generator, 6u);
    cpu->io.pwm_active_duty[generator][1] =
        (control & PWM_MASTER_DUTY) != 0u ? master : pwm_register(cpu, generator, 0x0eu);
    cpu->io.pwm_active_phase[generator][0] = pwm_register(cpu, generator, 8u);
    cpu->io.pwm_active_phase[generator][1] = pwm_register(cpu, generator, 0x10u);
    cpu->io.pwm_active_dead_time[generator][0] = pwm_register(cpu, generator, 0x0au);
    cpu->io.pwm_active_dead_time[generator][1] = pwm_register(cpu, generator, 0x0cu);
}

static uint16_t pwm_output_io(const Dspic33* cpu, uint8_t generator) {
    uint16_t io = pwm_register(cpu, generator, 2u);
    if ((io & PWM_OVERRIDE_SYNCHRONIZED) == 0u) {
        return io;
    }
    return (uint16_t)((io & ~PWM_SYNCHRONIZED_IO) |
                      (cpu->io.pwm_active_io[generator] & PWM_SYNCHRONIZED_IO));
}

static uint16_t pwm_period(const Dspic33* cpu, uint8_t generator, uint8_t output) {
    uint16_t control = pwm_register(cpu, generator, 0u);
    uint16_t mode = pwm_register(cpu, generator, 2u) & PWM_MODE_MASK;
    if ((control & PWM_INDEPENDENT_TIME_BASE) != 0u) {
        return output != 0u && mode == PWM_MODE_INDEPENDENT
                   ? cpu->io.pwm_active_phase[generator][1]
                   : cpu->io.pwm_active_phase[generator][0];
    }
    return cpu->io.pwm_active_period[(control & 0x0008u) != 0u ? 1u : 0u];
}

static uint16_t pwm_shifted_counter(uint16_t counter, uint16_t phase, uint16_t period) {
    uint32_t modulus = (uint32_t)period + 1u;
    return (uint16_t)((counter + modulus - phase % modulus) % modulus);
}

static bool pwm_input_active(uint32_t inputs, uint8_t source, bool inverted) {
    bool high = (inputs & ((uint32_t)1u << source)) != 0u;
    return inverted ? !high : high;
}

static bool pwm_fault_active(const Dspic33* cpu, uint8_t generator) {
    uint16_t fault = pwm_register(cpu, generator, 4u);
    uint8_t source = (uint8_t)((fault >> 3u) & 0x1fu);
    return pwm_input_active(cpu->io.pwm_fault_inputs, source, (fault & 0x0004u) != 0u);
}

static bool pwm_current_limit_active(const Dspic33* cpu, uint8_t generator) {
    uint16_t fault = pwm_register(cpu, generator, 4u);
    uint8_t source = (uint8_t)((fault >> 10u) & 0x1fu);
    return pwm_input_active(cpu->io.pwm_current_limit_inputs, source, (fault & 0x0200u) != 0u);
}

static void pwm_waveform_pair(const Dspic33* cpu, uint8_t generator, bool* high, bool* low);

static bool pwm_state_blanked(const Dspic33* cpu, uint8_t generator, bool current_limit) {
    uint16_t leading = pwm_register(cpu, generator, 0x1au);
    uint16_t auxiliary = pwm_register(cpu, generator, 0x1eu);
    bool high = cpu->io.pwm[generator * 2u] != 0u;
    bool low = cpu->io.pwm[generator * 2u + 1u] != 0u;
    uint8_t source = (uint8_t)((auxiliary >> 8u) & 0x0fu);
    bool blank_source = false;
    if (source != 0u && source <= pwm_generator_count(cpu)) {
        bool blank_low;
        pwm_waveform_pair(cpu, (uint8_t)(source - 1u), &blank_source, &blank_low);
    }
    if (current_limit ? (leading & 0x0400u) == 0u : (leading & 0x0800u) == 0u) {
        return false;
    }
    if (cpu->io.pwm_leb_ticks[generator] != 0u) {
        return true;
    }
    return ((leading & 0x0020u) != 0u && blank_source) ||
           ((leading & 0x0010u) != 0u && !blank_source) || ((leading & 0x0008u) != 0u && high) ||
           ((leading & 0x0004u) != 0u && !high) || ((leading & 0x0002u) != 0u && low) ||
           ((leading & 0x0001u) != 0u && !low);
}

static void pwm_refresh_status(Dspic33* cpu, uint8_t generator) {
    uint16_t base = pwm_generator_base(generator);
    uint16_t control = raw_word(cpu, base);
    bool fault = pwm_fault_active(cpu, generator);
    bool current_limit = pwm_current_limit_active(cpu, generator);
    if ((control & PWM_FAULT_INTERRUPT) == 0u) {
        control = fault ? (uint16_t)(control | PWM_FAULT_STATUS)
                        : (uint16_t)(control & ~PWM_FAULT_STATUS);
    }
    if ((control & PWM_CURRENT_LIMIT_INTERRUPT) == 0u) {
        control = current_limit ? (uint16_t)(control | PWM_CURRENT_LIMIT_STATUS)
                                : (uint16_t)(control & ~PWM_CURRENT_LIMIT_STATUS);
    }
    raw_write_word(cpu, base, control);
}

static bool pwm_waveform(uint16_t counter, uint16_t duty) { return counter <= duty; }

static uint16_t pwm_saturated_add(uint16_t value, uint16_t increment) {
    return UINT16_MAX - value > increment ? (uint16_t)(value + increment) : UINT16_MAX;
}

typedef enum {
    PWM_COMPENSATION_ORDINARY,
    PWM_COMPENSATION_ZERO,
    PWM_COMPENSATION_FULL
} PwmCompensationResult;

static uint16_t pwm_compensated_duty(const Dspic33* cpu, uint8_t generator, uint16_t duty,
                                     uint16_t compensation) {
    bool input = (cpu->io.pwm_dead_time_sampled & (uint8_t)(1u << generator)) != 0u;
    bool polarity = (pwm_register(cpu, generator, 0u) & 0x0020u) != 0u;
    if (input == polarity) {
        return duty > compensation ? (uint16_t)(duty - compensation) : 0u;
    }
    return pwm_saturated_add(duty, compensation);
}

static uint16_t pwm_b1_edge_compensated_duty(const Dspic33* cpu, uint8_t generator, uint16_t duty,
                                             uint16_t compensation, PwmCompensationResult* result) {
    bool input = (cpu->io.pwm_dead_time_sampled & (uint8_t)(1u << generator)) != 0u;
    bool polarity = (pwm_register(cpu, generator, 0u) & 0x0020u) != 0u;
    uint16_t period = pwm_period(cpu, generator, 0u);
    uint32_t threshold = (uint32_t)compensation * 2u;
    *result = PWM_COMPENSATION_ORDINARY;
    if (input == polarity && (uint32_t)duty < threshold) {
        *result = PWM_COMPENSATION_ZERO;
        return 0u;
    }
    if (input != polarity && (uint32_t)duty + threshold >= period) {
        *result = PWM_COMPENSATION_FULL;
        return period;
    }
    return pwm_compensated_duty(cpu, generator, duty, compensation);
}

static void pwm_waveform_pair(const Dspic33* cpu, uint8_t generator, bool* high, bool* low) {
    uint16_t control = pwm_register(cpu, generator, 0u);
    uint16_t io = pwm_register(cpu, generator, 2u);
    uint16_t mode = io & PWM_MODE_MASK;
    uint16_t high_counter = cpu->io.pwm_counter[generator][0];
    uint16_t low_counter = cpu->io.pwm_counter[generator][1];
    uint16_t dead_mode = control & 0x00c0u;
    uint16_t primary_dead = cpu->io.pwm_active_dead_time[generator][0];
    uint16_t alternate_dead = cpu->io.pwm_active_dead_time[generator][1];
    bool primary = pwm_waveform(high_counter, cpu->io.pwm_active_duty[generator][0]);
    if (mode == PWM_MODE_INDEPENDENT) {
        *high = primary;
        *low = pwm_waveform(low_counter, cpu->io.pwm_active_duty[generator][1]);
    } else if (mode == PWM_MODE_REDUNDANT) {
        *high = primary;
        *low = primary;
    } else if (mode == PWM_MODE_PUSH_PULL) {
        bool second = (cpu->io.pwm_push_pull & (uint8_t)(1u << generator)) != 0u;
        *high = primary && !second;
        *low = primary && second;
    } else {
        uint16_t duty = cpu->io.pwm_active_duty[generator][0];
        if (dead_mode == 0x0080u) {
            *high = primary;
            *low = !primary;
        } else if (dead_mode == 0x00c0u) {
            if ((control & PWM_CENTER_ALIGNED) != 0u) {
                uint16_t compensated = pwm_compensated_duty(cpu, generator, duty, primary_dead);
                uint16_t half_dead = (uint16_t)((alternate_dead + 1u) / 2u);
                *high = high_counter <= compensated;
                *low = high_counter > pwm_saturated_add(compensated, half_dead);
            } else {
                PwmCompensationResult result;
                uint16_t compensated =
                    pwm_b1_edge_compensated_duty(cpu, generator, duty, primary_dead, &result);
                if (result == PWM_COMPENSATION_ZERO) {
                    *high = false;
                    *low = true;
                } else if (result == PWM_COMPENSATION_FULL) {
                    *high = true;
                    *low = false;
                } else {
                    *high = high_counter >= alternate_dead && high_counter <= compensated;
                    *low = high_counter > pwm_saturated_add(compensated, alternate_dead);
                }
            }
        } else if (dead_mode == 0x0040u && (control & PWM_CENTER_ALIGNED) == 0u) {
            *high = high_counter <= pwm_saturated_add(duty, alternate_dead);
            *low = (uint32_t)high_counter + primary_dead > duty;
        } else if ((control & PWM_CENTER_ALIGNED) != 0u) {
            uint16_t half_dead = (uint16_t)((alternate_dead + 1u) / 2u);
            *high = high_counter <= (duty > half_dead ? (uint16_t)(duty - half_dead) : 0u);
            *low = high_counter > pwm_saturated_add(duty, half_dead);
        } else {
            if (duty < alternate_dead) {
                *high = high_counter <= duty;
                *low = high_counter > duty;
            } else {
                *high = high_counter >= primary_dead && high_counter <= duty;
                *low = high_counter > pwm_saturated_add(duty, alternate_dead);
            }
        }
        return;
    }
    if (dead_mode == 0u) {
        uint16_t dead_low_counter = mode == PWM_MODE_INDEPENDENT ? low_counter : high_counter;
        if ((control & PWM_CENTER_ALIGNED) != 0u) {
            uint16_t half_dead = (uint16_t)((alternate_dead + 1u) / 2u);
            uint16_t high_duty = cpu->io.pwm_active_duty[generator][0];
            uint16_t low_duty =
                mode == PWM_MODE_INDEPENDENT ? cpu->io.pwm_active_duty[generator][1] : high_duty;
            *high = *high && high_counter <=
                                 (high_duty > half_dead ? (uint16_t)(high_duty - half_dead) : 0u);
            *low = *low && dead_low_counter <=
                               (low_duty > half_dead ? (uint16_t)(low_duty - half_dead) : 0u);
        } else {
            *high = *high && high_counter >= primary_dead;
            *low = *low && dead_low_counter >= alternate_dead;
        }
    }
}

static void pwm_apply_protection(const Dspic33* cpu, uint8_t generator, bool* high, bool* low) {
    uint16_t fault = pwm_register(cpu, generator, 4u);
    uint16_t io = pwm_register(cpu, generator, 2u);
    uint8_t bit = (uint8_t)(1u << generator);
    bool fault_active =
        ((cpu->io.pwm_fault_latched | cpu->io.pwm_fault_cycle) & bit) != 0u ||
        (pwm_fault_active(cpu, generator) && !pwm_state_blanked(cpu, generator, false) &&
         (fault & PWM_FAULT_MODE_MASK) != PWM_FAULT_DISABLED);
    bool current_active =
        ((cpu->io.pwm_current_cycle & bit) != 0u ||
         (pwm_current_limit_active(cpu, generator) && !pwm_state_blanked(cpu, generator, true))) &&
        (fault & PWM_CURRENT_LIMIT_MODE) != 0u;
    if ((fault & 0x8000u) != 0u) {
        if (current_active) {
            *high = (io & 0x0020u) != 0u;
        }
        if (fault_active) {
            *low = (io & 0x0010u) != 0u;
        }
    } else if (fault_active) {
        *high = (io & 0x0020u) != 0u;
        *low = (io & 0x0010u) != 0u;
    } else if (current_active) {
        *high = (io & 0x0008u) != 0u;
        *low = (io & 0x0004u) != 0u;
    }
}

static void pwm_logical_output(const Dspic33* cpu, uint8_t generator, bool* high, bool* low) {
    uint16_t io = pwm_output_io(cpu, generator);
    pwm_waveform_pair(cpu, generator, high, low);
    pwm_apply_protection(cpu, generator, high, low);
    if ((io & PWM_OVERRIDE_HIGH) != 0u) {
        *high = (io & 0x0080u) != 0u;
    }
    if ((io & PWM_OVERRIDE_LOW) != 0u) {
        *low = (io & 0x0040u) != 0u;
    }
    if ((io & PWM_SWAP) != 0u) {
        bool swap = *high;
        *high = *low;
        *low = swap;
    }
}

static void pwm_update_output(Dspic33* cpu, uint8_t generator) {
    uint16_t io = pwm_output_io(cpu, generator);
    uint16_t auxiliary = pwm_register(cpu, generator, 0x1eu);
    uint16_t leading = pwm_register(cpu, generator, 0x1au);
    bool previous_high = cpu->io.pwm[generator * 2u] != 0u;
    bool previous_low = cpu->io.pwm[generator * 2u + 1u] != 0u;
    bool high;
    bool low;
    pwm_logical_output(cpu, generator, &high, &low);
    if ((auxiliary & 0x0003u) != 0u) {
        uint8_t chop_source = (uint8_t)((auxiliary >> 2u) & 0x0fu);
        uint16_t chop_period = (uint16_t)((raw_word(cpu, PWM_GLOBAL_BASE + 0x1au) & 0x03ffu) + 1u);
        bool chop_high = false;
        if (chop_source == 0u) {
            chop_high = (raw_word(cpu, PWM_GLOBAL_BASE + 0x1au) & 0x8000u) != 0u &&
                        cpu->io.pwm_chop_counter < (uint16_t)((chop_period + 1u) / 2u);
        } else if (chop_source <= pwm_generator_count(cpu)) {
            bool chop_low;
            pwm_logical_output(cpu, (uint8_t)(chop_source - 1u), &chop_high, &chop_low);
        }
        if ((auxiliary & 0x0002u) != 0u) {
            high = high && chop_high;
        }
        if ((auxiliary & 0x0001u) != 0u) {
            low = low && chop_high;
        }
    }
    if ((io & PWM_POLARITY_HIGH) != 0u) {
        high = !high;
    }
    if ((io & PWM_POLARITY_LOW) != 0u) {
        low = !low;
    }
    if ((io & PWM_PIN_HIGH) == 0u) {
        high = false;
    }
    if ((io & PWM_PIN_LOW) == 0u) {
        low = false;
    }
    cpu->io.pwm[generator * 2u] = high ? 1u : 0u;
    cpu->io.pwm[generator * 2u + 1u] = low ? 1u : 0u;
    if ((previous_high != high || previous_low != low) && !cpu->io.pwm_batch_updating) {
        comparator_evaluate_all(cpu);
    }
    if ((!previous_high && high && (leading & 0x8000u) != 0u) ||
        (previous_high && !high && (leading & 0x4000u) != 0u) ||
        (!previous_low && low && (leading & 0x2000u) != 0u) ||
        (previous_low && !low && (leading & 0x1000u) != 0u)) {
        cpu->io.pwm_leb_ticks[generator] = pwm_register(cpu, generator, 0x1cu);
    }
}

static void pwm_emit_adc_trigger(Dspic33* cpu, uint8_t source) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        dspic33_adc_trigger(cpu, module, source, 0u);
    }
}

static void pwm_special_match(Dspic33* cpu, uint8_t time_base) {
    uint16_t control_address = (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu));
    uint16_t control = raw_word(cpu, control_address);
    uint8_t postscale = (uint8_t)(control & 0x000fu);
    cpu->io.pwm_special_count[time_base]++;
    if (cpu->io.pwm_special_count[time_base] <= postscale) {
        return;
    }
    cpu->io.pwm_special_count[time_base] = 0u;
    pwm_emit_adc_trigger(cpu, time_base == 0u ? 3u : 5u);
    if ((control & PWM_SPECIAL_INTERRUPT) != 0u) {
        raw_write_word(cpu, control_address, (uint16_t)(control | PWM_SPECIAL_STATUS));
        dspic33_raise_interrupt(cpu, time_base == 0u ? 57u : 73u);
    }
}

static void pwm_generator_match(Dspic33* cpu, uint8_t generator) {
    uint16_t control = pwm_register(cpu, generator, 0u);
    uint16_t trigger_control = pwm_register(cpu, generator, 0x14u);
    uint8_t start = (uint8_t)(trigger_control & 0x003fu);
    uint8_t divider = (uint8_t)(trigger_control >> 12u);
    uint16_t base = pwm_generator_base(generator);
    if (cpu->io.pwm_cycle_count[generator] < start) {
        return;
    }
    cpu->io.pwm_trigger_count[generator]++;
    if (cpu->io.pwm_trigger_count[generator] <= divider) {
        return;
    }
    cpu->io.pwm_trigger_count[generator] = 0u;
    pwm_emit_adc_trigger(cpu, (uint8_t)(8u + generator));
    if ((control & PWM_TRIGGER_INTERRUPT) != 0u) {
        raw_write_word(cpu, base, (uint16_t)(control | PWM_TRIGGER_STATUS));
        dspic33_raise_interrupt(cpu, pwm_irqs[generator]);
    }
}

static bool pwm_advance_independent_counter(Dspic33* cpu, uint8_t generator, uint8_t output) {
    uint16_t control = pwm_register(cpu, generator, 0u);
    uint16_t period = pwm_period(cpu, generator, output);
    uint16_t* counter = &cpu->io.pwm_counter[generator][output];
    uint8_t bit = (uint8_t)(1u << generator);
    bool descending = (cpu->io.pwm_direction[output] & bit) != 0u;
    if ((control & PWM_CENTER_ALIGNED) == 0u) {
        if (*counter >= period) {
            *counter = 0u;
            return true;
        }
        (*counter)++;
        return false;
    }
    if (period == 0u) {
        *counter = 0u;
        return true;
    }
    if (!descending) {
        if (*counter >= period) {
            cpu->io.pwm_direction[output] |= bit;
            (*counter)--;
        } else {
            (*counter)++;
        }
        return false;
    }
    if (*counter <= 1u) {
        *counter = 0u;
        cpu->io.pwm_direction[output] &= (uint8_t)~bit;
        return true;
    }
    (*counter)--;
    return false;
}

static bool pwm_advance_master_counter(Dspic33* cpu, uint8_t time_base, uint64_t cycle) {
    uint16_t* counter = &cpu->io.pwm_master_counter[time_base];
    uint16_t period = cpu->io.pwm_active_period[time_base];
    if (*counter >= period) {
        *counter = 0u;
        if ((raw_word(cpu, (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu))) &
             0x0100u) != 0u) {
            cpu->io.pwm_sync_until[time_base] = cycle + 12u;
            comparator_filter_clock(cpu, (uint8_t)(2u + time_base), 1u);
        }
        return true;
    }
    (*counter)++;
    return false;
}

static void pwm_cycle_boundary(Dspic33* cpu, uint8_t generator, bool period_updated) {
    uint8_t bit = (uint8_t)(1u << generator);
    uint16_t fault = pwm_register(cpu, generator, 4u);
    uint16_t io = pwm_register(cpu, generator, 2u);
    bool immediate = (pwm_register(cpu, generator, 0u) & PWM_IMMEDIATE_UPDATE) != 0u;
    bool delayed = period_updated && !immediate && (cpu->io.pwm_timing_update & bit) != 0u;
    if (!delayed) {
        cpu->io.pwm_dead_time_sampled = (uint8_t)((cpu->io.pwm_dead_time_sampled & ~bit) |
                                                  (cpu->io.pwm_dead_time_inputs & bit));
        if (!immediate) {
            pwm_latch_generator(cpu, generator);
        }
        cpu->io.pwm_timing_update &= (uint8_t)~bit;
    }
    cpu->io.pwm_cycle_count[generator]++;
    if ((pwm_register(cpu, generator, 2u) & PWM_MODE_MASK) == PWM_MODE_PUSH_PULL) {
        cpu->io.pwm_push_pull ^= bit;
    }
    if ((io & PWM_OVERRIDE_SYNCHRONIZED) != 0u) {
        cpu->io.pwm_active_io[generator] = io;
    }
    if (!delayed && (cpu->io.pwm_fault_release & bit) != 0u && !pwm_fault_active(cpu, generator)) {
        cpu->io.pwm_fault_latched &= (uint8_t)~bit;
        cpu->io.pwm_fault_release &= (uint8_t)~bit;
    }
    if (!delayed &&
        (!pwm_fault_active(cpu, generator) || (fault & PWM_FAULT_MODE_MASK) != PWM_FAULT_CYCLE)) {
        cpu->io.pwm_fault_cycle &= (uint8_t)~bit;
    }
    if (!delayed &&
        (!pwm_current_limit_active(cpu, generator) || (fault & PWM_CURRENT_LIMIT_MODE) == 0u)) {
        cpu->io.pwm_current_cycle &= (uint8_t)~bit;
    }
}

static void pwm_tick(Dspic33* cpu, uint8_t time_base, uint64_t cycle) {
    bool master_boundary;
    bool period_updated = false;
    uint8_t generator;
    uint16_t chop_period = (uint16_t)((raw_word(cpu, PWM_GLOBAL_BASE + 0x1au) & 0x03ffu) + 1u);
    master_boundary = pwm_advance_master_counter(cpu, time_base, cycle);
    if (time_base == 0u) {
        cpu->io.pwm_chop_counter = (uint16_t)((cpu->io.pwm_chop_counter + 1u) % chop_period);
    }
    if (master_boundary &&
        (raw_word(cpu, (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu))) & 0x0400u) ==
            0u) {
        uint16_t period_address = (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 4u : 0x12u));
        cpu->io.pwm_active_period[time_base] = raw_word(cpu, period_address);
        period_updated = (cpu->io.pwm_period_update & (uint8_t)(1u << time_base)) != 0u;
        cpu->io.pwm_period_update &= (uint8_t)~(1u << time_base);
    }
    if (cpu->io.pwm_master_counter[time_base] ==
        raw_word(cpu, (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 6u : 0x14u)))) {
        pwm_special_match(cpu, time_base);
    }
    cpu->io.pwm_batch_updating = true;
    for (generator = 0u; generator < pwm_generator_count(cpu); generator++) {
        uint16_t control = pwm_register(cpu, generator, 0u);
        bool independent = (control & PWM_INDEPENDENT_TIME_BASE) != 0u;
        bool boundary;
        uint8_t master = (control & 0x0008u) != 0u ? 1u : 0u;
        if (master != time_base || pwm_generator_pmd_disabled(cpu, generator)) {
            continue;
        }
        if (independent) {
            boundary = pwm_advance_independent_counter(cpu, generator, 0u);
            pwm_advance_independent_counter(cpu, generator, 1u);
            if (boundary) {
                pwm_cycle_boundary(cpu, generator, false);
            }
        } else {
            if (master_boundary) {
                pwm_cycle_boundary(cpu, generator, period_updated);
            }
            cpu->io.pwm_counter[generator][0] = pwm_shifted_counter(
                cpu->io.pwm_master_counter[master], cpu->io.pwm_active_phase[generator][0],
                cpu->io.pwm_active_period[master]);
            cpu->io.pwm_counter[generator][1] = pwm_shifted_counter(
                cpu->io.pwm_master_counter[master], cpu->io.pwm_active_phase[generator][1],
                cpu->io.pwm_active_period[master]);
        }
        if (cpu->io.pwm_counter[generator][0] == pwm_register(cpu, generator, 0x12u)) {
            pwm_generator_match(cpu, generator);
        }
    }
    for (generator = 0u; generator < pwm_generator_count(cpu); generator++) {
        uint16_t control = pwm_register(cpu, generator, 0u);
        uint8_t master = (control & 0x0008u) != 0u ? 1u : 0u;
        if (master != time_base || pwm_generator_pmd_disabled(cpu, generator)) {
            continue;
        }
        pwm_refresh_status(cpu, generator);
        pwm_update_output(cpu, generator);
    }
    cpu->io.pwm_batch_updating = false;
    comparator_evaluate_all(cpu);
    refresh_pwm_pins(cpu);
}

static void advance_pwm(Dspic33* cpu, uint64_t cycles) {
    uint8_t time_base;
    uint64_t subcycles;
    uint8_t generator;
    if (!pwm_power_enabled(cpu) || cycles == 0u) {
        return;
    }
    subcycles = cycles * 2u;
    for (generator = 0u; generator < pwm_generator_count(cpu); generator++) {
        if (pwm_generator_pmd_disabled(cpu, generator)) {
            continue;
        }
        if (cpu->io.pwm_leb_ticks[generator] > subcycles) {
            cpu->io.pwm_leb_ticks[generator] =
                (uint16_t)(cpu->io.pwm_leb_ticks[generator] - subcycles);
        } else {
            cpu->io.pwm_leb_ticks[generator] = 0u;
        }
    }
    for (time_base = 0u; time_base < 2u; time_base++) {
        uint16_t divider = pwm_divider(cpu, time_base);
        uint64_t previous_fraction = cpu->io.pwm_fraction[time_base];
        uint64_t accumulated = previous_fraction + subcycles;
        uint64_t ticks = accumulated / divider;
        uint64_t elapsed_subcycles = divider - previous_fraction;
        cpu->io.pwm_fraction[time_base] = (uint32_t)(accumulated % divider);
        while (ticks-- != 0u) {
            uint64_t cycle = cpu->device_cycles - cycles + (elapsed_subcycles + 1u) / 2u;
            pwm_tick(cpu, time_base, cycle);
            elapsed_subcycles += divider;
        }
    }
}

static void pwm_start(Dspic33* cpu) {
    uint8_t generator;
    memset(cpu->io.pwm_counter, 0, sizeof(cpu->io.pwm_counter));
    memset(cpu->io.pwm_cycle_count, 0, sizeof(cpu->io.pwm_cycle_count));
    memset(cpu->io.pwm_trigger_count, 0, sizeof(cpu->io.pwm_trigger_count));
    memset(cpu->io.pwm_special_count, 0, sizeof(cpu->io.pwm_special_count));
    memset(cpu->io.pwm_direction, 0, sizeof(cpu->io.pwm_direction));
    cpu->io.pwm_master_counter[0] = 0u;
    cpu->io.pwm_master_counter[1] = 0u;
    cpu->io.pwm_push_pull = 0u;
    memset(cpu->io.pwm_fraction, 0, sizeof(cpu->io.pwm_fraction));
    cpu->io.pwm_chop_counter = 0u;
    cpu->io.pwm_dead_time_sampled = cpu->io.pwm_dead_time_inputs;
    cpu->io.pwm_period_update = 0u;
    cpu->io.pwm_timing_update = 0u;
    pwm_latch_periods(cpu);
    for (generator = 0u; generator < pwm_generator_count(cpu); generator++) {
        uint8_t bit = (uint8_t)(1u << generator);
        uint16_t fault = pwm_register(cpu, generator, 4u);
        if (pwm_generator_pmd_disabled(cpu, generator)) {
            continue;
        }
        if (!pwm_fault_active(cpu, generator)) {
            cpu->io.pwm_fault_cycle &= (uint8_t)~bit;
            if ((cpu->io.pwm_fault_release & bit) != 0u) {
                cpu->io.pwm_fault_latched &= (uint8_t)~bit;
                cpu->io.pwm_fault_release &= (uint8_t)~bit;
            }
        }
        if (!pwm_current_limit_active(cpu, generator) || (fault & PWM_CURRENT_LIMIT_MODE) == 0u) {
            cpu->io.pwm_current_cycle &= (uint8_t)~bit;
        }
        cpu->io.pwm_active_io[generator] = pwm_register(cpu, generator, 2u);
        pwm_latch_generator(cpu, generator);
        if ((pwm_register(cpu, generator, 0u) & PWM_INDEPENDENT_TIME_BASE) == 0u) {
            uint8_t master = (pwm_register(cpu, generator, 0u) & 0x0008u) != 0u ? 1u : 0u;
            cpu->io.pwm_counter[generator][0] = pwm_shifted_counter(
                0u, cpu->io.pwm_active_phase[generator][0], cpu->io.pwm_active_period[master]);
            cpu->io.pwm_counter[generator][1] = pwm_shifted_counter(
                0u, cpu->io.pwm_active_phase[generator][1], cpu->io.pwm_active_period[master]);
        }
        pwm_refresh_status(cpu, generator);
    }
    cpu->io.pwm_batch_updating = true;
    for (generator = 0u; generator < pwm_generator_count(cpu); generator++) {
        if (pwm_generator_pmd_disabled(cpu, generator)) {
            continue;
        }
        pwm_update_output(cpu, generator);
        if (pwm_register(cpu, generator, 0x12u) == 0u) {
            pwm_generator_match(cpu, generator);
        }
    }
    cpu->io.pwm_batch_updating = false;
    comparator_evaluate_all(cpu);
    if (raw_word(cpu, PWM_GLOBAL_BASE + 6u) == 0u) {
        pwm_special_match(cpu, 0u);
    }
    if (raw_word(cpu, PWM_GLOBAL_BASE + 0x14u) == 0u) {
        pwm_special_match(cpu, 1u);
    }
    refresh_pwm_pins(cpu);
}

static void pwm_input_event(Dspic33* cpu, uint8_t source, bool high, bool current_limit) {
    uint32_t bit = (uint32_t)1u << source;
    uint8_t generator;
    uint32_t* inputs =
        current_limit ? &cpu->io.pwm_current_limit_inputs : &cpu->io.pwm_fault_inputs;
    if (high) {
        *inputs |= bit;
    } else {
        *inputs &= ~bit;
    }
    if (!current_limit && (source == 1u || source == 3u)) {
        comparator_evaluate_all(cpu);
    }
    for (generator = 0u; generator < pwm_generator_count(cpu); generator++) {
        uint16_t base = pwm_generator_base(generator);
        uint16_t control = raw_word(cpu, base);
        uint16_t fault = raw_word(cpu, (uint16_t)(base + 4u));
        uint8_t selected =
            current_limit ? (uint8_t)((fault >> 10u) & 0x1fu) : (uint8_t)((fault >> 3u) & 0x1fu);
        bool active = current_limit ? pwm_current_limit_active(cpu, generator)
                                    : pwm_fault_active(cpu, generator);
        uint8_t generator_bit = (uint8_t)(1u << generator);
        if (pwm_generator_pmd_disabled(cpu, generator) || selected != source ||
            pwm_state_blanked(cpu, generator, current_limit)) {
            continue;
        }
        cpu->io.pwm_timing_update |= generator_bit;
        if (current_limit) {
            if (active) {
                if ((fault & PWM_CURRENT_LIMIT_MODE) != 0u) {
                    cpu->io.pwm_current_cycle |= generator_bit;
                }
                raw_write_word(cpu, (uint16_t)(base + 0x18u), cpu->io.pwm_counter[generator][0]);
                if ((control & PWM_CURRENT_LIMIT_INTERRUPT) != 0u) {
                    raw_write_word(cpu, base, (uint16_t)(control | PWM_CURRENT_LIMIT_STATUS));
                    dspic33_raise_interrupt(cpu, pwm_irqs[generator]);
                }
                if ((control & (PWM_INDEPENDENT_TIME_BASE | PWM_EXTERNAL_RESET)) ==
                        (PWM_INDEPENDENT_TIME_BASE | PWM_EXTERNAL_RESET) &&
                    (fault & PWM_CURRENT_LIMIT_MODE) == 0u) {
                    cpu->io.pwm_counter[generator][0] = 0u;
                }
            }
        } else if (active) {
            if ((fault & PWM_FAULT_MODE_MASK) == 0u) {
                cpu->io.pwm_fault_latched |= generator_bit;
            } else if ((fault & PWM_FAULT_MODE_MASK) == PWM_FAULT_CYCLE) {
                cpu->io.pwm_fault_cycle |= generator_bit;
            }
            if ((control & PWM_FAULT_INTERRUPT) != 0u) {
                raw_write_word(cpu, base, (uint16_t)(control | PWM_FAULT_STATUS));
                dspic33_raise_interrupt(cpu, pwm_irqs[generator]);
            }
        } else {
            cpu->io.pwm_fault_release |= generator_bit;
        }
        pwm_refresh_status(cpu, generator);
        pwm_update_output(cpu, generator);
    }
    refresh_pwm_pins(cpu);
}

static void pwm_sync_event(Dspic33* cpu, uint8_t input, bool high) {
    uint8_t bit = (uint8_t)(1u << input);
    bool previous = (cpu->io.pwm_sync_inputs & bit) != 0u;
    uint8_t time_base;
    if (high) {
        cpu->io.pwm_sync_inputs |= bit;
    } else {
        cpu->io.pwm_sync_inputs &= (uint8_t)~bit;
    }
    for (time_base = 0u; time_base < 2u; time_base++) {
        uint16_t address = (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu));
        uint16_t control = raw_word(cpu, address);
        bool falling = (control & 0x0200u) != 0u;
        uint8_t selected = (uint8_t)((control >> 4u) & 7u);
        bool edge = falling ? previous && !high : !previous && high;
        if ((control & 0x0080u) != 0u && selected == input && edge) {
            cpu->io.pwm_master_counter[time_base] = 0u;
        }
    }
}

static void pwm_dead_time_event(Dspic33* cpu, uint8_t generator, bool high) {
    uint8_t bit = (uint8_t)(1u << generator);
    if (high) {
        cpu->io.pwm_dead_time_inputs |= bit;
    } else {
        cpu->io.pwm_dead_time_inputs &= (uint8_t)~bit;
    }
    if (!pwm_power_enabled(cpu)) {
        cpu->io.pwm_dead_time_sampled = (uint8_t)((cpu->io.pwm_dead_time_sampled & ~bit) |
                                                  (cpu->io.pwm_dead_time_inputs & bit));
    } else {
        cpu->io.pwm_timing_update |= bit;
    }
}

static uint8_t pwm_pps_selection(const Dspic33* cpu, uint16_t address, uint8_t shift) {
    return (uint8_t)((raw_word(cpu, address) >> shift) & 0x007fu);
}

static bool pwm_pps_input_high(const Dspic33* cpu, uint8_t selection) {
    bool high;
    return pps_physical_input_high(cpu, selection, &high) && high;
}

static void pwm_refresh_input(Dspic33* cpu, uint8_t source, bool high, bool current_limit) {
    uint32_t bit = (uint32_t)1u << source;
    uint32_t inputs = current_limit ? cpu->io.pwm_current_limit_inputs : cpu->io.pwm_fault_inputs;
    uint32_t direct = current_limit ? cpu->io.pwm_current_limit_direct : cpu->io.pwm_fault_direct;
    if ((direct & bit) == 0u && ((inputs & bit) != 0u) != high) {
        pwm_input_event(cpu, source, high, current_limit);
    }
}

static void refresh_pwm_inputs(Dspic33* cpu) {
    static const uint16_t fault_addresses[7] = {0x06b8u, 0x06b8u, 0x06bau, 0x06bau,
                                                0x06f4u, 0x06f4u, 0x06f6u};
    static const uint8_t fault_shifts[7] = {0u, 8u, 0u, 8u, 0u, 8u, 0u};
    static const uint16_t dead_time_addresses[DSPIC33_PWM_MAX_COUNT] = {
        0x06ecu, 0x06eeu, 0x06eeu, 0x06f0u, 0x06f0u, 0x06f2u, 0x06f2u};
    static const uint8_t dead_time_shifts[DSPIC33_PWM_MAX_COUNT] = {8u, 0u, 8u, 0u, 8u, 0u, 8u};
    static const uint16_t sync_addresses[2] = {0x06eau, 0x06ecu};
    static const uint8_t sync_shifts[2] = {8u, 0u};
    uint8_t source;
    if (cpu->io.pwm_refreshing_inputs) {
        return;
    }
    cpu->io.pwm_refreshing_inputs = true;
    for (source = 0u; source < 7u; source++) {
        bool high = pwm_pps_input_high(
            cpu, pwm_pps_selection(cpu, fault_addresses[source], fault_shifts[source]));
        pwm_refresh_input(cpu, source, high, false);
        pwm_refresh_input(cpu, source, high, true);
    }
    for (source = 0u; source < DSPIC33_COMPARATOR_COUNT; source++) {
        bool high = (cpu->io.comparator.output_high & (uint8_t)(1u << source)) != 0u;
        pwm_refresh_input(cpu, (uint8_t)(8u + source), high, false);
        pwm_refresh_input(cpu, (uint8_t)(8u + source), high, true);
    }
    for (source = 0u; source < pwm_generator_count(cpu); source++) {
        uint8_t bit = (uint8_t)(1u << source);
        bool high = pwm_pps_input_high(
            cpu, pwm_pps_selection(cpu, dead_time_addresses[source], dead_time_shifts[source]));
        if ((cpu->io.pwm_dead_time_direct & bit) == 0u &&
            ((cpu->io.pwm_dead_time_inputs & bit) != 0u) != high) {
            pwm_dead_time_event(cpu, source, high);
        }
    }
    for (source = 0u; source < 2u; source++) {
        uint8_t bit = (uint8_t)(1u << source);
        bool high = pwm_pps_input_high(
            cpu, pwm_pps_selection(cpu, sync_addresses[source], sync_shifts[source]));
        if ((cpu->io.pwm_sync_direct & bit) == 0u &&
            ((cpu->io.pwm_sync_inputs & bit) != 0u) != high) {
            pwm_sync_event(cpu, source, high);
        }
    }
    cpu->io.pwm_refreshing_inputs = false;
}

static void refresh_pwm_pins(Dspic33* cpu) { refresh_physical_pin_inputs(cpu); }

static void run_pwm_pmd(Dspic33* cpu, uint16_t source, uint32_t value) {
    uint16_t generation;
    uint8_t bit;
    uint8_t generator;
    if (source > pwm_generator_count(cpu)) {
        return;
    }
    generation = (uint16_t)(value >> PWM_PMD_EVENT_GENERATION_SHIFT);
    if (generation != cpu->io.pwm_pmd_generation[source]) {
        return;
    }
    bit = (uint8_t)(1u << source);
    if ((value & PWM_PMD_EVENT_DISABLED) != 0u) {
        cpu->io.pwm_pmd_disabled |= bit;
    } else {
        cpu->io.pwm_pmd_disabled &= (uint8_t)~bit;
    }
    if ((raw_word(cpu, PWM_GLOBAL_BASE) & PWM_ENABLE) != 0u) {
        cpu->io.pwm_batch_updating = true;
        for (generator = 0u; generator < pwm_generator_count(cpu); generator++) {
            if (!pwm_generator_pmd_disabled(cpu, generator)) {
                pwm_refresh_status(cpu, generator);
                pwm_update_output(cpu, generator);
            }
        }
        cpu->io.pwm_batch_updating = false;
        comparator_evaluate_all(cpu);
    }
    refresh_pwm_pins(cpu);
}

static bool pwm_pin_value(const Dspic33* cpu, uint8_t port, uint8_t pin, bool* high) {
    uint8_t generator;
    bool high_output;
    uint16_t io;
    uint16_t control = raw_word(cpu, PWM_GLOBAL_BASE);
    if (port == 4u && pin < 8u) {
        generator = (uint8_t)(pin / 2u);
        high_output = (pin & 1u) != 0u;
    } else if (port == 2u && pin >= 1u && pin <= 4u) {
        generator = (uint8_t)(4u + (pin - 1u) / 2u);
        high_output = (pin & 1u) == 0u;
    } else {
        return false;
    }
    io = pwm_register(cpu, generator, 2u);
    if ((control & PWM_ENABLE) == 0u || pwm_generator_pmd_disabled(cpu, generator) ||
        (cpu->power_state == DSPIC33_POWER_IDLE && (control & PWM_STOP_IDLE) != 0u) ||
        (io & (high_output ? PWM_PIN_HIGH : PWM_PIN_LOW)) == 0u) {
        return false;
    }
    *high = cpu->io.pwm[generator * 2u + (high_output ? 0u : 1u)] != 0u;
    return true;
}
