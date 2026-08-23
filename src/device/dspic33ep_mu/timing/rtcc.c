#include "device/dspic33ep_mu/internal.h"

static const uint16_t rtcc_calendar_masks[4] = {0x7f7fu, 0x073fu, 0x1f3fu, 0x00ffu};
static const uint16_t rtcc_alarm_masks[3] = {0x7f7fu, 0x073fu, 0x1f3fu};

bool dspic33_device_internal_nvm_key_authorized(const Dspic33* cpu) {
    return cpu->nvm.key_stage == 2u && cpu->nvm.key_instruction != UINT64_MAX &&
           cpu->instructions == cpu->nvm.key_instruction + 1u &&
           cpu->interrupt_count == cpu->nvm.key_interrupt_count &&
           cpu->trap_count == cpu->nvm.key_trap_count;
}

static uint8_t rtcc_bcd_decode(uint8_t bcd_value) {
    return (uint8_t)((bcd_value >> 4u) * 10u + (bcd_value & 0x0fu));
}

static uint8_t rtcc_bcd_encode(uint8_t decimal_value) {
    return (uint8_t)(((decimal_value / 10u) << 4u) | (decimal_value % 10u));
}

static bool rtcc_bcd_valid(uint8_t bcd_value, uint8_t minimum, uint8_t maximum) {
    const uint8_t decimal_value = rtcc_bcd_decode(bcd_value);
    return (bcd_value & 0x0fu) <= 9u && (bcd_value >> 4u) <= 9u && decimal_value >= minimum &&
           decimal_value <= maximum;
}

static uint8_t rtcc_month_days(uint8_t calendar_year, uint8_t calendar_month) {
    static const uint8_t days[] = {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u};
    if (calendar_month == 2u && calendar_year % 4u == 0u) {
        return 29u;
    }
    return days[calendar_month - 1u];
}

static bool rtcc_calendar_valid(const Dspic33Rtcc* rtcc) {
    const uint8_t calendar_second = (uint8_t)rtcc->calendar[0];
    const uint8_t calendar_minute = (uint8_t)(rtcc->calendar[0] >> 8u);
    const uint8_t calendar_hour = (uint8_t)rtcc->calendar[1];
    const uint8_t calendar_weekday = (uint8_t)(rtcc->calendar[1] >> 8u);
    const uint8_t calendar_day = (uint8_t)rtcc->calendar[2];
    const uint8_t calendar_month = (uint8_t)(rtcc->calendar[2] >> 8u);
    const uint8_t calendar_year = (uint8_t)rtcc->calendar[3];

    if (!rtcc_bcd_valid(calendar_second, 0u, 59u) || !rtcc_bcd_valid(calendar_minute, 0u, 59u) ||
        !rtcc_bcd_valid(calendar_hour, 0u, 23u) || calendar_weekday > 6u ||
        !rtcc_bcd_valid(calendar_month, 1u, 12u) || !rtcc_bcd_valid(calendar_year, 0u, 99u)) {
        return false;
    }

    return rtcc_bcd_valid(
        calendar_day, 1u,
        rtcc_month_days(rtcc_bcd_decode(calendar_year), rtcc_bcd_decode(calendar_month)));
}

static void rtcc_increment_calendar(Dspic33* cpu) {
    Dspic33Rtcc* rtcc = &cpu->io.rtcc;

    if (!rtcc_calendar_valid(rtcc)) {
        return;
    }

    uint8_t current_second = rtcc_bcd_decode((uint8_t)rtcc->calendar[0]);
    uint8_t current_minute = rtcc_bcd_decode((uint8_t)(rtcc->calendar[0] >> 8u));
    uint8_t current_hour = rtcc_bcd_decode((uint8_t)rtcc->calendar[1]);
    uint8_t current_weekday = (uint8_t)(rtcc->calendar[1] >> 8u);
    uint8_t current_day = rtcc_bcd_decode((uint8_t)rtcc->calendar[2]);
    uint8_t current_month = rtcc_bcd_decode((uint8_t)(rtcc->calendar[2] >> 8u));
    uint8_t current_year = rtcc_bcd_decode((uint8_t)rtcc->calendar[3]);

    current_second++;
    if (current_second == 60u) {
        current_second = 0u;
        current_minute++;
        if (current_minute == 60u) {
            current_minute = 0u;
            current_hour++;
            if (current_hour == 24u) {
                current_hour = 0u;
                current_weekday = (uint8_t)((current_weekday + 1u) % 7u);
                current_day++;
                if (current_day > rtcc_month_days(current_year, current_month)) {
                    current_day = 1u;
                    current_month++;
                    if (current_month == 13u) {
                        current_month = 1u;
                        current_year = (uint8_t)((current_year + 1u) % 100u);
                    }
                }
            }
        }
    }
    rtcc->calendar[0] = (uint16_t)(((uint16_t)rtcc_bcd_encode(current_minute) << 8u) |
                                   rtcc_bcd_encode(current_second));
    rtcc->calendar[1] =
        (uint16_t)(((uint16_t)current_weekday << 8u) | rtcc_bcd_encode(current_hour));
    rtcc->calendar[2] =
        (uint16_t)(((uint16_t)rtcc_bcd_encode(current_month) << 8u) | rtcc_bcd_encode(current_day));
    rtcc->calendar[3] = rtcc_bcd_encode(current_year);
}

static void rtcc_apply_calibration(Dspic33* cpu) {
    const uint16_t control_value = dspic33_device_internal_raw_word(cpu, RTCC_CONTROL);
    int16_t calibration_value = (int16_t)(control_value & 0x00ffu);
    int32_t prescaler;
    if (calibration_value >= 0x80) {
        calibration_value -= 0x100;
    }
    prescaler = (int32_t)cpu->io.rtcc.prescaler + calibration_value * 4;
    cpu->io.rtcc.prescaler = (uint16_t)prescaler;
    cpu->io.rtcc.calibration_pending = false;
}

static void rtcc_set_status(Dspic33* cpu, uint16_t status) {
    const uint16_t control_value = dspic33_device_internal_raw_word(cpu, RTCC_CONTROL);
    dspic33_device_internal_raw_write_word(
        cpu, RTCC_CONTROL, (uint16_t)((control_value & ~(RTCC_SYNC | RTCC_HALF_SECOND)) | status));
}

static bool rtcc_alarm_matches(const Dspic33* cpu, bool full_second) {
    const uint16_t alarm_control = dspic33_device_internal_raw_word(cpu, RTCC_ALARM_CONTROL);
    const uint8_t match_mask = (uint8_t)((alarm_control & RTCC_ALARM_MASK) >> 10u);
    const Dspic33Rtcc* rtcc = &cpu->io.rtcc;
    uint8_t calendar_second = (uint8_t)rtcc->calendar[0];
    uint8_t calendar_minute = (uint8_t)(rtcc->calendar[0] >> 8u);
    uint8_t calendar_hour = (uint8_t)rtcc->calendar[1];
    uint8_t calendar_weekday = (uint8_t)(rtcc->calendar[1] >> 8u);
    uint8_t calendar_day = (uint8_t)rtcc->calendar[2];
    uint8_t calendar_month = (uint8_t)(rtcc->calendar[2] >> 8u);
    uint8_t alarm_second = (uint8_t)rtcc->alarm[0];
    uint8_t alarm_minute = (uint8_t)(rtcc->alarm[0] >> 8u);
    uint8_t alarm_hour = (uint8_t)rtcc->alarm[1];
    uint8_t alarm_weekday = (uint8_t)(rtcc->alarm[1] >> 8u);
    uint8_t alarm_day = (uint8_t)rtcc->alarm[2];
    uint8_t alarm_month = (uint8_t)(rtcc->alarm[2] >> 8u);
    if ((alarm_control & RTCC_ALARM_ENABLE) == 0u || match_mask > 9u) {
        return false;
    }
    if (match_mask == 0u) {
        return true;
    }
    if (!full_second || match_mask == 1u) {
        return full_second;
    }
    if ((calendar_second & 0x0fu) != (alarm_second & 0x0fu)) {
        return false;
    }
    if (match_mask == 2u) {
        return true;
    }
    if (calendar_second != alarm_second) {
        return false;
    }
    if (match_mask == 3u) {
        return true;
    }
    if ((calendar_minute & 0x0fu) != (alarm_minute & 0x0fu)) {
        return false;
    }
    if (match_mask == 4u) {
        return true;
    }
    if (calendar_minute != alarm_minute) {
        return false;
    }
    if (match_mask == 5u) {
        return true;
    }
    if (calendar_hour != alarm_hour) {
        return false;
    }
    if (match_mask == 6u) {
        return true;
    }
    if (match_mask == 7u) {
        return calendar_weekday == alarm_weekday;
    }
    if (calendar_day != alarm_day) {
        return false;
    }
    return match_mask == 8u || calendar_month == alarm_month;
}

static void rtcc_alarm_event(Dspic33* cpu) {
    uint16_t alarm_control = dspic33_device_internal_raw_word(cpu, RTCC_ALARM_CONTROL);
    uint8_t repeat_count = (uint8_t)alarm_control;

    cpu->io.rtcc.alarm_output = !cpu->io.rtcc.alarm_output;
    dspic33_raise_interrupt(cpu, RTCC_IRQ);
    if ((alarm_control & RTCC_ALARM_CHIME) != 0u) {
        repeat_count--;
    } else if (repeat_count == 0u) {
        alarm_control &= (uint16_t)~RTCC_ALARM_ENABLE;
    } else {
        repeat_count--;
    }
    dspic33_device_internal_raw_write_word(cpu, RTCC_ALARM_CONTROL,
                                           (uint16_t)((alarm_control & 0xff00u) | repeat_count));
}

static bool rtcc_operating(const Dspic33* cpu) {
    return !cpu->io.rtcc.pmd_disabled &&
           (dspic33_device_internal_raw_word(cpu, RTCC_CONTROL) & RTCC_ENABLE) != 0u &&
           (dspic33_device_internal_raw_word(cpu, 0x0742u) & RTCC_LPOSC_ENABLE) != 0u;
}

static void rtcc_clock_edge(Dspic33* cpu) {
    uint16_t status = 0u;
    bool full_second = false;

    if (!rtcc_operating(cpu)) {
        return;
    }

    cpu->io.rtcc.prescaler++;
    if (cpu->io.rtcc.prescaler == RTCC_HALF_SECOND_EDGE) {
        status |= RTCC_HALF_SECOND;
    } else if (cpu->io.rtcc.prescaler >= RTCC_PRESCALER_EDGES) {
        const uint8_t previous_second = (uint8_t)cpu->io.rtcc.calendar[0];

        cpu->io.rtcc.prescaler = 0u;
        full_second = true;
        rtcc_increment_calendar(cpu);
        cpu->io.rtcc.calibration_pending =
            previous_second == 0x59u && (uint8_t)cpu->io.rtcc.calendar[0] == 0u;
    } else if (cpu->io.rtcc.prescaler == RTCC_CALIBRATION_EDGE &&
               cpu->io.rtcc.calibration_pending) {
        rtcc_apply_calibration(cpu);
    } else {
        status = dspic33_device_internal_raw_word(cpu, RTCC_CONTROL) & RTCC_HALF_SECOND;
    }
    if (cpu->io.rtcc.prescaler >= RTCC_PRESCALER_EDGES - RTCC_SYNC_EDGES) {
        status |= RTCC_SYNC;
    }
    rtcc_set_status(cpu, status);
    if ((cpu->io.rtcc.prescaler == RTCC_HALF_SECOND_EDGE || full_second) &&
        rtcc_alarm_matches(cpu, full_second)) {
        rtcc_alarm_event(cpu);
    }
}

void dspic33_device_internal_run_rtcc(Dspic33* cpu, uint16_t event_source, uint32_t event_value) {
    if (event_source == RTCC_EVENT_PMD_SOURCE) {
        const uint16_t event_generation = (uint16_t)(event_value >> 1u);

        if (event_generation == cpu->io.rtcc.pmd_generation) {
            cpu->io.rtcc.pmd_disabled = (event_value & 1u) != 0u;
        }
        return;
    }

    while (event_value-- != 0u) {
        rtcc_clock_edge(cpu);
    }
}

static void rtcc_decrement_pointer(Dspic33* cpu, uint16_t control_address, uint16_t pointer_mask) {
    uint16_t control_value = dspic33_device_internal_raw_word(cpu, control_address);
    const uint16_t pointer = (uint16_t)((control_value & pointer_mask) >> 8u);

    if (pointer != 0u) {
        control_value = (uint16_t)((control_value & ~pointer_mask) | ((pointer - 1u) << 8u));
        dspic33_device_internal_raw_write_word(cpu, control_address, control_value);
    }
}

static bool rtcc_read_complete(const Dspic33* cpu, uint16_t address) {
    return !cpu->io.cpu_read_valid || cpu->io.cpu_read_width == 1u ||
           address == cpu->io.cpu_read_address + 1u;
}

uint8_t dspic33_device_internal_rtcc_read_window(Dspic33* cpu, uint16_t address, bool alarm) {
    const uint16_t control_address = alarm ? RTCC_ALARM_CONTROL : RTCC_CONTROL;
    const uint16_t pointer_mask = alarm ? RTCC_ALARM_POINTER_MASK : RTCC_POINTER_MASK;
    const uint16_t pointer =
        (uint16_t)((dspic33_device_internal_raw_word(cpu, control_address) & pointer_mask) >> 8u);
    const uint16_t value_mask =
        alarm ? (pointer < 3u ? rtcc_alarm_masks[pointer] : 0u) : rtcc_calendar_masks[pointer];
    const uint16_t window_value =
        alarm ? (pointer < 3u ? cpu->io.rtcc.alarm[pointer] : 0u) : cpu->io.rtcc.calendar[pointer];

    const uint8_t result = (uint8_t)((window_value & value_mask) >> ((address & 1u) * 8u));
    if (rtcc_read_complete(cpu, address)) {
        rtcc_decrement_pointer(cpu, control_address, pointer_mask);
    }
    return result;
}

static uint8_t rtcc_write_width(const Dspic33* cpu) {
    if (cpu->io.dma_transfer_active) {
        return cpu->io.dma_transfer_width;
    }
    return cpu->io.cpu_write_valid ? cpu->io.cpu_write_width : 1u;
}

static uint16_t rtcc_window_write_value(const Dspic33* cpu, uint16_t address,
                                        uint16_t previous_value) {
    if (rtcc_write_width(cpu) == 2u) {
        return dspic33_device_internal_raw_word(cpu, (uint16_t)(address & 0xfffeu));
    }
    if ((address & 1u) != 0u) {
        return (uint16_t)((previous_value & 0x00ffu) | ((uint16_t)cpu->data[address] << 8u));
    }
    return (uint16_t)((previous_value & 0xff00u) | cpu->data[address]);
}

static bool rtcc_write_decrements_pointer(const Dspic33* cpu, uint16_t address) {
    return rtcc_write_width(cpu) == 2u || (address & 1u) != 0u;
}

static void update_rtcc_window(Dspic33* cpu, uint16_t address, bool alarm) {
    const uint16_t control_address = alarm ? RTCC_ALARM_CONTROL : RTCC_CONTROL;
    const uint16_t pointer_mask = alarm ? RTCC_ALARM_POINTER_MASK : RTCC_POINTER_MASK;
    const uint16_t pointer =
        (uint16_t)((dspic33_device_internal_raw_word(cpu, control_address) & pointer_mask) >> 8u);
    const uint16_t previous_value =
        alarm ? (pointer < 3u ? cpu->io.rtcc.alarm[pointer] : 0u) : cpu->io.rtcc.calendar[pointer];
    const uint16_t window_value = rtcc_window_write_value(cpu, address, previous_value);

    if (alarm || (dspic33_device_internal_raw_word(cpu, RTCC_CONTROL) & RTCC_WRITE_ENABLE) != 0u) {
        if (alarm && pointer < 3u) {
            cpu->io.rtcc.alarm[pointer] = window_value & rtcc_alarm_masks[pointer];
        } else {
            if (!alarm) {
                cpu->io.rtcc.calendar[pointer] = window_value & rtcc_calendar_masks[pointer];
            }
            if (!alarm && pointer == 0u) {
                cpu->io.rtcc.prescaler = 0u;
                cpu->io.rtcc.calibration_pending = false;
                rtcc_set_status(cpu, 0u);
            }
        }
    }
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(address & 0xfffeu), 0u);
    if (rtcc_write_decrements_pointer(cpu, address)) {
        rtcc_decrement_pointer(cpu, control_address, pointer_mask);
    }
}

static void update_rtcc_control(Dspic33* cpu, uint16_t previous_control) {
    uint16_t control_value = dspic33_device_internal_raw_word(cpu, RTCC_CONTROL);
    const bool previous_write_enable = (previous_control & RTCC_WRITE_ENABLE) != 0u;
    const bool requested_write_enable = (control_value & RTCC_WRITE_ENABLE) != 0u;

    if (!previous_write_enable && requested_write_enable) {
        if (!dspic33_device_internal_nvm_key_authorized(cpu) || !cpu->instruction_active ||
            cpu->current_instruction_cycles != 1u) {
            control_value &= (uint16_t)~RTCC_WRITE_ENABLE;
        }
        cpu->nvm.key_stage = 0u;
    }

    if (!previous_write_enable) {
        control_value =
            (uint16_t)((control_value & ~RTCC_ENABLE) | (previous_control & RTCC_ENABLE));
    }

    dspic33_device_internal_raw_write_word(cpu, RTCC_CONTROL, control_value);
}

static void update_rtcc_pmd(Dspic33* cpu, uint16_t previous_pmd) {
    const bool pmd_disabled =
        (dspic33_device_internal_raw_word(cpu, RTCC_PMD_ADDRESS) & RTCC_PMD) != 0u;

    if (((previous_pmd & RTCC_PMD) != 0u) == pmd_disabled) {
        return;
    }

    cpu->io.rtcc.pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_RTCC, RTCC_EVENT_PMD_SOURCE,
                          ((uint32_t)cpu->io.rtcc.pmd_generation << 1u) | (pmd_disabled ? 1u : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        dspic33_device_internal_raw_write_word(cpu, RTCC_PMD_ADDRESS, previous_pmd);
        cpu->io.rtcc.pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_internal_update_rtcc_register(Dspic33* cpu, uint16_t address,
                                                  uint16_t previous_value) {
    const uint16_t register_address = (uint16_t)(address & 0xfffeu);

    if (register_address == RTCC_PMD_ADDRESS) {
        update_rtcc_pmd(cpu, previous_value);
        return;
    }
    if (register_address < RTCC_ALARM_VALUE || register_address > RTCC_CONTROL) {
        return;
    }
    if (cpu->io.rtcc.pmd_disabled) {
        dspic33_device_internal_raw_write_word(cpu, register_address, previous_value);
        return;
    }

    if (register_address == RTCC_ALARM_VALUE) {
        update_rtcc_window(cpu, address, true);
    } else if (register_address == RTCC_VALUE) {
        update_rtcc_window(cpu, address, false);
    } else if (register_address == RTCC_CONTROL) {
        update_rtcc_control(cpu, previous_value);
    }
}
