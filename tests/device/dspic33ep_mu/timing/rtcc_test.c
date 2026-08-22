#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

enum {
    RTCC_ALARM_VALUE = 0x0620u,
    RTCC_ALARM_CONTROL = 0x0622u,
    RTCC_VALUE = 0x0624u,
    RTCC_CONTROL = 0x0626u,
    RTCC_PAD_CONTROL = 0x0efeu,
    RTCC_PMD_ADDRESS = 0x0764u,
    RTCC_NVM_KEY = 0x072eu,
    RTCC_ENABLE = 0x8000u,
    RTCC_WRITE_ENABLE = 0x2000u,
    RTCC_SYNC = 0x1000u,
    RTCC_HALF_SECOND = 0x0800u,
    RTCC_OUTPUT_ENABLE = 0x0400u,
    RTCC_ALARM_ENABLE = 0x8000u,
    RTCC_ALARM_CHIME = 0x4000u,
    RTCC_PMD = 0x0200u,
    RTCC_IRQ = 62u,
    RTCC_FLAG_ADDRESS = 0x0806u,
    RTCC_ENABLE_ADDRESS = 0x0826u,
    RTCC_PRIORITY_ADDRESS = 0x085eu,
    RTCC_INTERRUPT_BIT = 0x4000u,
    RTCC_VECTOR = 0x0360u,
    RTCC_SEQUENCE_BASE = 0x0400u,
    MOVE_KEY_55 = 0x200550u,
    MOVE_KEY_AA = 0x200aa0u,
    WRITE_NVM_KEY = 0x883970u,
    SET_RTC_WRITE_ENABLE = 0xa8a627u,
    MOVE_DOUBLE_TO_RTC_CONTROL = 0xbe8900u,
    RESET_OPCODE = 0xfe0000u
};

static const uint16_t calendar_masks[4] = {0x7f7fu, 0x073fu, 0x1f3fu, 0x00ffu};
static const uint16_t alarm_masks[4] = {0x7f7fu, 0x073fu, 0x1f3fu, 0x0000u};

static bool step_instructions(Dspic33* cpu, uint8_t count) {
    uint8_t index;
    for (index = 0u; index < count; index++) {
        if (dspic33_step(cpu) != DSPIC33_RUNNING) {
            return false;
        }
    }
    return true;
}

static void load_write_enable_sequence(Dspic33* cpu) {
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE, MOVE_KEY_55);
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 2u, WRITE_NVM_KEY);
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 4u, MOVE_KEY_AA);
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 6u, WRITE_NVM_KEY);
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 8u, SET_RTC_WRITE_ENABLE);
    cpu->pc = RTCC_SEQUENCE_BASE;
}

static bool authorize_calendar_write(Dspic33* cpu) {
    load_write_enable_sequence(cpu);
    return step_instructions(cpu, 5u) &&
           (dspic33_read_word(cpu, RTCC_CONTROL) & RTCC_WRITE_ENABLE) != 0u;
}

static void enable_clock(Dspic33* cpu) {
    cpu->data[0x0742u] |= 0x02u;
    if (authorize_calendar_write(cpu)) {
        dspic33_write_word(cpu, RTCC_CONTROL,
                           (uint16_t)(dspic33_read_word(cpu, RTCC_CONTROL) | RTCC_ENABLE));
    }
}

static void configure_interrupt(Dspic33* cpu, uint8_t priority) {
    dspic33_write_word(
        cpu, RTCC_ENABLE_ADDRESS,
        (uint16_t)(dspic33_read_word(cpu, RTCC_ENABLE_ADDRESS) | RTCC_INTERRUPT_BIT));
    dspic33_write_word(cpu, RTCC_PRIORITY_ADDRESS,
                       (uint16_t)((dspic33_read_word(cpu, RTCC_PRIORITY_ADDRESS) & 0xf8ffu) |
                                  ((uint16_t)priority << 8u)));
    cpu->program[(0x0014u + RTCC_IRQ * 2u) / 2u] = RTCC_VECTOR;
    cpu->w[15] = 0x1800u;
}

static bool interrupt_flag(Dspic33* cpu) {
    return (dspic33_read_word(cpu, RTCC_FLAG_ADDRESS) & RTCC_INTERRUPT_BIT) != 0u;
}

static void set_calendar(Dspic33* cpu, uint16_t minute_second, uint16_t weekday_hour,
                         uint16_t month_day, uint16_t year) {
    cpu->io.rtcc.calendar[0] = minute_second;
    cpu->io.rtcc.calendar[1] = weekday_hour;
    cpu->io.rtcc.calendar[2] = month_day;
    cpu->io.rtcc.calendar[3] = year;
}

static void set_alarm(Dspic33* cpu, uint16_t minute_second, uint16_t weekday_hour,
                      uint16_t month_day) {
    cpu->io.rtcc.alarm[0] = minute_second;
    cpu->io.rtcc.alarm[1] = weekday_hour;
    cpu->io.rtcc.alarm[2] = month_day;
}

static bool clock_edges(Dspic33* cpu, uint32_t edges) {
    return dspic33_rtcc_clock(cpu, edges, 0u) && dspic33_device_advance(cpu, 0u);
}

static void reset_access_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, RTCC_ALARM_CONTROL) == 0u, "ALCFGRPT reset");
    expect(state, dspic33_read_word(cpu, RTCC_CONTROL) == 0u, "RCFGCAL reset");
    expect(state, dspic33_read_word(cpu, RTCC_PAD_CONTROL) == 0u, "PADCFG1 reset");
    expect(state, !cpu->io.rtcc.alarm_output && !cpu->io.rtcc.pmd_disabled, "RTCC runtime reset");
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, UINT16_MAX);
    expect(state, dspic33_read_word(cpu, RTCC_ALARM_CONTROL) == UINT16_MAX, "ALCFGRPT access mask");
    dspic33_write_word(cpu, RTCC_CONTROL, UINT16_MAX);
    expect(state, dspic33_read_word(cpu, RTCC_CONTROL) == 0x07ffu,
           "RCFGCAL protected and access masks");
    dspic33_write_word(cpu, RTCC_PAD_CONTROL, UINT16_MAX);
    expect(state, dspic33_read_word(cpu, RTCC_PAD_CONTROL) == 0x0003u, "PADCFG1 access mask");
    expect(state, !dspic33_rtcc_clock(cpu, 0u, 0u), "reject zero RTCC edge batch");
    expect(state, !dspic33_rtcc_output(cpu, NULL), "reject null RTCC output");
}

static void authorization_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, RTCC_CONTROL, RTCC_WRITE_ENABLE | RTCC_ENABLE);
    expect(state, dspic33_read_word(cpu, RTCC_CONTROL) == 0u,
           "RTCWREN and RTCEN reject unkeyed write");
    expect(state, authorize_calendar_write(cpu), "canonical RTCWREN sequence");
    expect(state, cpu->nvm.key_stage == 0u, "RTCWREN consumes NVMKEY state");
    dspic33_write_word(cpu, RTCC_CONTROL, (uint16_t)(RTCC_WRITE_ENABLE | RTCC_ENABLE | 0x0300u));
    expect(state,
           (dspic33_read_word(cpu, RTCC_CONTROL) & (RTCC_WRITE_ENABLE | RTCC_ENABLE | 0x0300u)) ==
               (RTCC_WRITE_ENABLE | RTCC_ENABLE | 0x0300u),
           "RTCWREN permits protected configuration");
    dspic33_write_word(cpu, RTCC_CONTROL, 0u);
    expect(state, dspic33_read_word(cpu, RTCC_CONTROL) == 0u, "software clears RTCWREN and RTCEN");

    dspic33_reset(cpu, 0u);
    load_write_enable_sequence(cpu);
    expect(state, step_instructions(cpu, 4u), "advance keys before delayed RTCWREN");
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 8u, 0x000000u);
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 10u, SET_RTC_WRITE_ENABLE);
    expect(state, step_instructions(cpu, 2u), "execute delayed RTCWREN sequence");
    expect(state, (dspic33_read_word(cpu, RTCC_CONTROL) & RTCC_WRITE_ENABLE) == 0u,
           "delayed RTCWREN rejected");

    dspic33_reset(cpu, 0u);
    load_write_enable_sequence(cpu);
    expect(state, step_instructions(cpu, 4u), "advance keys before multi-cycle write");
    cpu->w[0] = RTCC_WRITE_ENABLE;
    cpu->w[1] = 0u;
    cpu->w[2] = RTCC_CONTROL;
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 8u, MOVE_DOUBLE_TO_RTC_CONTROL);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, RTCC_CONTROL) & RTCC_WRITE_ENABLE) == 0u,
           "multi-cycle RCFGCAL write cannot set RTCWREN");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE, MOVE_KEY_AA);
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 2u, WRITE_NVM_KEY);
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 4u, MOVE_KEY_55);
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 6u, WRITE_NVM_KEY);
    dspic33_load_program_word(cpu, RTCC_SEQUENCE_BASE + 8u, SET_RTC_WRITE_ENABLE);
    cpu->pc = RTCC_SEQUENCE_BASE;
    expect(state, step_instructions(cpu, 5u), "execute reversed RTCWREN keys");
    expect(state, (dspic33_read_word(cpu, RTCC_CONTROL) & RTCC_WRITE_ENABLE) == 0u,
           "reversed RTCWREN keys rejected");

    dspic33_reset(cpu, 0u);
    load_write_enable_sequence(cpu);
    expect(state, step_instructions(cpu, 4u), "unlock RTCWREN before interrupt");
    cpu->w[15] = 0x1800u;
    dspic33_load_program_word(cpu, 0x0016u, RTCC_SEQUENCE_BASE + 8u);
    dspic33_write_word(cpu, 0x0800u, 0x0002u);
    dspic33_write_word(cpu, 0x0820u, 0x0002u);
    dspic33_write_word(cpu, 0x0840u, 0x0030u);
    expect(state, dspic33_device_service_interrupt(cpu), "interrupt after RTCWREN keys");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute RTCWREN after interrupt");
    expect(state, (dspic33_read_word(cpu, RTCC_CONTROL) & RTCC_WRITE_ENABLE) == 0u,
           "interrupt invalidates RTCWREN keys");

    dspic33_reset(cpu, 0u);
    load_write_enable_sequence(cpu);
    expect(state, step_instructions(cpu, 4u), "unlock RTCWREN before trap");
    cpu->w[15] = 0x1800u;
    dspic33_load_program_word(cpu, 0x000eu, RTCC_SEQUENCE_BASE + 8u);
    dspic33_raise_dma_collision_trap(cpu);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute RTCWREN after trap");
    expect(state, (dspic33_read_word(cpu, RTCC_CONTROL) & RTCC_WRITE_ENABLE) == 0u,
           "trap invalidates RTCWREN keys");
}

static void pointer_read_cases(TestState* state, Dspic33* cpu) {
    uint8_t pointer;
    for (pointer = 0u; pointer < 4u; pointer++) {
        uint16_t expected_pointer = pointer == 0u ? 0u : (uint16_t)(pointer - 1u);
        dspic33_reset(cpu, 0u);
        cpu->io.rtcc.calendar[pointer] = UINT16_MAX;
        cpu->data[RTCC_CONTROL] = 0u;
        cpu->data[RTCC_CONTROL + 1u] = pointer;
        expect(state, dspic33_read_word(cpu, RTCC_VALUE) == calendar_masks[pointer],
               "RTCVAL word read masks selected calendar value");
        expect(state, ((dspic33_read_word(cpu, RTCC_CONTROL) >> 8u) & 3u) == expected_pointer,
               "RTCVAL word read decrements RTCPTR once");

        cpu->data[RTCC_CONTROL + 1u] = pointer;
        expect(state, dspic33_read_byte(cpu, RTCC_VALUE) == (uint8_t)calendar_masks[pointer],
               "RTCVAL low-byte read returns selected calendar byte");
        expect(state, ((dspic33_read_word(cpu, RTCC_CONTROL) >> 8u) & 3u) == expected_pointer,
               "RTCVAL low-byte read decrements RTCPTR");

        cpu->data[RTCC_CONTROL + 1u] = pointer;
        expect(state,
               dspic33_read_byte(cpu, RTCC_VALUE + 1u) == (uint8_t)(calendar_masks[pointer] >> 8u),
               "RTCVAL high-byte read returns selected calendar byte");
        expect(state, ((dspic33_read_word(cpu, RTCC_CONTROL) >> 8u) & 3u) == expected_pointer,
               "RTCVAL high-byte read decrements RTCPTR");

        dspic33_reset(cpu, 0u);
        if (pointer < 3u) {
            cpu->io.rtcc.alarm[pointer] = UINT16_MAX;
        }
        cpu->data[RTCC_ALARM_CONTROL + 1u] = pointer;
        expect(state, dspic33_read_word(cpu, RTCC_ALARM_VALUE) == alarm_masks[pointer],
               "ALRMVAL word read handles selected alarm slot");
        expect(state, ((dspic33_read_word(cpu, RTCC_ALARM_CONTROL) >> 8u) & 3u) == expected_pointer,
               "ALRMVAL word read decrements ALRMPTR once");

        cpu->data[RTCC_ALARM_CONTROL + 1u] = pointer;
        expect(state, dspic33_read_byte(cpu, RTCC_ALARM_VALUE) == (uint8_t)alarm_masks[pointer],
               "ALRMVAL low-byte read returns selected alarm byte");
        expect(state, ((dspic33_read_word(cpu, RTCC_ALARM_CONTROL) >> 8u) & 3u) == expected_pointer,
               "ALRMVAL low-byte read decrements ALRMPTR");

        cpu->data[RTCC_ALARM_CONTROL + 1u] = pointer;
        expect(state,
               dspic33_read_byte(cpu, RTCC_ALARM_VALUE + 1u) ==
                   (uint8_t)(alarm_masks[pointer] >> 8u),
               "ALRMVAL high-byte read returns selected alarm byte");
        expect(state, ((dspic33_read_word(cpu, RTCC_ALARM_CONTROL) >> 8u) & 3u) == expected_pointer,
               "ALRMVAL high-byte read decrements ALRMPTR");
    }
}

static void pointer_write_cases(TestState* state, Dspic33* cpu) {
    uint8_t pointer;
    for (pointer = 0u; pointer < 4u; pointer++) {
        uint16_t expected_pointer = pointer == 0u ? 0u : (uint16_t)(pointer - 1u);
        dspic33_reset(cpu, 0u);
        expect(state, authorize_calendar_write(cpu), "authorize RTCVAL word write");
        dspic33_write_word(cpu, RTCC_CONTROL,
                           (uint16_t)(RTCC_WRITE_ENABLE | ((uint16_t)pointer << 8u)));
        dspic33_write_word(cpu, RTCC_VALUE, UINT16_MAX);
        expect(state, cpu->io.rtcc.calendar[pointer] == calendar_masks[pointer],
               "RTCVAL word write masks selected calendar value");
        expect(state, ((dspic33_read_word(cpu, RTCC_CONTROL) >> 8u) & 3u) == expected_pointer,
               "RTCVAL word write decrements RTCPTR");

        dspic33_reset(cpu, 0u);
        cpu->data[RTCC_ALARM_CONTROL + 1u] = pointer;
        dspic33_write_word(cpu, RTCC_ALARM_VALUE, UINT16_MAX);
        expect(state,
               pointer < 3u ? cpu->io.rtcc.alarm[pointer] == alarm_masks[pointer]
                            : cpu->io.rtcc.alarm[0] == 0u && cpu->io.rtcc.alarm[1] == 0u &&
                                  cpu->io.rtcc.alarm[2] == 0u,
               "ALRMVAL write handles selected alarm slot");
        expect(state, ((dspic33_read_word(cpu, RTCC_ALARM_CONTROL) >> 8u) & 3u) == expected_pointer,
               "ALRMVAL write decrements ALRMPTR");
    }

    dspic33_reset(cpu, 0u);
    cpu->io.rtcc.calendar[0] = 0x1234u;
    dspic33_write_word(cpu, RTCC_VALUE, 0x5678u);
    expect(state, cpu->io.rtcc.calendar[0] == 0x1234u, "RTCVAL write requires RTCWREN");

    dspic33_reset(cpu, 0u);
    expect(state, authorize_calendar_write(cpu), "authorize split RTCVAL write");
    cpu->io.rtcc.calendar[2] = 0x0102u;
    dspic33_write_word(cpu, RTCC_CONTROL, RTCC_WRITE_ENABLE | 0x0200u);
    dspic33_write_byte(cpu, RTCC_VALUE, 0x15u);
    expect(state, cpu->io.rtcc.calendar[2] == 0x0115u,
           "RTCVAL low-byte write updates selected byte");
    expect(state, (dspic33_read_word(cpu, RTCC_CONTROL) & 0x0300u) == 0x0200u,
           "RTCVAL low-byte write preserves pointer");
    dspic33_write_byte(cpu, RTCC_VALUE + 1u, 0x12u);
    expect(state, cpu->io.rtcc.calendar[2] == 0x1215u,
           "RTCVAL high-byte write updates selected byte");
    expect(state, (dspic33_read_word(cpu, RTCC_CONTROL) & 0x0300u) == 0x0100u,
           "RTCVAL high-byte write decrements pointer");

    dspic33_reset(cpu, 0u);
    cpu->io.rtcc.alarm[2] = 0x0102u;
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, 0x0200u);
    dspic33_write_byte(cpu, RTCC_ALARM_VALUE, 0x15u);
    expect(state, cpu->io.rtcc.alarm[2] == 0x0115u, "ALRMVAL low-byte write updates selected byte");
    expect(state, (dspic33_read_word(cpu, RTCC_ALARM_CONTROL) & 0x0300u) == 0x0200u,
           "ALRMVAL low-byte write preserves pointer");
    dspic33_write_byte(cpu, RTCC_ALARM_VALUE + 1u, 0x12u);
    expect(state, cpu->io.rtcc.alarm[2] == 0x1215u,
           "ALRMVAL high-byte write updates selected byte");
    expect(state, (dspic33_read_word(cpu, RTCC_ALARM_CONTROL) & 0x0300u) == 0x0100u,
           "ALRMVAL high-byte write decrements pointer");
}

static void transfer_context_cases(TestState* state, Dspic33* cpu) {
    uint8_t alarm;
    for (alarm = 0u; alarm < 2u; alarm++) {
        uint16_t value_address = alarm != 0u ? RTCC_ALARM_VALUE : RTCC_VALUE;
        uint16_t control_address = alarm != 0u ? RTCC_ALARM_CONTROL : RTCC_CONTROL;
        uint16_t* slot;
        dspic33_reset(cpu, 0u);
        if (alarm == 0u) {
            expect(state, authorize_calendar_write(cpu), "authorize transfer-context RTCVAL write");
            dspic33_write_word(cpu, RTCC_CONTROL, RTCC_WRITE_ENABLE | 0x0200u);
            slot = &cpu->io.rtcc.calendar[2];
        } else {
            dspic33_write_word(cpu, RTCC_ALARM_CONTROL, 0x0200u);
            slot = &cpu->io.rtcc.alarm[2];
        }
        *slot = 0x0102u;
        cpu->io.dma_transfer_active = true;
        cpu->io.dma_transfer_width = 1u;
        dspic33_write_byte(cpu, value_address, 0x15u);
        cpu->io.dma_transfer_active = false;
        expect(state, *slot == 0x0115u, "internal byte-low transfer context updates RTCC window");
        expect(state, (dspic33_read_word(cpu, control_address) & 0x0300u) == 0x0200u,
               "internal byte-low transfer context preserves RTCC pointer");

        cpu->io.dma_transfer_active = true;
        cpu->io.dma_transfer_width = 1u;
        dspic33_write_byte(cpu, value_address + 1u, 0x12u);
        cpu->io.dma_transfer_active = false;
        expect(state, *slot == 0x1215u, "internal byte-high transfer context updates RTCC window");
        expect(state, (dspic33_read_word(cpu, control_address) & 0x0300u) == 0x0100u,
               "internal byte-high transfer context decrements RTCC pointer");

        if (alarm == 0u) {
            dspic33_write_word(cpu, RTCC_CONTROL, RTCC_WRITE_ENABLE | 0x0200u);
        } else {
            dspic33_write_word(cpu, RTCC_ALARM_CONTROL, 0x0200u);
        }
        cpu->io.dma_transfer_active = true;
        cpu->io.dma_transfer_width = 2u;
        dspic33_write_word(cpu, value_address, 0x1231u);
        cpu->io.dma_transfer_active = false;
        expect(state, *slot == 0x1231u, "internal word transfer context updates RTCC window");
        expect(state, (dspic33_read_word(cpu, control_address) & 0x0300u) == 0x0100u,
               "internal word transfer context decrements RTCC pointer once");
    }
}

static void calendar_cases(TestState* state, Dspic33* cpu) {
    static const struct {
        uint16_t before[4];
        uint16_t after[4];
    } transitions[] = {
        {{0x0000u, 0x0000u, 0x0101u, 0x0000u}, {0x0001u, 0x0000u, 0x0101u, 0x0000u}},
        {{0x0059u, 0x0000u, 0x0101u, 0x0000u}, {0x0100u, 0x0000u, 0x0101u, 0x0000u}},
        {{0x5959u, 0x0012u, 0x0101u, 0x0000u}, {0x0000u, 0x0013u, 0x0101u, 0x0000u}},
        {{0x5959u, 0x0623u, 0x0131u, 0x0023u}, {0x0000u, 0x0000u, 0x0201u, 0x0023u}},
        {{0x5959u, 0x0023u, 0x0228u, 0x0023u}, {0x0000u, 0x0100u, 0x0301u, 0x0023u}},
        {{0x5959u, 0x0023u, 0x0228u, 0x0024u}, {0x0000u, 0x0100u, 0x0229u, 0x0024u}},
        {{0x5959u, 0x0623u, 0x1231u, 0x0099u}, {0x0000u, 0x0000u, 0x0101u, 0x0000u}},
    };
    size_t index;
    for (index = 0u; index < sizeof(transitions) / sizeof(transitions[0]); index++) {
        dspic33_reset(cpu, 0u);
        enable_clock(cpu);
        memcpy(cpu->io.rtcc.calendar, transitions[index].before, sizeof(transitions[index].before));
        expect(state, clock_edges(cpu, 32768u), "schedule calendar second");
        expect(state,
               memcmp(cpu->io.rtcc.calendar, transitions[index].after,
                      sizeof(transitions[index].after)) == 0,
               "calendar advances documented BCD boundary");
    }

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    expect(state, clock_edges(cpu, 16384u), "advance half second");
    expect(state,
           cpu->io.rtcc.prescaler == 16384u &&
               (dspic33_read_word(cpu, RTCC_CONTROL) & RTCC_HALF_SECOND) != 0u,
           "half-second status and prescaler");
    expect(state, clock_edges(cpu, 16352u), "advance to synchronization window");
    expect(state, (dspic33_read_word(cpu, RTCC_CONTROL) & RTCC_SYNC) != 0u,
           "RTCSYNC asserts for final 32 edges");
    expect(state, clock_edges(cpu, 32u), "complete synchronized second");
    expect(state,
           cpu->io.rtcc.prescaler == 0u &&
               (dspic33_read_word(cpu, RTCC_CONTROL) & (RTCC_SYNC | RTCC_HALF_SECOND)) == 0u,
           "second rollover clears status");

    dspic33_reset(cpu, 0u);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    expect(state, clock_edges(cpu, 32768u), "disabled clock edge batch accepted");
    expect(state, cpu->io.rtcc.calendar[0] == 0u && cpu->io.rtcc.prescaler == 0u,
           "disabled RTCC ignores SOSC edges");
    enable_clock(cpu);
    cpu->data[0x0742u] &= 0xfdu;
    expect(state, clock_edges(cpu, 32768u), "missing SOSC clock edge batch accepted");
    expect(state, cpu->io.rtcc.calendar[0] == 0u && cpu->io.rtcc.prescaler == 0u,
           "missing SOSC stops RTCC");
}

static void alarm_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t mismatch_minute_second[] = {0x3413u, 0x3422u, 0x3512u, 0x4412u,
                                                      0x3412u, 0x3412u, 0x3412u, 0x3412u};
    static const uint16_t mismatch_weekday_hour[] = {0x0212u, 0x0212u, 0x0212u, 0x0212u,
                                                     0x0213u, 0x0312u, 0x0212u, 0x0212u};
    static const uint16_t mismatch_month_day[] = {0x0810u, 0x0810u, 0x0810u, 0x0810u,
                                                  0x0810u, 0x0810u, 0x0811u, 0x0910u};
    uint8_t mask;
    for (mask = 0u; mask < 10u; mask++) {
        dspic33_reset(cpu, 0u);
        enable_clock(cpu);
        set_calendar(cpu, 0x3411u, 0x0212u, 0x0810u, 0x0026u);
        set_alarm(cpu, 0x3412u, 0x0212u, 0x0810u);
        cpu->io.rtcc.prescaler = 32767u;
        dspic33_write_word(cpu, RTCC_ALARM_CONTROL,
                           (uint16_t)(RTCC_ALARM_ENABLE | ((uint16_t)mask << 10u)));
        expect(state, clock_edges(cpu, 1u), "advance alarm comparison edge");
        expect(state, interrupt_flag(cpu), "matching alarm raises RTCCIF");
        expect(state, cpu->io.rtcc.alarm_output, "matching alarm toggles output");
        expect(state, (dspic33_read_word(cpu, RTCC_ALARM_CONTROL) & RTCC_ALARM_ENABLE) == 0u,
               "single alarm disables after match");
    }

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE | RTCC_ALARM_CHIME | 2u);
    expect(state, clock_edges(cpu, 16384u), "chime half-second event");
    expect(state,
           (dspic33_read_word(cpu, RTCC_ALARM_CONTROL) & 0x00ffu) == 1u &&
               cpu->io.rtcc.alarm_output,
           "chime decrements repeat and toggles output");
    dspic33_write_word(cpu, RTCC_FLAG_ADDRESS, 0u);
    expect(state, clock_edges(cpu, 16384u), "chime full-second event");
    expect(state,
           (dspic33_read_word(cpu, RTCC_ALARM_CONTROL) & 0x00ffu) == 0u &&
               !cpu->io.rtcc.alarm_output && interrupt_flag(cpu),
           "chime reaches zero and remains enabled");
    expect(state, clock_edges(cpu, 16384u), "chime repeat wraps");
    expect(state, (dspic33_read_word(cpu, RTCC_ALARM_CONTROL) & 0x80ffu) == 0x80ffu,
           "chime wraps ARPT and remains enabled");

    for (mask = 10u; mask < 16u; mask++) {
        dspic33_reset(cpu, 0u);
        enable_clock(cpu);
        set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
        dspic33_write_word(cpu, RTCC_ALARM_CONTROL,
                           (uint16_t)(RTCC_ALARM_ENABLE | ((uint16_t)mask << 10u)));
        expect(state, clock_edges(cpu, 32768u), "advance reserved AMASK value");
        expect(state, !interrupt_flag(cpu) && !cpu->io.rtcc.alarm_output,
               "reserved AMASK has deterministic inactive behavior");
    }

    for (mask = 2u; mask < 10u; mask++) {
        uint8_t index = (uint8_t)(mask - 2u);
        dspic33_reset(cpu, 0u);
        enable_clock(cpu);
        set_calendar(cpu, 0x3411u, 0x0212u, 0x0810u, 0x0026u);
        set_alarm(cpu, mismatch_minute_second[index], mismatch_weekday_hour[index],
                  mismatch_month_day[index]);
        cpu->io.rtcc.prescaler = 32767u;
        dspic33_write_word(cpu, RTCC_ALARM_CONTROL,
                           (uint16_t)(RTCC_ALARM_ENABLE | ((uint16_t)mask << 10u)));
        expect(state, clock_edges(cpu, 1u), "advance mismatched alarm comparison");
        expect(state, !interrupt_flag(cpu) && !cpu->io.rtcc.alarm_output,
               "mismatched alarm field does not trigger");
    }

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE | 1u);
    expect(state, clock_edges(cpu, 16384u), "advance repeating non-chime alarm");
    expect(state, (dspic33_read_word(cpu, RTCC_ALARM_CONTROL) & 0x80ffu) == 0x8000u,
           "non-chime alarm decrements a nonzero repeat count");
}

static void calibration_cases(TestState* state, Dspic33* cpu) {
    uint16_t calibration;
    for (calibration = 0u; calibration <= UINT8_MAX; calibration++) {
        int16_t signed_calibration = (int16_t)calibration;
        uint16_t adjusted_prescaler;
        uint32_t remaining_edges;
        if (signed_calibration >= 0x80) {
            signed_calibration -= 0x100;
        }
        adjusted_prescaler = (uint16_t)(512 + signed_calibration * 4);
        remaining_edges = 32768u - adjusted_prescaler;
        dspic33_reset(cpu, 0u);
        enable_clock(cpu);
        set_calendar(cpu, 0x0059u, 0x0000u, 0x0101u, 0x0000u);
        dspic33_write_byte(cpu, RTCC_CONTROL, (uint8_t)calibration);
        cpu->io.rtcc.prescaler = 32767u;
        expect(state,
               clock_edges(cpu, 1u) && cpu->io.rtcc.calendar[0] == 0x0100u &&
                   cpu->io.rtcc.prescaler == 0u && cpu->io.rtcc.calibration_pending,
               "minute rollover arms RTCC calibration");
        expect(state,
               clock_edges(cpu, 511u) && cpu->io.rtcc.prescaler == 511u &&
                   cpu->io.rtcc.calibration_pending,
               "RTCC calibration waits for edge 512");
        expect(state,
               clock_edges(cpu, 1u) && cpu->io.rtcc.prescaler == adjusted_prescaler &&
                   !cpu->io.rtcc.calibration_pending,
               "signed RTCC calibration adjusts the prescaler");
        expect(state,
               clock_edges(cpu, remaining_edges) && cpu->io.rtcc.calendar[0] == 0x0101u &&
                   cpu->io.rtcc.prescaler == 0u,
               "RTCC calibration adjusts the next-second duration");
    }

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_byte(cpu, RTCC_CONTROL, UINT8_MAX);
    expect(state,
           clock_edges(cpu, 512u) && cpu->io.rtcc.prescaler == 512u &&
               !cpu->io.rtcc.calibration_pending,
           "second zero without rollover does not calibrate");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0059u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_byte(cpu, RTCC_CONTROL, 1u);
    cpu->io.rtcc.prescaler = 32767u;
    clock_edges(cpu, 1u);
    clock_edges(cpu, 511u);
    dspic33_write_byte(cpu, RTCC_CONTROL, 127u);
    expect(state,
           clock_edges(cpu, 1u) && cpu->io.rtcc.prescaler == 1020u &&
               !cpu->io.rtcc.calibration_pending,
           "CAL write before edge 512 affects current calibration");
    dspic33_write_byte(cpu, RTCC_CONTROL, 128u);
    expect(state,
           clock_edges(cpu, 31748u) && cpu->io.rtcc.calendar[0] == 0x0101u &&
               cpu->io.rtcc.prescaler == 0u,
           "CAL write after edge 512 cannot alter completed calibration");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0059u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_byte(cpu, RTCC_CONTROL, UINT8_MAX);
    cpu->io.rtcc.prescaler = 32767u;
    expect(state, clock_edges(cpu, 1u) && cpu->io.rtcc.calibration_pending,
           "minute rollover arms calibration before MINSEC write");
    dspic33_write_word(cpu, RTCC_CONTROL,
                       (uint16_t)(dspic33_read_word(cpu, RTCC_CONTROL) & ~0x0300u));
    dspic33_write_word(cpu, RTCC_VALUE, 0u);
    expect(state, cpu->io.rtcc.prescaler == 0u && !cpu->io.rtcc.calibration_pending,
           "MINSEC write clears pending calibration");
    expect(state,
           clock_edges(cpu, 512u) && cpu->io.rtcc.prescaler == 512u &&
               !cpu->io.rtcc.calibration_pending,
           "cleared calibration cannot adjust the rewritten minute");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0059u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_byte(cpu, RTCC_CONTROL, 1u);
    cpu->io.rtcc.prescaler = 32767u;
    expect(state,
           clock_edges(cpu, 1u) && clock_edges(cpu, 100u) && cpu->io.rtcc.prescaler == 100u &&
               cpu->io.rtcc.calibration_pending,
           "RTCC calibration advances before disable");
    dspic33_write_word(cpu, RTCC_CONTROL,
                       (uint16_t)(dspic33_read_word(cpu, RTCC_CONTROL) & ~RTCC_ENABLE));
    expect(state,
           clock_edges(cpu, 500u) && cpu->io.rtcc.prescaler == 100u &&
               cpu->io.rtcc.calibration_pending,
           "RTCC disable freezes pending calibration");
    dspic33_write_word(cpu, RTCC_CONTROL,
                       (uint16_t)(dspic33_read_word(cpu, RTCC_CONTROL) | RTCC_ENABLE));
    expect(state,
           clock_edges(cpu, 412u) && cpu->io.rtcc.prescaler == 516u &&
               !cpu->io.rtcc.calibration_pending,
           "RTCC re-enable resumes pending calibration");

    for (uint8_t power = DSPIC33_POWER_SLEEP; power <= DSPIC33_POWER_IDLE; power++) {
        dspic33_reset(cpu, 0u);
        enable_clock(cpu);
        set_calendar(cpu, 0x0059u, 0x0000u, 0x0101u, 0x0000u);
        dspic33_write_byte(cpu, RTCC_CONTROL, 1u);
        cpu->io.rtcc.prescaler = 32767u;
        clock_edges(cpu, 1u);
        clock_edges(cpu, 100u);
        cpu->power_state = (Dspic33PowerState)power;
        expect(state,
               clock_edges(cpu, 412u) && cpu->io.rtcc.prescaler == 516u &&
                   !cpu->io.rtcc.calibration_pending,
               "RTCC calibration continues in power-saving mode");
    }

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0059u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_byte(cpu, RTCC_CONTROL, 1u);
    cpu->io.rtcc.prescaler = 32767u;
    clock_edges(cpu, 1u);
    clock_edges(cpu, 100u);
    cpu->data[0x0742u] &= 0xfdu;
    expect(state,
           clock_edges(cpu, 412u) && cpu->io.rtcc.prescaler == 100u &&
               cpu->io.rtcc.calibration_pending,
           "missing SOSC freezes pending RTCC calibration");
    cpu->data[0x0742u] |= 0x02u;
    expect(state,
           clock_edges(cpu, 412u) && cpu->io.rtcc.prescaler == 516u &&
               !cpu->io.rtcc.calibration_pending,
           "restored SOSC resumes pending RTCC calibration");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0059u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_byte(cpu, RTCC_CONTROL, 1u);
    cpu->io.rtcc.prescaler = 32767u;
    clock_edges(cpu, 1u);
    clock_edges(cpu, 100u);
    dspic33_write_word(cpu, RTCC_PMD_ADDRESS, RTCC_PMD);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.rtcc.pmd_disabled &&
               cpu->io.rtcc.calibration_pending,
           "RTCC PMD disable preserves pending calibration");
    uint16_t disabled_value = dspic33_read_word(cpu, RTCC_VALUE);
    dspic33_write_word(cpu, RTCC_VALUE, 0xffffu);
    expect(state, dspic33_read_word(cpu, RTCC_VALUE) == disabled_value,
           "RTCC PMD blocks calendar writes");
    expect(state,
           clock_edges(cpu, 412u) && cpu->io.rtcc.prescaler == 100u &&
               cpu->io.rtcc.calibration_pending,
           "RTCC PMD freezes pending calibration");
    dspic33_write_word(cpu, RTCC_PMD_ADDRESS, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.rtcc.pmd_disabled &&
               clock_edges(cpu, 412u) && cpu->io.rtcc.prescaler == 516u &&
               !cpu->io.rtcc.calibration_pending,
           "RTCC PMD re-enable resumes pending calibration");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0059u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_byte(cpu, RTCC_CONTROL, 1u);
    cpu->io.rtcc.prescaler = 32767u;
    expect(state,
           clock_edges(cpu, 1u) && clock_edges(cpu, 100u) && cpu->io.rtcc.calibration_pending,
           "RTCC calibration advances before warm reset");
    dspic33_load_program_word(cpu, 0u, RESET_OPCODE);
    cpu->pc = 0u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->io.rtcc.prescaler == 100u &&
               cpu->io.rtcc.calibration_pending &&
               (dspic33_read_word(cpu, RTCC_CONTROL) & 0x00ffu) == 1u,
           "warm reset preserves RTCC calibration phase");
    expect(state,
           clock_edges(cpu, 412u) && cpu->io.rtcc.prescaler == 516u &&
               !cpu->io.rtcc.calibration_pending,
           "warm-reset calibration completes at retained edge");

    {
        Dspic33 copy;
        bool initialized = dspic33_initialize(&copy);
        expect(state, initialized, "initialize RTCC calibration copy");
        if (initialized) {
            dspic33_reset(cpu, 0u);
            enable_clock(cpu);
            set_calendar(cpu, 0x0059u, 0x0000u, 0x0101u, 0x0000u);
            dspic33_write_byte(cpu, RTCC_CONTROL, 1u);
            cpu->io.rtcc.prescaler = 32767u;
            expect(state,
                   clock_edges(cpu, 1u) && clock_edges(cpu, 100u) &&
                       cpu->io.rtcc.calibration_pending,
                   "RTCC calibration advances before copy");
            expect(state, dspic33_copy(&copy, cpu), "copy pending RTCC calibration");
            expect(state, clock_edges(cpu, 412u) && clock_edges(&copy, 412u),
                   "advance copied RTCC calibrations");
            expect(state,
                   cpu->io.rtcc.prescaler == 516u && copy.io.rtcc.prescaler == 516u &&
                       !cpu->io.rtcc.calibration_pending && !copy.io.rtcc.calibration_pending,
                   "copied RTCC calibrations complete independently");
            dspic33_release(&copy);
        }
    }

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0059u, 0x0000u, 0x0101u, 0x0000u);
    cpu->io.rtcc.prescaler = 32767u;
    expect(state, clock_edges(cpu, 1u) && cpu->io.rtcc.calibration_pending,
           "RTCC calibration arms before POR");
    dspic33_reset(cpu, 0u);
    expect(state, cpu->io.rtcc.prescaler == 0u && !cpu->io.rtcc.calibration_pending,
           "POR clears RTCC calibration phase");
}

static void interrupt_output_power_cases(TestState* state, Dspic33* cpu) {
    uint8_t priority;
    bool high;
    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
    expect(state, clock_edges(cpu, 16384u), "raise enabled RTCC interrupt");
    expect(state, interrupt_flag(cpu) && !dspic33_device_wake(cpu),
           "RTCCIF sets with IEC disabled and cannot wake");
    configure_interrupt(cpu, 3u);
    expect(state, dspic33_device_service_interrupt(cpu), "service RTCC interrupt");
    expect(state, cpu->last_interrupt == RTCC_IRQ && cpu->pc == RTCC_VECTOR,
           "RTCC interrupt uses IRQ62 vector");

    dspic33_reset(cpu, 0u);
    configure_interrupt(cpu, 3u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state, !dspic33_device_wake(cpu), "RTCC IEC without RTCCIF cannot wake");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    configure_interrupt(cpu, 0u);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
    clock_edges(cpu, 16384u);
    expect(state, !dspic33_device_wake(cpu), "priority-zero RTCC alarm cannot wake");

    for (priority = 1u; priority <= 2u; priority++) {
        uint32_t pc;
        uint16_t stack;
        dspic33_reset(cpu, 0u);
        enable_clock(cpu);
        configure_interrupt(cpu, priority);
        cpu->sr = 0x0040u;
        cpu->power_state = DSPIC33_POWER_SLEEP;
        set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
        dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
        clock_edges(cpu, 16384u);
        pc = cpu->pc;
        stack = cpu->w[15];
        expect(state,
               dspic33_device_wake(cpu) && cpu->pc == pc && cpu->w[15] == stack &&
                   cpu->interrupt_count == 0u && interrupt_flag(cpu),
               "low-or-equal RTCC alarm wakes without interrupt frame");
        cpu->sr = 0u;
        expect(state,
               dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == RTCC_IRQ &&
                   cpu->interrupt_count == 1u,
               "retained RTCCIF vectors after lowering IPL");
    }

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    configure_interrupt(cpu, 2u);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
    clock_edges(cpu, 16384u);
    dspic33_write_word(cpu, 0x0820u, 0x0002u);
    dspic33_write_word(cpu, 0x0840u, 0x0040u);
    dspic33_write_word(cpu, 0x0800u, 0x0002u);
    expect(state,
           dspic33_device_wake(cpu) && cpu->last_interrupt == 1u && cpu->interrupt_count == 1u,
           "concurrent higher-priority IRQ wins wake vector");
    expect(state, interrupt_flag(cpu), "concurrent higher IRQ retains RTCCIF");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    configure_interrupt(cpu, 3u);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
    clock_edges(cpu, 16384u);
    dspic33_write_word(cpu, 0x08c2u, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->power_state == DSPIC33_POWER_ACTIVE &&
               cpu->interrupt_count == 0u && interrupt_flag(cpu),
           "GIE-disabled RTCC alarm wakes without vectoring");
    expect(state, cpu->last_interrupt == UINT16_MAX && cpu->w[15] == 0x1800u,
           "GIE-disabled wake leaves control state unchanged");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    configure_interrupt(cpu, 3u);
    cpu->disicnt = 2u;
    dspic33_load_program_word(cpu, cpu->pc, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
    clock_edges(cpu, 16384u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->power_state == DSPIC33_POWER_ACTIVE &&
               cpu->interrupt_count == 0u && cpu->disicnt == 1u && interrupt_flag(cpu),
           "DISI masks RTCC vector while allowing Sleep wake");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->interrupt_count == 0u &&
               cpu->disicnt == 0u && interrupt_flag(cpu),
           "RTCCIF remains pending through final DISI cycle");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->interrupt_count == 1u &&
               cpu->last_interrupt == RTCC_IRQ,
           "retained RTCCIF vectors after DISI expires");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    configure_interrupt(cpu, 7u);
    cpu->disicnt = 2u;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
    clock_edges(cpu, 16384u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->interrupt_count == 1u &&
               cpu->last_interrupt == RTCC_IRQ,
           "priority-seven RTCC alarm vectors through DISI");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    configure_interrupt(cpu, 2u);
    cpu->sr = 0x0040u;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
    expect(state, clock_edges(cpu, 16384u), "raise equal-priority sleep alarm");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "equal-priority alarm wakes processor");
    expect(state,
           cpu->power_state == DSPIC33_POWER_ACTIVE && interrupt_flag(cpu) &&
               cpu->interrupt_count == 0u && cpu->w[15] == 0x1800u,
           "equal-priority wake retains flag without interrupt frame");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    configure_interrupt(cpu, 3u);
    cpu->sr = 0x0040u;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
    clock_edges(cpu, 16384u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "higher-priority alarm wakes and vectors");
    expect(state,
           cpu->last_interrupt == RTCC_IRQ && cpu->interrupt_count == 1u && cpu->w[15] == 0x1804u,
           "higher-priority alarm stacks interrupt frame");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    configure_interrupt(cpu, 3u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
    expect(state, clock_edges(cpu, 16384u), "advance RTCC alarm while Idle");
    expect(state,
           cpu->io.rtcc.prescaler == 16384u && interrupt_flag(cpu) &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->power_state == DSPIC33_POWER_ACTIVE &&
               cpu->last_interrupt == RTCC_IRQ,
           "RTCC continues in Idle and alarm wakes through vector");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    dspic33_write_word(cpu, RTCC_CONTROL,
                       (uint16_t)(dspic33_read_word(cpu, RTCC_CONTROL) | RTCC_OUTPUT_ENABLE));
    expect(state, dspic33_rtcc_output(cpu, &high) && !high, "alarm output starts low");
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, RTCC_ALARM_ENABLE);
    clock_edges(cpu, 16384u);
    expect(state, dspic33_rtcc_output(cpu, &high) && high, "alarm event drives RTCC output");
    dspic33_write_word(cpu, RTCC_PAD_CONTROL, 0x0002u);
    expect(state, dspic33_rtcc_output(cpu, &high) && high, "RTSECSEL selects half-second output");
    clock_edges(cpu, 16384u);
    expect(state, dspic33_rtcc_output(cpu, &high) && !high, "seconds output follows HALFSEC");
    dspic33_write_word(cpu, RTCC_CONTROL,
                       (uint16_t)(dspic33_read_word(cpu, RTCC_CONTROL) & ~RTCC_OUTPUT_ENABLE));
    expect(state, !dspic33_rtcc_output(cpu, &high), "RTCOE disables logical output API");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    dspic33_write_word(cpu, RTCC_PMD_ADDRESS, RTCC_PMD);
    expect(state, !cpu->io.rtcc.pmd_disabled, "RTCC PMD disable is delayed");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.rtcc.pmd_disabled,
           "RTCC PMD disable applies after one cycle");
    expect(state, dspic33_read_word(cpu, RTCC_CONTROL) == 0u,
           "RTCC PMD reads module registers as zero");
    expect(state, clock_edges(cpu, 32768u) && cpu->io.rtcc.calendar[0] == 0u,
           "RTCC PMD blocks clock edges");
    dspic33_write_word(cpu, RTCC_PMD_ADDRESS, 0u);
    expect(state, dspic33_device_advance(cpu, 1u) && !cpu->io.rtcc.pmd_disabled,
           "RTCC PMD re-enable is delayed");
    expect(state, cpu->io.rtcc.calendar[0] == 0u, "RTCC PMD preserves calendar state");
}

static void lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize RTCC copy");
    if (!initialized) {
        return;
    }

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x1234u, 0x0212u, 0x0810u, 0x0026u);
    set_alarm(cpu, 0x5678u, 0x0311u, 0x0912u);
    cpu->io.rtcc.prescaler = 123u;
    cpu->io.rtcc.alarm_output = true;
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, 0x91a5u);
    expect(state, dspic33_rtcc_clock(cpu, 10u, 2u), "schedule RTCC edges before copy");
    expect(state, dspic33_copy(&copy, cpu), "copy pending RTCC state");
    expect(state, dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u),
           "advance original and copied RTCC");
    expect(state,
           memcmp(&copy.io.rtcc, &cpu->io.rtcc, sizeof(cpu->io.rtcc)) == 0 &&
               dspic33_read_word(&copy, RTCC_CONTROL) == dspic33_read_word(cpu, RTCC_CONTROL),
           "copy preserves RTCC runtime and registers");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x1234u, 0x0212u, 0x0810u, 0x0026u);
    set_alarm(cpu, 0x5678u, 0x0311u, 0x0912u);
    cpu->io.rtcc.prescaler = 123u;
    cpu->io.rtcc.alarm_output = true;
    dspic33_write_word(cpu, RTCC_ALARM_CONTROL, 0x91a5u);
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    cpu->pc = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute warm reset with RTCC");
    expect(state,
           cpu->io.rtcc.calendar[0] == 0x1234u && cpu->io.rtcc.calendar[3] == 0x0026u &&
               cpu->io.rtcc.prescaler == 123u,
           "warm reset preserves calendar and prescaler");
    expect(state, dspic33_read_word(cpu, RTCC_CONTROL) == (RTCC_ENABLE | RTCC_WRITE_ENABLE),
           "warm reset preserves RCFGCAL");
    expect(state, dspic33_read_word(cpu, RTCC_ALARM_CONTROL) == 0x91a5u,
           "warm reset preserves alarm control");
    expect(state,
           cpu->io.rtcc.alarm[0] == 0x5678u && cpu->io.rtcc.alarm[2] == 0x0912u &&
               cpu->io.rtcc.alarm_output && !cpu->io.rtcc.pmd_disabled &&
               cpu->io.rtcc.pmd_generation == 0u,
           "warm reset preserves alarm state and resets PMD state");

    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x1234u, 0x0212u, 0x0810u, 0x0026u);
    expect(state, dspic33_rtcc_clock(cpu, 100u, 5u), "schedule RTCC edges before POR");
    dspic33_reset(cpu, 0u);
    expect(state, cpu->events.count == 0u && cpu->io.rtcc.prescaler == 0u,
           "POR cancels RTCC event and clears prescaler");
    expect(state, cpu->io.rtcc.calendar[0] == 0u && cpu->io.rtcc.calendar[3] == 0u,
           "POR clears deterministic calendar state");
    expect(state, dspic33_device_advance(cpu, 5u), "advance after RTCC POR");
    expect(state, cpu->io.rtcc.prescaler == 0u, "stale RTCC event cannot advance after POR");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    expect(state, !dspic33_rtcc_clock(cpu, 1u, 1u), "RTCC edge scheduling reports overflow");
    expect(state, cpu->events.count == 0u && cpu->io.rtcc.prescaler == 0u,
           "failed RTCC edge schedule leaves state unchanged");
    dspic33_write_word(cpu, RTCC_PMD_ADDRESS, RTCC_PMD);
    expect(state,
           (dspic33_read_word(cpu, RTCC_PMD_ADDRESS) & RTCC_PMD) == 0u &&
               cpu->io.rtcc.pmd_generation == 2u,
           "failed RTCC PMD transition rolls back and invalidates generation");
    expect(state,
           !cpu->io.rtcc.pmd_disabled && cpu->events.count == 0u &&
               cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "failed RTCC PMD transition preserves effective state");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, RTCC_PMD_ADDRESS, RTCC_PMD);
    dspic33_write_word(cpu, RTCC_PMD_ADDRESS, 0u);
    expect(state, cpu->io.rtcc.pmd_generation == 2u && cpu->events.count == 2u,
           "rapid RTCC PMD toggle queues generations");
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.rtcc.pmd_disabled && cpu->events.count == 0u,
           "stale RTCC PMD event cannot override latest state");
    dspic33_release(&copy);
}

static void long_sequence_cases(TestState* state, Dspic33* cpu) {
    uint8_t elapsed;
    dspic33_reset(cpu, 0u);
    enable_clock(cpu);
    set_calendar(cpu, 0x0000u, 0x0000u, 0x0101u, 0x0000u);
    for (elapsed = 1u; elapsed <= 85u; elapsed++) {
        uint8_t expected_second = (uint8_t)(elapsed % 60u);
        uint8_t expected_minute = (uint8_t)(elapsed / 60u);
        expect(state, clock_edges(cpu, 32768u), "advance RTCC long sequence second");
        expect(state,
               cpu->io.rtcc.calendar[0] ==
                   (uint16_t)(((expected_minute / 10u * 16u + expected_minute % 10u) << 8u) |
                              (expected_second / 10u * 16u + expected_second % 10u)),
               "RTCC long sequence calendar value");
    }
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize RTCC processor");
    if (initialized) {
        reset_access_cases(&state, &cpu);
        authorization_cases(&state, &cpu);
        pointer_read_cases(&state, &cpu);
        pointer_write_cases(&state, &cpu);
        transfer_context_cases(&state, &cpu);
        calendar_cases(&state, &cpu);
        calibration_cases(&state, &cpu);
        alarm_cases(&state, &cpu);
        interrupt_output_power_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        long_sequence_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
