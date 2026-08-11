#ifndef OPENTEC_DSPIC33_H
#define OPENTEC_DSPIC33_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DSPIC33_DATA_SIZE 0x100000u
#define DSPIC33_PROGRAM_LIMIT 0x55800u
#define DSPIC33_AUXILIARY_PROGRAM_BASE 0x7fc000u
#define DSPIC33_AUXILIARY_PROGRAM_LIMIT 0x800000u
#define DSPIC33_PROGRAM_WORDS (DSPIC33_PROGRAM_LIMIT / 2u)
#define DSPIC33_AUXILIARY_PROGRAM_WORDS                                                \
    ((DSPIC33_AUXILIARY_PROGRAM_LIMIT - DSPIC33_AUXILIARY_PROGRAM_BASE) / 2u)
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
#define DSPIC33_IRQ_COUNT 142u
#define DSPIC33_IRQ_GROUP_COUNT ((DSPIC33_IRQ_COUNT + 15u) / 16u)
#define DSPIC33_UART_COUNT 4u
#define DSPIC33_UART_FIFO_SIZE 4u
#define DSPIC33_UART_QUEUE_SIZE 1024u
#define DSPIC33_SPI_COUNT 4u
#define DSPIC33_I2C_COUNT 2u
#define DSPIC33_I2C_QUEUE_SIZE 64u
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
#define DSPIC33_PMP_QUEUE_SIZE 8192u
#define DSPIC33_INPUT_CAPTURE_COUNT 16u
#define DSPIC33_INPUT_CAPTURE_FIFO_SIZE 4u
#define DSPIC33_OUTPUT_COMPARE_COUNT 16u
#define DSPIC33_OUTPUT_COMPARE_FAULT_COUNT 3u
#define DSPIC33_COMPARATOR_COUNT 3u
#define DSPIC33_COMPARATOR_INPUT_COUNT 4u
#define DSPIC33_QEI_COUNT 2u
#define DSPIC33_DCI_BUFFER_COUNT 4u
#define DSPIC33_DCI_QUEUE_SIZE 64u

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
    DSPIC33_EVENT_I2C,
    DSPIC33_EVENT_CAN,
    DSPIC33_EVENT_USB,
    DSPIC33_EVENT_NVM,
    DSPIC33_EVENT_CRC,
    DSPIC33_EVENT_PMP,
    DSPIC33_EVENT_INPUT_CAPTURE,
    DSPIC33_EVENT_OUTPUT_COMPARE,
    DSPIC33_EVENT_OUTPUT_COMPARE_FAULT,
    DSPIC33_EVENT_COMPARATOR,
    DSPIC33_EVENT_RTCC,
    DSPIC33_EVENT_QEI,
    DSPIC33_EVENT_DCI,
    DSPIC33_EVENT_AUX_PLL,
    DSPIC33_EVENT_OSCILLATOR
} Dspic33EventType;

typedef struct {
    uint64_t cycle;
    uint64_t sequence;
    uint64_t paused_remaining;
    uint32_t value;
    uint16_t source;
    Dspic33EventType type;
    bool paused;
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
    uint32_t words[16];
    uint32_t data_latch;
    uint32_t shift_data;
    uint32_t polynomial;
    uint16_t generation;
    uint16_t pmd_generation;
    uint8_t head;
    uint8_t count;
    uint8_t data_width;
    uint8_t polynomial_width;
    uint8_t bits_remaining;
    bool little_endian;
    bool active;
    bool pmd_disabled;
} Dspic33Crc;

typedef struct {
    uint64_t cycle;
    uint16_t address;
    uint16_t control;
    uint16_t mode;
    uint16_t value;
    uint8_t width;
} Dspic33PmpTransfer;

typedef struct {
    Dspic33PmpTransfer transfers[DSPIC33_PMP_QUEUE_SIZE];
    uint16_t head;
    uint16_t count;
} Dspic33PmpQueue;

typedef struct {
    uint64_t cycle;
    uint16_t value;
} Dspic33PmpResponse;

typedef struct {
    Dspic33PmpResponse responses[DSPIC33_PMP_QUEUE_SIZE];
    uint16_t head;
    uint16_t count;
} Dspic33PmpResponseQueue;

typedef struct {
    Dspic33PmpQueue output;
    Dspic33PmpResponseQueue input;
    Dspic33PmpTransfer completing;
    Dspic33PmpTransfer last_read;
    uint16_t address;
    uint16_t control;
    uint16_t mode;
    uint16_t generation;
    uint16_t completing_generation;
    uint16_t pmd_generation;
    uint16_t value;
    uint8_t width;
    uint8_t slave_read_index;
    uint8_t slave_write_index;
    bool active;
    bool completing_active;
    bool reading;
    bool completing_reading;
    bool last_read_valid;
    bool pmd_disabled;
} Dspic33Pmp;

typedef struct {
    uint16_t words[DSPIC33_INPUT_CAPTURE_FIFO_SIZE];
    uint8_t head;
    uint8_t count;
} Dspic33InputCaptureFifo;

typedef struct {
    Dspic33InputCaptureFifo fifo[DSPIC33_INPUT_CAPTURE_COUNT];
    uint16_t timer[DSPIC33_INPUT_CAPTURE_COUNT];
    uint16_t generation[DSPIC33_INPUT_CAPTURE_COUNT];
    uint16_t pmd_generation[DSPIC33_INPUT_CAPTURE_COUNT];
    uint16_t input_high;
    uint16_t pmd_disabled;
    uint16_t sync_output_high;
    uint16_t sync_reset_pending;
    uint8_t interrupt_count[DSPIC33_INPUT_CAPTURE_COUNT];
    uint8_t prescaler_count[DSPIC33_INPUT_CAPTURE_COUNT];
} Dspic33InputCapture;

typedef struct {
    uint16_t active_r[DSPIC33_OUTPUT_COMPARE_COUNT];
    uint16_t active_rs[DSPIC33_OUTPUT_COMPARE_COUNT];
    uint16_t generation[DSPIC33_OUTPUT_COMPARE_COUNT];
    uint16_t timer_generation[DSPIC33_OUTPUT_COMPARE_COUNT];
    uint16_t pmd_generation[DSPIC33_OUTPUT_COMPARE_COUNT];
    uint16_t output_high;
    uint16_t pmd_disabled;
    uint16_t sync_reset_pending;
    uint16_t deferred_sync_pulses;
    uint16_t activation_pending;
    uint16_t fault_held;
    uint16_t fault_interrupt_pending;
    uint8_t phase[DSPIC33_OUTPUT_COMPARE_COUNT];
    uint8_t fault_inputs;
    uint8_t fault_direct_mask;
    bool clock_advancing;
    bool sync_emitted[DSPIC33_OUTPUT_COMPARE_COUNT];
    uint64_t activation_cycle[DSPIC33_OUTPUT_COMPARE_COUNT];
} Dspic33OutputCompare;

typedef enum {
    DSPIC33_COMPARATOR_INPUT_POSITIVE,
    DSPIC33_COMPARATOR_INPUT_NEGATIVE_2,
    DSPIC33_COMPARATOR_INPUT_NEGATIVE_1,
    DSPIC33_COMPARATOR_INPUT_NEGATIVE_3
} Dspic33ComparatorInput;

typedef struct {
    uint16_t input[DSPIC33_COMPARATOR_COUNT][DSPIC33_COMPARATOR_INPUT_COUNT];
    uint64_t rearm_cycle[DSPIC33_COMPARATOR_COUNT];
    uint16_t pmd_generation;
    uint8_t output_high;
    uint8_t last_read_cout;
    bool pmd_disabled;
} Dspic33Comparator;

typedef struct {
    uint16_t calendar[4];
    uint16_t alarm[3];
    uint16_t prescaler;
    uint16_t pmd_generation;
    bool alarm_output;
    bool pmd_disabled;
    bool calibration_pending;
} Dspic33Rtcc;

typedef enum {
    DSPIC33_QEI_PHASE_A,
    DSPIC33_QEI_PHASE_B,
    DSPIC33_QEI_INDEX,
    DSPIC33_QEI_HOME
} Dspic33QeiInput;

typedef struct {
    uint64_t counter_fraction[DSPIC33_QEI_COUNT];
    uint64_t filter_fraction[DSPIC33_QEI_COUNT];
    uint16_t pmd_generation[DSPIC33_QEI_COUNT];
    uint8_t filtered_inputs[DSPIC33_QEI_COUNT];
    uint8_t logical_inputs[DSPIC33_QEI_COUNT];
    uint8_t filter_stability[DSPIC33_QEI_COUNT][4];
    uint8_t home_index_count[DSPIC33_QEI_COUNT];
    int8_t direction[DSPIC33_QEI_COUNT];
    bool interval_armed[DSPIC33_QEI_COUNT];
    bool interval_hold_locked[DSPIC33_QEI_COUNT];
    bool index_latched[DSPIC33_QEI_COUNT];
    bool pmd_disabled[DSPIC33_QEI_COUNT];
} Dspic33Qei;

enum { DSPIC33_PPS_REGISTER_COUNT = 60u };

typedef struct {
    uint16_t shadow[DSPIC33_PPS_REGISTER_COUNT];
    bool one_way_committed;
} Dspic33Pps;

typedef struct {
    uint64_t cycle;
    uint16_t value;
    uint8_t slot;
    bool driven;
} Dspic33DciTransfer;

typedef struct {
    Dspic33DciTransfer transfers[DSPIC33_DCI_QUEUE_SIZE];
    uint8_t head;
    uint8_t count;
} Dspic33DciQueue;

typedef struct {
    Dspic33DciQueue output;
    uint16_t receive[DSPIC33_DCI_BUFFER_COUNT];
    uint16_t transmit[DSPIC33_DCI_BUFFER_COUNT];
    uint16_t last_transmit[DSPIC33_DCI_BUFFER_COUNT];
    uint16_t input;
    uint16_t serial_input;
    uint16_t generation;
    uint16_t pmd_generation;
    uint8_t receive_unread;
    uint8_t receive_overflow;
    uint8_t receive_buffered;
    uint8_t transmit_written;
    uint8_t transmit_underflow;
    uint8_t transmit_buffered;
    uint8_t buffer;
    uint8_t slot;
    uint8_t serial_bits;
    uint8_t disable_frames;
    bool pps_clock_high;
    bool pps_frame_high;
    bool pps_frame_pending;
    bool serial_output_high;
    bool serial_output_driven;
    bool serial_delay;
    bool output_frame_high;
    bool started;
    bool initialized;
    bool disable_pending;
    bool internal_scheduled;
    bool pmd_disabled;
    bool transmit_empty;
} Dspic33Dci;

typedef enum {
    DSPIC33_I2C_START,
    DSPIC33_I2C_RESTART,
    DSPIC33_I2C_STOP,
    DSPIC33_I2C_WRITE,
    DSPIC33_I2C_READ,
    DSPIC33_I2C_ACKNOWLEDGE,
    DSPIC33_I2C_COLLISION
} Dspic33I2cTransferType;

typedef struct {
    Dspic33I2cTransferType type;
    uint16_t value;
    bool acknowledge;
    bool master;
} Dspic33I2cTransfer;

typedef struct {
    Dspic33I2cTransfer transfers[DSPIC33_I2C_QUEUE_SIZE];
    uint8_t head;
    uint8_t count;
} Dspic33I2cQueue;

typedef struct {
    uint64_t cycle;
    uint8_t value;
    bool acknowledge;
} Dspic33I2cResponse;

typedef struct {
    Dspic33I2cResponse responses[DSPIC33_I2C_QUEUE_SIZE];
    uint8_t head;
    uint8_t count;
} Dspic33I2cResponseQueue;

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
    Dspic33I2cQueue i2c_tx[DSPIC33_I2C_COUNT];
    Dspic33I2cResponseQueue i2c_response[DSPIC33_I2C_COUNT];
    uint16_t i2c_slave_address[DSPIC33_I2C_COUNT];
    uint8_t i2c_generation[DSPIC33_I2C_COUNT];
    uint8_t i2c_pmd_generation[DSPIC33_I2C_COUNT];
    uint8_t i2c_pmd_disabled;
    uint8_t i2c_master_active;
    uint8_t i2c_slave_active;
    uint8_t i2c_slave_read;
    uint8_t i2c_slave_rejected;
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
    uint16_t gpio_driven[DSPIC33_GPIO_PORT_COUNT];
    uint16_t gpio_cn_reference[DSPIC33_GPIO_PORT_COUNT];
    uint16_t gpio_cn_values[DSPIC33_GPIO_PORT_COUNT];
    uint16_t gpio_cn_qualified[DSPIC33_GPIO_PORT_COUNT];
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
    uint16_t dma_active;
    uint16_t spi_shift[DSPIC33_SPI_COUNT];
    uint16_t spi_generation[DSPIC33_SPI_COUNT];
    uint8_t spi_busy;
    uint8_t spi_selected;
    uint64_t cpu_write_cycle;
    uint32_t cpu_write_address;
    uint16_t cpu_write_previous;
    uint8_t cpu_write_width;
    bool cpu_write_valid;
    uint32_t cpu_read_address;
    uint8_t cpu_read_width;
    bool cpu_read_valid;
    uint8_t dma_transfer_width;
    bool dma_transfer_active;
    Dspic33Crc crc;
    Dspic33Pmp pmp;
    Dspic33InputCapture input_capture;
    Dspic33OutputCompare output_compare;
    Dspic33Comparator comparator;
    Dspic33Rtcc rtcc;
    Dspic33Qei qei;
    Dspic33Pps pps;
    Dspic33Dci dci;
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
    bool usb_host_attached;
    uint32_t auxiliary_pll_generation;
} Dspic33Io;

typedef enum {
    DSPIC33_RUNNING,
    DSPIC33_RETURNED,
    DSPIC33_STOPPED,
    DSPIC33_SLEEPING,
    DSPIC33_IDLING,
    DSPIC33_HALTED,
    DSPIC33_TRAPPED,
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
    uint32_t vector;
    uint16_t trap;
    uint8_t priority;
    uint8_t delay;
    bool active;
    bool auxiliary_program;
} Dspic33PendingSoftTrap;

typedef struct {
    uint32_t address;
    uint32_t latches[DSPIC33_WRITE_LATCH_WORDS];
    uint64_t completion_cycle;
    uint64_t key_interrupt_count;
    uint64_t key_trap_count;
    uint64_t key_instruction;
    uint16_t control;
    uint16_t reset_cause;
    uint8_t key_stage;
    uint8_t reset_kind;
    bool active;
    bool auxiliary_origin;
    bool reset_pending;
} Dspic33Nvm;

typedef struct {
    uint64_t key_interrupt_count;
    uint64_t key_trap_count;
    uint64_t key_instruction;
    uint32_t generation;
    uint8_t key_lane;
    uint8_t key_stage;
    bool active;
    bool automatic;
    bool lock_pending;
    bool source_ready;
} Dspic33Oscillator;

typedef struct {
    uint32_t ticks;
    bool reset_pending;
} Dspic33Watchdog;

typedef struct {
    uint32_t* program;
    uint32_t* auxiliary_program;
    uint32_t* persistent_program;
    uint32_t write_latches[DSPIC33_WRITE_LATCH_WORDS];
    uint8_t* data;
    uint8_t configuration[DSPIC33_CONFIGURATION_SIZE];
    uint16_t w[16];
    uint16_t shadow_w[4];
    uint16_t initialized_working_registers;
    uint16_t shadow_status;
    int64_t accumulator[2];
    uint32_t pc;
    uint16_t sr;
    uint16_t corcon;
    uint16_t splim;
    bool splim_enabled;
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
    bool repeat_psv_started;
    bool repeat_psv_reentry;
    uint8_t do_depth;
    uint32_t repeat_pc;
    uint32_t do_start[4];
    uint32_t do_end[4];
    uint16_t do_count[4];
    uint8_t do_terminate[4];
    uint64_t instructions;
    uint64_t cycles;
    uint64_t device_cycles;
    uint32_t unsupported_opcode;
    uint16_t last_interrupt;
    uint32_t last_interrupt_return;
    uint64_t interrupt_count;
    uint64_t software_reset_count;
    uint64_t illegal_reset_count;
    uint64_t trap_count;
    uint32_t last_trap_return;
    uint16_t reset_interrupt;
    uint16_t last_trap;
    Dspic33PendingSoftTrap pending_soft_traps[4];
    uint32_t address_error_return;
    uint32_t current_instruction_pc;
    bool instruction_active;
    uint8_t current_instruction_cycles;
    uint16_t instruction_working_register_writes;
    uint16_t instruction_source_address_registers;
    uint16_t previous_working_register_writes;
    bool non_cpu_sfr_read;
    bool psv_read;
    bool psv_repeat_optimized;
    bool address_error;
    bool address_error_access_allowed;
    bool address_error_working_state_completed;
    bool address_error_accumulator_state_completed;
    bool address_error_control_state_completed;
    uint32_t sequential_program_hole_pc;
    bool illegal_reset;
    bool stop_on_trap;
    bool async_events_enabled;
    uint16_t interrupt_log_irq[16];
    uint32_t interrupt_log_entry[16];
    uint32_t interrupt_log_return[16];
    uint16_t interrupt_deferred[DSPIC33_IRQ_GROUP_COUNT];
    uint16_t interrupt_deferred_next[DSPIC33_IRQ_GROUP_COUNT];
    uint8_t gie_disable_deferred;
    uint8_t gie_disable_deferred_next;
    Dspic33Nvm nvm;
    Dspic33Oscillator oscillator;
    Dspic33Watchdog watchdog;
    Dspic33EventQueue events;
    uint8_t qei_inputs[DSPIC33_QEI_COUNT];
    Dspic33Io io;
    Dspic33PowerState power_state;
    Dspic33StopReason stop_reason;
} Dspic33;

bool dspic33_initialize(Dspic33* cpu);
void dspic33_set_working_register(Dspic33* cpu, uint8_t reg, uint16_t value);
void dspic33_destroy(Dspic33* cpu);
bool dspic33_copy(Dspic33* destination, const Dspic33* source);
void dspic33_reset(Dspic33* cpu, uint32_t entry);
void dspic33_watchdog_advance_lprc(Dspic33* cpu, uint64_t ticks);
bool dspic33_load_program_word(Dspic33* cpu, uint32_t address, uint32_t word);
bool dspic33_program_range_implemented(uint32_t address, uint32_t size);
bool dspic33_codeguard_admit_program_flow(Dspic33* cpu, uint32_t origin,
                                          uint32_t target);
void dspic33_raise_program_vector_error(Dspic33* cpu, uint32_t return_pc,
                                        bool auxiliary_vector);
void dspic33_complete_nvm(Dspic33* cpu);
bool dspic33_complete_nvm_reset(Dspic33* cpu);
bool dspic33_load_configuration_word(Dspic33* cpu, uint32_t address, uint32_t word);
uint32_t dspic33_read_program_word(const Dspic33* cpu, uint32_t address);
uint8_t dspic33_read_program_byte(const Dspic33* cpu, uint32_t address);
uint8_t dspic33_read_configuration_byte(const Dspic33* cpu, uint32_t address);
void dspic33_write_byte(Dspic33* cpu, uint32_t address, uint8_t value);
void dspic33_write_word(Dspic33* cpu, uint32_t address, uint16_t value);
uint8_t dspic33_read_byte(Dspic33* cpu, uint32_t address);
uint16_t dspic33_read_word(Dspic33* cpu, uint32_t address);
bool dspic33_schedule(Dspic33* cpu, Dspic33EventType type, uint16_t source,
                      uint32_t value, uint64_t delay);
void dspic33_reorder_events(Dspic33* cpu);
void dspic33_raise_interrupt(Dspic33* cpu, uint16_t irq);
void dspic33_raise_oscillator_fail_trap(Dspic33* cpu);
bool dspic33_oscillator_failure_detected(Dspic33* cpu);
void dspic33_set_generic_hard_trap_source(Dspic33* cpu, bool active);
void dspic33_set_generic_soft_trap_source(Dspic33* cpu, bool active);
bool dspic33_uart_receive(Dspic33* cpu, uint8_t channel, uint8_t value, uint64_t delay);
bool dspic33_uart_receive_frame(Dspic33* cpu, uint8_t channel,
                                const Dspic33UartFrame* frame, uint64_t delay);
bool dspic33_uart_set_cts(Dspic33* cpu, uint8_t channel, bool clear, uint64_t delay);
bool dspic33_uart_transmit(Dspic33* cpu, uint8_t channel, Dspic33UartFrame* frame);
bool dspic33_spi_receive(Dspic33* cpu, uint8_t channel, uint16_t value, uint64_t delay);
bool dspic33_spi_select(Dspic33* cpu, uint8_t channel, bool selected, uint64_t delay);
bool dspic33_i2c_respond(Dspic33* cpu, uint8_t channel, uint8_t value, bool acknowledge,
                         uint64_t delay);
bool dspic33_i2c_slave_start(Dspic33* cpu, uint8_t channel, uint16_t address, bool read,
                             bool ten_bit, uint64_t delay);
bool dspic33_i2c_slave_write(Dspic33* cpu, uint8_t channel, uint8_t value,
                             uint64_t delay);
bool dspic33_i2c_slave_read(Dspic33* cpu, uint8_t channel, bool acknowledge,
                            uint64_t delay);
bool dspic33_i2c_slave_stop(Dspic33* cpu, uint8_t channel, uint64_t delay);
bool dspic33_i2c_collision(Dspic33* cpu, uint8_t channel, uint64_t delay);
bool dspic33_i2c_transmit(Dspic33* cpu, uint8_t channel, Dspic33I2cTransfer* transfer);
bool dspic33_dma_request(Dspic33* cpu, uint8_t request, uint16_t indirect_address,
                         uint64_t delay);
bool dspic33_pmp_respond(Dspic33* cpu, uint16_t value, uint64_t delay);
bool dspic33_pmp_transmit(Dspic33* cpu, Dspic33PmpTransfer* transfer);
bool dspic33_pmp_slave_read(Dspic33* cpu, uint8_t address, uint64_t delay);
bool dspic33_pmp_slave_write(Dspic33* cpu, uint8_t address, uint8_t value,
                             uint64_t delay);
void dspic33_device_power_state_changed(Dspic33* cpu);
bool dspic33_input_capture_input(Dspic33* cpu, uint8_t channel, bool high,
                                 uint64_t delay);
bool dspic33_input_capture_pin(Dspic33* cpu, uint8_t pin, bool high, uint64_t delay);
void dspic33_configuration_mismatch_reset(Dspic33* cpu);
bool dspic33_output_compare_output(const Dspic33* cpu, uint8_t channel, bool* high);
bool dspic33_output_compare_pin(const Dspic33* cpu, uint8_t pin, bool* high);
bool dspic33_output_compare_fault(Dspic33* cpu, uint8_t source, bool high,
                                  uint64_t delay);
bool dspic33_output_compare_fault_pin(Dspic33* cpu, uint8_t pin, bool high,
                                      uint64_t delay);
bool dspic33_comparator_input(Dspic33* cpu, uint8_t comparator,
                              Dspic33ComparatorInput input, uint16_t level,
                              uint64_t delay);
bool dspic33_comparator_output(const Dspic33* cpu, uint8_t comparator, bool* high);
bool dspic33_comparator_pin(const Dspic33* cpu, uint8_t pin, bool* high);
bool dspic33_rtcc_clock(Dspic33* cpu, uint32_t edges, uint64_t delay);
bool dspic33_rtcc_output(const Dspic33* cpu, bool* high);
bool dspic33_qei_input(Dspic33* cpu, uint8_t channel, Dspic33QeiInput input, bool high,
                       uint64_t delay);
bool dspic33_qei_compare_output(const Dspic33* cpu, uint8_t channel, bool* high);
void dspic33_dci_input(Dspic33* cpu, uint16_t value);
bool dspic33_dci_clock(Dspic33* cpu, uint16_t value, bool frame_sync, uint64_t delay);
bool dspic33_dci_transmit(Dspic33* cpu, Dspic33DciTransfer* transfer);
bool dspic33_dci_pin(const Dspic33* cpu, uint8_t pin, bool* high);
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
bool dspic33_gpio_drive(Dspic33* cpu, uint8_t port, uint16_t value, uint16_t mask);
bool dspic33_gpio_release(Dspic33* cpu, uint8_t port, uint16_t mask);
bool dspic33_gpio_pin(const Dspic33* cpu, uint8_t port, uint8_t bit, bool* high);
void dspic33_gpio_input(Dspic33* cpu, uint8_t port, uint16_t value);
void dspic33_set_async_events(Dspic33* cpu, bool enabled);
void dspic33_check_stack_address(Dspic33* cpu, int32_t address, bool wrapped,
                                 uint8_t delay);
void dspic33_set_math_error_source(Dspic33* cpu, bool active);
Dspic33StopReason dspic33_step(Dspic33* cpu);
Dspic33StopReason dspic33_run(Dspic33* cpu, uint64_t instruction_limit);
Dspic33StopReason dspic33_run_until(Dspic33* cpu, uint32_t stop_address,
                                    uint64_t instruction_limit);
const char* dspic33_stop_reason_name(Dspic33StopReason reason);

#endif
