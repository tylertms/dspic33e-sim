#include "architecture/dspic33/execution/control/internal.h"

static bool move_matrix_registers_match(const Dspic33* cpu, const uint16_t expected[16]) {
    uint8_t reg;
    for (reg = 0u; reg < 16u; reg++) {
        if (cpu->w[reg] != expected[reg]) {
            return false;
        }
    }
    return true;
}

static void generic_move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x780000u; opcode <= 0x7fffffu; opcode += 257u) {
        uint8_t source_register = (uint8_t)(opcode & 0x0fu);
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t offset_register = (uint8_t)((opcode >> 15u) & 0x0fu);
        bool byte_mode = (opcode & 0x004000u) != 0u;
        uint8_t width = byte_mode ? 1u : 2u;
        uint16_t expected[16];
        MoveMatrixOperand source;
        MoveMatrixOperand destination;
        uint16_t value;
        uint64_t cycles;
        bool matches;

        dspic33_control_test_prepare_move_registers(cpu, expected, 0x2000u, 2u);
        source = dspic33_control_test_resolve_move_matrix_operand(
            expected, source_mode, source_register, offset_register, width);
        if (source.direct) {
            value = byte_mode ? (uint8_t)expected[source_register] : expected[source_register];
        } else if (byte_mode) {
            dspic33_write_byte(cpu, source.address, 0xa5u);
            value = 0x00a5u;
        } else {
            dspic33_write_word(cpu, source.address, 0xa55au);
            value = 0xa55au;
        }
        destination = dspic33_control_test_resolve_move_matrix_operand(
            expected, destination_mode, destination_register, offset_register, width);
        if (destination.direct) {
            if (byte_mode) {
                expected[destination_register] =
                    (uint16_t)((expected[destination_register] & 0xff00u) | value);
            } else {
                expected[destination_register] = value;
            }
            if (destination_register == 15u) {
                expected[destination_register] &= 0xfffeu;
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles - cycles == 1u && cpu->sr == 0x010fu &&
                  cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                  cpu->last_trap == UINT16_MAX && move_matrix_registers_match(cpu, expected);
        if (!destination.direct) {
            matches = matches && (byte_mode ? dspic33_read_byte(cpu, destination.address) == value
                                            : dspic33_read_word(cpu, destination.address) == value);
        }
        expect_dsp_matrix_case(state, matches, opcode, "generic MOV encoding");
    }
}

static int16_t move_offset_literal(uint32_t opcode, bool byte_mode) {
    uint16_t encoded = (uint16_t)((((opcode >> 15u) & 0x0fu) << 6u) |
                                  (((opcode >> 11u) & 0x07u) << 3u) | ((opcode >> 4u) & 0x07u));
    int16_t offset = (int16_t)(encoded | ((encoded & 0x0200u) != 0u ? 0xfc00u : 0u));
    return byte_mode ? offset : (int16_t)(offset * 2);
}

static void offset_move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x900000u; opcode <= 0x9fffffu; opcode += 257u) {
        uint8_t source = (uint8_t)(opcode & 0x0fu);
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        bool byte_mode = (opcode & 0x004000u) != 0u;
        bool store = (opcode & 0x080000u) != 0u;
        int16_t offset = move_offset_literal(opcode, byte_mode);
        uint16_t expected[16];
        uint16_t address;
        uint16_t value;
        uint64_t cycles;
        bool matches;

        dspic33_control_test_prepare_move_registers(cpu, expected, 0x4000u, 2u);
        if (store) {
            address = (uint16_t)(expected[destination] + offset);
            value = byte_mode ? (uint8_t)expected[source] : expected[source];
        } else {
            address = (uint16_t)(expected[source] + offset);
            value = byte_mode ? 0x00a5u : 0xa55au;
            if (byte_mode) {
                dspic33_write_byte(cpu, address, (uint8_t)value);
                expected[destination] = (uint16_t)((expected[destination] & 0xff00u) | value);
            } else {
                dspic33_write_word(cpu, address, value);
                expected[destination] = value;
            }
            if (destination == 15u) {
                expected[destination] &= 0xfffeu;
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles - cycles == 1u && cpu->sr == 0x010fu &&
                  cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                  cpu->last_trap == UINT16_MAX && move_matrix_registers_match(cpu, expected);
        if (store) {
            matches = matches && (byte_mode ? dspic33_read_byte(cpu, address) == value
                                            : dspic33_read_word(cpu, address) == value);
        }
        expect_dsp_matrix_case(state, matches, opcode, "offset MOV encoding");
    }
}

static void move_double_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xbe0000u; opcode <= 0xbeffffu; opcode += 257u) {
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        uint8_t source_register = (uint8_t)(opcode & 0x0fu);
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
        bool load = (opcode & 0xfff880u) == 0xbe0000u && source_mode <= 5u &&
                    (source_mode != 0u || (source_register & 1u) == 0u);
        bool store =
            (opcode & 0xffc071u) == 0xbe8000u && destination_mode >= 1u && destination_mode <= 5u;
        if (!load && !store) {
            dspic33_control_test_run_invalid_move_matrix_case(state, cpu, opcode);
            continue;
        }
        uint16_t expected[16];
        MoveMatrixOperand source;
        MoveMatrixOperand destination;
        uint16_t low;
        uint16_t high;
        uint64_t cycles;
        bool matches;

        dspic33_control_test_prepare_move_registers(cpu, expected, 0x3000u, 4u);
        source = dspic33_control_test_resolve_move_matrix_operand(expected, source_mode,
                                                                  source_register, 0u, 4u);
        if (source.direct) {
            source_register &= 0x0eu;
            low = expected[source_register];
            high = expected[source_register + 1u];
        } else {
            dspic33_write_word(cpu, source.address, 0x1111u);
            dspic33_write_word(cpu, (uint16_t)(source.address + 2u), 0x2222u);
            low = 0x1111u;
            high = 0x2222u;
        }
        destination = dspic33_control_test_resolve_move_matrix_operand(
            expected, destination_mode, destination_register, 0u, 4u);
        if (destination.direct) {
            destination_register &= 0x0eu;
            expected[destination_register] = low;
            expected[destination_register + 1u] = high;
            if (destination_register + 1u == 15u) {
                expected[15] &= 0xfffeu;
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles - cycles == 2u && cpu->sr == 0x010fu &&
                  cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                  cpu->last_trap == UINT16_MAX && move_matrix_registers_match(cpu, expected);
        if (!destination.direct) {
            matches = matches && dspic33_read_word(cpu, destination.address) == low &&
                      dspic33_read_word(cpu, (uint16_t)(destination.address + 2u)) == high;
        }
        expect_dsp_matrix_case(state, matches, opcode, "MOV.D encoding");
        if (load) {
        } else {
        }
    }
}

static void move_data_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    expect(state, dspic33_load_program_word(cpu, 0x000006u, 0x000340u),
           "load direct data MOV address trap vector");
    for (opcode = 0x800000u; opcode <= 0x8fffffu; opcode += 257u) {
        bool store = (opcode & 0x080000u) != 0u;
        uint8_t reg = (uint8_t)(opcode & 0x0fu);
        uint16_t encoded_address = (uint16_t)((((opcode >> 4u) & 0x7fffu) << 1u) & 0xffffu);
        uint32_t address;
        uint16_t transfer_value;
        uint64_t cycles;
        Dspic33StopReason reason;
        bool matches;

        dspic33_reset(cpu, 0u);
        dspic33_set_async_events(cpu, false);
        dspic33_control_test_prepare_move_matrix_case(cpu);
        cpu->stop_on_trap = true;
        cpu->dsrpag = 1u;
        cpu->dswpag = 1u;
        dspic33_set_working_register(cpu, 15u, 0x5000u);
        address = encoded_address < 0x8000u ? encoded_address
                                            : (uint32_t)(0x8000u | (encoded_address & 0x7fffu));
        if (store) {
            dspic33_set_working_register(cpu, reg,
                                         address >= 0x1000u ? (uint16_t)(0xa500u | reg) : 0u);
            transfer_value = cpu->w[reg];
        } else {
            dspic33_set_working_register(cpu, reg, 0x5a5au);
            transfer_value = 0xa55au;
            if (address >= 0x1000u && encoded_address < 0xe000u) {
                dspic33_write_word(cpu, address, 0xa55au);
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode);
        reason = dspic33_step(cpu);
        if (encoded_address >= 0xe000u) {
            matches = matches && reason == DSPIC33_TRAPPED && cpu->pc == 0x000340u &&
                      cpu->last_trap == 1u && cpu->last_trap_return == 2u;
        } else {
            matches = matches && reason == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->last_trap == UINT16_MAX;
        }
        matches =
            matches && cpu->cycles > cycles && cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
        if (address >= 0x1000u && encoded_address < 0xe000u) {
            matches =
                matches &&
                (store ? dspic33_read_word(cpu, address) == transfer_value
                       : cpu->w[reg] == (reg == 15u ? (transfer_value & 0xfffeu) : transfer_value));
        }
        expect_dsp_matrix_case(state, matches, opcode, "direct data MOV encoding");
    }
}

static uint16_t move_matrix_logic_status(uint16_t status, uint16_t value, bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    value &= mask;
    status &= (uint16_t)~0x000au;
    if (value == 0u) {
        status |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        status |= 0x0008u;
    }
    return status;
}

static uint16_t move_matrix_file_value(uint16_t address, bool byte_mode) {
    switch (address & 3u) {
    case 0u:
        return 0u;
    case 1u:
        return byte_mode ? 0x0080u : 0x8000u;
    default:
        return byte_mode ? 0x0034u : 0x1234u;
    }
}

static void file_move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint16_t address;
    uint8_t byte_mode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    expect(state, dspic33_load_program_word(cpu, 0x000006u, 0x000340u),
           "load file MOV address trap vector");
    for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        for (address = 0u; address < 0x2000u; address += 31u) {
            uint32_t opcode = 0xb7a000u | ((uint32_t)byte_mode << 14u) | address;
            uint64_t cycles;
            Dspic33StopReason reason;
            bool matches;

            dspic33_reset(cpu, 0u);
            dspic33_set_async_events(cpu, false);
            dspic33_control_test_prepare_move_matrix_case(cpu);
            cpu->stop_on_trap = true;
            dspic33_set_working_register(cpu, 0u, address >= 0x1000u ? 0xa5a5u : 0u);
            dspic33_set_working_register(cpu, 15u, 0x5000u);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode);
            reason = dspic33_step(cpu);
            if (byte_mode == 0u && (address & 1u) != 0u) {
                matches = matches && reason == DSPIC33_TRAPPED && cpu->pc == 0x000340u &&
                          cpu->last_trap == 1u && cpu->last_trap_return == 2u;
            } else {
                matches = matches && reason == DSPIC33_RUNNING && cpu->pc == 2u &&
                          cpu->last_trap == UINT16_MAX;
            }
            matches = matches && cpu->cycles > cycles && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset;
            if (address >= 0x1000u && (byte_mode != 0u || (address & 1u) == 0u)) {
                matches = matches && (byte_mode != 0u ? dspic33_read_byte(cpu, address) == 0xa5u
                                                      : dspic33_read_word(cpu, address) == 0xa5a5u);
            }
            expect_dsp_matrix_case(state, matches, opcode, "WREG-to-file MOV encoding");
        }
    }

    for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        uint8_t file_destination;
        for (file_destination = 0u; file_destination < 2u; file_destination++) {
            for (address = 0u; address < 0x2000u; address += 31u) {
                uint32_t opcode = 0xbf8000u | ((uint32_t)byte_mode << 14u) |
                                  ((uint32_t)file_destination << 13u) | address;
                uint16_t expected[16];
                uint16_t value;
                uint16_t expected_status;
                uint64_t cycles;
                Dspic33StopReason reason;
                bool matches;

                dspic33_reset(cpu, 0u);
                dspic33_set_async_events(cpu, false);
                dspic33_control_test_prepare_move_registers(cpu, expected, 0x2000u, 2u);
                cpu->stop_on_trap = true;
                value = move_matrix_file_value(address, byte_mode != 0u);
                if (address >= 0x1000u) {
                    if (byte_mode != 0u) {
                        dspic33_write_byte(cpu, address, (uint8_t)value);
                    } else {
                        dspic33_write_word(cpu, address, value);
                    }
                } else if (address == 0x002eu) {
                    value = 2u;
                } else if (address == 0x0030u) {
                    value = 0u;
                } else {
                    value = byte_mode != 0u ? dspic33_read_byte(cpu, address)
                                            : dspic33_read_word(cpu, address);
                }
                expected_status = move_matrix_logic_status(0x010fu, value, byte_mode != 0u);
                if (file_destination == 0u) {
                    expected[0] =
                        byte_mode != 0u ? (uint16_t)((expected[0] & 0xff00u) | value) : value;
                }
                cycles = cpu->cycles;
                matches = dspic33_load_program_word(cpu, 0u, opcode);
                reason = dspic33_step(cpu);
                if (byte_mode == 0u && (address & 1u) != 0u) {
                    matches = matches && reason == DSPIC33_TRAPPED && cpu->pc == 0x000340u &&
                              cpu->last_trap == 1u && cpu->last_trap_return == 2u;
                } else {
                    matches = matches && reason == DSPIC33_RUNNING && cpu->pc == 2u &&
                              cpu->last_trap == UINT16_MAX && cpu->sr == expected_status &&
                              move_matrix_registers_match(cpu, expected);
                }
                matches = matches && cpu->cycles > cycles && cpu->unsupported_opcode == 0u &&
                          !cpu->illegal_reset;
                expect_dsp_matrix_case(state, matches, opcode, "file-to-destination MOV encoding");
            }
        }
    }
}

void dspic33_control_test_move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    dspic33_control_test_move_literal_encoding_matrix_cases(state, cpu);
    dspic33_control_test_move_register_encoding_matrix_cases(state, cpu);
    dspic33_control_test_movpag_encoding_matrix_cases(state, cpu);
    generic_move_encoding_matrix_cases(state, cpu);
    offset_move_encoding_matrix_cases(state, cpu);
    move_double_encoding_matrix_cases(state, cpu);
    move_data_encoding_matrix_cases(state, cpu);
    file_move_encoding_matrix_cases(state, cpu);
}

static void prepare_flash_read_erratum_case(TestState* state, Dspic33* cpu, uint32_t start) {
    reset_processor_test(cpu, start);
    load_instruction(state, cpu, start, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    expect(state,
           dspic33_load_program_word(cpu, 0x1000u, 0x001000u) &&
               dspic33_load_program_word(cpu, 0x1002u, 0u),
           "load B1 Flash-read erratum PSV MOV.D data");
    dspic33_set_working_register(cpu, 1u, 0x9000u);
    cpu->dsrpag = 0x0200u;
    cpu->tblpag = 0u;
}

void dspic33_control_test_flash_read_erratum_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t flash_read_pairs[][2] = {
        {OPCODE_MOV_W1_W2, OPCODE_MOV_W1_W2},
        {OPCODE_TBLRDL_W2_W3, OPCODE_MOV_W1_W2},
    };
    Dspic33 copy;
    bool initialized;
    size_t index;

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "B1 back-to-back Flash-read sequence reports an undefined silicon result");

    for (index = 0u; index < sizeof(flash_read_pairs) / sizeof(flash_read_pairs[0]); index++) {
        prepare_flash_read_erratum_case(state, cpu, 0x200u);
        load_instruction(state, cpu, 0x202u, OPCODE_NOP);
        load_instruction(state, cpu, 0x204u, OPCODE_NOP);
        load_instruction(state, cpu, 0x206u, flash_read_pairs[index][0]);
        load_instruction(state, cpu, 0x208u, flash_read_pairs[index][1]);
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
               "B1 PSV and mixed back-to-back Flash reads share the erratum boundary");
    }

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_NOP);
    load_instruction(state, cpu, 0x20au, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING,
           "separating Flash reads with a NOP applies the documented workaround");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, 0x370000u);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x20au, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING,
           "BRA to the next instruction applies the documented flow workaround");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, 0x090001u);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "REPEAT-ended connecting code does not arm the B1 Flash-read erratum");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0300u, OPCODE_NOP);
    load_instruction(state, cpu, 0x0302u, OPCODE_RETFIE);
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING,
           "B1 Flash-read connecting code reaches the interrupt boundary");
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u &&
               !cpu->flash_read_erratum_armed,
           "interrupt vectoring cancels the B1 Flash-read sequence");
    dspic33_write_word(cpu, 0x0800u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0204u &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "post-interrupt Flash reads execute outside the cancelled erratum sequence");

    prepare_flash_read_erratum_case(state, cpu, 0x202u);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_NOP);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x20au, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "misaligned PSV MOV.D does not arm the B1 Flash-read erratum");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->flash_read_erratum_candidate,
           "first qualifying Flash read arms the B1 erratum boundary");
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize B1 Flash-read erratum copy");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy armed B1 Flash-read state");
        expect(state,
               dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED &&
                   dspic33_step(&copy) == DSPIC33_SILICON_RESULT_UNDEFINED,
               "copied B1 Flash-read state retains the same sequence boundary");
        dspic33_release(&copy);
    }

    dspic33_reset(cpu, 0x200u);
    expect(state,
           !cpu->flash_read_erratum_armed && !cpu->flash_read_erratum_candidate &&
               cpu->flash_read_connecting_words == 0u && !cpu->flash_read_connecting_ends_repeat,
           "reset clears B1 Flash-read sequence state");
}

void dspic33_control_test_do_flash_access_erratum_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t opcodes[] = {
        OPCODE_TBLRDL_W2_W3,
        OPCODE_TBLWTL_W2_W3,
        OPCODE_MOV_W1_W2,
    };
    size_t index;
    for (index = 0u; index < sizeof(opcodes) / sizeof(opcodes[0]); index++) {
        uint32_t boundary;
        for (boundary = 0x0200u; boundary <= 0x0204u; boundary += 4u) {
            reset_processor_test(cpu, boundary);
            load_instruction(state, cpu, boundary, opcodes[index]);
            cpu->do_depth = 1u;
            cpu->do_start[0] = 0x0200u;
            cpu->do_end[0] = 0x0204u;
            cpu->do_count[0] = 1u;
            cpu->dostart = 0x0200u;
            cpu->doend = 0x0204u;
            cpu->dcount = 1u;
            dspic33_set_working_register(cpu, 1u, 0xc000u);
            dspic33_set_working_register(cpu, 2u, 0u);
            dspic33_set_working_register(cpu, 3u, 0x1357u);
            cpu->dsrpag = 0x0200u;
            expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
                   "B1 DO boundary Flash access reports an undefined silicon result");
        }
    }

    reset_processor_test(cpu, 0x0202u);
    load_instruction(state, cpu, 0x0202u, OPCODE_TBLRDL_W2_W3);
    cpu->do_depth = 1u;
    cpu->do_start[0] = 0x0200u;
    cpu->do_end[0] = 0x0204u;
    cpu->do_count[0] = 1u;
    cpu->dostart = 0x0200u;
    cpu->doend = 0x0204u;
    cpu->dcount = 1u;
    dspic33_set_working_register(cpu, 2u, 0u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "Flash access inside a DO body remains defined away from both boundaries");
}

void dspic33_control_test_illegal_condition_reset_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t preserved_addresses[] = {
        0x0742u, 0x0744u, 0x0746u, 0x0748u, 0x0758u, 0x075au,
    };
    uint16_t preserved_values[sizeof(preserved_addresses) / sizeof(preserved_addresses[0])];
    Dspic33 copy;
    size_t index;

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ILLEGAL);
    dspic33_set_working_register(cpu, 0u, 0x1234u);
    dspic33_set_working_register(cpu, 1u, 0x5000u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    cpu->splim = 0x6000u;
    cpu->splim_enabled = true;
    cpu->rcount = 3u;
    cpu->dcount = 4u;
    cpu->dostart = 0x100u;
    cpu->doend = 0x200u;
    cpu->tblpag = 0xa5u;
    cpu->dsrpag = 0x123u;
    cpu->dswpag = 0x123u;
    cpu->repeat_active = 1u;
    cpu->do_depth = 1u;
    cpu->data[0x0740u] = 0x83u;
    cpu->data[0x0741u] = 0u;
    for (index = 0u; index < sizeof(preserved_addresses) / sizeof(preserved_addresses[0]);
         index++) {
        uint16_t address = preserved_addresses[index];
        preserved_values[index] = (uint16_t)(0xa100u + index);
        cpu->data[address] = (uint8_t)preserved_values[index];
        cpu->data[address + 1u] = (uint8_t)(preserved_values[index] >> 8u);
    }
    dspic33_write_word(cpu, 0x074eu, 0x8500u);
    cpu->io.adc[3] = 0x0456u;
    cpu->io.gpio[2] = 0x789au;
    cpu->io.uart_cts = 0x05u;
    cpu->io.spi_selected = 0x09u;
    cpu->io.timer_gate = 0x0105u;
    cpu->io.pwm_dead_time_inputs = 0x25u;
    cpu->io.pwm_sync_inputs = 0x02u;
    cpu->io.pwm_fault_inputs = 0x81234567u;
    cpu->io.pwm_current_limit_inputs = 0x89abcdefu;
    cpu->io.pwm_dead_time_direct = 0x25u;
    cpu->io.pwm_sync_direct = 0x02u;
    cpu->io.pwm_fault_direct = 0x81234567u;
    cpu->io.pwm_current_limit_direct = 0x89abcdefu;
    cpu->io.usb_host_attached = true;
    cpu->io.timer_enabled = 0xffffu;
    cpu->io.uart_rx_fifo[0].count = 1u;
    cpu->io.usb_host_pending = true;
    cpu->io.cpu_write_valid = true;
    expect(state, dspic33_uart_set_cts(cpu, 0u, true, 20u),
           "schedule peripheral state before illegal reset");
    dspic33_write_word(cpu, 0x0100u, 0xffffu);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    dspic33_write_word(cpu, 0x5002u, 0x5555u);
    expect_illegal_reset(state, cpu, "known illegal opcode resets processor");
    expect(state,
           dspic33_read_word(cpu, 0x0740u) == 0x4083u && cpu->sr == 0u && cpu->corcon == 0x0020u &&
               cpu->splim == 0u && !cpu->splim_enabled,
           "illegal reset preserves RCON history and resets core state");
    expect(state,
           cpu->rcount == 0u && cpu->dcount == 0u && cpu->dostart == 0u && cpu->doend == 0u &&
               cpu->tblpag == 0u && cpu->dsrpag == 1u && cpu->dswpag == 1u &&
               cpu->repeat_active == 0u && cpu->do_depth == 0u,
           "illegal reset restores loop and page state");
    expect(state,
           dspic33_read_word(cpu, 0x5000u) == 0xaaaau &&
               dspic33_read_word(cpu, 0x5002u) == 0x5555u && dspic33_read_word(cpu, 0x0100u) == 0u,
           "warm reset retains RAM without writing an exception frame");
    for (index = 0u; index < sizeof(preserved_addresses) / sizeof(preserved_addresses[0]);
         index++) {
        expect(state, dspic33_read_word(cpu, preserved_addresses[index]) == preserved_values[index],
               "warm reset retains oscillator and RTCC register");
    }
    expect(state, dspic33_read_word(cpu, 0x074eu) == 0u,
           "warm reset clears reference oscillator control");
    expect(state,
           cpu->io.adc[3] == 0x0456u && cpu->io.gpio[2] == 0x789au && cpu->io.uart_cts == 0x05u &&
               cpu->io.spi_selected == 0x09u && cpu->io.timer_gate == 0x0105u &&
               cpu->io.pwm_dead_time_inputs == 0x25u && cpu->io.pwm_sync_inputs == 0x02u &&
               cpu->io.pwm_fault_inputs == 0x81234567u &&
               cpu->io.pwm_current_limit_inputs == 0x89abcdefu && cpu->io.usb_host_attached,
           "warm reset retains external input state");
    expect(state,
           cpu->io.timer_enabled == 0u && cpu->io.uart_rx_fifo[0].count == 0u &&
               !cpu->io.usb_host_pending && !cpu->io.cpu_write_valid && cpu->events.count == 1u &&
               cpu->events.items[0].external && cpu->events.items[0].type == DSPIC33_EVENT_UART &&
               cpu->events.items[0].cycle == 20u,
           "warm reset clears internal peripheral execution state");
    expect(state, dspic33_initialize(&copy), "initialize illegal reset copy");
    expect(state, dspic33_copy(&copy, cpu), "copy illegal reset state");
    expect(state,
           copy.illegal_reset && copy.illegal_reset_count == 1u &&
               copy.initialized_working_registers == 0x8000u,
           "copy retains illegal reset lifecycle state");
    dspic33_release(&copy);
    load_instruction(state, cpu, 0u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->illegal_reset,
           "next instruction clears transient illegal reset state");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_TBL_LITERAL);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->tblpag == 0x00a5u &&
               cpu->illegal_reset_count == 0u && cpu->cycles == 1u,
           "MOVPAG literal PP2 writes TBLPAG");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_INVALID_LITERAL);
    expect_illegal_reset(state, cpu, "MOVPAG literal PP3 resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_TBL_W1);
    dspic33_set_working_register(cpu, 1u, 0x00a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->tblpag == 0x00a5u &&
               cpu->illegal_reset_count == 0u && cpu->cycles == 1u,
           "MOVPAG register PP2 writes TBLPAG");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_TBL_W1);
    cpu->w[1] = 0x00a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->tblpag == 0x00a5u &&
               cpu->illegal_reset_count == 0u,
           "MOVPAG data source ignores initialization tag");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_INVALID_W1);
    dspic33_set_working_register(cpu, 1u, 0x00a5u);
    expect_illegal_reset(state, cpu, "MOVPAG register PP3 resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    cpu->w[1] = 0x1000u;
    dspic33_write_word(cpu, 0x1000u, 0x1234u);
    expect_illegal_reset(state, cpu, "raw host W pointer does not initialize register");

    dspic33_reset(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_BYTE_LITERAL_W1);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W1_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (cpu->initialized_working_registers & 0x0002u) == 0u,
           "byte instruction destination leaves W pointer uninitialized");
    expect_illegal_reset(state, cpu, "byte instruction result remains invalid pointer");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W15_W2);
    dspic33_write_word(cpu, 0x1000u, 0xa5a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0xa5a5u &&
               cpu->illegal_reset_count == 0u,
           "W15 indirect access is initialized after reset");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_set_working_register(cpu, 1u, 0u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "host word setter initializes same-value pointer");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_write_byte(cpu, 2u, 0u);
    expect_illegal_reset(state, cpu, "single byte W alias leaves pointer uninitialized");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_write_byte(cpu, 2u, 0u);
    dspic33_write_byte(cpu, 3u, 0x10u);
    expect_illegal_reset(state, cpu, "two byte W aliases leave pointer uninitialized");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_write_word(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x1234u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1234u &&
               (cpu->initialized_working_registers & 0x0006u) == 0x0006u,
           "word W alias initializes pointer and destination");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_set_working_register(cpu, 1u, 0x1000u);
    dspic33_write_byte(cpu, 2u, 0u);
    dspic33_write_word(cpu, 0x1000u, 0x5678u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x5678u,
           "byte W alias preserves prior initialized state");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1);
    cpu->w[1] = 0x5000u;
    dspic33_set_working_register(cpu, 2u, 0xbeefu);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    expect_illegal_reset(state, cpu, "uninitialized store pointer resets processor");
    expect(state, dspic33_read_word(cpu, 0x5000u) == 0xaaaau,
           "uninitialized store does not modify RAM");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ADD_W2_W4_POST_INCREMENT_W5_POST_DECREMENT);
    dspic33_set_working_register(cpu, 2u, 1u);
    dspic33_set_working_register(cpu, 4u, 0x1000u);
    cpu->w[5] = 0x5000u;
    dspic33_write_word(cpu, 0x1000u, 2u);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    expect_illegal_reset(state, cpu, "uninitialized two-operand destination resets processor");
    expect(state, !cpu->address_error, "illegal reset clears address error flag");
    expect(state, !cpu->address_error_access_allowed,
           "illegal reset clears address error access state");
    expect(state, !cpu->address_error_working_state_completed,
           "illegal reset clears address error working state");
    expect(state, !cpu->address_error_accumulator_state_completed,
           "illegal reset clears address error accumulator state");
    expect(state, !cpu->address_error_control_state_completed,
           "illegal reset clears address error control state");
    expect(state, cpu->address_error_return == 0u, "illegal reset clears address error return");
    expect(state, cpu->sr == 0u && cpu->corcon == 0x0020u,
           "illegal reset discards post-validation flags");
    expect(state,
           dspic33_read_word(cpu, 0x1000u) == 2u && dspic33_read_word(cpu, 0x5000u) == 0xaaaau,
           "uninitialized destination preserves source and destination data");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ADD_W2_W4_POST_INCREMENT_W5);
    cpu->w[2] = 7u;
    cpu->w[4] = 0x1000u;
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    dspic33_write_word(cpu, 0x1000u, 0xabcdu);
    expect_illegal_reset(state, cpu, "uninitialized binary source resets before direct result");
    expect(state,
           cpu->w[5] == 0u && cpu->sr == 0u && cpu->corcon == 0x0020u &&
               dspic33_read_word(cpu, 0x1000u) == 0xabcdu,
           "binary source reset prevents data, result and flag mutation");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_COMPARE_ZERO_W4_POST_INCREMENT);
    cpu->w[4] = 0x1000u;
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    expect_illegal_reset(state, cpu, "uninitialized compare source resets before flags");
    expect(state, cpu->sr == 0u && cpu->corcon == 0x0020u,
           "compare source reset prevents flag mutation");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_ADD_W4_POST_INCREMENT);
    cpu->w[4] = 0x1000u;
    cpu->accumulator[0] = 0x12345678;
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    expect_illegal_reset(state, cpu, "uninitialized accumulator source resets before result");
    expect(state, cpu->accumulator[0] == 0 && cpu->sr == 0u && cpu->corcon == 0x0020u,
           "accumulator source reset prevents result and flag mutation");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    cpu->w[2] = 0x1000u;
    expect_illegal_reset(state, cpu, "uninitialized table pointer resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "initialized table pointer completes access");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W4_POST_INCREMENT);
    cpu->tblpag = 5u;
    dspic33_set_working_register(cpu, 2u, 0x5800u);
    cpu->w[4] = 0x1000u;
    expect_illegal_reset(
        state, cpu, "unimplemented table read with uninitialized destination resets processor");
    expect(state,
           !cpu->address_error && !cpu->address_error_access_allowed &&
               !cpu->address_error_working_state_completed &&
               !cpu->address_error_accumulator_state_completed &&
               !cpu->address_error_control_state_completed && cpu->address_error_return == 0u,
           "table destination reset clears address error lifecycle state");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    cpu->w[9] = 0x1000u;
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    expect_illegal_reset(state, cpu, "uninitialized DSP base resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    dspic33_set_working_register(cpu, 9u, 0x1000u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "initialized DSP bases complete access");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_WRITE_BACK);
    dspic33_set_working_register(cpu, 9u, 0x1000u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    cpu->w[13] = 0x5000u;
    expect_illegal_reset(state, cpu, "uninitialized DSP write-back pointer resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_WRITE_BACK);
    dspic33_set_working_register(cpu, 9u, 0x1000u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    dspic33_set_working_register(cpu, 13u, 0x5000u);
    cpu->w[4] = 1u;
    cpu->w[5] = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[13] == 0x5002u &&
               cpu->illegal_reset_count == 0u,
           "initialized DSP write-back ignores multiplicand tags");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_DIRECT_W13);
    cpu->w[4] = 1u;
    cpu->w[5] = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (cpu->initialized_working_registers & 0x2000u) != 0u &&
               cpu->illegal_reset_count == 0u,
           "direct DSP W13 result initializes destination");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_W0);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "computed jump register is not an address-pointer tag use");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_REPEAT_W0);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "REPEAT count register is not an address-pointer tag use");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DO_W0);
    load_instruction(state, cpu, 2u, 0x000002u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "DO count register is not an address-pointer tag use");

    dspic33_reset(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_PUSH_SHADOW);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W0_W2);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "PUSH.S values are not address-pointer tag uses");
    expect_illegal_reset(state, cpu, "PUSH.S does not initialize W0 pointer");
}
