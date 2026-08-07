#ifndef OPENTEC_DSPIC33_H
#define OPENTEC_DSPIC33_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DSPIC33_DATA_SIZE 0x10000u
#define DSPIC33_PROGRAM_LIMIT 0x55800u
#define DSPIC33_PROGRAM_WORDS (DSPIC33_PROGRAM_LIMIT / 2u)

typedef enum {
    DSPIC33_RUNNING,
    DSPIC33_RETURNED,
    DSPIC33_UNSUPPORTED_INSTRUCTION,
    DSPIC33_PROGRAM_BOUNDS,
    DSPIC33_INSTRUCTION_LIMIT
} Dspic33StopReason;

typedef struct {
    uint32_t* program;
    uint8_t data[DSPIC33_DATA_SIZE];
    uint16_t w[16];
    uint32_t pc;
    uint16_t sr;
    uint16_t call_depth;
    uint64_t instructions;
    uint32_t unsupported_opcode;
    Dspic33StopReason stop_reason;
} Dspic33;

bool dspic33_initialize(Dspic33* cpu);
void dspic33_destroy(Dspic33* cpu);
void dspic33_reset(Dspic33* cpu, uint32_t entry);
bool dspic33_load_program_word(Dspic33* cpu, uint32_t address, uint32_t word);
void dspic33_write_byte(Dspic33* cpu, uint16_t address, uint8_t value);
void dspic33_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
uint8_t dspic33_read_byte(const Dspic33* cpu, uint16_t address);
Dspic33StopReason dspic33_run(Dspic33* cpu, uint64_t instruction_limit);
const char* dspic33_stop_reason_name(Dspic33StopReason reason);

#endif
