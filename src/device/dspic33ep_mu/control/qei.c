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

static uint32_t qei_read_counter(const Dspic33* cpu, uint8_t channel, uint16_t low_offset) {
    const uint16_t qei_base = dspic33_device_qei_bases[channel];
    return (uint32_t)dspic33_device_internal_raw_word(cpu, (uint16_t)(qei_base + low_offset)) |
           ((uint32_t)dspic33_device_internal_raw_word(cpu, (uint16_t)(qei_base + low_offset + 2u))
            << 16u);
}

static void qei_write_counter(Dspic33* cpu, uint8_t channel, uint16_t low_offset, uint32_t value) {
    const uint16_t qei_base = dspic33_device_qei_bases[channel];
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(qei_base + low_offset), (uint16_t)value);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(qei_base + low_offset + 2u),
                                           (uint16_t)(value >> 16u));
}

static uint64_t qei_divider(uint16_t control) {
    static const uint16_t divisors[8] = {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u};
    return divisors[(control & QEI_CONTROL_DIVIDER_MASK) >> QEI_CONTROL_DIVIDER_SHIFT];
}

static uint64_t qei_filter_divider(uint16_t io_control) {
    uint8_t selection =
        (uint8_t)((io_control & QEI_IO_FILTER_DIVIDER_MASK) >> QEI_IO_FILTER_DIVIDER_SHIFT);
    return selection == 7u ? 256u : 1ull << selection;
}

static bool qei_filter_clock_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]);
    if (cpu->io.qei.pmd_disabled[channel] || cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE || (control & QEI_CONTROL_STOP_IDLE) == 0u;
}

static bool qei_clock_enabled(const Dspic33* cpu, uint8_t channel) {
    return qei_filter_clock_enabled(cpu, channel) &&
           (dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]) &
            QEI_CONTROL_ENABLE) != 0u;
}

static void qei_raise_status(Dspic33* cpu, uint8_t channel, uint16_t flag) {
    uint16_t address = (uint16_t)(dspic33_device_qei_bases[channel] + 4u);
    uint16_t status = (uint16_t)(dspic33_device_internal_raw_word(cpu, address) | flag);
    dspic33_device_internal_raw_write_word(cpu, address, status);
    if ((status & (flag >> 1u)) != 0u) {
        dspic33_raise_interrupt(cpu, dspic33_device_qei_irqs[channel]);
    }
}

static void qei_refresh_interrupt(Dspic33* cpu, uint8_t channel) {
    uint16_t status =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 4u));
    if (((status & QEI_STATUS_FLAG_MASK) >> 1u) & status & QEI_STATUS_ENABLE_MASK) {
        dspic33_raise_interrupt(cpu, dspic33_device_qei_irqs[channel]);
    }
}

static void qei_refresh_comparisons(Dspic33* cpu, uint8_t channel) {
    int32_t position = (int32_t)qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    int32_t greater_equal = (int32_t)qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
    int32_t less_equal = (int32_t)qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW);
    if (!qei_clock_enabled(cpu, channel)) {
        return;
    }
    if (position >= greater_equal) {
        qei_raise_status(cpu, channel, QEI_STATUS_HIGH_COMPARE);
    }
    if (position <= less_equal) {
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
    uint16_t control = dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]);
    uint8_t mode =
        (uint8_t)((control & QEI_CONTROL_POSITION_MODE_MASK) >> QEI_CONTROL_POSITION_MODE_SHIFT);
    uint32_t position = qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    uint32_t greater_equal = qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
    uint32_t less_equal = qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW);
    uint32_t lower_position = less_equal;
    uint32_t upper_position = greater_equal;
    if ((control & QEI_CONTROL_COUNT_MODE_MASK) >= 2u) {
        mode = 0u;
    }
    if (mode == 6u && (control & QEI_CONTROL_DIRECTION_INVERT) != 0u) {
        lower_position = greater_equal;
        upper_position = less_equal;
    }
    if (mode == 6u && direction > 0 && position == upper_position) {
        position = lower_position;
    } else if (mode == 6u && direction < 0 && position == lower_position) {
        position = upper_position;
    } else {
        if ((direction > 0 && position == 0x7fffffffu) ||
            (direction < 0 && position == 0x80000000u)) {
            qei_raise_status(cpu, channel, QEI_STATUS_POSITION_OVERFLOW);
        }
        position = direction > 0 ? position + 1u : position - 1u;
    }
    if (mode == 5u && position == greater_equal) {
        position = 0u;
    }
    qei_write_counter(cpu, channel, QEI_POSITION_LOW, position);
    cpu->io.qei.direction[channel] = direction;
    qei_refresh_comparisons(cpu, channel);
}

static void qei_update_velocity(Dspic33* cpu, uint8_t channel, int8_t direction) {
    uint16_t address = (uint16_t)(dspic33_device_qei_bases[channel] + QEI_VELOCITY);
    uint16_t velocity = dspic33_device_internal_raw_word(cpu, address);
    if ((direction > 0 && velocity == 0x7fffu) || (direction < 0 && velocity == 0x8000u)) {
        qei_raise_status(cpu, channel, QEI_STATUS_VELOCITY_OVERFLOW);
    }
    dspic33_device_internal_raw_write_word(
        cpu, address, direction > 0 ? (uint16_t)(velocity + 1u) : (uint16_t)(velocity - 1u));
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
    const uint16_t qei_base = dspic33_device_qei_bases[channel];
    if (!cpu->io.qei.interval_armed[channel]) {
        cpu->io.qei.interval_armed[channel] = true;
        qei_write_counter(cpu, channel, QEI_INTERVAL_LOW, 0u);
        return;
    }
    if (!cpu->io.qei.interval_hold_locked[channel]) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(qei_base + QEI_INTERVAL_HOLD_LOW),
            dspic33_device_internal_raw_word(cpu, (uint16_t)(qei_base + QEI_INTERVAL_LOW)));
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(qei_base + QEI_INTERVAL_HOLD_HIGH),
            dspic33_device_internal_raw_word(cpu, (uint16_t)(qei_base + QEI_INTERVAL_HIGH)));
    }
    qei_write_counter(cpu, channel, QEI_INTERVAL_LOW, 0u);
}

static void qei_count_pulse(Dspic33* cpu, uint8_t channel, int8_t direction) {
    qei_update_position(cpu, channel, direction);
    qei_update_velocity(cpu, channel, direction);
    qei_capture_interval(cpu, channel);
}

static void qei_timer_pulse(Dspic33* cpu, uint8_t channel, int8_t direction, bool position_gate,
                            uint8_t logical) {
    if (position_gate) {
        qei_update_position(cpu, channel, direction);
    }
    if (position_gate) {
        qei_update_velocity(cpu, channel, direction);
    }
    if ((logical & 4u) != 0u) {
        qei_update_index_counter(cpu, channel, direction);
    }
    if ((logical & 8u) != 0u) {
        qei_update_interval(cpu, channel, 1u);
    }
}

static bool qei_ranges_intersect(uint32_t first_low, uint32_t first_high, uint32_t second_low,
                                 uint32_t second_high) {
    return first_low <= second_high && second_low <= first_high;
}

static bool qei_path_hits_range(uint32_t start, int8_t direction, uint64_t ticks, uint32_t low,
                                uint32_t high) {
    uint32_t distance;
    uint32_t end;
    if (ticks > UINT32_MAX) {
        return true;
    }
    distance = (uint32_t)ticks;
    end = direction > 0 ? start + distance : start - distance;
    if (direction > 0) {
        if (end > start) {
            return qei_ranges_intersect(start + 1u, end, low, high);
        }
        return (start != UINT32_MAX && qei_ranges_intersect(start + 1u, UINT32_MAX, low, high)) ||
               qei_ranges_intersect(0u, end, low, high);
    }
    if (end < start) {
        return qei_ranges_intersect(end, start - 1u, low, high);
    }
    return (start != 0u && qei_ranges_intersect(0u, start - 1u, low, high)) ||
           qei_ranges_intersect(end, UINT32_MAX, low, high);
}

static bool qei_path_crosses_value(uint32_t start, int8_t direction, uint64_t ticks,
                                   uint32_t value) {
    return qei_path_hits_range(start, direction, ticks, value, value);
}

static bool qei_path_crosses_word(uint16_t start, int8_t direction, uint64_t ticks,
                                  uint16_t value) {
    uint16_t end;
    if (ticks > UINT16_MAX) {
        return true;
    }
    end = direction > 0 ? (uint16_t)(start + (uint16_t)ticks) : (uint16_t)(start - (uint16_t)ticks);
    if (direction > 0) {
        return end > start ? value > start && value <= end : value > start || value <= end;
    }
    return end < start ? value >= end && value < start : value < start || value >= end;
}

static void qei_advance_timer_ticks(Dspic33* cpu, uint8_t channel, int8_t direction,
                                    bool position_gate, uint8_t logical, uint64_t ticks) {
    uint16_t base = dspic33_device_qei_bases[channel];
    uint32_t delta = (uint32_t)ticks;
    if (ticks == 0u) {
        return;
    }
    if (position_gate) {
        uint32_t position = qei_read_counter(cpu, channel, QEI_POSITION_LOW);
        uint32_t position_key = position ^ 0x80000000u;
        uint32_t greater_equal =
            qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW) ^ 0x80000000u;
        uint32_t less_equal = qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW) ^ 0x80000000u;
        uint16_t velocity = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + QEI_VELOCITY));
        if (qei_path_crosses_value(position, direction, ticks,
                                   direction > 0 ? 0x80000000u : 0x7fffffffu)) {
            qei_raise_status(cpu, channel, QEI_STATUS_POSITION_OVERFLOW);
        }
        if (qei_path_hits_range(position_key, direction, ticks, greater_equal, UINT32_MAX)) {
            qei_raise_status(cpu, channel, QEI_STATUS_HIGH_COMPARE);
        }
        if (qei_path_hits_range(position_key, direction, ticks, 0u, less_equal)) {
            qei_raise_status(cpu, channel, QEI_STATUS_LOW_COMPARE);
        }
        if (qei_path_crosses_word(velocity, direction, ticks, direction > 0 ? 0x8000u : 0x7fffu)) {
            qei_raise_status(cpu, channel, QEI_STATUS_VELOCITY_OVERFLOW);
        }
        qei_write_counter(cpu, channel, QEI_POSITION_LOW,
                          direction > 0 ? position + delta : position - delta);
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + QEI_VELOCITY),
                                               direction > 0 ? (uint16_t)(velocity + delta)
                                                             : (uint16_t)(velocity - delta));
        cpu->io.qei.direction[channel] = direction;
    }
    if ((logical & 4u) != 0u) {
        uint32_t index = qei_read_counter(cpu, channel, QEI_INDEX_LOW);
        qei_write_counter(cpu, channel, QEI_INDEX_LOW,
                          direction > 0 ? index + delta : index - delta);
    }
    if ((logical & 8u) != 0u) {
        qei_update_interval(cpu, channel, ticks);
    }
}

static uint8_t qei_logical_inputs(const Dspic33* cpu, uint8_t channel) {
    uint16_t io_control =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
    uint8_t inputs = cpu->io.qei.filtered_inputs[channel] & QEI_IO_INPUT_MASK;
    if ((io_control & QEI_IO_SWAP) != 0u) {
        inputs = (uint8_t)((inputs & 0x0cu) | ((inputs & 1u) << 1u) | ((inputs & 2u) >> 1u));
    }
    return (uint8_t)(inputs ^ ((io_control & QEI_IO_POLARITY_MASK) >> 4u));
}

static void qei_index_event(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_device_qei_bases[channel];
    uint16_t control = dspic33_device_internal_raw_word(cpu, base);
    uint8_t mode =
        (uint8_t)((control & QEI_CONTROL_POSITION_MODE_MASK) >> QEI_CONTROL_POSITION_MODE_SHIFT);
    uint8_t count_mode = (uint8_t)(control & QEI_CONTROL_COUNT_MODE_MASK);
    if (count_mode >= 2u) {
        mode = 0u;
    }
    qei_raise_status(cpu, channel, QEI_STATUS_INDEX);
    if (count_mode < 2u) {
        if (count_mode == 0u && cpu->io.qei.direction[channel] > 0) {
            cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
            return;
        }
        qei_update_index_counter(cpu, channel, qei_current_direction(cpu, channel));
    }
    if (mode == 4u && cpu->io.qei.home_index_count[channel] == 1u) {
        cpu->io.qei.home_index_count[channel] = 2u;
    } else if (mode == 1u) {
        qei_write_counter(cpu, channel, QEI_POSITION_LOW, 0u);
    } else if (mode == 2u || (mode == 3u && cpu->io.qei.home_index_count[channel] >= 1u) ||
               (mode == 4u && cpu->io.qei.home_index_count[channel] >= 2u)) {
        qei_write_counter(cpu, channel, QEI_POSITION_LOW,
                          qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW));
        dspic33_device_internal_raw_write_word(
            cpu, base, (uint16_t)(control & ~QEI_CONTROL_POSITION_MODE_MASK));
        if (mode == 3u || mode == 4u) {
            qei_raise_status(cpu, channel, QEI_STATUS_INITIALIZED);
        }
    }
    qei_refresh_comparisons(cpu, channel);
}

static void qei_apply_filtered_inputs(Dspic33* cpu, uint8_t channel) {
    static const int8_t actions[16] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
    uint16_t control = dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]);
    uint8_t index_match =
        (uint8_t)((control & QEI_CONTROL_INDEX_MATCH_MASK) >> QEI_CONTROL_INDEX_MATCH_SHIFT);
    const uint8_t previous_inputs = cpu->io.qei.logical_inputs[channel];
    const uint8_t logical_inputs = qei_logical_inputs(cpu, channel);
    uint8_t mode = (uint8_t)(control & QEI_CONTROL_COUNT_MODE_MASK);
    cpu->io.qei.logical_inputs[channel] = logical_inputs;
    if ((logical_inputs & 4u) == 0u) {
        cpu->io.qei.index_latched[channel] = false;
    }
    if (!qei_clock_enabled(cpu, channel)) {
        return;
    }
    if (mode == 0u && (previous_inputs & 3u) != (logical_inputs & 3u)) {
        int8_t direction = actions[((previous_inputs & 3u) << 2u) | (logical_inputs & 3u)];
        if ((control & QEI_CONTROL_DIRECTION_INVERT) != 0u) {
            direction = (int8_t)-direction;
        }
        if (direction != 0) {
            qei_count_pulse(cpu, channel, direction);
        }
    } else if ((mode == 1u || mode == 2u) && (previous_inputs & 1u) == 0u &&
               (logical_inputs & 1u) != 0u) {
        const bool gate_allows =
            (control & QEI_CONTROL_GATE_ENABLE) == 0u || (logical_inputs & 2u) != 0u;
        if (mode == 1u) {
            int8_t direction = (logical_inputs & 2u) != 0u ? 1 : -1;
            if ((control & QEI_CONTROL_DIRECTION_INVERT) != 0u) {
                direction = (int8_t)-direction;
            }
            qei_count_pulse(cpu, channel, direction);
        } else {
            int8_t direction = (control & QEI_CONTROL_DIRECTION_INVERT) != 0u ? -1 : 1;
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

static void qei_set_physical_input(Dspic33* cpu, uint8_t channel, uint8_t input, bool high) {
    uint8_t bit = (uint8_t)(1u << input);
    uint8_t values = cpu->qei_inputs[channel];
    values = high ? (uint8_t)(values | bit) : (uint8_t)(values & ~bit);
    cpu->qei_inputs[channel] = values;
    uint16_t io_control =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
    if ((io_control & QEI_IO_FILTER_ENABLE) == 0u && !cpu->io.qei.pmd_disabled[channel]) {
        cpu->io.qei.filtered_inputs[channel] = values;
        qei_apply_filtered_inputs(cpu, channel);
    }
}

void dspic33_device_internal_qei_set_physical_inputs(Dspic33* cpu, uint8_t channel,
                                                     uint8_t values) {
    uint16_t io_control =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
    cpu->qei_inputs[channel] = values;
    if ((io_control & QEI_IO_FILTER_ENABLE) == 0u && !cpu->io.qei.pmd_disabled[channel]) {
        cpu->io.qei.filtered_inputs[channel] = values;
        qei_apply_filtered_inputs(cpu, channel);
    }
}

bool dspic33_device_internal_qei_compare_output_value(const Dspic33* cpu, uint8_t channel,
                                                      bool* high) {
    uint16_t io_control;
    uint8_t mode;
    int32_t position;
    int32_t greater_equal;
    int32_t less_equal;
    if (channel >= DSPIC33_QEI_COUNT || high == NULL || cpu->io.qei.pmd_disabled[channel]) {
        return false;
    }
    io_control =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
    mode = (uint8_t)((io_control & QEI_IO_OUTPUT_MASK) >> QEI_IO_OUTPUT_SHIFT);
    position = (int32_t)qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    greater_equal = (int32_t)qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
    less_equal = (int32_t)qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW);
    *high = (mode == 1u && position >= greater_equal) || (mode == 2u && position <= less_equal) ||
            (mode == 3u && (position >= greater_equal || position <= less_equal));
    return true;
}

static bool qei_pps_output_mapped(const Dspic33* cpu, uint8_t channel) {
    uint8_t function = (uint8_t)(47u + channel);
    for (size_t index = 0u;
         index < sizeof(dspic33_device_pps_outputs) / sizeof(dspic33_device_pps_outputs[0]);
         index++) {
        if (dspic33_device_internal_pps_output_function(
                cpu, dspic33_device_pps_outputs[index].pin) == function) {
            return true;
        }
    }
    return false;
}

static bool qei_output_high_at(uint8_t mode, uint32_t position, uint32_t greater_equal,
                               uint32_t less_equal) {
    int32_t signed_position = (int32_t)position;
    int32_t signed_greater_equal = (int32_t)greater_equal;
    int32_t signed_less_equal = (int32_t)less_equal;
    return (mode == 1u && signed_position >= signed_greater_equal) ||
           (mode == 2u && signed_position <= signed_less_equal) ||
           (mode == 3u &&
            (signed_position >= signed_greater_equal || signed_position <= signed_less_equal));
}

static uint64_t qei_output_transition_ticks(const Dspic33* cpu, uint8_t channel, int8_t direction) {
    uint16_t base = dspic33_device_qei_bases[channel];
    uint8_t mode = (uint8_t)((dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) &
                              QEI_IO_OUTPUT_MASK) >>
                             QEI_IO_OUTPUT_SHIFT);
    uint32_t position = qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    uint32_t greater_equal = qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
    uint32_t less_equal = qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW);
    uint32_t candidates[] = {greater_equal,   greater_equal - 1u, less_equal,
                             less_equal + 1u, 0x80000000u,        0x7fffffffu};
    bool current = qei_output_high_at(mode, position, greater_equal, less_equal);
    uint64_t boundary = UINT64_MAX;
    for (size_t index = 0u; index < sizeof(candidates) / sizeof(candidates[0]); index++) {
        uint32_t distance =
            direction > 0 ? candidates[index] - position : position - candidates[index];
        uint64_t ticks = distance == 0u ? UINT64_C(1) << 32u : distance;
        if (ticks < boundary &&
            qei_output_high_at(mode, candidates[index], greater_equal, less_equal) != current) {
            boundary = ticks;
        }
    }
    return boundary;
}

uint64_t dspic33_device_internal_qei_boundary_cycles(const Dspic33* cpu, uint64_t limit) {
    uint64_t boundary = limit;
    for (uint8_t channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = dspic33_device_qei_bases[channel];
        uint16_t control = dspic33_device_internal_raw_word(cpu, base);
        uint16_t io_control = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
        uint8_t logical = cpu->io.qei.logical_inputs[channel];
        bool gate_allows = (control & QEI_CONTROL_GATE_ENABLE) == 0u || (logical & 2u) != 0u;
        if (!qei_pps_output_mapped(cpu, channel) || (io_control & QEI_IO_OUTPUT_MASK) == 0u) {
            continue;
        }
        if ((io_control & QEI_IO_FILTER_ENABLE) != 0u && qei_filter_clock_enabled(cpu, channel) &&
            cpu->io.qei.filtered_inputs[channel] != cpu->qei_inputs[channel]) {
            uint64_t filter_cycles =
                qei_filter_divider(io_control) - cpu->io.qei.filter_fraction[channel];
            if (filter_cycles < boundary) {
                boundary = filter_cycles;
            }
        }
        if (qei_clock_enabled(cpu, channel) && (control & QEI_CONTROL_COUNT_MODE_MASK) == 3u &&
            gate_allows) {
            uint64_t ticks = qei_output_transition_ticks(
                cpu, channel, (control & QEI_CONTROL_DIRECTION_INVERT) != 0u ? -1 : 1);
            uint64_t divider = qei_divider(control);
            if (ticks != UINT64_MAX && ticks <= UINT64_MAX / divider) {
                uint64_t cycles = ticks * divider - cpu->io.qei.counter_fraction[channel];
                if (cycles < boundary) {
                    boundary = cycles;
                }
            }
        }
    }
    return boundary;
}

bool dspic33_device_internal_qei_pps_output_value(const Dspic33* cpu, uint8_t port, uint8_t bit,
                                                  bool* high) {
    for (size_t index = 0u;
         index < sizeof(dspic33_device_pps_pins) / sizeof(dspic33_device_pps_pins[0]); index++) {
        if (dspic33_device_pps_pins[index].port == port &&
            dspic33_device_pps_pins[index].bit == bit) {
            uint8_t function = dspic33_device_internal_pps_output_function(
                cpu, dspic33_device_pps_pins[index].pin);
            if (function == 47u || function == 48u) {
                uint8_t channel = (uint8_t)(function - 47u);
                if ((dspic33_device_internal_raw_word(
                         cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u)) &
                     QEI_IO_OUTPUT_MASK) != 0u) {
                    return dspic33_device_internal_qei_compare_output_value(cpu, channel, high);
                }
            }
            return false;
        }
    }
    return false;
}

static void qei_filter_ticks(Dspic33* cpu, uint8_t channel, uint64_t ticks) {
    uint8_t input;
    bool changed = false;
    for (input = 0u; input < 4u; input++) {
        uint8_t bit = (uint8_t)(1u << input);
        bool raw = (cpu->qei_inputs[channel] & bit) != 0u;
        bool filtered = (cpu->io.qei.filtered_inputs[channel] & bit) != 0u;
        if (raw == filtered) {
            cpu->io.qei.filter_stability[channel][input] = 0u;
        } else {
            uint64_t stability = cpu->io.qei.filter_stability[channel][input] + ticks;
            if (stability >= 3u) {
                if (raw) {
                    cpu->io.qei.filtered_inputs[channel] |= bit;
                } else {
                    cpu->io.qei.filtered_inputs[channel] &= (uint8_t)~bit;
                }
                cpu->io.qei.filter_stability[channel][input] = 0u;
                changed = true;
            } else {
                cpu->io.qei.filter_stability[channel][input] = (uint8_t)stability;
            }
        }
    }
    if (changed) {
        qei_apply_filtered_inputs(cpu, channel);
    }
}

static uint64_t qei_accumulate_ticks(uint64_t* fraction, uint64_t cycles, uint64_t divider) {
    uint64_t ticks = cycles / divider;
    uint64_t remainder = cycles % divider;
    if (remainder != 0u && *fraction >= divider - remainder) {
        ticks++;
        *fraction -= divider - remainder;
    } else {
        *fraction += remainder;
    }
    return ticks;
}

static void qei_advance_counters(Dspic33* cpu, uint8_t channel, uint16_t control, uint64_t cycles) {
    uint64_t ticks =
        qei_accumulate_ticks(&cpu->io.qei.counter_fraction[channel], cycles, qei_divider(control));
    if ((control & QEI_CONTROL_COUNT_MODE_MASK) == 3u) {
        uint8_t logical = cpu->io.qei.logical_inputs[channel];
        bool gate_allows = (control & QEI_CONTROL_GATE_ENABLE) == 0u || (logical & 2u) != 0u;
        int8_t direction = (control & QEI_CONTROL_DIRECTION_INVERT) != 0u ? -1 : 1;
        qei_advance_timer_ticks(cpu, channel, direction, gate_allows, logical, ticks);
    } else if ((control & QEI_CONTROL_COUNT_MODE_MASK) < 2u) {
        qei_update_interval(cpu, channel, ticks);
    }
}

static void qei_advance_channel(Dspic33* cpu, uint8_t channel, uint64_t cycles) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, dspic33_device_qei_bases[channel]);
    uint16_t io_control =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
    uint64_t divider;
    uint64_t remaining;
    bool counters_enabled = qei_clock_enabled(cpu, channel);
    if (!qei_filter_clock_enabled(cpu, channel) || cycles == 0u) {
        return;
    }
    if ((io_control & QEI_IO_FILTER_ENABLE) == 0u) {
        if (counters_enabled) {
            qei_advance_counters(cpu, channel, control, cycles);
        }
        return;
    }
    divider = qei_filter_divider(io_control);
    remaining = cycles;
    while (remaining != 0u) {
        uint64_t until_sample = divider - cpu->io.qei.filter_fraction[channel];
        uint64_t segment = remaining < until_sample ? remaining : until_sample;
        if (counters_enabled) {
            qei_advance_counters(cpu, channel, control, segment);
        }
        cpu->io.qei.filter_fraction[channel] += segment;
        remaining -= segment;
        if (cpu->io.qei.filter_fraction[channel] == divider) {
            cpu->io.qei.filter_fraction[channel] = 0u;
            qei_filter_ticks(cpu, channel, 1u);
        }
        if (cpu->io.qei.filtered_inputs[channel] == cpu->qei_inputs[channel] && remaining != 0u) {
            if (counters_enabled) {
                qei_advance_counters(cpu, channel, control, remaining);
            }
            qei_accumulate_ticks(&cpu->io.qei.filter_fraction[channel], remaining, divider);
            memset(cpu->io.qei.filter_stability[channel], 0,
                   sizeof(cpu->io.qei.filter_stability[channel]));
            remaining = 0u;
        }
    }
}

void dspic33_device_internal_advance_qei(Dspic33* cpu, uint64_t cycles) {
    uint8_t channel;
    bool output_changed = false;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        bool before = false;
        bool before_valid = qei_pps_output_mapped(cpu, channel) &&
                            dspic33_device_internal_qei_compare_output_value(cpu, channel, &before);
        qei_advance_channel(cpu, channel, cycles);
        if (before_valid) {
            bool after = false;
            output_changed =
                output_changed ||
                (dspic33_device_internal_qei_compare_output_value(cpu, channel, &after) &&
                 before != after);
        }
    }
    if (output_changed) {
        dspic33_device_internal_refresh_physical_pin_inputs(cpu);
    }
}

void dspic33_device_internal_run_qei(Dspic33* cpu, uint16_t source, uint32_t value) {
    if (source >= QEI_PMD_EVENT_BASE) {
        uint8_t channel = (uint8_t)(source - QEI_PMD_EVENT_BASE);
        uint16_t generation = (uint16_t)(value >> 1u);
        if (channel < DSPIC33_QEI_COUNT && generation == cpu->io.qei.pmd_generation[channel]) {
            cpu->io.qei.pmd_disabled[channel] = (value & 1u) != 0u;
            if (!cpu->io.qei.pmd_disabled[channel]) {
                uint16_t io_control = dspic33_device_internal_raw_word(
                    cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
                if ((io_control & QEI_IO_FILTER_ENABLE) == 0u) {
                    uint8_t match = (uint8_t)((dspic33_device_internal_raw_word(
                                                   cpu, dspic33_device_qei_bases[channel]) &
                                               QEI_CONTROL_INDEX_MATCH_MASK) >>
                                              QEI_CONTROL_INDEX_MATCH_SHIFT);
                    cpu->io.qei.filtered_inputs[channel] = cpu->qei_inputs[channel];
                    cpu->io.qei.logical_inputs[channel] = qei_logical_inputs(cpu, channel);
                    if ((cpu->io.qei.logical_inputs[channel] & 4u) == 0u) {
                        cpu->io.qei.index_latched[channel] = false;
                    } else if ((cpu->io.qei.logical_inputs[channel] & 3u) == match) {
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
    if (source < DSPIC33_QEI_COUNT * 4u) {
        uint8_t channel = (uint8_t)(source / 4u);
        bool before = false;
        bool mapped = qei_pps_output_mapped(cpu, channel);
        bool before_valid =
            mapped && dspic33_device_internal_qei_compare_output_value(cpu, channel, &before);
        qei_set_physical_input(cpu, channel, (uint8_t)(source % 4u), value != 0u);
        if (before_valid) {
            bool after = false;
            if (dspic33_device_internal_qei_compare_output_value(cpu, channel, &after) &&
                before != after) {
                dspic33_device_internal_refresh_physical_pin_inputs(cpu);
            }
        }
    }
}

static void qei_update_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    static const uint16_t addresses[DSPIC33_QEI_COUNT] = {0x0760u, 0x0764u};
    static const uint16_t masks[DSPIC33_QEI_COUNT] = {0x0400u, 0x0020u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        bool disabled;
        if (address != addresses[channel] ||
            ((previous ^ dspic33_device_internal_raw_word(cpu, address)) & masks[channel]) == 0u) {
            continue;
        }
        disabled = (dspic33_device_internal_raw_word(cpu, address) & masks[channel]) != 0u;
        cpu->io.qei.pmd_generation[channel]++;
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_QEI, (uint16_t)(QEI_PMD_EVENT_BASE + channel),
                              ((uint32_t)cpu->io.qei.pmd_generation[channel] << 1u) |
                                  (disabled ? 1u : 0u),
                              dspic33_device_instruction_cycles(cpu, 1u))) {
            dspic33_device_internal_raw_write_word(cpu, address, previous);
            cpu->io.qei.pmd_generation[channel]++;
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
}

void dspic33_device_internal_update_qei_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                 uint16_t requested) {
    uint8_t channel;
    uint16_t offset;
    if (address == 0x0760u || address == 0x0764u) {
        qei_update_pmd(cpu, address, previous);
        return;
    }
    if (!qei_register(address, &channel, &offset)) {
        return;
    }
    if (cpu->io.qei.pmd_disabled[channel]) {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(address & 0xfffeu), previous);
        return;
    }
    if (offset == 4u) {
        uint16_t status = dspic33_device_internal_raw_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 4u));
        status = (uint16_t)((status & QEI_STATUS_ENABLE_MASK) |
                            (previous & requested & QEI_STATUS_FLAG_MASK));
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 4u), status);
        qei_refresh_interrupt(cpu, channel);
    } else if (offset == 0u || offset == 2u) {
        memset(cpu->io.qei.filter_stability[channel], 0,
               sizeof(cpu->io.qei.filter_stability[channel]));
        cpu->io.qei.counter_fraction[channel] = 0u;
        cpu->io.qei.filter_fraction[channel] = 0u;
        if (offset == 2u && (dspic33_device_internal_raw_word(
                                 cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u)) &
                             QEI_IO_FILTER_ENABLE) == 0u) {
            cpu->io.qei.filtered_inputs[channel] = cpu->qei_inputs[channel];
        }
        cpu->io.qei.logical_inputs[channel] = qei_logical_inputs(cpu, channel);
        if (offset == 0u) {
            qei_refresh_comparisons(cpu, channel);
        }
    } else if (offset == QEI_POSITION_LOW) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_POSITION_HIGH),
            dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_POSITION_HOLD)));
        qei_refresh_comparisons(cpu, channel);
    } else if (offset == QEI_POSITION_HIGH) {
        qei_refresh_comparisons(cpu, channel);
    } else if (offset == QEI_INDEX_LOW) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_INDEX_HIGH),
            dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_INDEX_HOLD)));
    } else if (offset == QEI_GREATER_EQUAL_LOW || offset == QEI_GREATER_EQUAL_HIGH ||
               offset == QEI_LESS_EQUAL_LOW || offset == QEI_LESS_EQUAL_HIGH) {
        qei_refresh_comparisons(cpu, channel);
    }
}

static bool qei_read_complete(const Dspic33* cpu, uint16_t address) {
    return !cpu->io.cpu_read_valid || cpu->io.cpu_read_width == 1u ||
           address == cpu->io.cpu_read_address + 1u;
}

bool dspic33_device_internal_qei_read_register(Dspic33* cpu, uint16_t address, uint8_t* value) {
    uint8_t channel;
    uint16_t offset;
    if (!qei_register(address, &channel, &offset)) {
        return false;
    }
    if (cpu->io.qei.pmd_disabled[channel]) {
        *value = 0u;
        return true;
    }
    if (offset == 2u) {
        uint16_t io_control = dspic33_device_internal_raw_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + 2u));
        io_control =
            (uint16_t)((io_control & ~QEI_IO_INPUT_MASK) | cpu->io.qei.logical_inputs[channel]);
        *value = (uint8_t)(io_control >> ((address & 1u) * 8u));
    }
    if (offset == QEI_POSITION_LOW && (address & 1u) == 0u) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_POSITION_HOLD),
            dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_POSITION_HIGH)));
    } else if (offset == QEI_INDEX_LOW && (address & 1u) == 0u) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_INDEX_HOLD),
            dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_INDEX_HIGH)));
    } else if (offset == QEI_INTERVAL_HOLD_LOW && (address & 1u) == 0u) {
        cpu->io.qei.interval_hold_locked[channel] = true;
    } else if (offset == QEI_INTERVAL_HOLD_HIGH && qei_read_complete(cpu, address)) {
        cpu->io.qei.interval_hold_locked[channel] = false;
    } else if (offset == QEI_VELOCITY && qei_read_complete(cpu, address)) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_qei_bases[channel] + QEI_VELOCITY), 0u);
    }
    return true;
}
