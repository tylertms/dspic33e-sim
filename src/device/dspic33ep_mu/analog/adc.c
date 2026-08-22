#include "device/dspic33ep_mu/internal.h"

uint16_t dspic33_device_internal_adc_register(const Dspic33* cpu, uint8_t module, uint16_t offset) {
    return dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_adc_controls[module] + offset));
}

static bool adc_12_bit(const Dspic33* cpu, uint8_t module) {
    return module == 0u &&
           (dspic33_device_internal_adc_register(cpu, module, 0u) & ADC_12_BIT) != 0u;
}

static bool adc_power_enabled(const Dspic33* cpu, uint8_t module) {
    uint16_t control = dspic33_device_internal_adc_register(cpu, module, 0u);
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
    return (dspic33_device_internal_adc_register(cpu, module, 4u) & 0x8000u) != 0u;
}

static uint8_t adc_channel_count(const Dspic33* cpu, uint8_t module) {
    uint16_t selection;
    if (adc_12_bit(cpu, module)) {
        return 1u;
    }
    selection = dspic33_device_internal_adc_register(cpu, module, 2u) & ADC_CHANNELS_MASK;
    if (selection == 0u) {
        return 1u;
    }
    return selection == 0x0100u ? 2u : 4u;
}

static uint8_t adc_next_scan_channel(Dspic33* cpu, uint8_t module) {
    uint8_t limit = module == 0u ? 32u : 16u;
    uint8_t offset;
    uint32_t selected = module == 0u
                            ? ((uint32_t)dspic33_device_internal_raw_word(cpu, 0x032eu) << 16u) |
                                  dspic33_device_internal_raw_word(cpu, 0x0330u)
                            : dspic33_device_internal_raw_word(cpu, 0x0370u);
    if (selected == 0u) {
        return (uint8_t)(dspic33_device_internal_adc_register(cpu, module, 8u) & 0x001fu);
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
        if (!mux_b && (dspic33_device_internal_adc_register(cpu, module, 2u) & ADC_SCAN) != 0u) {
            return adc_next_scan_channel(cpu, module);
        }
        channels = dspic33_device_internal_adc_register(cpu, module, 8u);
        return (uint8_t)((channels >> (mux_b ? 8u : 0u)) & 0x001fu);
    }
    channels = dspic33_device_internal_adc_register(cpu, module, 6u);
    return (uint8_t)(lane - 1u + (((channels >> (mux_b ? 8u : 0u)) & 1u) != 0u ? 3u : 0u));
}

static uint8_t adc_negative_channel(const Dspic33* cpu, uint8_t module, uint8_t lane, bool mux_b) {
    uint16_t channels;
    uint16_t negative;
    if (lane == 0u) {
        channels = dspic33_device_internal_adc_register(cpu, module, 8u);
        return (channels & (mux_b ? 0x8000u : 0x0080u)) != 0u ? 1u : UINT8_MAX;
    }
    channels = dspic33_device_internal_adc_register(cpu, module, 6u);
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
    if ((dspic33_device_internal_raw_word(cpu, dspic33_device_gpio_analog_addresses[port]) &
         mask) == 0u ||
        (cpu->io.adc_pmd_disabled & 1u) != 0u ||
        (channel < 16u && (cpu->io.adc_pmd_disabled & 2u) != 0u)) {
        return 0u;
    }
    if ((dspic33_device_internal_raw_word(cpu, dspic33_device_gpio_tris_addresses[port]) & mask) ==
        0u) {
        return (dspic33_device_internal_raw_word(cpu, dspic33_device_gpio_latch_addresses[port]) &
                mask) != 0u
                   ? 0x0fffu
                   : 0u;
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
    uint16_t format = dspic33_device_internal_adc_register(cpu, module, 0u) & ADC_FORMAT_MASK;
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
    uint16_t timing = dspic33_device_internal_adc_register(cpu, module, 4u);
    return (timing & 0x8000u) != 0u ? 1u : (uint64_t)(timing & 0x00ffu) + 1u;
}

static uint64_t adc_sample_cycles(const Dspic33* cpu, uint8_t module) {
    return ((dspic33_device_internal_adc_register(cpu, module, 4u) >> 8u) & 0x001fu) *
           adc_clock_cycles(cpu, module);
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
    uint8_t value =
        (uint8_t)((dspic33_device_internal_adc_register(cpu, module, 2u) >> 2u) & 0x001fu);
    uint16_t dma = module == 0u ? dspic33_device_internal_raw_word(cpu, 0x0332u)
                                : dspic33_device_internal_raw_word(cpu, 0x0372u);
    if (module != 0u || (dma & ADC_DMA_ENABLE) == 0u) {
        value &= 0x0fu;
    }
    return (uint8_t)(value + 1u);
}

static uint16_t adc_dma_address(Dspic33* cpu, uint8_t module, uint8_t channel) {
    uint16_t control = dspic33_device_internal_adc_register(cpu, module, 0u);
    uint16_t dma = module == 0u ? dspic33_device_internal_raw_word(cpu, 0x0332u)
                                : dspic33_device_internal_raw_word(cpu, 0x0372u);
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
    uint16_t control2 = dspic33_device_internal_adc_register(cpu, module, 2u);
    cpu->io.adc_sample_count[module] = 0u;
    cpu->io.adc_scan_index[module] = 0u;
    if ((control2 & ADC_BUFFER_SPLIT) != 0u) {
        control2 ^= ADC_SECOND_BUFFER;
        cpu->io.adc_buffer_index[module] = (control2 & ADC_SECOND_BUFFER) != 0u ? 8u : 0u;
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_adc_controls[module] + 2u), control2);
    } else {
        cpu->io.adc_buffer_index[module] = 0u;
    }
}
static void adc_store_result(Dspic33* cpu, uint8_t module, uint8_t channel, uint16_t result) {
    uint16_t dma = module == 0u ? dspic33_device_internal_raw_word(cpu, 0x0332u)
                                : dspic33_device_internal_raw_word(cpu, 0x0372u);
    uint16_t indirect = 0u;
    bool increment_boundary;
    if ((dma & ADC_DMA_ENABLE) != 0u) {
        indirect = adc_dma_address(cpu, module, channel);
        dspic33_device_internal_raw_write_word(cpu, dspic33_device_adc_buffers[module], result);
        dspic33_dma_request(cpu, dspic33_device_adc_irqs[module], indirect, 0u);
    } else {
        dspic33_device_internal_raw_write_word(
            cpu,
            (uint16_t)(dspic33_device_adc_buffers[module] +
                       (cpu->io.adc_buffer_index[module] & 0x0fu) * 2u),
            result);
    }
    cpu->io.adc_buffer_index[module] = (uint8_t)((cpu->io.adc_buffer_index[module] + 1u) & 0x0fu);
    cpu->io.adc_sample_count[module]++;
    increment_boundary = cpu->io.adc_sample_count[module] >= adc_increment_threshold(cpu, module);
    if (increment_boundary) {
        adc_increment_boundary(cpu, module);
    }
    if ((dma & ADC_DMA_ENABLE) != 0u || increment_boundary) {
        dspic33_raise_interrupt(cpu, dspic33_device_adc_irqs[module]);
        if (module == 0u) {
            dspic33_device_internal_output_compare_pulse_source(cpu, OUTPUT_COMPARE_SYNC_ADC1);
        }
    }
}

void dspic33_device_internal_adc_abort(Dspic33* cpu, uint8_t module) {
    uint16_t control = dspic33_device_internal_adc_register(cpu, module, 0u);
    cpu->io.adc_generation[module]++;
    cpu->io.adc_latched_count[module] = 0u;
    cpu->io.adc_conversion_index[module] = 0u;
    dspic33_device_internal_raw_write_word(cpu, dspic33_device_adc_controls[module],
                                           (uint16_t)(control & ~(ADC_SAMPLE | ADC_DONE)));
}

static bool adc_schedule(Dspic33* cpu, uint8_t module, uint8_t source, bool complete,
                         uint64_t delay) {
    if (dspic33_schedule(cpu, DSPIC33_EVENT_ADC, module,
                         adc_event_value(cpu, module, source, complete), delay)) {
        return true;
    }
    dspic33_device_internal_adc_abort(cpu, module);
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
    control = dspic33_device_internal_adc_register(cpu, module, 0u);
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
    dspic33_device_internal_raw_write_word(cpu, dspic33_device_adc_controls[module],
                                           source == 1u ? (uint16_t)(control & ~ADC_DONE)
                                                        : (uint16_t)(control | ADC_DONE));
    if (cpu->power_state == DSPIC33_POWER_SLEEP &&
        !dspic33_device_internal_interrupt_enabled(cpu, dspic33_device_adc_irqs[module])) {
        cpu->io.adc_sleep_disabled |= (uint8_t)(1u << module);
        return;
    }
    if ((control & ADC_AUTO_SAMPLE) != 0u) {
        dspic33_device_internal_adc_begin_sampling(cpu, module);
    }
}

void dspic33_device_internal_adc_start_conversion(Dspic33* cpu, uint8_t module) {
    uint16_t control = dspic33_device_internal_adc_register(cpu, module, 0u);
    uint16_t control2 = dspic33_device_internal_adc_register(cpu, module, 2u);
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
        (dspic33_device_internal_adc_register(cpu, module, 4u) & 0x8000u) == 0u &&
        ((dspic33_device_internal_adc_register(cpu, module, 4u) >> 8u) & 0x001fu) < 12u &&
        (dspic33_device_internal_adc_register(cpu, module, 4u) & 0x00ffu) == 2u && count >= 2u) {
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
    dspic33_device_internal_raw_write_word(cpu, dspic33_device_adc_controls[module],
                                           (uint16_t)(control & ~(ADC_SAMPLE | ADC_DONE)));
    if (module == 0u) {
        dspic33_device_internal_input_capture_pulse_source(cpu, INPUT_CAPTURE_SYNC_ADC1);
    }
    adc_schedule(cpu, module, (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u), true,
                 adc_conversion_cycles(cpu, module));
}

void dspic33_device_internal_adc_begin_sampling(Dspic33* cpu, uint8_t module) {
    uint16_t control = dspic33_device_internal_adc_register(cpu, module, 0u);
    uint8_t source;
    if ((control & ADC_ON) == 0u || !adc_power_enabled(cpu, module)) {
        return;
    }
    cpu->io.adc_generation[module]++;
    control |= ADC_SAMPLE;
    dspic33_device_internal_raw_write_word(cpu, dspic33_device_adc_controls[module], control);
    source = (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u);
    if (source == 7u) {
        adc_schedule(cpu, module, source, false, adc_sample_cycles(cpu, module));
    }
}

void dspic33_device_internal_adc_update_power_state(Dspic33* cpu) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        uint16_t control = dspic33_device_internal_adc_register(cpu, module, 0u);
        uint8_t bit = (uint8_t)(1u << module);
        if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
            cpu->io.adc_sleep_disabled &= (uint8_t)~bit;
            if ((control & (ADC_ON | ADC_AUTO_SAMPLE | ADC_SAMPLE)) == (ADC_ON | ADC_AUTO_SAMPLE) &&
                cpu->io.adc_latched_count[module] == 0u) {
                dspic33_device_internal_adc_begin_sampling(cpu, module);
            }
            continue;
        }
        if ((cpu->power_state == DSPIC33_POWER_IDLE && (control & ADC_STOP_IDLE) != 0u) ||
            (cpu->power_state == DSPIC33_POWER_SLEEP &&
             (dspic33_device_internal_adc_register(cpu, module, 4u) & 0x8000u) == 0u)) {
            if ((control & ADC_SAMPLE) != 0u || cpu->io.adc_latched_count[module] != 0u) {
                dspic33_device_internal_adc_abort(cpu, module);
            }
        }
    }
}

void dspic33_device_internal_run_adc(Dspic33* cpu, uint8_t module, uint32_t event_value) {
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
    if (((dspic33_device_internal_adc_register(cpu, module, 0u) & ADC_TRIGGER_MASK) >> 4u) ==
        source) {
        dspic33_device_internal_adc_start_conversion(cpu, module);
    }
}
