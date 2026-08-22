
static bool nvm_key_authorized(const Dspic33* cpu) {
    return cpu->nvm.key_stage == 2u && cpu->nvm.key_instruction != UINT64_MAX &&
           cpu->instructions == cpu->nvm.key_instruction + 1u &&
           cpu->interrupt_count == cpu->nvm.key_interrupt_count &&
           cpu->trap_count == cpu->nvm.key_trap_count;
}

static uint8_t rtcc_bcd_decode(uint8_t value) {
    return (uint8_t)((value >> 4u) * 10u + (value & 0x0fu));
}

static uint8_t rtcc_bcd_encode(uint8_t value) {
    return (uint8_t)(((value / 10u) << 4u) | (value % 10u));
}

static bool rtcc_bcd_valid(uint8_t value, uint8_t minimum, uint8_t maximum) {
    uint8_t decoded = rtcc_bcd_decode(value);
    return (value & 0x0fu) <= 9u && (value >> 4u) <= 9u && decoded >= minimum && decoded <= maximum;
}

static uint8_t rtcc_month_days(uint8_t year, uint8_t month) {
    static const uint8_t days[] = {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u};
    if (month == 2u && year % 4u == 0u) {
        return 29u;
    }
    return days[month - 1u];
}

static bool rtcc_calendar_valid(const Dspic33Rtcc* rtcc) {
    uint8_t second = (uint8_t)rtcc->calendar[0];
    uint8_t minute = (uint8_t)(rtcc->calendar[0] >> 8u);
    uint8_t hour = (uint8_t)rtcc->calendar[1];
    uint8_t weekday = (uint8_t)(rtcc->calendar[1] >> 8u);
    uint8_t day = (uint8_t)rtcc->calendar[2];
    uint8_t month = (uint8_t)(rtcc->calendar[2] >> 8u);
    uint8_t year = (uint8_t)rtcc->calendar[3];
    if (!rtcc_bcd_valid(second, 0u, 59u) || !rtcc_bcd_valid(minute, 0u, 59u) ||
        !rtcc_bcd_valid(hour, 0u, 23u) || weekday > 6u || !rtcc_bcd_valid(month, 1u, 12u) ||
        !rtcc_bcd_valid(year, 0u, 99u)) {
        return false;
    }
    return rtcc_bcd_valid(day, 1u, rtcc_month_days(rtcc_bcd_decode(year), rtcc_bcd_decode(month)));
}

static void rtcc_increment_calendar(Dspic33* cpu) {
    Dspic33Rtcc* rtcc = &cpu->io.rtcc;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t weekday;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    if (!rtcc_calendar_valid(rtcc)) {
        return;
    }
    second = rtcc_bcd_decode((uint8_t)rtcc->calendar[0]);
    minute = rtcc_bcd_decode((uint8_t)(rtcc->calendar[0] >> 8u));
    hour = rtcc_bcd_decode((uint8_t)rtcc->calendar[1]);
    weekday = (uint8_t)(rtcc->calendar[1] >> 8u);
    day = rtcc_bcd_decode((uint8_t)rtcc->calendar[2]);
    month = rtcc_bcd_decode((uint8_t)(rtcc->calendar[2] >> 8u));
    year = rtcc_bcd_decode((uint8_t)rtcc->calendar[3]);
    second++;
    if (second == 60u) {
        second = 0u;
        minute++;
        if (minute == 60u) {
            minute = 0u;
            hour++;
            if (hour == 24u) {
                hour = 0u;
                weekday = (uint8_t)((weekday + 1u) % 7u);
                day++;
                if (day > rtcc_month_days(year, month)) {
                    day = 1u;
                    month++;
                    if (month == 13u) {
                        month = 1u;
                        year = (uint8_t)((year + 1u) % 100u);
                    }
                }
            }
        }
    }
    rtcc->calendar[0] =
        (uint16_t)(((uint16_t)rtcc_bcd_encode(minute) << 8u) | rtcc_bcd_encode(second));
    rtcc->calendar[1] = (uint16_t)(((uint16_t)weekday << 8u) | rtcc_bcd_encode(hour));
    rtcc->calendar[2] = (uint16_t)(((uint16_t)rtcc_bcd_encode(month) << 8u) | rtcc_bcd_encode(day));
    rtcc->calendar[3] = rtcc_bcd_encode(year);
}

static void rtcc_apply_calibration(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, RTCC_CONTROL);
    int16_t calibration = (int16_t)(control & 0x00ffu);
    int32_t prescaler;
    if (calibration >= 0x80) {
        calibration -= 0x100;
    }
    prescaler = (int32_t)cpu->io.rtcc.prescaler + calibration * 4;
    cpu->io.rtcc.prescaler = (uint16_t)prescaler;
    cpu->io.rtcc.calibration_pending = false;
}

static void rtcc_set_status(Dspic33* cpu, uint16_t status) {
    uint16_t control = raw_word(cpu, RTCC_CONTROL);
    raw_write_word(cpu, RTCC_CONTROL,
                   (uint16_t)((control & ~(RTCC_SYNC | RTCC_HALF_SECOND)) | status));
}

static bool rtcc_alarm_matches(const Dspic33* cpu, bool full_second) {
    uint16_t control = raw_word(cpu, RTCC_ALARM_CONTROL);
    uint8_t mask = (uint8_t)((control & RTCC_ALARM_MASK) >> 10u);
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
    if ((control & RTCC_ALARM_ENABLE) == 0u || mask > 9u) {
        return false;
    }
    if (mask == 0u) {
        return true;
    }
    if (!full_second || mask == 1u) {
        return full_second;
    }
    if ((calendar_second & 0x0fu) != (alarm_second & 0x0fu)) {
        return false;
    }
    if (mask == 2u) {
        return true;
    }
    if (calendar_second != alarm_second) {
        return false;
    }
    if (mask == 3u) {
        return true;
    }
    if ((calendar_minute & 0x0fu) != (alarm_minute & 0x0fu)) {
        return false;
    }
    if (mask == 4u) {
        return true;
    }
    if (calendar_minute != alarm_minute) {
        return false;
    }
    if (mask == 5u) {
        return true;
    }
    if (calendar_hour != alarm_hour) {
        return false;
    }
    if (mask == 6u) {
        return true;
    }
    if (mask == 7u) {
        return calendar_weekday == alarm_weekday;
    }
    if (calendar_day != alarm_day) {
        return false;
    }
    return mask == 8u || calendar_month == alarm_month;
}

static void rtcc_alarm_event(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, RTCC_ALARM_CONTROL);
    uint8_t repeat = (uint8_t)control;
    cpu->io.rtcc.alarm_output = !cpu->io.rtcc.alarm_output;
    dspic33_raise_interrupt(cpu, RTCC_IRQ);
    if ((control & RTCC_ALARM_CHIME) != 0u) {
        repeat--;
    } else if (repeat == 0u) {
        control &= (uint16_t)~RTCC_ALARM_ENABLE;
    } else {
        repeat--;
    }
    raw_write_word(cpu, RTCC_ALARM_CONTROL, (uint16_t)((control & 0xff00u) | repeat));
}

static bool rtcc_operating(const Dspic33* cpu) {
    return !cpu->io.rtcc.pmd_disabled && (raw_word(cpu, RTCC_CONTROL) & RTCC_ENABLE) != 0u &&
           (raw_word(cpu, 0x0742u) & RTCC_LPOSC_ENABLE) != 0u;
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
        uint8_t previous_second = (uint8_t)cpu->io.rtcc.calendar[0];
        cpu->io.rtcc.prescaler = 0u;
        full_second = true;
        rtcc_increment_calendar(cpu);
        cpu->io.rtcc.calibration_pending =
            previous_second == 0x59u && (uint8_t)cpu->io.rtcc.calendar[0] == 0u;
    } else if (cpu->io.rtcc.prescaler == RTCC_CALIBRATION_EDGE &&
               cpu->io.rtcc.calibration_pending) {
        rtcc_apply_calibration(cpu);
    } else {
        status = raw_word(cpu, RTCC_CONTROL) & RTCC_HALF_SECOND;
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

static void run_rtcc(Dspic33* cpu, uint16_t source, uint32_t value) {
    if (source == RTCC_EVENT_PMD_SOURCE) {
        uint16_t generation = (uint16_t)(value >> 1u);
        if (generation == cpu->io.rtcc.pmd_generation) {
            cpu->io.rtcc.pmd_disabled = (value & 1u) != 0u;
        }
        return;
    }
    while (value-- != 0u) {
        rtcc_clock_edge(cpu);
    }
}

static void rtcc_decrement_pointer(Dspic33* cpu, uint16_t control_address, uint16_t pointer_mask) {
    uint16_t control = raw_word(cpu, control_address);
    uint16_t pointer = (uint16_t)((control & pointer_mask) >> 8u);
    if (pointer != 0u) {
        control = (uint16_t)((control & ~pointer_mask) | ((pointer - 1u) << 8u));
        raw_write_word(cpu, control_address, control);
    }
}

static bool rtcc_read_complete(const Dspic33* cpu, uint16_t address) {
    return !cpu->io.cpu_read_valid || cpu->io.cpu_read_width == 1u ||
           address == cpu->io.cpu_read_address + 1u;
}

static uint8_t rtcc_read_window(Dspic33* cpu, uint16_t address, bool alarm) {
    uint16_t control_address = alarm ? RTCC_ALARM_CONTROL : RTCC_CONTROL;
    uint16_t pointer_mask = alarm ? RTCC_ALARM_POINTER_MASK : RTCC_POINTER_MASK;
    uint16_t pointer = (uint16_t)((raw_word(cpu, control_address) & pointer_mask) >> 8u);
    uint16_t mask =
        alarm ? (pointer < 3u ? rtcc_alarm_masks[pointer] : 0u) : rtcc_calendar_masks[pointer];
    uint16_t value =
        alarm ? (pointer < 3u ? cpu->io.rtcc.alarm[pointer] : 0u) : cpu->io.rtcc.calendar[pointer];
    uint8_t result = (uint8_t)((value & mask) >> ((address & 1u) * 8u));
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

static uint16_t rtcc_window_write_value(const Dspic33* cpu, uint16_t address, uint16_t previous) {
    if (rtcc_write_width(cpu) == 2u) {
        return raw_word(cpu, (uint16_t)(address & 0xfffeu));
    }
    if ((address & 1u) != 0u) {
        return (uint16_t)((previous & 0x00ffu) | ((uint16_t)cpu->data[address] << 8u));
    }
    return (uint16_t)((previous & 0xff00u) | cpu->data[address]);
}

static bool rtcc_write_decrements_pointer(const Dspic33* cpu, uint16_t address) {
    return rtcc_write_width(cpu) == 2u || (address & 1u) != 0u;
}

static void update_rtcc_window(Dspic33* cpu, uint16_t address, bool alarm) {
    uint16_t control_address = alarm ? RTCC_ALARM_CONTROL : RTCC_CONTROL;
    uint16_t pointer_mask = alarm ? RTCC_ALARM_POINTER_MASK : RTCC_POINTER_MASK;
    uint16_t pointer = (uint16_t)((raw_word(cpu, control_address) & pointer_mask) >> 8u);
    uint16_t previous =
        alarm ? (pointer < 3u ? cpu->io.rtcc.alarm[pointer] : 0u) : cpu->io.rtcc.calendar[pointer];
    uint16_t value = rtcc_window_write_value(cpu, address, previous);
    if (alarm || (raw_word(cpu, RTCC_CONTROL) & RTCC_WRITE_ENABLE) != 0u) {
        if (alarm && pointer < 3u) {
            cpu->io.rtcc.alarm[pointer] = value & rtcc_alarm_masks[pointer];
        } else {
            if (!alarm) {
                cpu->io.rtcc.calendar[pointer] = value & rtcc_calendar_masks[pointer];
            }
            if (!alarm && pointer == 0u) {
                cpu->io.rtcc.prescaler = 0u;
                cpu->io.rtcc.calibration_pending = false;
                rtcc_set_status(cpu, 0u);
            }
        }
    }
    raw_write_word(cpu, (uint16_t)(address & 0xfffeu), 0u);
    if (rtcc_write_decrements_pointer(cpu, address)) {
        rtcc_decrement_pointer(cpu, control_address, pointer_mask);
    }
}

static void update_rtcc_control(Dspic33* cpu, uint16_t previous) {
    uint16_t control = raw_word(cpu, RTCC_CONTROL);
    bool previous_write_enable = (previous & RTCC_WRITE_ENABLE) != 0u;
    bool requested_write_enable = (control & RTCC_WRITE_ENABLE) != 0u;
    if (!previous_write_enable && requested_write_enable) {
        if (!nvm_key_authorized(cpu) || !cpu->instruction_active ||
            cpu->current_instruction_cycles != 1u) {
            control &= (uint16_t)~RTCC_WRITE_ENABLE;
        }
        cpu->nvm.key_stage = 0u;
    }
    if (!previous_write_enable) {
        control = (uint16_t)((control & ~RTCC_ENABLE) | (previous & RTCC_ENABLE));
    }
    raw_write_word(cpu, RTCC_CONTROL, control);
}

static void update_rtcc_pmd(Dspic33* cpu, uint16_t previous) {
    bool disabled = (raw_word(cpu, RTCC_PMD_ADDRESS) & RTCC_PMD) != 0u;
    if (((previous & RTCC_PMD) != 0u) == disabled) {
        return;
    }
    cpu->io.rtcc.pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_RTCC, RTCC_EVENT_PMD_SOURCE,
                          ((uint32_t)cpu->io.rtcc.pmd_generation << 1u) | (disabled ? 1u : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        raw_write_word(cpu, RTCC_PMD_ADDRESS, previous);
        cpu->io.rtcc.pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void update_rtcc_register(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    if (base == RTCC_PMD_ADDRESS) {
        update_rtcc_pmd(cpu, previous);
        return;
    }
    if (base < RTCC_ALARM_VALUE || base > RTCC_CONTROL) {
        return;
    }
    if (cpu->io.rtcc.pmd_disabled) {
        raw_write_word(cpu, base, previous);
        return;
    }
    if (base == RTCC_ALARM_VALUE) {
        update_rtcc_window(cpu, address, true);
    } else if (base == RTCC_VALUE) {
        update_rtcc_window(cpu, address, false);
    } else if (base == RTCC_CONTROL) {
        update_rtcc_control(cpu, previous);
    }
}
