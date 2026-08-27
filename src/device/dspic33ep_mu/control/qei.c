#include "device/dspic33ep_mu/internal.h"

static bool qei_register(uint16_t address, uint8_t* channel, uint16_t* register_offset) {
    const uint16_t aligned_address = (uint16_t)(address & 0xfffeu);
    for (uint8_t qei_index = 0u; qei_index < DSPIC33_QEI_COUNT; qei_index++) {
        if (aligned_address >= dspic33_device_qei_bases[qei_index] &&
            aligned_address <= dspic33_device_qei_bases[qei_index] + QEI_LESS_EQUAL_HIGH) {
            *channel = qei_index;
            *register_offset = (uint16_t)(aligned_address - dspic33_device_qei_bases[qei_index]);
            return true;
        }
    }
    return false;
}

static uint32_t qei_read_counter(const Dspic33* cpu, uint8_t channel, uint16_t counter_low_offset) {
    const uint16_t qei_base = dspic33_device_qei_bases[channel];
    return (uint32_t)dspic33_device_internal_raw_word(cpu,
                                                      (uint16_t)(qei_base + counter_low_offset)) |
           ((uint32_t)dspic33_device_internal_raw_word(
                cpu, (uint16_t)(qei_base + counter_low_offset + 2u))
            << 16u);
}

static void qei_write_counter(Dspic33* cpu, uint8_t channel, uint16_t counter_low_offset,
                              uint32_t counter_value) {
    const uint16_t qei_base = dspic33_device_qei_bases[channel];
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(qei_base + counter_low_offset),
                                           (uint16_t)counter_value);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(qei_base + counter_low_offset + 2u),
                                           (uint16_t)(counter_value >> 16u));
}

static uint64_t qei_divider(uint16_t control_word) {
    static const uint16_t divider_values[8] = {1u, 2u, 4u, 8u, 16u, 32u, 64u, 256u};

    return divider_values[(control_word & QEI_CONTROL_DIVIDER_MASK) >> QEI_CONTROL_DIVIDER_SHIFT];
}

static uint64_t qei_filter_divider(uint16_t io_control) {
    const uint8_t divider_selection =
        (uint8_t)((io_control & QEI_IO_FILTER_DIVIDER_MASK) >> QEI_IO_FILTER_DIVIDER_SHIFT);

    return divider_selection == 7u ? 256u : 1ull << divider_selection;
}

static bool qei_filter_clock_enabled(const Dspic33* cpu, uint8_t channel) {
    const uint16_t control_word =
        dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]);

    if (cpu->io.qei.pmd_disabled[channel] || cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE || (control_word & QEI_CONTROL_STOP_IDLE) == 0u;
}

static bool qei_clock_enabled(const Dspic33* cpu, uint8_t channel) {
    return qei_filter_clock_enabled(cpu, channel) &&
           (dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]) &
            QEI_CONTROL_ENABLE) != 0u;
}

static void qei_raise_status(Dspic33* cpu, uint8_t channel, uint16_t flag) {
    const uint16_t status_address = (uint16_t)(dspic33_device_qei_bases[channel] + 4u);
    const uint16_t status_word =
        (uint16_t)(dspic33_device_internal_raw_word(cpu, status_address) | flag);

    dspic33_device_internal_raw_write_word(cpu, status_address, status_word);
    if ((status_word & (flag >> 1u)) != 0u) {
        dspic33_raise_interrupt(cpu, dspic33_device_qei_irqs[channel]);
    }
}

static void qei_refresh_interrupt(Dspic33* cpu, uint8_t channel) {
    const uint16_t status_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 4u));
    if (((status_word & QEI_STATUS_FLAG_MASK) >> 1u) & status_word & QEI_STATUS_ENABLE_MASK) {
        dspic33_raise_interrupt(cpu, dspic33_device_qei_irqs[channel]);
    }
}

static void qei_refresh_comparisons(Dspic33* cpu, uint8_t channel) {
    const int32_t position_value = (int32_t)qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    const int32_t high_compare_value =
        (int32_t)qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
    const int32_t low_compare_value = (int32_t)qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW);

    if (!qei_clock_enabled(cpu, channel)) {
        return;
    }
    if (position_value >= high_compare_value) {
        qei_raise_status(cpu, channel, QEI_STATUS_HIGH_COMPARE);
    }
    if (position_value <= low_compare_value) {
        qei_raise_status(cpu, channel, QEI_STATUS_LOW_COMPARE);
    }
}

static int8_t qei_current_direction(const Dspic33* cpu, uint8_t channel) {
    if (cpu->io.qei.direction[channel] != 0) {
        return cpu->io.qei.direction[channel];
    }
    return (dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]) &
            QEI_CONTROL_DIRECTION_INVERT) != 0u
               ? -1
               : 1;
}

static void qei_update_position(Dspic33* cpu, uint8_t channel, int8_t direction) {
    const uint16_t control_word =
        dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]);
    uint8_t position_mode = (uint8_t)((control_word & QEI_CONTROL_POSITION_MODE_MASK) >>
                                      QEI_CONTROL_POSITION_MODE_SHIFT);
    uint32_t position_value = qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    const uint32_t high_compare_value = qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
    const uint32_t low_compare_value = qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW);
    uint32_t lower_position = low_compare_value;
    uint32_t upper_position = high_compare_value;

    if ((control_word & QEI_CONTROL_COUNT_MODE_MASK) >= 2u) {
        position_mode = 0u;
    }
    if (position_mode == 6u && (control_word & QEI_CONTROL_DIRECTION_INVERT) != 0u) {
        lower_position = high_compare_value;
        upper_position = low_compare_value;
    }
    if (position_mode == 6u && direction > 0 && position_value == upper_position) {
        position_value = lower_position;
    } else if (position_mode == 6u && direction < 0 && position_value == lower_position) {
        position_value = upper_position;
    } else {
        if ((direction > 0 && position_value == 0x7fffffffu) ||
            (direction < 0 && position_value == 0x80000000u)) {
            qei_raise_status(cpu, channel, QEI_STATUS_POSITION_OVERFLOW);
        }
        position_value = direction > 0 ? position_value + 1u : position_value - 1u;
    }
    if (position_mode == 5u && position_value == high_compare_value) {
        position_value = 0u;
    }
    qei_write_counter(cpu, channel, QEI_POSITION_LOW, position_value);
    cpu->io.qei.direction[channel] = direction;
    qei_refresh_comparisons(cpu, channel);
}

static void qei_update_velocity(Dspic33* cpu, uint8_t channel, int8_t direction) {
    const uint16_t velocity_address = (uint16_t)(dspic33_device_qei_bases[channel] + QEI_VELOCITY);
    const uint16_t velocity_value = dspic33_device_internal_raw_word(cpu, velocity_address);

    if ((direction > 0 && velocity_value == 0x7fffu) ||
        (direction < 0 && velocity_value == 0x8000u)) {
        qei_raise_status(cpu, channel, QEI_STATUS_VELOCITY_OVERFLOW);
    }
    dspic33_device_internal_raw_write_word(cpu, velocity_address,
                                           direction > 0 ? (uint16_t)(velocity_value + 1u)
                                                         : (uint16_t)(velocity_value - 1u));
}

static void qei_update_index_counter(Dspic33* cpu, uint8_t channel, int8_t direction) {
    const uint32_t index_count = qei_read_counter(cpu, channel, QEI_INDEX_LOW);
    qei_write_counter(cpu, channel, QEI_INDEX_LOW,
                      direction > 0 ? index_count + 1u : index_count - 1u);
}

static void qei_update_interval(Dspic33* cpu, uint8_t channel, uint64_t ticks) {
    const uint32_t interval_count = qei_read_counter(cpu, channel, QEI_INTERVAL_LOW);
    qei_write_counter(cpu, channel, QEI_INTERVAL_LOW, interval_count + (uint32_t)ticks);
}

static void qei_capture_interval(Dspic33* cpu, uint8_t channel) {
    const uint16_t register_base = dspic33_device_qei_bases[channel];
    if (!cpu->io.qei.interval_armed[channel]) {
        cpu->io.qei.interval_armed[channel] = true;
        qei_write_counter(cpu, channel, QEI_INTERVAL_LOW, 0u);
        return;
    }
    if (!cpu->io.qei.interval_hold_locked[channel]) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(register_base + QEI_INTERVAL_HOLD_LOW),
            dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + QEI_INTERVAL_LOW)));
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(register_base + QEI_INTERVAL_HOLD_HIGH),
            dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + QEI_INTERVAL_HIGH)));
    }
    qei_write_counter(cpu, channel, QEI_INTERVAL_LOW, 0u);
}

static void qei_count_pulse(Dspic33* cpu, uint8_t channel, int8_t direction) {
    qei_update_position(cpu, channel, direction);
    qei_update_velocity(cpu, channel, direction);
    qei_capture_interval(cpu, channel);
}

static void qei_timer_pulse(Dspic33* cpu, uint8_t channel, int8_t direction, bool position_gate,
                            uint8_t logical_inputs) {
    if (position_gate) {
        qei_update_position(cpu, channel, direction);
    }
    if (position_gate) {
        qei_update_velocity(cpu, channel, direction);
    }
    if ((logical_inputs & 4u) != 0u) {
        qei_update_index_counter(cpu, channel, direction);
    }
    if ((logical_inputs & 8u) != 0u) {
        qei_update_interval(cpu, channel, 1u);
    }
}

static bool qei_ranges_intersect(uint32_t first_range_low, uint32_t first_range_high,
                                 uint32_t second_range_low, uint32_t second_range_high) {
    return first_range_low <= second_range_high && second_range_low <= first_range_high;
}

static bool qei_path_hits_range(uint32_t start_position, int8_t direction, uint64_t tick_count,
                                uint32_t range_low, uint32_t range_high) {
    if (tick_count > UINT32_MAX) {
        return true;
    }
    const uint32_t distance = (uint32_t)tick_count;
    const uint32_t end_position =
        direction > 0 ? start_position + distance : start_position - distance;

    if (direction > 0) {
        if (end_position > start_position) {
            return qei_ranges_intersect(start_position + 1u, end_position, range_low, range_high);
        }
        return (start_position != UINT32_MAX &&
                qei_ranges_intersect(start_position + 1u, UINT32_MAX, range_low, range_high)) ||
               qei_ranges_intersect(0u, end_position, range_low, range_high);
    }
    if (end_position < start_position) {
        return qei_ranges_intersect(end_position, start_position - 1u, range_low, range_high);
    }
    return (start_position != 0u &&
            qei_ranges_intersect(0u, start_position - 1u, range_low, range_high)) ||
           qei_ranges_intersect(end_position, UINT32_MAX, range_low, range_high);
}

static bool qei_path_crosses_value(uint32_t start_position, int8_t direction, uint64_t tick_count,
                                   uint32_t target_value) {
    return qei_path_hits_range(start_position, direction, tick_count, target_value, target_value);
}

static bool qei_path_crosses_word(uint16_t start_value, int8_t direction, uint64_t tick_count,
                                  uint16_t target_value) {
    if (tick_count > UINT16_MAX) {
        return true;
    }
    const uint16_t end_value = direction > 0 ? (uint16_t)(start_value + (uint16_t)tick_count)
                                             : (uint16_t)(start_value - (uint16_t)tick_count);

    if (direction > 0) {
        return end_value > start_value ? target_value > start_value && target_value <= end_value
                                       : target_value > start_value || target_value <= end_value;
    }
    return end_value < start_value ? target_value >= end_value && target_value < start_value
                                   : target_value < start_value || target_value >= end_value;
}

static void qei_advance_timer_ticks(Dspic33* cpu, uint8_t channel, int8_t direction,
                                    bool position_gate_enabled, uint8_t logical_inputs,
                                    uint64_t tick_count) {
    const uint16_t register_base = dspic33_device_qei_bases[channel];
    const uint32_t tick_delta = (uint32_t)tick_count;

    if (tick_count == 0u) {
        return;
    }
    if (position_gate_enabled) {
        const uint32_t position_value = qei_read_counter(cpu, channel, QEI_POSITION_LOW);
        const uint32_t position_key = position_value ^ 0x80000000u;
        const uint32_t high_compare_value =
            qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW) ^ 0x80000000u;
        const uint32_t low_compare_value =
            qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW) ^ 0x80000000u;
        const uint16_t velocity_value =
            dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + QEI_VELOCITY));

        if (qei_path_crosses_value(position_value, direction, tick_count,
                                   direction > 0 ? 0x80000000u : 0x7fffffffu)) {
            qei_raise_status(cpu, channel, QEI_STATUS_POSITION_OVERFLOW);
        }
        if (qei_path_hits_range(position_key, direction, tick_count, high_compare_value,
                                UINT32_MAX)) {
            qei_raise_status(cpu, channel, QEI_STATUS_HIGH_COMPARE);
        }
        if (qei_path_hits_range(position_key, direction, tick_count, 0u, low_compare_value)) {
            qei_raise_status(cpu, channel, QEI_STATUS_LOW_COMPARE);
        }
        if (qei_path_crosses_word(velocity_value, direction, tick_count,
                                  direction > 0 ? 0x8000u : 0x7fffu)) {
            qei_raise_status(cpu, channel, QEI_STATUS_VELOCITY_OVERFLOW);
        }
        qei_write_counter(cpu, channel, QEI_POSITION_LOW,
                          direction > 0 ? position_value + tick_delta
                                        : position_value - tick_delta);
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(register_base + QEI_VELOCITY),
                                               direction > 0
                                                   ? (uint16_t)(velocity_value + tick_delta)
                                                   : (uint16_t)(velocity_value - tick_delta));
        cpu->io.qei.direction[channel] = direction;
    }
    if ((logical_inputs & 4u) != 0u) {
        const uint32_t index_value = qei_read_counter(cpu, channel, QEI_INDEX_LOW);
        qei_write_counter(cpu, channel, QEI_INDEX_LOW,
                          direction > 0 ? index_value + tick_delta : index_value - tick_delta);
    }
    if ((logical_inputs & 8u) != 0u) {
        qei_update_interval(cpu, channel, tick_count);
    }
}

static uint8_t qei_logical_inputs(const Dspic33* cpu, uint8_t channel) {
    const uint16_t io_control_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
    uint8_t input_bits = cpu->io.qei.filtered_inputs[channel] & QEI_IO_INPUT_MASK;

    if ((io_control_word & QEI_IO_SWAP) != 0u) {
        input_bits =
            (uint8_t)((input_bits & 0x0cu) | ((input_bits & 1u) << 1u) | ((input_bits & 2u) >> 1u));
    }
    return (uint8_t)(input_bits ^ ((io_control_word & QEI_IO_POLARITY_MASK) >> 4u));
}

static void qei_index_event(Dspic33* cpu, uint8_t channel) {
    const uint16_t register_base = dspic33_device_qei_bases[channel];
    const uint16_t control_word = dspic33_device_internal_raw_word(cpu, register_base);
    uint8_t position_mode = (uint8_t)((control_word & QEI_CONTROL_POSITION_MODE_MASK) >>
                                      QEI_CONTROL_POSITION_MODE_SHIFT);
    const uint8_t count_mode = (uint8_t)(control_word & QEI_CONTROL_COUNT_MODE_MASK);

    if (count_mode >= 2u) {
        position_mode = 0u;
    }
    qei_raise_status(cpu, channel, QEI_STATUS_INDEX);
    if (count_mode < 2u) {
        if (count_mode == 0u && cpu->io.qei.direction[channel] > 0) {
            cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
            return;
        }
        qei_update_index_counter(cpu, channel, qei_current_direction(cpu, channel));
    }
    if (position_mode == 4u && cpu->io.qei.home_index_count[channel] == 1u) {
        cpu->io.qei.home_index_count[channel] = 2u;
    } else if (position_mode == 1u) {
        qei_write_counter(cpu, channel, QEI_POSITION_LOW, 0u);
    } else if (position_mode == 2u ||
               (position_mode == 3u && cpu->io.qei.home_index_count[channel] >= 1u) ||
               (position_mode == 4u && cpu->io.qei.home_index_count[channel] >= 2u)) {
        qei_write_counter(cpu, channel, QEI_POSITION_LOW,
                          qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW));
        dspic33_device_internal_raw_write_word(
            cpu, register_base, (uint16_t)(control_word & ~QEI_CONTROL_POSITION_MODE_MASK));
        if (position_mode == 3u || position_mode == 4u) {
            qei_raise_status(cpu, channel, QEI_STATUS_INITIALIZED);
        }
    }
    qei_refresh_comparisons(cpu, channel);
}

static void qei_apply_filtered_inputs(Dspic33* cpu, uint8_t channel) {
    static const int8_t actions[16] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
    const uint16_t control_word =
        dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]);
    const uint8_t index_match =
        (uint8_t)((control_word & QEI_CONTROL_INDEX_MATCH_MASK) >> QEI_CONTROL_INDEX_MATCH_SHIFT);
    const uint8_t previous_inputs = cpu->io.qei.logical_inputs[channel];
    const uint8_t logical_inputs = qei_logical_inputs(cpu, channel);
    const uint8_t count_mode = (uint8_t)(control_word & QEI_CONTROL_COUNT_MODE_MASK);

    cpu->io.qei.logical_inputs[channel] = logical_inputs;
    if ((logical_inputs & 4u) == 0u) {
        cpu->io.qei.index_latched[channel] = false;
    }
    if (!qei_clock_enabled(cpu, channel)) {
        return;
    }
    if (count_mode == 0u && (previous_inputs & 3u) != (logical_inputs & 3u)) {
        int8_t direction = actions[((previous_inputs & 3u) << 2u) | (logical_inputs & 3u)];
        if ((control_word & QEI_CONTROL_DIRECTION_INVERT) != 0u) {
            direction = (int8_t)-direction;
        }
        if (direction != 0) {
            qei_count_pulse(cpu, channel, direction);
        }
    } else if ((count_mode == 1u || count_mode == 2u) && (previous_inputs & 1u) == 0u &&
               (logical_inputs & 1u) != 0u) {
        const bool gate_allows =
            (control_word & QEI_CONTROL_GATE_ENABLE) == 0u || (logical_inputs & 2u) != 0u;
        if (count_mode == 1u) {
            int8_t direction = (logical_inputs & 2u) != 0u ? 1 : -1;
            if ((control_word & QEI_CONTROL_DIRECTION_INVERT) != 0u) {
                direction = (int8_t)-direction;
            }
            qei_count_pulse(cpu, channel, direction);
        } else {
            int8_t direction = (control_word & QEI_CONTROL_DIRECTION_INVERT) != 0u ? -1 : 1;
            qei_timer_pulse(cpu, channel, direction, gate_allows, logical_inputs);
        }
    }
    if ((previous_inputs & 8u) == 0u && (logical_inputs & 8u) != 0u) {
        cpu->io.qei.home_index_count[channel] = 1u;
        qei_raise_status(cpu, channel, QEI_STATUS_HOME);
        if ((dspic33_device_internal_raw_word(cpu,
                                              (uint16_t)(dspic33_device_qei_bases[channel] + 2u)) &
             QEI_IO_CAPTURE_HOME) != 0u) {
            qei_write_counter(cpu, channel, QEI_GREATER_EQUAL_LOW,
                              qei_read_counter(cpu, channel, QEI_POSITION_LOW));
        }
    }
    if (!cpu->io.qei.index_latched[channel] && (logical_inputs & 4u) != 0u &&
        (logical_inputs & 3u) == index_match) {
        cpu->io.qei.index_latched[channel] = true;
        qei_index_event(cpu, channel);
    }
}

static void qei_set_physical_input(Dspic33* cpu, uint8_t channel, uint8_t input_index,
                                   bool is_high) {
    const uint8_t input_mask = (uint8_t)(1u << input_index);
    uint8_t input_values = cpu->qei_inputs[channel];

    input_values =
        is_high ? (uint8_t)(input_values | input_mask) : (uint8_t)(input_values & ~input_mask);
    cpu->qei_inputs[channel] = input_values;
    const uint16_t io_control_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
    if ((io_control_word & QEI_IO_FILTER_ENABLE) == 0u && !cpu->io.qei.pmd_disabled[channel]) {
        cpu->io.qei.filtered_inputs[channel] = input_values;
        qei_apply_filtered_inputs(cpu, channel);
    }
}

void dspic33_device_internal_qei_set_physical_inputs(Dspic33* cpu, uint8_t channel,
                                                     uint8_t input_values) {
    const uint16_t io_control_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
    cpu->qei_inputs[channel] = input_values;
    if ((io_control_word & QEI_IO_FILTER_ENABLE) == 0u && !cpu->io.qei.pmd_disabled[channel]) {
        cpu->io.qei.filtered_inputs[channel] = input_values;
        qei_apply_filtered_inputs(cpu, channel);
    }
}

bool dspic33_device_internal_qei_compare_output_value(const Dspic33* cpu, uint8_t channel,
                                                      bool* is_high) {
    uint16_t io_control_word;
    uint8_t output_mode;
    int32_t position_value;
    int32_t high_compare_value;
    int32_t low_compare_value;

    if (channel >= DSPIC33_QEI_COUNT || is_high == NULL || cpu->io.qei.pmd_disabled[channel]) {
        return false;
    }
    io_control_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
    output_mode = (uint8_t)((io_control_word & QEI_IO_OUTPUT_MASK) >> QEI_IO_OUTPUT_SHIFT);
    position_value = (int32_t)qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    high_compare_value = (int32_t)qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
    low_compare_value = (int32_t)qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW);

    *is_high = (output_mode == 1u && position_value >= high_compare_value) ||
               (output_mode == 2u && position_value <= low_compare_value) ||
               (output_mode == 3u && position_value >= high_compare_value &&
                position_value <= low_compare_value);
    return true;
}

static bool qei_pps_output_mapped(const Dspic33* cpu, uint8_t channel) {
    const uint8_t output_function = (uint8_t)(47u + channel);
    for (size_t output_index = 0u;
         output_index < sizeof(dspic33_device_pps_outputs) / sizeof(dspic33_device_pps_outputs[0]);
         output_index++) {
        if (dspic33_device_internal_pps_output_function(
                cpu, dspic33_device_pps_outputs[output_index].pin) == output_function) {
            return true;
        }
    }
    return false;
}

static bool qei_output_high_at(uint8_t output_mode, uint32_t position_value,
                               uint32_t high_compare_value, uint32_t low_compare_value) {
    const int32_t signed_position = (int32_t)position_value;
    const int32_t signed_high_compare = (int32_t)high_compare_value;
    const int32_t signed_low_compare = (int32_t)low_compare_value;

    return (output_mode == 1u && signed_position >= signed_high_compare) ||
           (output_mode == 2u && signed_position <= signed_low_compare) ||
           (output_mode == 3u && signed_position >= signed_high_compare &&
            signed_position <= signed_low_compare);
}

static uint64_t qei_output_transition_ticks(const Dspic33* cpu, uint8_t channel, int8_t direction) {
    const uint16_t register_base = dspic33_device_qei_bases[channel];
    const uint8_t output_mode =
        (uint8_t)((dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 2u)) &
                   QEI_IO_OUTPUT_MASK) >>
                  QEI_IO_OUTPUT_SHIFT);
    const uint32_t position_value = qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    const uint32_t high_compare_value = qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
    const uint32_t low_compare_value = qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW);
    const uint32_t transition_candidates[] = {high_compare_value, high_compare_value - 1u,
                                              low_compare_value,  low_compare_value + 1u,
                                              0x80000000u,        0x7fffffffu};
    const bool is_currently_high =
        qei_output_high_at(output_mode, position_value, high_compare_value, low_compare_value);
    uint64_t transition_boundary = UINT64_MAX;

    for (size_t candidate_index = 0u;
         candidate_index < sizeof(transition_candidates) / sizeof(transition_candidates[0]);
         candidate_index++) {
        const uint32_t candidate_distance =
            direction > 0 ? transition_candidates[candidate_index] - position_value
                          : position_value - transition_candidates[candidate_index];
        const uint64_t candidate_ticks =
            candidate_distance == 0u ? UINT64_C(1) << 32u : candidate_distance;

        if (candidate_ticks < transition_boundary &&
            qei_output_high_at(output_mode, transition_candidates[candidate_index],
                               high_compare_value, low_compare_value) != is_currently_high) {
            transition_boundary = candidate_ticks;
        }
    }
    return transition_boundary;
}

uint64_t dspic33_device_internal_qei_boundary_cycles(const Dspic33* cpu, uint64_t maximum_cycles) {
    uint64_t earliest_boundary_cycles = maximum_cycles;

    for (uint8_t channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        const uint16_t register_base = dspic33_device_qei_bases[channel];
        const uint16_t control_word = dspic33_device_internal_raw_word(cpu, register_base);
        const uint16_t io_control_word =
            dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 2u));
        const uint8_t logical_inputs = cpu->io.qei.logical_inputs[channel];
        const bool gate_allows =
            (control_word & QEI_CONTROL_GATE_ENABLE) == 0u || (logical_inputs & 2u) != 0u;

        if (!qei_pps_output_mapped(cpu, channel) || (io_control_word & QEI_IO_OUTPUT_MASK) == 0u) {
            continue;
        }
        if ((io_control_word & QEI_IO_FILTER_ENABLE) != 0u &&
            qei_filter_clock_enabled(cpu, channel) &&
            cpu->io.qei.filtered_inputs[channel] != cpu->qei_inputs[channel]) {
            const uint64_t filter_cycles =
                qei_filter_divider(io_control_word) - cpu->io.qei.filter_fraction[channel];
            if (filter_cycles < earliest_boundary_cycles) {
                earliest_boundary_cycles = filter_cycles;
            }
        }
        if (qei_clock_enabled(cpu, channel) && (control_word & QEI_CONTROL_COUNT_MODE_MASK) == 3u &&
            gate_allows) {
            const uint64_t transition_ticks = qei_output_transition_ticks(
                cpu, channel, (control_word & QEI_CONTROL_DIRECTION_INVERT) != 0u ? -1 : 1);
            const uint64_t counter_divider = qei_divider(control_word);
            if (transition_ticks != UINT64_MAX &&
                transition_ticks <= UINT64_MAX / counter_divider) {
                const uint64_t transition_cycles =
                    transition_ticks * counter_divider - cpu->io.qei.counter_fraction[channel];
                if (transition_cycles < earliest_boundary_cycles) {
                    earliest_boundary_cycles = transition_cycles;
                }
            }
        }
    }
    return earliest_boundary_cycles;
}

bool dspic33_device_internal_qei_pps_output_value(const Dspic33* cpu, uint8_t port,
                                                  uint8_t port_bit, bool* is_high) {
    for (size_t pin_index = 0u;
         pin_index < sizeof(dspic33_device_pps_pins) / sizeof(dspic33_device_pps_pins[0]);
         pin_index++) {
        if (dspic33_device_pps_pins[pin_index].port == port &&
            dspic33_device_pps_pins[pin_index].bit == port_bit) {
            const uint8_t output_function = dspic33_device_internal_pps_output_function(
                cpu, dspic33_device_pps_pins[pin_index].pin);
            if (output_function == 47u || output_function == 48u) {
                const uint8_t channel = (uint8_t)(output_function - 47u);
                if ((dspic33_device_internal_raw_word(
                         cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u)) &
                     QEI_IO_OUTPUT_MASK) != 0u) {
                    return dspic33_device_internal_qei_compare_output_value(cpu, channel, is_high);
                }
            }
            return false;
        }
    }
    return false;
}

static void qei_filter_ticks(Dspic33* cpu, uint8_t channel, uint64_t tick_count) {
    bool inputs_changed = false;

    for (uint8_t input_index = 0u; input_index < 4u; input_index++) {
        const uint8_t input_mask = (uint8_t)(1u << input_index);
        const bool raw_level = (cpu->qei_inputs[channel] & input_mask) != 0u;
        const bool filtered_level = (cpu->io.qei.filtered_inputs[channel] & input_mask) != 0u;

        if (raw_level == filtered_level) {
            cpu->io.qei.filter_stability[channel][input_index] = 0u;
        } else {
            const uint64_t stability_ticks =
                cpu->io.qei.filter_stability[channel][input_index] + tick_count;
            if (stability_ticks >= 3u) {
                if (raw_level) {
                    cpu->io.qei.filtered_inputs[channel] |= input_mask;
                } else {
                    cpu->io.qei.filtered_inputs[channel] &= (uint8_t)~input_mask;
                }
                cpu->io.qei.filter_stability[channel][input_index] = 0u;
                inputs_changed = true;
            } else {
                cpu->io.qei.filter_stability[channel][input_index] = (uint8_t)stability_ticks;
            }
        }
    }
    if (inputs_changed) {
        qei_apply_filtered_inputs(cpu, channel);
    }
}

static uint64_t qei_accumulate_ticks(uint64_t* fractional_cycles, uint64_t cycle_count,
                                     uint64_t divider) {
    uint64_t tick_count = cycle_count / divider;
    const uint64_t remainder_cycles = cycle_count % divider;

    if (remainder_cycles != 0u && *fractional_cycles >= divider - remainder_cycles) {
        tick_count++;
        *fractional_cycles -= divider - remainder_cycles;
    } else {
        *fractional_cycles += remainder_cycles;
    }
    return tick_count;
}

static void qei_advance_counters(Dspic33* cpu, uint8_t channel, uint16_t control_word,
                                 uint64_t cycle_count) {
    const uint64_t tick_count = qei_accumulate_ticks(&cpu->io.qei.counter_fraction[channel],
                                                     cycle_count, qei_divider(control_word));

    if ((control_word & QEI_CONTROL_COUNT_MODE_MASK) == 3u) {
        const uint8_t logical_inputs = cpu->io.qei.logical_inputs[channel];
        const bool gate_allows =
            (control_word & QEI_CONTROL_GATE_ENABLE) == 0u || (logical_inputs & 2u) != 0u;
        const int8_t direction = (control_word & QEI_CONTROL_DIRECTION_INVERT) != 0u ? -1 : 1;

        qei_advance_timer_ticks(cpu, channel, direction, gate_allows, logical_inputs, tick_count);
    } else if ((control_word & QEI_CONTROL_COUNT_MODE_MASK) < 2u) {
        qei_update_interval(cpu, channel, tick_count);
    }
}

static void qei_advance_channel(Dspic33* cpu, uint8_t channel, uint64_t cycles) {
    const uint16_t control_word =
        dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]);
    const uint16_t io_control_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
    uint64_t filter_divider;
    uint64_t remaining_cycles;
    const bool are_counters_enabled = qei_clock_enabled(cpu, channel);

    if (!qei_filter_clock_enabled(cpu, channel) || cycles == 0u) {
        return;
    }
    if ((io_control_word & QEI_IO_FILTER_ENABLE) == 0u) {
        if (are_counters_enabled) {
            qei_advance_counters(cpu, channel, control_word, cycles);
        }
        return;
    }
    filter_divider = qei_filter_divider(io_control_word);
    remaining_cycles = cycles;
    while (remaining_cycles != 0u) {
        const uint64_t until_sample_cycles = filter_divider - cpu->io.qei.filter_fraction[channel];
        const uint64_t segment_cycles =
            remaining_cycles < until_sample_cycles ? remaining_cycles : until_sample_cycles;

        if (are_counters_enabled) {
            qei_advance_counters(cpu, channel, control_word, segment_cycles);
        }
        cpu->io.qei.filter_fraction[channel] += segment_cycles;
        remaining_cycles -= segment_cycles;
        if (cpu->io.qei.filter_fraction[channel] == filter_divider) {
            cpu->io.qei.filter_fraction[channel] = 0u;
            qei_filter_ticks(cpu, channel, 1u);
        }
        if (cpu->io.qei.filtered_inputs[channel] == cpu->qei_inputs[channel] &&
            remaining_cycles != 0u) {
            if (are_counters_enabled) {
                qei_advance_counters(cpu, channel, control_word, remaining_cycles);
            }
            qei_accumulate_ticks(&cpu->io.qei.filter_fraction[channel], remaining_cycles,
                                 filter_divider);
            memset(cpu->io.qei.filter_stability[channel], 0,
                   sizeof(cpu->io.qei.filter_stability[channel]));
            remaining_cycles = 0u;
        }
    }
}

void dspic33_device_internal_advance_qei(Dspic33* cpu, uint64_t cycles) {
    bool output_state_changed = false;

    for (uint8_t channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        bool output_was_high = false;
        const bool output_was_valid =
            qei_pps_output_mapped(cpu, channel) &&
            dspic33_device_internal_qei_compare_output_value(cpu, channel, &output_was_high);

        qei_advance_channel(cpu, channel, cycles);
        if (output_was_valid) {
            bool output_is_high = false;
            output_state_changed =
                output_state_changed ||
                (dspic33_device_internal_qei_compare_output_value(cpu, channel, &output_is_high) &&
                 output_was_high != output_is_high);
        }
    }
    if (output_state_changed) {
        dspic33_device_internal_refresh_physical_pin_inputs(cpu);
    }
}

void dspic33_device_internal_run_qei(Dspic33* cpu, uint16_t event_source, uint32_t event_value) {
    if (event_source >= QEI_PMD_EVENT_BASE) {
        const uint8_t channel = (uint8_t)(event_source - QEI_PMD_EVENT_BASE);
        const uint16_t event_generation = (uint16_t)(event_value >> 1u);

        if (channel < DSPIC33_QEI_COUNT &&
            event_generation == cpu->io.qei.pmd_generation[channel]) {
            cpu->io.qei.pmd_disabled[channel] = (event_value & 1u) != 0u;
            if (!cpu->io.qei.pmd_disabled[channel]) {
                const uint16_t io_control_word = dspic33_device_internal_raw_word(
                    cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
                if ((io_control_word & QEI_IO_FILTER_ENABLE) == 0u) {
                    const uint8_t index_match =
                        (uint8_t)((dspic33_device_internal_raw_word(
                                       cpu, dspic33_device_qei_bases[channel]) &
                                   QEI_CONTROL_INDEX_MATCH_MASK) >>
                                  QEI_CONTROL_INDEX_MATCH_SHIFT);
                    cpu->io.qei.filtered_inputs[channel] = cpu->qei_inputs[channel];
                    cpu->io.qei.logical_inputs[channel] = qei_logical_inputs(cpu, channel);
                    if ((cpu->io.qei.logical_inputs[channel] & 4u) == 0u) {
                        cpu->io.qei.index_latched[channel] = false;
                    } else if ((cpu->io.qei.logical_inputs[channel] & 3u) == index_match) {
                        cpu->io.qei.index_latched[channel] = true;
                    }
                }
            }
            if (qei_pps_output_mapped(cpu, channel)) {
                dspic33_device_internal_refresh_physical_pin_inputs(cpu);
            }
        }
        return;
    }
    if (event_source < DSPIC33_QEI_COUNT * 4u) {
        const uint8_t channel = (uint8_t)(event_source / 4u);
        bool output_was_high = false;
        const bool output_was_valid =
            qei_pps_output_mapped(cpu, channel) &&
            dspic33_device_internal_qei_compare_output_value(cpu, channel, &output_was_high);

        qei_set_physical_input(cpu, channel, (uint8_t)(event_source % 4u), event_value != 0u);
        if (output_was_valid) {
            bool output_is_high = false;
            if (dspic33_device_internal_qei_compare_output_value(cpu, channel, &output_is_high) &&
                output_was_high != output_is_high) {
                dspic33_device_internal_refresh_physical_pin_inputs(cpu);
            }
        }
    }
}

static void qei_update_pmd(Dspic33* cpu, uint16_t address, uint16_t previous_value) {
    static const uint16_t pmd_addresses[DSPIC33_QEI_COUNT] = {0x0760u, 0x0764u};
    static const uint16_t pmd_masks[DSPIC33_QEI_COUNT] = {0x0400u, 0x0020u};

    for (uint8_t channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        if (address != pmd_addresses[channel] ||
            ((previous_value ^ dspic33_device_internal_raw_word(cpu, address)) &
             pmd_masks[channel]) == 0u) {
            continue;
        }
        const bool is_disabled =
            (dspic33_device_internal_raw_word(cpu, address) & pmd_masks[channel]) != 0u;
        cpu->io.qei.pmd_generation[channel]++;
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_QEI, (uint16_t)(QEI_PMD_EVENT_BASE + channel),
                              ((uint32_t)cpu->io.qei.pmd_generation[channel] << 1u) |
                                  (is_disabled ? 1u : 0u),
                              dspic33_device_instruction_cycles(cpu, 1u))) {
            dspic33_device_internal_raw_write_word(cpu, address, previous_value);
            cpu->io.qei.pmd_generation[channel]++;
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
}

void dspic33_device_internal_update_qei_register(Dspic33* cpu, uint16_t address,
                                                 uint16_t previous_value,
                                                 uint16_t requested_value) {
    uint8_t channel;

    uint16_t register_offset;
    if (address == 0x0760u || address == 0x0764u) {
        qei_update_pmd(cpu, address, previous_value);
        return;
    }
    if (!qei_register(address, &channel, &register_offset)) {
        return;
    }
    if (cpu->io.qei.pmd_disabled[channel]) {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(address & 0xfffeu), previous_value);
        return;
    }
    if (register_offset == 4u) {
        uint16_t status_word = dspic33_device_internal_raw_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 4u));
        status_word = (uint16_t)((status_word & QEI_STATUS_ENABLE_MASK) |
                                 (previous_value & requested_value & QEI_STATUS_FLAG_MASK));
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 4u), status_word);
        qei_refresh_interrupt(cpu, channel);
    } else if (register_offset == 0u || register_offset == 2u) {
        memset(cpu->io.qei.filter_stability[channel], 0,
               sizeof(cpu->io.qei.filter_stability[channel]));
        cpu->io.qei.counter_fraction[channel] = 0u;
        cpu->io.qei.filter_fraction[channel] = 0u;
        if (register_offset == 2u && (dspic33_device_internal_raw_word(
                                          cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u)) &
                                      QEI_IO_FILTER_ENABLE) == 0u) {
            cpu->io.qei.filtered_inputs[channel] = cpu->qei_inputs[channel];
        }
        cpu->io.qei.logical_inputs[channel] = qei_logical_inputs(cpu, channel);
        if (register_offset == 0u) {
            qei_refresh_comparisons(cpu, channel);
        }
    } else if (register_offset == QEI_POSITION_LOW) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_POSITION_HIGH),
            dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_POSITION_HOLD)));
        qei_refresh_comparisons(cpu, channel);
    } else if (register_offset == QEI_POSITION_HIGH) {
        qei_refresh_comparisons(cpu, channel);
    } else if (register_offset == QEI_INDEX_LOW) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_INDEX_HIGH),
            dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_INDEX_HOLD)));
    } else if (register_offset == QEI_GREATER_EQUAL_LOW ||
               register_offset == QEI_GREATER_EQUAL_HIGH || register_offset == QEI_LESS_EQUAL_LOW ||
               register_offset == QEI_LESS_EQUAL_HIGH) {
        qei_refresh_comparisons(cpu, channel);
    }
}

static bool qei_read_complete(const Dspic33* cpu, uint16_t address) {
    return !cpu->io.cpu_read_valid || cpu->io.cpu_read_width == 1u ||
           address == cpu->io.cpu_read_address + 1u;
}

bool dspic33_device_internal_qei_read_register(Dspic33* cpu, uint16_t address,
                                               uint8_t* read_value) {
    uint8_t channel;
    uint16_t register_offset;

    if (!qei_register(address, &channel, &register_offset)) {
        return false;
    }
    if (cpu->io.qei.pmd_disabled[channel]) {
        *read_value = 0u;
        return true;
    }
    if (register_offset == 2u) {
        uint16_t io_control_word = dspic33_device_internal_raw_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
        io_control_word = (uint16_t)((io_control_word & ~QEI_IO_INPUT_MASK) |
                                     cpu->io.qei.logical_inputs[channel]);
        *read_value = (uint8_t)(io_control_word >> ((address & 1u) * 8u));
    }
    if (register_offset == QEI_POSITION_LOW && (address & 1u) == 0u) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_POSITION_HOLD),
            dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_POSITION_HIGH)));
    } else if (register_offset == QEI_INDEX_LOW && (address & 1u) == 0u) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_INDEX_HOLD),
            dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_INDEX_HIGH)));
    } else if (register_offset == QEI_INTERVAL_HOLD_LOW && (address & 1u) == 0u) {
        cpu->io.qei.interval_hold_locked[channel] = true;
    } else if (register_offset == QEI_INTERVAL_HOLD_HIGH && qei_read_complete(cpu, address)) {
        cpu->io.qei.interval_hold_locked[channel] = false;
    } else if (register_offset == QEI_VELOCITY && qei_read_complete(cpu, address)) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_VELOCITY), 0u);
    }
    return true;
}
