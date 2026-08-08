#ifndef OPENTEC_DSPIC33_H
#define OPENTEC_DSPIC33_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DSPIC33_DATA_SIZE 0x100000u
#define DSPIC33_PROGRAM_LIMIT 0x55800u
#define DSPIC33_PROGRAM_WORDS (DSPIC33_PROGRAM_LIMIT / 2u)
#define DSPIC33_PERSISTENT_PROGRAM_BASE 0x1000000u
#define DSPIC33_PERSISTENT_PROGRAM_LIMIT 0x1010000u
#define DSPIC33_PERSISTENT_PROGRAM_WORDS                                               \
    ((DSPIC33_PERSISTENT_PROGRAM_LIMIT - DSPIC33_PERSISTENT_PROGRAM_BASE) / 2u)
#define DSPIC33_WRITE_LATCH_BASE 0xfa0000u
#define DSPIC33_WRITE_LATCH_LIMIT 0xfa0100u
#define DSPIC33_WRITE_LATCH_WORDS                                                      \
    ((DSPIC33_WRITE_LATCH_LIMIT - DSPIC33_WRITE_LATCH_BASE) / 2u)
#define DSPIC33_CONFIGURATION_BASE 0xf80000u
#define DSPIC33_CONFIGURATION_SIZE 0x20u
#define DSPIC33_IRQ_COUNT 133u
#define DSPIC33_IRQ_GROUP_COUNT ((DSPIC33_IRQ_COUNT + 15u) / 16u)
#define DSPIC33_UART_COUNT 4u
#define DSPIC33_UART_FIFO_SIZE 4u
#define DSPIC33_UART_QUEUE_SIZE 1024u
#define DSPIC33_SPI_COUNT 4u
#define DSPIC33_CAN_COUNT 2u
#define DSPIC33_TIMER_COUNT 9u
#define DSPIC33_DMA_COUNT 15u
#define DSPIC33_ADC_COUNT 2u
#define DSPIC33_ADC_CHANNEL_COUNT 32u
#define DSPIC33_PWM_COUNT 6u
#define DSPIC33_PWM_OUTPUT_COUNT (DSPIC33_PWM_COUNT * 2u)
#define DSPIC33_PWM_INPUT_COUNT 32u
#define DSPIC33_GPIO_PORT_COUNT 7u
#define DSPIC33_USB_ENDPOINT_COUNT 16u
#define DSPIC33_USB_PACKET_SIZE 1023u
#define DSPIC33_USB_PACKET_QUEUE_SIZE 64u
#define DSPIC33_USB_PENDING_COUNT 64u

typedef enum {
    DSPIC33_EVENT_INTERRUPT,
    DSPIC33_EVENT_TIMER,
    DSPIC33_EVENT_TIMER_GATE,
    DSPIC33_EVENT_DMA,
    DSPIC33_EVENT_ADC,
    DSPIC33_EVENT_PWM_FAULT,
    DSPIC33_EVENT_PWM_CURRENT_LIMIT,
    DSPIC33_EVENT_PWM_DEAD_TIME,
    DSPIC33_EVENT_PWM_SYNC,
    DSPIC33_EVENT_UART,
    DSPIC33_EVENT_SPI,
    DSPIC33_EVENT_SPI_SELECT,
    DSPIC33_EVENT_CAN,
    DSPIC33_EVENT_USB,
    DSPIC33_EVENT_NVM,
    DSPIC33_EVENT_AUX_PLL
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

typedef enum {
    DSPIC33_UART_PARITY_NONE,
    DSPIC33_UART_PARITY_EVEN,
    DSPIC33_UART_PARITY_ODD
} Dspic33UartParity;

typedef struct {
    uint16_t value;
    uint16_t baud_period;
    uint8_t data_bits;
    uint8_t stop_bits;
    Dspic33UartParity parity;
    bool parity_error;
    bool framing_error;
    bool break_signal;
    bool inverted;
    bool irda;
} Dspic33UartFrame;

typedef struct {
    Dspic33UartFrame frames[DSPIC33_UART_FIFO_SIZE];
    uint8_t head;
    uint8_t count;
} Dspic33UartFifo;

typedef struct {
    Dspic33UartFrame frames[DSPIC33_UART_QUEUE_SIZE];
    uint16_t head;
    uint16_t count;
} Dspic33UartQueue;

typedef struct {
    uint16_t words[8];
    uint8_t head;
    uint8_t count;
} Dspic33WordQueue;

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

typedef enum {
    DSPIC33_USB_PID_OUT = 0x01u,
    DSPIC33_USB_PID_SOF = 0x05u,
    DSPIC33_USB_PID_IN = 0x09u,
    DSPIC33_USB_PID_SETUP = 0x0du
} Dspic33UsbPid;

typedef enum {
    DSPIC33_USB_HANDSHAKE_NONE,
    DSPIC33_USB_HANDSHAKE_ACK,
    DSPIC33_USB_HANDSHAKE_NAK,
    DSPIC33_USB_HANDSHAKE_STALL,
    DSPIC33_USB_HANDSHAKE_TIMEOUT,
    DSPIC33_USB_HANDSHAKE_ERROR
} Dspic33UsbHandshake;

typedef enum {
    DSPIC33_USB_BUS_RESET,
    DSPIC33_USB_BUS_SOF,
    DSPIC33_USB_BUS_IDLE,
    DSPIC33_USB_BUS_RESUME,
    DSPIC33_USB_BUS_ATTACH,
    DSPIC33_USB_BUS_DETACH,
    DSPIC33_USB_BUS_ERROR,
    DSPIC33_USB_BUS_OTG_STATE
} Dspic33UsbBusEvent;

typedef struct {
    uint8_t data[DSPIC33_USB_PACKET_SIZE];
    uint16_t size;
    uint8_t address;
    uint8_t endpoint;
    uint8_t pid;
    uint8_t error;
    Dspic33UsbHandshake handshake;
    bool data1;
    bool low_speed;
} Dspic33UsbPacket;

typedef struct {
    Dspic33UsbPacket packets[DSPIC33_USB_PACKET_QUEUE_SIZE];
    uint8_t head;
    uint8_t count;
} Dspic33UsbQueue;

typedef struct {
    Dspic33UsbPacket packet;
    Dspic33UsbBusEvent event;
    uint16_t value;
    bool active;
    bool bus_event;
} Dspic33UsbPending;

typedef struct {
    Dspic33UartFifo uart_rx_fifo[DSPIC33_UART_COUNT];
    Dspic33UartFifo uart_tx_fifo[DSPIC33_UART_COUNT];
    Dspic33UartQueue uart_tx[DSPIC33_UART_COUNT];
    Dspic33UartFrame uart_tx_shift[DSPIC33_UART_COUNT];
    Dspic33UartFrame uart_rx_hold[DSPIC33_UART_COUNT];
    uint16_t uart_generation[DSPIC33_UART_COUNT];
    uint8_t uart_tx_active;
    uint8_t uart_tx_scheduled;
    uint8_t uart_rx_hold_valid;
    uint8_t uart_cts;
    Dspic33ByteQueue spi_tx[DSPIC33_SPI_COUNT];
    Dspic33WordQueue spi_tx_fifo[DSPIC33_SPI_COUNT];
    Dspic33WordQueue spi_rx_fifo[DSPIC33_SPI_COUNT];
    Dspic33CanQueue can_rx[DSPIC33_CAN_COUNT];
    Dspic33CanQueue can_tx[DSPIC33_CAN_COUNT];
    uint16_t can_filter_window[DSPIC33_CAN_COUNT][48];
    uint16_t can_rx_words[DSPIC33_CAN_COUNT][8];
    uint16_t can_tx_words[DSPIC33_CAN_COUNT][8];
    uint8_t can_rx_word[DSPIC33_CAN_COUNT];
    uint8_t can_tx_word[DSPIC33_CAN_COUNT];
    uint8_t can_rx_buffer[DSPIC33_CAN_COUNT];
    uint8_t can_rx_filter[DSPIC33_CAN_COUNT];
    uint8_t can_tx_buffer[DSPIC33_CAN_COUNT];
    uint8_t can_last_buffer[DSPIC33_CAN_COUNT];
    uint8_t can_last_filter[DSPIC33_CAN_COUNT];
    uint8_t can_fifo_write[DSPIC33_CAN_COUNT];
    uint8_t can_rx_busy;
    uint8_t can_tx_busy;
    uint16_t adc[DSPIC33_ADC_CHANNEL_COUNT];
    uint16_t adc_latched[DSPIC33_ADC_COUNT][4];
    uint16_t adc_generation[DSPIC33_ADC_COUNT];
    uint8_t adc_latched_channel[DSPIC33_ADC_COUNT][4];
    uint8_t adc_latched_count[DSPIC33_ADC_COUNT];
    uint8_t adc_buffer_index[DSPIC33_ADC_COUNT];
    uint8_t adc_sample_count[DSPIC33_ADC_COUNT];
    uint8_t adc_scan_index[DSPIC33_ADC_COUNT];
    uint8_t adc_dma_sample[DSPIC33_ADC_COUNT][DSPIC33_ADC_CHANNEL_COUNT];
    uint8_t adc_mux_b;
    uint16_t gpio[DSPIC33_GPIO_PORT_COUNT];
    uint16_t pwm[DSPIC33_PWM_OUTPUT_COUNT];
    uint16_t pwm_master_counter[2];
    uint16_t pwm_counter[DSPIC33_PWM_COUNT][2];
    uint16_t pwm_active_period[2];
    uint16_t pwm_active_duty[DSPIC33_PWM_COUNT][2];
    uint16_t pwm_active_phase[DSPIC33_PWM_COUNT][2];
    uint16_t pwm_active_dead_time[DSPIC33_PWM_COUNT][2];
    uint16_t pwm_active_io[DSPIC33_PWM_COUNT];
    uint16_t pwm_leb_ticks[DSPIC33_PWM_COUNT];
    uint16_t pwm_chop_counter;
    uint8_t pwm_cycle_count[DSPIC33_PWM_COUNT];
    uint8_t pwm_trigger_count[DSPIC33_PWM_COUNT];
    uint8_t pwm_special_count[2];
    uint8_t pwm_direction[2];
    uint8_t pwm_push_pull;
    uint8_t pwm_fault_latched;
    uint8_t pwm_fault_cycle;
    uint8_t pwm_current_cycle;
    uint8_t pwm_fault_release;
    uint8_t pwm_dead_time_inputs;
    uint8_t pwm_dead_time_sampled;
    uint8_t pwm_sync_inputs;
    uint32_t pwm_fault_inputs;
    uint32_t pwm_current_limit_inputs;
    uint32_t pwm_fraction[2];
    uint64_t pwm_sync_until[2];
    uint32_t timer_fraction[DSPIC33_TIMER_COUNT];
    uint16_t timer_enabled;
    uint16_t timer_gate;
    uint16_t timer_external_started;
    uint16_t timer_interrupt_pending;
    uint16_t dma_index[DSPIC33_DMA_COUNT];
    uint16_t dma_generation[DSPIC33_DMA_COUNT];
    uint32_t dma_start_a[DSPIC33_DMA_COUNT];
    uint32_t dma_start_b[DSPIC33_DMA_COUNT];
    uint32_t dma_address[DSPIC33_DMA_COUNT];
    uint16_t dma_enabled;
    uint16_t dma_bank;
    uint16_t dma_half_raised;
    uint16_t dma_forced_pending;
    uint16_t dma_peripheral_pending;
    uint16_t spi_shift[DSPIC33_SPI_COUNT];
    uint16_t spi_generation[DSPIC33_SPI_COUNT];
    uint8_t spi_busy;
    uint8_t spi_selected;
    uint64_t cpu_write_cycle;
    uint32_t cpu_write_address;
    uint16_t cpu_write_previous;
    uint8_t cpu_write_width;
    bool cpu_write_valid;
    bool dma_transfer_active;
    Dspic33UsbPending usb_pending[DSPIC33_USB_PENDING_COUNT];
    Dspic33UsbQueue usb_tx;
    uint8_t usb_next_bank[DSPIC33_USB_ENDPOINT_COUNT][2];
    uint8_t usb_status[4];
    uint8_t usb_status_head;
    uint8_t usb_status_count;
    uint8_t usb_last_endpoint;
    uint8_t usb_host_pid;
    uint8_t usb_host_endpoint;
    Dspic33UsbHandshake usb_last_handshake;
    bool usb_host_pending;
    uint8_t oscillator_unlock;
} Dspic33Io;

typedef enum {
    DSPIC33_RUNNING,
    DSPIC33_RETURNED,
    DSPIC33_STOPPED,
    DSPIC33_SLEEPING,
    DSPIC33_IDLING,
    DSPIC33_HALTED,
    DSPIC33_UNSUPPORTED_INSTRUCTION,
    DSPIC33_PROGRAM_BOUNDS,
    DSPIC33_INSTRUCTION_LIMIT,
    DSPIC33_EVENT_QUEUE_ERROR
} Dspic33StopReason;

typedef enum {
    DSPIC33_POWER_ACTIVE,
    DSPIC33_POWER_SLEEP,
    DSPIC33_POWER_IDLE
} Dspic33PowerState;

typedef struct {
    uint32_t* program;
    uint32_t* persistent_program;
    uint32_t write_latches[DSPIC33_WRITE_LATCH_WORDS];
    uint8_t* data;
    uint8_t configuration[DSPIC33_CONFIGURATION_SIZE];
    uint16_t w[16];
    uint16_t shadow_w[4];
    uint16_t shadow_status;
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
    uint8_t do_terminate[4];
    uint64_t instructions;
    uint64_t cycles;
    uint32_t unsupported_opcode;
    uint16_t last_interrupt;
    uint32_t last_interrupt_return;
    uint64_t interrupt_count;
    uint64_t software_reset_count;
    uint64_t trap_count;
    uint16_t reset_interrupt;
    uint16_t last_trap;
    uint16_t interrupt_log_irq[16];
    uint32_t interrupt_log_entry[16];
    uint32_t interrupt_log_return[16];
    uint16_t interrupt_deferred[DSPIC33_IRQ_GROUP_COUNT];
    uint16_t interrupt_deferred_next[DSPIC33_IRQ_GROUP_COUNT];
    uint8_t gie_disable_deferred;
    uint8_t gie_disable_deferred_next;
    Dspic33EventQueue events;
    Dspic33Io io;
    Dspic33PowerState power_state;
    Dspic33StopReason stop_reason;
} Dspic33;

bool dspic33_initialize(Dspic33* cpu);
void dspic33_destroy(Dspic33* cpu);
bool dspic33_copy(Dspic33* destination, const Dspic33* source);
void dspic33_reset(Dspic33* cpu, uint32_t entry);
bool dspic33_load_program_word(Dspic33* cpu, uint32_t address, uint32_t word);
void dspic33_complete_nvm(Dspic33* cpu);
bool dspic33_load_configuration_word(Dspic33* cpu, uint32_t address, uint32_t word);
uint8_t dspic33_read_program_byte(const Dspic33* cpu, uint32_t address);
uint8_t dspic33_read_configuration_byte(const Dspic33* cpu, uint32_t address);
void dspic33_write_byte(Dspic33* cpu, uint32_t address, uint8_t value);
void dspic33_write_word(Dspic33* cpu, uint32_t address, uint16_t value);
uint8_t dspic33_read_byte(Dspic33* cpu, uint32_t address);
uint16_t dspic33_read_word(Dspic33* cpu, uint32_t address);
bool dspic33_schedule(Dspic33* cpu, Dspic33EventType type, uint16_t source,
                      uint32_t value, uint64_t delay);
void dspic33_raise_interrupt(Dspic33* cpu, uint16_t irq);
bool dspic33_uart_receive(Dspic33* cpu, uint8_t channel, uint8_t value, uint64_t delay);
bool dspic33_uart_receive_frame(Dspic33* cpu, uint8_t channel,
                                const Dspic33UartFrame* frame, uint64_t delay);
bool dspic33_uart_set_cts(Dspic33* cpu, uint8_t channel, bool clear, uint64_t delay);
bool dspic33_uart_transmit(Dspic33* cpu, uint8_t channel, Dspic33UartFrame* frame);
bool dspic33_spi_receive(Dspic33* cpu, uint8_t channel, uint16_t value, uint64_t delay);
bool dspic33_spi_select(Dspic33* cpu, uint8_t channel, bool selected, uint64_t delay);
bool dspic33_dma_request(Dspic33* cpu, uint8_t request, uint16_t indirect_address,
                         uint64_t delay);
bool dspic33_timer_pulse(Dspic33* cpu, uint8_t timer, uint32_t pulses, uint64_t delay);
bool dspic33_timer_gate(Dspic33* cpu, uint8_t timer, bool high, uint64_t delay);
bool dspic33_adc_trigger(Dspic33* cpu, uint8_t module, uint8_t source, uint64_t delay);
bool dspic33_pwm_fault(Dspic33* cpu, uint8_t source, bool high, uint64_t delay);
bool dspic33_pwm_current_limit(Dspic33* cpu, uint8_t source, bool high, uint64_t delay);
bool dspic33_pwm_dead_time(Dspic33* cpu, uint8_t generator, bool high, uint64_t delay);
bool dspic33_pwm_sync(Dspic33* cpu, uint8_t input, bool high, uint64_t delay);
bool dspic33_pwm_sync_output(const Dspic33* cpu, uint8_t time_base);
bool dspic33_pwm_output(const Dspic33* cpu, uint8_t generator, bool high);
bool dspic33_can_receive(Dspic33* cpu, uint8_t channel, const Dspic33CanFrame* frame,
                         uint64_t delay);
bool dspic33_can_error(Dspic33* cpu, uint8_t channel, bool transmit, uint8_t count,
                       uint64_t delay);
bool dspic33_can_transmit(Dspic33* cpu, uint8_t channel, Dspic33CanFrame* frame);
bool dspic33_usb_receive(Dspic33* cpu, uint8_t endpoint, const uint8_t* data,
                         uint16_t size, uint64_t delay);
bool dspic33_usb_token(Dspic33* cpu, uint8_t address, uint8_t endpoint,
                       Dspic33UsbPid pid, const uint8_t* data, uint16_t size,
                       bool data1, uint64_t delay);
bool dspic33_usb_receive_toggle(Dspic33* cpu, uint8_t endpoint, const uint8_t* data,
                                uint16_t size, bool data1, uint64_t delay);
bool dspic33_usb_setup(Dspic33* cpu, uint8_t endpoint, const uint8_t* data,
                       uint16_t size, uint64_t delay);
bool dspic33_usb_request(Dspic33* cpu, uint8_t endpoint, uint64_t delay);
bool dspic33_usb_host_response(Dspic33* cpu, Dspic33UsbHandshake handshake,
                               const uint8_t* data, uint16_t size, bool data1,
                               uint64_t delay);
bool dspic33_usb_bus(Dspic33* cpu, Dspic33UsbBusEvent event, uint16_t value,
                     uint64_t delay);
bool dspic33_usb_transmit(Dspic33* cpu, Dspic33UsbPacket* packet);
void dspic33_adc_input(Dspic33* cpu, uint8_t channel, uint16_t value);
void dspic33_gpio_input(Dspic33* cpu, uint8_t port, uint16_t value);
Dspic33StopReason dspic33_step(Dspic33* cpu);
Dspic33StopReason dspic33_run(Dspic33* cpu, uint64_t instruction_limit);
Dspic33StopReason dspic33_run_until(Dspic33* cpu, uint32_t stop_address,
                                    uint64_t instruction_limit);
const char* dspic33_stop_reason_name(Dspic33StopReason reason);

#endif
