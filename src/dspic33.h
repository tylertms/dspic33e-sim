#ifndef OPENTEC_DSPIC33_H
#define OPENTEC_DSPIC33_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DSPIC33_DATA_SIZE 0x100000u
#define DSPIC33_PROGRAM_LIMIT 0x55800u
#define DSPIC33_PROGRAM_WORDS (DSPIC33_PROGRAM_LIMIT / 2u)
#define DSPIC33_IRQ_COUNT 133u
#define DSPIC33_UART_COUNT 4u
#define DSPIC33_SPI_COUNT 4u
#define DSPIC33_CAN_COUNT 2u
#define DSPIC33_TIMER_COUNT 9u
#define DSPIC33_DMA_COUNT 15u
#define DSPIC33_ADC_CHANNEL_COUNT 32u
#define DSPIC33_GPIO_PORT_COUNT 7u

typedef enum {
    DSPIC33_EVENT_INTERRUPT,
    DSPIC33_EVENT_TIMER,
    DSPIC33_EVENT_DMA,
    DSPIC33_EVENT_ADC,
    DSPIC33_EVENT_UART,
    DSPIC33_EVENT_SPI,
    DSPIC33_EVENT_CAN,
    DSPIC33_EVENT_USB
} Dspic33EventType;

typedef struct {
    uint64_t cycle;
    uint64_t sequence;
    uint32_t value;
    uint16_t source;
    Dspic33EventType type;
} Dspic33Event;

typedef struct {
    Dspic33Event* items;
    size_t count;
    size_t capacity;
    uint64_t sequence;
} Dspic33EventQueue;

typedef struct {
    uint8_t bytes[1024];
    uint16_t head;
    uint16_t count;
} Dspic33ByteQueue;

typedef struct {
    uint32_t identifier;
    uint8_t data[8];
    uint8_t length;
    bool extended;
    bool remote;
} Dspic33CanFrame;

typedef struct {
    Dspic33CanFrame frames[64];
    uint8_t head;
    uint8_t count;
} Dspic33CanQueue;

typedef struct {
    Dspic33ByteQueue uart_rx[DSPIC33_UART_COUNT];
    Dspic33ByteQueue uart_tx[DSPIC33_UART_COUNT];
    Dspic33ByteQueue spi_rx[DSPIC33_SPI_COUNT];
    Dspic33ByteQueue spi_tx[DSPIC33_SPI_COUNT];
    Dspic33CanQueue can_rx[DSPIC33_CAN_COUNT];
    Dspic33CanQueue can_tx[DSPIC33_CAN_COUNT];
    uint16_t adc[DSPIC33_ADC_CHANNEL_COUNT];
    uint16_t gpio[DSPIC33_GPIO_PORT_COUNT];
    uint16_t pwm[12];
    uint32_t timer_fraction[DSPIC33_TIMER_COUNT];
    uint16_t timer_enabled;
    uint16_t dma_index[DSPIC33_DMA_COUNT];
    uint16_t dma_enabled;
    uint16_t dma_bank;
    uint8_t usb[4096];
    uint16_t usb_size;
} Dspic33Io;

typedef enum {
    DSPIC33_RUNNING,
    DSPIC33_RETURNED,
    DSPIC33_HALTED,
    DSPIC33_UNSUPPORTED_INSTRUCTION,
    DSPIC33_PROGRAM_BOUNDS,
    DSPIC33_INSTRUCTION_LIMIT,
    DSPIC33_EVENT_QUEUE_ERROR
} Dspic33StopReason;

typedef struct {
    uint32_t* program;
    uint8_t* data;
    uint16_t w[16];
    int64_t accumulator[2];
    uint32_t pc;
    uint16_t sr;
    uint16_t corcon;
    uint16_t splim;
    uint16_t rcount;
    uint16_t dcount;
    uint32_t dostart;
    uint32_t doend;
    uint16_t tblpag;
    uint16_t dsrpag;
    uint16_t dswpag;
    uint16_t disicnt;
    uint16_t call_depth;
    uint8_t interrupt_depth;
    uint8_t repeat_active;
    uint8_t do_depth;
    uint32_t repeat_pc;
    uint32_t do_start[4];
    uint32_t do_end[4];
    uint16_t do_count[4];
    uint64_t instructions;
    uint64_t cycles;
    uint32_t unsupported_opcode;
    Dspic33EventQueue events;
    Dspic33Io io;
    Dspic33StopReason stop_reason;
} Dspic33;

bool dspic33_initialize(Dspic33* cpu);
void dspic33_destroy(Dspic33* cpu);
void dspic33_reset(Dspic33* cpu, uint32_t entry);
bool dspic33_load_program_word(Dspic33* cpu, uint32_t address, uint32_t word);
void dspic33_write_byte(Dspic33* cpu, uint32_t address, uint8_t value);
void dspic33_write_word(Dspic33* cpu, uint32_t address, uint16_t value);
uint8_t dspic33_read_byte(Dspic33* cpu, uint32_t address);
uint16_t dspic33_read_word(Dspic33* cpu, uint32_t address);
bool dspic33_schedule(Dspic33* cpu, Dspic33EventType type, uint16_t source,
                      uint32_t value, uint64_t delay);
void dspic33_raise_interrupt(Dspic33* cpu, uint16_t irq);
bool dspic33_uart_receive(Dspic33* cpu, uint8_t channel, uint8_t value, uint64_t delay);
bool dspic33_spi_receive(Dspic33* cpu, uint8_t channel, uint16_t value, uint64_t delay);
bool dspic33_can_receive(Dspic33* cpu, uint8_t channel, const Dspic33CanFrame* frame,
                         uint64_t delay);
void dspic33_adc_input(Dspic33* cpu, uint8_t channel, uint16_t value);
void dspic33_gpio_input(Dspic33* cpu, uint8_t port, uint16_t value);
Dspic33StopReason dspic33_run(Dspic33* cpu, uint64_t instruction_limit);
const char* dspic33_stop_reason_name(Dspic33StopReason reason);

#endif
