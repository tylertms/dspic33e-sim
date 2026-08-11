#include "device.h"

#include <stdlib.h>
#include <string.h>

#include "i2c.h"

static void complete_oscillator_event(Dspic33* cpu, uint16_t phase,
                                      uint32_t generation);
static void oscillator_configuration_changed(Dspic33* cpu, uint8_t previous);
static void oscillator_startup_configuration_changed(Dspic33* cpu, uint8_t previous);
static void oscillator_pll_configuration_changed(Dspic33* cpu, uint8_t previous);
static void start_automatic_oscillator_switch(Dspic33* cpu, uint8_t source);
static void reset_main_oscillator(Dspic33* cpu);
static void refresh_gpio_change_notification(Dspic33* cpu);
static void refresh_external_interrupts(Dspic33* cpu);
static void output_compare_pulse_source(Dspic33* cpu, uint8_t source);
static void output_compare_update_power_state(Dspic33* cpu);
static void output_compare_raise(Dspic33* cpu, uint8_t channel);
static void output_compare_refresh_fault_pps_inputs(Dspic33* cpu);
static void dci_discard_internal_events(Dspic33* cpu);
static void dci_update_power_state(Dspic33* cpu);
static void dci_refresh_pps_inputs(Dspic33* cpu);
static void comparator_update_filter_power(Dspic33* cpu);
static void comparator_evaluate_all(Dspic33* cpu);
static uint64_t spi_transfer_cycles(const Dspic33* cpu, uint8_t channel);
static uint8_t dci_pps_selection(const Dspic33* cpu, uint16_t address, uint8_t shift);
static bool dci_pps_input_high(const Dspic33* cpu, uint8_t selection);
static uint16_t gpio_pin_values(const Dspic33* cpu, uint8_t port);

static const uint16_t timer_registers[DSPIC33_TIMER_COUNT] = {
    0x0100u, 0x0106u, 0x010au, 0x0114u, 0x0118u, 0x0122u, 0x0126u, 0x0130u, 0x0134u};
static const uint16_t timer_periods[DSPIC33_TIMER_COUNT] = {
    0x0102u, 0x010cu, 0x010eu, 0x011au, 0x011cu, 0x0128u, 0x012au, 0x0136u, 0x0138u};
static const uint16_t timer_controls[DSPIC33_TIMER_COUNT] = {
    0x0104u, 0x0110u, 0x0112u, 0x011eu, 0x0120u, 0x012cu, 0x012eu, 0x013au, 0x013cu};
static const uint16_t timer_holding_registers[4] = {0x0108u, 0x0116u, 0x0124u, 0x0132u};
static const uint8_t timer_irqs[DSPIC33_TIMER_COUNT] = {3u,  7u,  8u,  27u, 28u,
                                                        47u, 48u, 51u, 52u};
static const uint8_t dma_irqs[DSPIC33_DMA_COUNT] = {
    4u, 14u, 24u, 36u, 46u, 61u, 68u, 69u, 118u, 119u, 120u, 121u, 130u, 131u, 132u};
static const uint8_t uart_rx_irqs[DSPIC33_UART_COUNT] = {11u, 30u, 82u, 88u};
static const uint8_t uart_tx_irqs[DSPIC33_UART_COUNT] = {12u, 31u, 83u, 89u};
static const uint8_t uart_error_irqs[DSPIC33_UART_COUNT] = {65u, 66u, 81u, 87u};
static const uint8_t spi_error_irqs[DSPIC33_SPI_COUNT] = {9u, 32u, 90u, 122u};
static const uint8_t spi_irqs[DSPIC33_SPI_COUNT] = {10u, 33u, 91u, 123u};
static const uint8_t spi_dma_requests[DSPIC33_SPI_COUNT] = {10u, 33u, 91u, 123u};
static const uint16_t can_bases[DSPIC33_CAN_COUNT] = {0x0400u, 0x0500u};
static const uint8_t can_rx_irqs[DSPIC33_CAN_COUNT] = {34u, 55u};
static const uint8_t can_event_irqs[DSPIC33_CAN_COUNT] = {35u, 56u};
static const uint8_t can_tx_irqs[DSPIC33_CAN_COUNT] = {70u, 71u};
static const uint8_t can_rx_requests[DSPIC33_CAN_COUNT] = {34u, 55u};
static const uint8_t can_tx_requests[DSPIC33_CAN_COUNT] = {70u, 71u};
static const uint16_t uart_bases[DSPIC33_UART_COUNT] = {0x0220u, 0x0230u, 0x0250u,
                                                        0x02b0u};
static const uint16_t spi_bases[DSPIC33_SPI_COUNT] = {0x0240u, 0x0260u, 0x02a0u,
                                                      0x02c0u};
static const uint16_t adc_buffers[DSPIC33_ADC_COUNT] = {0x0300u, 0x0340u};
static const uint16_t adc_controls[DSPIC33_ADC_COUNT] = {0x0320u, 0x0360u};
static const uint8_t adc_irqs[DSPIC33_ADC_COUNT] = {13u, 21u};
static const uint8_t input_capture_irqs[DSPIC33_INPUT_CAPTURE_COUNT] = {
    1u,  5u,   37u,  38u,  39u,  40u,  22u,  23u,
    93u, 125u, 127u, 129u, 135u, 137u, 139u, 141u};
static const uint16_t input_capture_pps_registers[DSPIC33_INPUT_CAPTURE_COUNT / 2u] = {
    0x06aeu, 0x06b0u, 0x06b2u, 0x06b4u, 0x06e2u, 0x06e4u, 0x06e6u, 0x06e8u};
static const uint8_t output_compare_irqs[DSPIC33_OUTPUT_COMPARE_COUNT] = {
    2u,  6u,   25u,  26u,  41u,  42u,  43u,  44u,
    92u, 124u, 126u, 128u, 134u, 136u, 138u, 140u};
static const uint8_t external_interrupt_irqs[DSPIC33_EXTERNAL_INTERRUPT_COUNT] = {
    0u, 20u, 29u, 53u, 54u};
static const uint8_t pwm_irqs[DSPIC33_PWM_COUNT] = {94u, 95u, 96u, 97u, 98u, 99u};
static const uint16_t qei_bases[DSPIC33_QEI_COUNT] = {0x01c0u, 0x05c0u};
static const uint8_t qei_irqs[DSPIC33_QEI_COUNT] = {58u, 75u};
static const uint16_t gpio_port_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e02u, 0x0e12u, 0x0e22u, 0x0e32u, 0x0e42u, 0x0e52u, 0x0e62u};
static const uint16_t gpio_tris_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e00u, 0x0e10u, 0x0e20u, 0x0e30u, 0x0e40u, 0x0e50u, 0x0e60u};
static const uint16_t gpio_latch_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e04u, 0x0e14u, 0x0e24u, 0x0e34u, 0x0e44u, 0x0e54u, 0x0e64u};
static const uint16_t gpio_open_drain_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e06u, 0x0e16u, 0x0e26u, 0x0e36u, 0x0e46u, 0x0e56u, 0x0e66u};
static const uint16_t gpio_change_notification_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e08u, 0x0e18u, 0x0e28u, 0x0e38u, 0x0e48u, 0x0e58u, 0x0e68u};
static const uint16_t gpio_analog_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e0eu, 0x0e1eu, 0x0e2eu, 0x0e3eu, 0x0e4eu, 0u, 0x0e6eu};
static const uint16_t gpio_pull_up_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e0au, 0x0e1au, 0x0e2au, 0x0e3au, 0x0e4au, 0x0e5au, 0x0e6au};
static const uint16_t gpio_pull_down_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e0cu, 0x0e1cu, 0x0e2cu, 0x0e3cu, 0x0e4cu, 0x0e5cu, 0x0e6cu};
static const uint16_t gpio_port_masks[DSPIC33_GPIO_PORT_COUNT] = {
    0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x313fu, 0xf3cfu};
static const uint16_t gpio_input_only_masks[DSPIC33_GPIO_PORT_COUNT] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0x000cu};

typedef struct {
    uint16_t address;
    uint16_t value;
} Dspic33ResetValue;

typedef struct {
    uint16_t address;
    uint16_t writable;
} Dspic33RegisterMask;

typedef struct {
    uint16_t address;
    uint8_t pin;
    uint8_t shift;
} Dspic33PpsOutput;

typedef struct {
    uint8_t pin;
    uint8_t port;
    uint8_t bit;
} Dspic33PpsPin;

static const Dspic33PpsOutput pps_outputs[] = {
    {0x0680u, 64u, 0u},  {0x0680u, 65u, 8u},  {0x0682u, 66u, 0u},  {0x0682u, 67u, 8u},
    {0x0684u, 68u, 0u},  {0x0684u, 69u, 8u},  {0x0686u, 70u, 0u},  {0x0686u, 71u, 8u},
    {0x0688u, 79u, 0u},  {0x0688u, 80u, 8u},  {0x068au, 82u, 0u},  {0x068au, 84u, 8u},
    {0x068cu, 85u, 0u},  {0x068cu, 87u, 8u},  {0x068eu, 96u, 0u},  {0x068eu, 97u, 8u},
    {0x0690u, 98u, 0u},  {0x0690u, 99u, 8u},  {0x0692u, 100u, 0u}, {0x0692u, 101u, 8u},
    {0x0696u, 104u, 0u}, {0x0696u, 108u, 8u}, {0x0698u, 109u, 0u}, {0x0698u, 112u, 8u},
    {0x069au, 113u, 0u}, {0x069au, 118u, 8u}, {0x069cu, 120u, 0u}, {0x069cu, 125u, 8u},
    {0x069eu, 126u, 0u}, {0x069eu, 127u, 8u}};

static const Dspic33PpsPin pps_pins[] = {
    {16u, 0u, 0u},   {17u, 0u, 1u},   {18u, 0u, 2u},   {19u, 0u, 3u},  {20u, 0u, 4u},
    {21u, 0u, 5u},   {22u, 0u, 6u},   {23u, 0u, 7u},   {30u, 0u, 14u}, {31u, 0u, 15u},
    {32u, 1u, 0u},   {33u, 1u, 1u},   {34u, 1u, 2u},   {35u, 1u, 3u},  {36u, 1u, 4u},
    {37u, 1u, 5u},   {38u, 1u, 6u},   {39u, 1u, 7u},   {40u, 1u, 8u},  {41u, 1u, 9u},
    {42u, 1u, 10u},  {43u, 1u, 11u},  {44u, 1u, 12u},  {45u, 1u, 13u}, {46u, 1u, 14u},
    {47u, 1u, 15u},  {49u, 2u, 1u},   {50u, 2u, 2u},   {51u, 2u, 3u},  {52u, 2u, 4u},
    {60u, 2u, 12u},  {61u, 2u, 13u},  {62u, 2u, 14u},  {64u, 3u, 0u},  {65u, 3u, 1u},
    {66u, 3u, 2u},   {67u, 3u, 3u},   {68u, 3u, 4u},   {69u, 3u, 5u},  {70u, 3u, 6u},
    {71u, 3u, 7u},   {72u, 3u, 8u},   {73u, 3u, 9u},   {74u, 3u, 10u}, {75u, 3u, 11u},
    {76u, 3u, 12u},  {77u, 3u, 13u},  {78u, 3u, 14u},  {79u, 3u, 15u}, {80u, 4u, 0u},
    {81u, 4u, 1u},   {82u, 4u, 2u},   {83u, 4u, 3u},   {84u, 4u, 4u},  {85u, 4u, 5u},
    {86u, 4u, 6u},   {87u, 4u, 7u},   {88u, 4u, 8u},   {89u, 4u, 9u},  {96u, 5u, 0u},
    {97u, 5u, 1u},   {98u, 5u, 2u},   {99u, 5u, 3u},   {100u, 5u, 4u}, {101u, 5u, 5u},
    {104u, 5u, 8u},  {108u, 5u, 12u}, {109u, 5u, 13u}, {112u, 6u, 0u}, {113u, 6u, 1u},
    {118u, 6u, 6u},  {119u, 6u, 7u},  {120u, 6u, 8u},  {121u, 6u, 9u}, {124u, 6u, 12u},
    {125u, 6u, 13u}, {126u, 6u, 14u}, {127u, 6u, 15u}};

enum {
    TIMER_ON = 0x8000u,
    TIMER_STOP_IDLE = 0x2000u,
    TIMER_GATE = 0x0040u,
    TIMER_PRESCALE_MASK = 0x0030u,
    TIMER_32_BIT = 0x0008u,
    TIMER_SYNC = 0x0004u,
    TIMER_EXTERNAL = 0x0002u,
    DMA_CHANNEL_BASE = 0x0b00u,
    DMA_CHANNEL_STRIDE = 0x0010u,
    DMA_CON_CHEN = 0x8000u,
    DMA_CON_SIZE_BYTE = 0x4000u,
    DMA_CON_RAM_TO_PERIPHERAL = 0x2000u,
    DMA_CON_HALF = 0x1000u,
    DMA_CON_NULL_WRITE = 0x0800u,
    DMA_CON_AMODE_MASK = 0x0030u,
    DMA_CON_AMODE_FIXED = 0x0010u,
    DMA_CON_AMODE_PERIPHERAL = 0x0020u,
    DMA_CON_MODE_MASK = 0x0003u,
    DMA_CON_MODE_ONE_SHOT = 0x0001u,
    DMA_CON_MODE_PING_PONG = 0x0002u,
    DMA_REQ_FORCE = 0x8000u,
    DMA_REQ_SOURCE_MASK = 0x00ffu,
    DMA_EVENT_FORCE = 0x00010000u,
    DMA_EVENT_GENERATION_SHIFT = 17u,
    DMA_EVENT_GENERATION_MASK = 0x7fffu,
    DMA_CHANNEL_MASK = 0x7fffu,
    DMA_PWC = 0x0bf0u,
    DMA_RQC = 0x0bf2u,
    DMA_PPS = 0x0bf4u,
    DMA_LCA = 0x0bf6u,
    DMA_SADRL = 0x0bf8u,
    DMA_SADRH = 0x0bfau,
    NVM_CONTROL = 0x0728u,
    NVM_ADDRESS = 0x072au,
    NVM_ADDRESS_HIGH = 0x072cu,
    NVM_KEY = 0x072eu,
    NVM_WRITE = 0x8000u,
    NVM_WRITE_ENABLE = 0x4000u,
    NVM_WRITE_ERROR = 0x2000u,
    OSCILLATOR_CONTROL = 0x0742u,
    OSCILLATOR_CURRENT_MASK = 0x7000u,
    OSCILLATOR_REQUEST_MASK = 0x0700u,
    OSCILLATOR_CONFIGURATION_SWITCH_DISABLE = 0x0080u,
    OSCILLATOR_CLOCK_LOCK = 0x0080u,
    OSCILLATOR_CONFIGURATION_CLOCK_LOCK = 0x0040u,
    OSCILLATOR_IO_LOCK = 0x0040u,
    OSCILLATOR_PLL_LOCK = 0x0020u,
    OSCILLATOR_CLOCK_FAIL = 0x0008u,
    OSCILLATOR_LP_ENABLE = 0x0002u,
    OSCILLATOR_SWITCH_ENABLE = 0x0001u,
    OSCILLATOR_SWITCH_DELAY = 32u,
    OSCILLATOR_SOURCE_DELAY = 1u,
    OSCILLATOR_CONFIGURATION_PLL_LOCK = 0x0020u,
    OSCILLATOR_EVENT_SWITCH = 0u,
    OSCILLATOR_EVENT_LOCK = 2u,
    CRC_CONTROL = 0x0640u,
    CRC_CONFIG = 0x0642u,
    CRC_POLYNOMIAL_LOW = 0x0644u,
    CRC_POLYNOMIAL_HIGH = 0x0646u,
    CRC_DATA_LOW = 0x0648u,
    CRC_DATA_HIGH = 0x064au,
    CRC_SHIFT_LOW = 0x064cu,
    CRC_SHIFT_HIGH = 0x064eu,
    CRC_PMD_ADDRESS = 0x0764u,
    CRC_ENABLE = 0x8000u,
    CRC_STOP_IDLE = 0x2000u,
    CRC_WORD_COUNT_MASK = 0x1f00u,
    CRC_FULL = 0x0080u,
    CRC_EMPTY = 0x0040u,
    CRC_INTERRUPT_EMPTY = 0x0020u,
    CRC_GO = 0x0010u,
    CRC_LITTLE_ENDIAN = 0x0008u,
    CRC_PMD = 0x0080u,
    CRC_BITS_PER_CYCLE = 2u,
    CRC_IRQ = 67u,
    CRC_EVENT_PMD_SOURCE = UINT16_MAX,
    PMP_CONTROL = 0x0600u,
    PMP_MODE = 0x0602u,
    PMP_ADDRESS = 0x0604u,
    PMP_OUTPUT_2 = 0x0606u,
    PMP_DATA = 0x0608u,
    PMP_INPUT_2 = 0x060au,
    PMP_ADDRESS_ENABLE_REGISTER = 0x060cu,
    PMP_STATUS = 0x060eu,
    PMP_ENABLE = 0x8000u,
    PMP_BUSY = 0x8000u,
    PMP_INTERRUPT_MODE_MASK = 0x6000u,
    PMP_INTERRUPT_EACH = 0x2000u,
    PMP_INTERRUPT_LAST = 0x6000u,
    PMP_DATA_16_BIT = 0x0400u,
    PMP_MASTER_MODE_MASK = 0x0300u,
    PMP_SLAVE_ADDRESSABLE = 0x0100u,
    PMP_MASTER_MODE_2 = 0x0200u,
    PMP_MASTER_MODE_1 = 0x0300u,
    PMP_BUFFERED_SLAVE = 0x1800u,
    PMP_INCREMENT_MODE_MASK = 0x1800u,
    PMP_INCREMENT_ADDRESS = 0x0800u,
    PMP_DECREMENT_ADDRESS = 0x1000u,
    PMP_ADDRESS_MUX_MASK = 0x1800u,
    PMP_CHIP_SELECT_FUNCTION_MASK = 0x00c0u,
    PMP_WAIT_BEGIN_MASK = 0x00c0u,
    PMP_WAIT_MIDDLE_MASK = 0x003cu,
    PMP_WAIT_END_MASK = 0x0003u,
    PMP_DMA_REQUEST = 0x2du,
    PMP_IRQ = 45u,
    PMP_EVENT_COMPLETE = 0u,
    PMP_EVENT_CLEAR_BUSY = 1u,
    PMP_EVENT_PMD = 2u,
    PMP_EVENT_SLAVE_READ = 3u,
    PMP_EVENT_SLAVE_WRITE = 4u,
    PMP_PMD_ADDRESS = 0x0764u,
    PMP_PMD = 0x0100u,
    PMP_STOP_IDLE = 0x2000u,
    PMP_READ_STROBE_ENABLE = 0x0100u,
    PMP_WRITE_STROBE_ENABLE = 0x0200u,
    PMP_CHIP_SELECT_ENABLE = 0x4000u,
    PMP_ADDRESS_ENABLE = 0x0003u,
    PMP_INPUT_FULL = 0x8000u,
    PMP_INPUT_OVERFLOW = 0x4000u,
    PMP_INPUT_BUFFER_MASK = 0x0f00u,
    PMP_OUTPUT_EMPTY = 0x0080u,
    PMP_OUTPUT_UNDERFLOW = 0x0040u,
    PMP_OUTPUT_BUFFER_MASK = 0x000fu,
    INPUT_CAPTURE_BASE = 0x0140u,
    INPUT_CAPTURE_STRIDE = 0x0008u,
    INPUT_CAPTURE_CON1_WRITABLE = 0x3c67u,
    INPUT_CAPTURE_CON2_WRITABLE = 0x01dfu,
    INPUT_CAPTURE_TIMER_SOURCE_MASK = 0x1c00u,
    INPUT_CAPTURE_TIMER_SOURCE_FP = 0x1c00u,
    INPUT_CAPTURE_32_BIT = 0x0100u,
    INPUT_CAPTURE_TRIGGER = 0x0080u,
    INPUT_CAPTURE_TRIGGER_STATUS = 0x0040u,
    INPUT_CAPTURE_INTERRUPT_MASK = 0x0060u,
    INPUT_CAPTURE_OVERFLOW = 0x0010u,
    INPUT_CAPTURE_NOT_EMPTY = 0x0008u,
    INPUT_CAPTURE_MODE_MASK = 0x0007u,
    INPUT_CAPTURE_MODE_EVERY_EDGE = 0x0001u,
    INPUT_CAPTURE_MODE_FALLING = 0x0002u,
    INPUT_CAPTURE_MODE_RISING = 0x0003u,
    INPUT_CAPTURE_MODE_EVERY_FOURTH_RISING = 0x0004u,
    INPUT_CAPTURE_MODE_EVERY_SIXTEENTH_RISING = 0x0005u,
    INPUT_CAPTURE_MODE_INTERRUPT = 0x0007u,
    INPUT_CAPTURE_STOP_IDLE = 0x2000u,
    INPUT_CAPTURE_SYNC_MASK = 0x001fu,
    INPUT_CAPTURE_SYNC_OC_FIRST = 1u,
    INPUT_CAPTURE_SYNC_TIMER_FIRST = 11u,
    INPUT_CAPTURE_SYNC_IC_FIRST = 16u,
    INPUT_CAPTURE_SYNC_COMPARATOR_FIRST = 24u,
    INPUT_CAPTURE_SYNC_ADC1 = 27u,
    INPUT_CAPTURE_SYNC_RESERVED = 28u,
    INPUT_CAPTURE_EVENT_KIND_MASK = 0x00000007u,
    INPUT_CAPTURE_EVENT_INPUT = 0u,
    INPUT_CAPTURE_EVENT_CAPTURE = 1u,
    INPUT_CAPTURE_EVENT_INTERRUPT = 2u,
    INPUT_CAPTURE_EVENT_PIN = 3u,
    INPUT_CAPTURE_EVENT_PMD = 4u,
    INPUT_CAPTURE_EVENT_HIGH = 0x00000008u,
    INPUT_CAPTURE_EVENT_PAIRED = 0x00000010u,
    INPUT_CAPTURE_EVENT_PMD_DISABLED = 0x00000008u,
    INPUT_CAPTURE_EVENT_GENERATION_SHIFT = 8u,
    OUTPUT_COMPARE_BASE = 0x0900u,
    OUTPUT_COMPARE_STRIDE = 0x000au,
    OUTPUT_COMPARE_CON1_WRITABLE = 0x3fffu,
    OUTPUT_COMPARE_CON2_WRITABLE = 0xf1ffu,
    OUTPUT_COMPARE_TIMER_SOURCE_MASK = 0x1c00u,
    OUTPUT_COMPARE_TIMER_SOURCE_TIMER2 = 0x0000u,
    OUTPUT_COMPARE_TIMER_SOURCE_TIMER3 = 0x0400u,
    OUTPUT_COMPARE_TIMER_SOURCE_TIMER4 = 0x0800u,
    OUTPUT_COMPARE_TIMER_SOURCE_TIMER5 = 0x0c00u,
    OUTPUT_COMPARE_TIMER_SOURCE_TIMER1 = 0x1000u,
    OUTPUT_COMPARE_TIMER_SOURCE_FP = 0x1c00u,
    OUTPUT_COMPARE_STOP_IDLE = 0x2000u,
    OUTPUT_COMPARE_FAULT_ENABLE_MASK = 0x0380u,
    OUTPUT_COMPARE_FAULT_STATUS_MASK = 0x0070u,
    OUTPUT_COMPARE_CON1_UNSUPPORTED = 0x0000u,
    OUTPUT_COMPARE_MODE_MASK = 0x0007u,
    OUTPUT_COMPARE_MODE_SINGLE_HIGH = 0x0001u,
    OUTPUT_COMPARE_MODE_SINGLE_LOW = 0x0002u,
    OUTPUT_COMPARE_MODE_SINGLE_TOGGLE = 0x0003u,
    OUTPUT_COMPARE_MODE_DUAL_SINGLE = 0x0004u,
    OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS = 0x0005u,
    OUTPUT_COMPARE_MODE_EDGE_PWM = 0x0006u,
    OUTPUT_COMPARE_MODE_CENTER_PWM = 0x0007u,
    OUTPUT_COMPARE_FAULT_INACTIVE = 0x8000u,
    OUTPUT_COMPARE_FAULT_OUTPUT = 0x4000u,
    OUTPUT_COMPARE_FAULT_TRISTATE = 0x2000u,
    OUTPUT_COMPARE_CON2_UNSUPPORTED = 0x0000u,
    OUTPUT_COMPARE_INVERT = 0x1000u,
    OUTPUT_COMPARE_32_BIT = 0x0100u,
    OUTPUT_COMPARE_TRIGGER = 0x0080u,
    OUTPUT_COMPARE_TRIGGER_STATUS = 0x0040u,
    OUTPUT_COMPARE_TRISTATE = 0x0020u,
    OUTPUT_COMPARE_TRIGGER_ONESHOT = 0x0008u,
    OUTPUT_COMPARE_SYNC_MASK = 0x001fu,
    OUTPUT_COMPARE_SYNC_NONE = 0x0000u,
    OUTPUT_COMPARE_SYNC_SELF = 0x001fu,
    OUTPUT_COMPARE_SYNC_OC_FIRST = 1u,
    OUTPUT_COMPARE_SYNC_TIMER_FIRST = 11u,
    OUTPUT_COMPARE_SYNC_IC_FIRST = 16u,
    OUTPUT_COMPARE_SYNC_COMPARATOR_FIRST = 24u,
    OUTPUT_COMPARE_SYNC_ADC1 = 27u,
    OUTPUT_COMPARE_SYNC_RESERVED = 28u,
    OUTPUT_COMPARE_SYNC_INT1 = 29u,
    OUTPUT_COMPARE_SYNC_INT2 = 30u,
    OUTPUT_COMPARE_EVENT_KIND_MASK = 0x0000000fu,
    OUTPUT_COMPARE_EVENT_PRIMARY = 0u,
    OUTPUT_COMPARE_EVENT_SECONDARY = 1u,
    OUTPUT_COMPARE_EVENT_BOUNDARY = 2u,
    OUTPUT_COMPARE_EVENT_APPLY_PRIMARY = 3u,
    OUTPUT_COMPARE_EVENT_APPLY_SECONDARY = 4u,
    OUTPUT_COMPARE_EVENT_INTERRUPT = 5u,
    OUTPUT_COMPARE_EVENT_SYNC = 6u,
    OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY = 7u,
    OUTPUT_COMPARE_EVENT_SYNC_PRIMARY = 8u,
    OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC = 9u,
    OUTPUT_COMPARE_EVENT_PMD = 10u,
    OUTPUT_COMPARE_EVENT_PMD_DISABLED = 0x00000010u,
    OUTPUT_COMPARE_EVENT_PMD_GENERATION_SHIFT = 5u,
    OUTPUT_COMPARE_EVENT_GENERATION_SHIFT = 4u,
    OUTPUT_COMPARE_FAULT_EVENT_HIGH = 0x00000001u,
    OUTPUT_COMPARE_FAULT_EVENT_PIN = 0x00000002u,
    OUTPUT_COMPARE_FAULT_PPS_AB = 0x06b6u,
    OUTPUT_COMPARE_FAULT_PPS_C = 0x06eau,
    COMPARATOR_STATUS = 0x0a80u,
    COMPARATOR_REFERENCE = 0x0a82u,
    COMPARATOR_BASE = 0x0a84u,
    COMPARATOR_STRIDE = 0x0008u,
    COMPARATOR_CONTROL_WRITABLE = 0xe2d3u,
    COMPARATOR_ENABLE = 0x8000u,
    COMPARATOR_OUTPUT_ENABLE = 0x4000u,
    COMPARATOR_POLARITY = 0x2000u,
    COMPARATOR_EVENT = 0x0200u,
    COMPARATOR_OUTPUT = 0x0100u,
    COMPARATOR_EVENT_POLARITY_MASK = 0x00c0u,
    COMPARATOR_REFERENCE_INTERNAL = 0x0010u,
    COMPARATOR_CHANNEL_MASK = 0x0003u,
    COMPARATOR_STOP_IDLE = 0x8000u,
    COMPARATOR_FILTER_ENABLE = 0x0008u,
    COMPARATOR_FILTER_SOURCE_MASK = 0x0070u,
    COMPARATOR_FILTER_DIVIDER_MASK = 0x0007u,
    COMPARATOR_REFERENCE_EXTERNAL = 0x0400u,
    COMPARATOR_REFERENCE_BAND_GAP_MASK = 0x0300u,
    COMPARATOR_REFERENCE_ENABLE = 0x0080u,
    COMPARATOR_REFERENCE_LOW_RANGE = 0x0020u,
    COMPARATOR_REFERENCE_SOURCE_EXTERNAL = 0x0010u,
    COMPARATOR_PMD_ADDRESS = 0x0764u,
    COMPARATOR_PMD = 0x0400u,
    COMPARATOR_IRQ = 18u,
    COMPARATOR_PPS_FUNCTION = 0x18u,
    COMPARATOR_EVENT_INPUT_COUNT =
        DSPIC33_COMPARATOR_COUNT * DSPIC33_COMPARATOR_INPUT_COUNT,
    COMPARATOR_EVENT_REFERENCE_FIRST = COMPARATOR_EVENT_INPUT_COUNT,
    COMPARATOR_EVENT_FILTER_FIRST = 0xfff0u,
    COMPARATOR_EVENT_PMD_SOURCE = UINT16_MAX,
    RTCC_ALARM_VALUE = 0x0620u,
    RTCC_ALARM_CONTROL = 0x0622u,
    RTCC_VALUE = 0x0624u,
    RTCC_CONTROL = 0x0626u,
    RTCC_PAD_CONTROL = 0x0efeu,
    RTCC_PMD_ADDRESS = 0x0764u,
    RTCC_ENABLE = 0x8000u,
    RTCC_WRITE_ENABLE = 0x2000u,
    RTCC_SYNC = 0x1000u,
    RTCC_HALF_SECOND = 0x0800u,
    RTCC_OUTPUT_ENABLE = 0x0400u,
    RTCC_POINTER_MASK = 0x0300u,
    RTCC_ALARM_ENABLE = 0x8000u,
    RTCC_ALARM_CHIME = 0x4000u,
    RTCC_ALARM_MASK = 0x3c00u,
    RTCC_ALARM_POINTER_MASK = 0x0300u,
    RTCC_PMD = 0x0200u,
    RTCC_LPOSC_ENABLE = 0x0002u,
    RTCC_SECONDS_OUTPUT = 0x0002u,
    RTCC_IRQ = 62u,
    RTCC_PRESCALER_EDGES = 32768u,
    RTCC_CALIBRATION_EDGE = 512u,
    RTCC_HALF_SECOND_EDGE = 16384u,
    RTCC_SYNC_EDGES = 32u,
    RTCC_EVENT_PMD_SOURCE = UINT16_MAX,
    UART_MODE_ENABLE = 0x8000u,
    UART_MODE_IREN = 0x1000u,
    UART_MODE_UEN_MASK = 0x0300u,
    UART_MODE_WAKE = 0x0080u,
    UART_MODE_LOOPBACK = 0x0040u,
    UART_MODE_AUTO_BAUD = 0x0020u,
    UART_MODE_HIGH_SPEED = 0x0008u,
    UART_MODE_DATA_MASK = 0x0006u,
    UART_MODE_NINE_BIT = 0x0006u,
    UART_MODE_TWO_STOP_BITS = 0x0001u,
    UART_STATUS_TX_INTERRUPT_HIGH = 0x8000u,
    UART_STATUS_TX_INVERT = 0x4000u,
    UART_STATUS_TX_INTERRUPT_LOW = 0x2000u,
    UART_STATUS_BREAK = 0x0800u,
    UART_STATUS_TX_ENABLE = 0x0400u,
    UART_STATUS_TX_FULL = 0x0200u,
    UART_STATUS_TX_EMPTY = 0x0100u,
    UART_STATUS_RX_INTERRUPT_MASK = 0x00c0u,
    UART_STATUS_ADDRESS_DETECT = 0x0020u,
    UART_STATUS_RX_IDLE = 0x0010u,
    UART_STATUS_PARITY_ERROR = 0x0008u,
    UART_STATUS_FRAMING_ERROR = 0x0004u,
    UART_STATUS_OVERRUN = 0x0002u,
    UART_STATUS_RX_AVAILABLE = 0x0001u,
    UART_EVENT_KIND_SHIFT = 30u,
    UART_EVENT_KIND_MASK = 0xc0000000u,
    UART_EVENT_RECEIVE = 0x00000000u,
    UART_EVENT_TRANSMIT = 0x40000000u,
    UART_EVENT_CTS = 0x80000000u,
    UART_EVENT_PARITY_ERROR = 0x00000200u,
    UART_EVENT_FRAMING_ERROR = 0x00000400u,
    UART_EVENT_BAUD_SHIFT = 11u,
    UART_EVENT_BAUD_MASK = 0x07fff800u,
    SPI_ENABLE = 0x8000u,
    SPI_STOP_IDLE = 0x2000u,
    SPI_BUFFER_COUNT_MASK = 0x0700u,
    SPI_SHIFT_EMPTY = 0x0080u,
    SPI_OVERFLOW = 0x0040u,
    SPI_RX_EMPTY = 0x0020u,
    SPI_INTERRUPT_MODE_MASK = 0x001cu,
    SPI_TX_FULL = 0x0002u,
    SPI_RX_FULL = 0x0001u,
    SPI_DISABLE_CLOCK = 0x1000u,
    SPI_MODE_16 = 0x0400u,
    SPI_SAMPLE_END = 0x0200u,
    SPI_MASTER = 0x0020u,
    SPI_FRAME_ENABLE = 0x8000u,
    SPI_FRAME_SLAVE = 0x4000u,
    SPI_FRAME_ACTIVE_HIGH = 0x2000u,
    SPI_FRAME_DELAY = 0x0002u,
    SPI_ENHANCED = 0x0001u,
    SPI_EVENT_EXTERNAL = 0x00010000u,
    SPI_EVENT_INTERNAL = 0x00020000u,
    SPI_EVENT_FRAME = 0x00030000u,
    SPI_EVENT_KIND_MASK = 0x00030000u,
    SPI_EVENT_GENERATION_SHIFT = 18u,
    SPI_EVENT_GENERATION_MASK = 0x3fffu,
    SPI_SELECT_ACTIVE = 0x00000001u,
    CAN_WINDOW = 0x0001u,
    CAN_CAPTURE = 0x0008u,
    CAN_MODE_MASK = 0x0700u,
    CAN_MODE_SHIFT = 8u,
    CAN_MODE_NORMAL = 0u,
    CAN_MODE_DISABLE = 1u,
    CAN_MODE_LOOPBACK = 2u,
    CAN_MODE_LISTEN = 3u,
    CAN_MODE_CONFIGURATION = 4u,
    CAN_MODE_LISTEN_ALL = 7u,
    CAN_ABORT_ALL = 0x1000u,
    CAN_STOP_IDLE = 0x2000u,
    CAN_INTERRUPT_TRANSMIT = 0x0001u,
    CAN_INTERRUPT_RECEIVE = 0x0002u,
    CAN_INTERRUPT_OVERFLOW = 0x0004u,
    CAN_INTERRUPT_FIFO = 0x0008u,
    CAN_INTERRUPT_ERROR = 0x0020u,
    CAN_INTERRUPT_WAKE = 0x0040u,
    CAN_INTERRUPT_INVALID = 0x0080u,
    CAN_ERROR_WARNING = 0x0100u,
    CAN_RECEIVE_WARNING = 0x0200u,
    CAN_TRANSMIT_WARNING = 0x0400u,
    CAN_RECEIVE_PASSIVE = 0x0800u,
    CAN_TRANSMIT_PASSIVE = 0x1000u,
    CAN_BUS_OFF = 0x2000u,
    CAN_BUFFER_TRANSMIT = 0x0080u,
    CAN_BUFFER_ABORTED = 0x0040u,
    CAN_BUFFER_LOST = 0x0020u,
    CAN_BUFFER_ERROR = 0x0010u,
    CAN_BUFFER_REQUEST = 0x0008u,
    CAN_BUFFER_REMOTE = 0x0004u,
    CAN_EVENT_RECEIVE_START = 1u,
    CAN_EVENT_RECEIVE_WORD = 2u,
    CAN_EVENT_RECEIVE_FINISH = 3u,
    CAN_EVENT_TRANSMIT_START = 4u,
    CAN_EVENT_TRANSMIT_WORD = 5u,
    CAN_EVENT_TRANSMIT_FINISH = 6u,
    CAN_EVENT_ERROR = 7u,
    CAN_EVENT_KIND_MASK = 0x000000ffu,
    CAN_EVENT_TRANSMIT_ERROR = 0x00000100u,
    CAN_EVENT_ERROR_COUNT_SHIFT = 16u,
    USB_OTGIR = 0x0488u,
    USB_OTGIE = 0x048au,
    USB_OTGSTAT = 0x048cu,
    USB_OTGCON = 0x048eu,
    USB_PWRC = 0x0490u,
    USB_IR = 0x04c0u,
    USB_IE = 0x04c2u,
    USB_EIR = 0x04c4u,
    USB_EIE = 0x04c6u,
    USB_STAT = 0x04c8u,
    USB_CON = 0x04cau,
    USB_ADDR = 0x04ccu,
    USB_BDTP1 = 0x04ceu,
    USB_FRML = 0x04d0u,
    USB_FRMH = 0x04d2u,
    USB_TOK = 0x04d4u,
    USB_SOF = 0x04d6u,
    USB_BDTP2 = 0x04d8u,
    USB_BDTP3 = 0x04dau,
    USB_CNFG1 = 0x04dcu,
    USB_CNFG2 = 0x04deu,
    USB_EP0 = 0x04e0u,
    USB_PWMRRS = 0x0580u,
    USB_PWMCON = 0x0582u,
    USB_POWER = 0x0001u,
    USB_SUSPEND = 0x0002u,
    USB_SLEEP_GUARD = 0x0010u,
    USB_ACTIVITY_PENDING = 0x0080u,
    USB_ENABLE = 0x0001u,
    USB_PING_PONG_RESET = 0x0002u,
    USB_RESUME = 0x0004u,
    USB_HOST_ENABLE = 0x0008u,
    USB_HOST_RESET = 0x0010u,
    USB_PACKET_DISABLE = 0x0020u,
    USB_TOKEN_BUSY = 0x0020u,
    USB_STALL_INTERRUPT = 0x0080u,
    USB_ATTACH_INTERRUPT = 0x0040u,
    USB_RESUME_INTERRUPT = 0x0020u,
    USB_IDLE_INTERRUPT = 0x0010u,
    USB_TRANSACTION_INTERRUPT = 0x0008u,
    USB_SOF_INTERRUPT = 0x0004u,
    USB_ERROR_INTERRUPT = 0x0002u,
    USB_DETACH_INTERRUPT = 0x0001u,
    USB_RESET_INTERRUPT = 0x0001u,
    USB_ENDPOINT_CONTROL_DISABLED = 0x0010u,
    USB_ENDPOINT_RX_ENABLE = 0x0008u,
    USB_ENDPOINT_TX_ENABLE = 0x0004u,
    USB_ENDPOINT_STALL = 0x0002u,
    USB_ENDPOINT_HANDSHAKE = 0x0001u,
    USB_DESCRIPTOR_OWNED = 0x0080u,
    USB_DESCRIPTOR_DATA1 = 0x0040u,
    USB_DESCRIPTOR_KEEP = 0x0020u,
    USB_DESCRIPTOR_NO_INCREMENT = 0x0010u,
    USB_DESCRIPTOR_DTS_ENABLE = 0x0008u,
    USB_DESCRIPTOR_STALL = 0x0004u,
    USB_DESCRIPTOR_PID_MASK = 0x003cu,
    USB_DESCRIPTOR_COUNT_MASK = 0x03ffu,
    USB_OTG_VOLTAGE_STATUS = 0x0009u,
    USB_ERROR_BUS_ACCESS = 0x0040u,
    USB_ERROR_DMA = 0x0020u,
    USB_ERROR_BTO = 0x0010u,
    USB_ERROR_DFN8 = 0x0008u,
    USB_ERROR_CRC16 = 0x0004u,
    USB_ERROR_CRC5 = 0x0002u,
    USB_ERROR_PID = 0x0001u,
    USB_IRQ = 86u,
    USB_FRAME_CYCLES = 60000u,
    ADC_ON = 0x8000u,
    ADC_STOP_IDLE = 0x2000u,
    ADC_BUFFER_ORDER = 0x1000u,
    ADC_12_BIT = 0x0400u,
    ADC_FORMAT_MASK = 0x0300u,
    ADC_TRIGGER_MASK = 0x00f0u,
    ADC_SIMULTANEOUS = 0x0008u,
    ADC_AUTO_SAMPLE = 0x0004u,
    ADC_SAMPLE = 0x0002u,
    ADC_DONE = 0x0001u,
    ADC_SCAN = 0x0400u,
    ADC_CHANNELS_MASK = 0x0300u,
    ADC_SECOND_BUFFER = 0x0080u,
    ADC_BUFFER_SPLIT = 0x0002u,
    ADC_ALTERNATE = 0x0001u,
    ADC_DMA_ENABLE = 0x0100u,
    ADC_DMA_LENGTH_MASK = 0x0007u,
    ADC_EVENT_COMPLETE = 0x00000100u,
    ADC_EVENT_SOURCE_MASK = 0x000000ffu,
    ADC_EVENT_GENERATION_SHIFT = 16u,
    PWM_GLOBAL_BASE = 0x0c00u,
    PWM_GENERATOR_BASE = 0x0c20u,
    PWM_GENERATOR_STRIDE = 0x0020u,
    PWM_ENABLE = 0x8000u,
    PWM_STOP_IDLE = 0x2000u,
    PWM_SPECIAL_STATUS = 0x1000u,
    PWM_SPECIAL_INTERRUPT = 0x0800u,
    PWM_INDEPENDENT_TIME_BASE = 0x0200u,
    PWM_MASTER_DUTY = 0x0100u,
    PWM_CENTER_ALIGNED = 0x0004u,
    PWM_EXTERNAL_RESET = 0x0002u,
    PWM_IMMEDIATE_UPDATE = 0x0001u,
    PWM_FAULT_STATUS = 0x8000u,
    PWM_CURRENT_LIMIT_STATUS = 0x4000u,
    PWM_TRIGGER_STATUS = 0x2000u,
    PWM_FAULT_INTERRUPT = 0x1000u,
    PWM_CURRENT_LIMIT_INTERRUPT = 0x0800u,
    PWM_TRIGGER_INTERRUPT = 0x0400u,
    PWM_PIN_HIGH = 0x8000u,
    PWM_PIN_LOW = 0x4000u,
    PWM_POLARITY_HIGH = 0x2000u,
    PWM_POLARITY_LOW = 0x1000u,
    PWM_MODE_MASK = 0x0c00u,
    PWM_MODE_REDUNDANT = 0x0400u,
    PWM_MODE_PUSH_PULL = 0x0800u,
    PWM_MODE_INDEPENDENT = 0x0c00u,
    PWM_OVERRIDE_HIGH = 0x0200u,
    PWM_OVERRIDE_LOW = 0x0100u,
    PWM_OVERRIDE_DATA = 0x00c0u,
    PWM_SWAP = 0x0002u,
    PWM_OVERRIDE_SYNCHRONIZED = 0x0001u,
    PWM_SYNCHRONIZED_IO =
        PWM_OVERRIDE_HIGH | PWM_OVERRIDE_LOW | PWM_OVERRIDE_DATA | PWM_SWAP,
    PWM_FAULT_MODE_MASK = 0x0003u,
    PWM_FAULT_CYCLE = 0x0001u,
    PWM_FAULT_DISABLED = 0x0003u,
    PWM_CURRENT_LIMIT_MODE = 0x0100u,
    PWM_INPUT_HIGH = 0x00000001u,
    PWM_INPUT_KIND = 0x00000002u,
    QEI_CONTROL_ENABLE = 0x8000u,
    QEI_CONTROL_STOP_IDLE = 0x2000u,
    QEI_CONTROL_POSITION_MODE_MASK = 0x1c00u,
    QEI_CONTROL_POSITION_MODE_SHIFT = 10u,
    QEI_CONTROL_INDEX_MATCH_MASK = 0x0300u,
    QEI_CONTROL_INDEX_MATCH_SHIFT = 8u,
    QEI_CONTROL_DIVIDER_MASK = 0x0070u,
    QEI_CONTROL_DIVIDER_SHIFT = 4u,
    QEI_CONTROL_DIRECTION_INVERT = 0x0008u,
    QEI_CONTROL_GATE_ENABLE = 0x0004u,
    QEI_CONTROL_COUNT_MODE_MASK = 0x0003u,
    QEI_IO_CAPTURE_HOME = 0x8000u,
    QEI_IO_FILTER_ENABLE = 0x4000u,
    QEI_IO_FILTER_DIVIDER_MASK = 0x3800u,
    QEI_IO_FILTER_DIVIDER_SHIFT = 11u,
    QEI_IO_OUTPUT_MASK = 0x0600u,
    QEI_IO_OUTPUT_SHIFT = 9u,
    QEI_IO_SWAP = 0x0100u,
    QEI_IO_POLARITY_MASK = 0x00f0u,
    QEI_IO_INPUT_MASK = 0x000fu,
    QEI_STATUS_ENABLE_MASK = 0x1555u,
    QEI_STATUS_FLAG_MASK = 0x2aaau,
    QEI_STATUS_INDEX = 0x0002u,
    QEI_STATUS_HOME = 0x0008u,
    QEI_STATUS_VELOCITY_OVERFLOW = 0x0020u,
    QEI_STATUS_INITIALIZED = 0x0080u,
    QEI_STATUS_POSITION_OVERFLOW = 0x0200u,
    QEI_STATUS_LOW_COMPARE = 0x0800u,
    QEI_STATUS_HIGH_COMPARE = 0x2000u,
    QEI_POSITION_LOW = 0x0006u,
    QEI_POSITION_HIGH = 0x0008u,
    QEI_POSITION_HOLD = 0x000au,
    QEI_VELOCITY = 0x000cu,
    QEI_INTERVAL_LOW = 0x000eu,
    QEI_INTERVAL_HIGH = 0x0010u,
    QEI_INTERVAL_HOLD_LOW = 0x0012u,
    QEI_INTERVAL_HOLD_HIGH = 0x0014u,
    QEI_INDEX_LOW = 0x0016u,
    QEI_INDEX_HIGH = 0x0018u,
    QEI_INDEX_HOLD = 0x001au,
    QEI_GREATER_EQUAL_LOW = 0x001cu,
    QEI_GREATER_EQUAL_HIGH = 0x001eu,
    QEI_LESS_EQUAL_LOW = 0x0020u,
    QEI_LESS_EQUAL_HIGH = 0x0022u,
    QEI_PMD_EVENT_BASE = 0x0100u,
    DCI_BASE = 0x0280u,
    DCI_CONTROL1 = 0x0280u,
    DCI_CONTROL2 = 0x0282u,
    DCI_CONTROL3 = 0x0284u,
    DCI_STATUS = 0x0286u,
    DCI_TRANSMIT_SLOTS = 0x0288u,
    DCI_RECEIVE_SLOTS = 0x028cu,
    DCI_RECEIVE_BASE = 0x0290u,
    DCI_TRANSMIT_BASE = 0x0298u,
    DCI_CONTROL_ENABLE = 0x8000u,
    DCI_CONTROL_STOP_IDLE = 0x2000u,
    DCI_CONTROL_LOOPBACK = 0x0800u,
    DCI_CONTROL_EXTERNAL_CLOCK = 0x0400u,
    DCI_CONTROL_SAMPLE_RISING = 0x0200u,
    DCI_CONTROL_EXTERNAL_FRAME = 0x0100u,
    DCI_CONTROL_UNDERFLOW_LAST = 0x0080u,
    DCI_CONTROL_TRISTATE = 0x0040u,
    DCI_CONTROL_DATA_JUSTIFY = 0x0020u,
    DCI_CONTROL_MODE_MASK = 0x0003u,
    DCI_MODE_MULTI = 0u,
    DCI_MODE_I2S = 1u,
    DCI_MODE_AC_LINK_16 = 2u,
    DCI_MODE_AC_LINK_20 = 3u,
    DCI_CONTROL_SUPPORTED_MASK = 0xafe3u,
    DCI_CONTROL2_BUFFER_MASK = 0x0c00u,
    DCI_CONTROL2_FRAME_MASK = 0x01e0u,
    DCI_CONTROL2_WORD_MASK = 0x000fu,
    DCI_STATUS_SLOT_MASK = 0x0f00u,
    DCI_STATUS_RECEIVE_OVERFLOW = 0x0008u,
    DCI_STATUS_RECEIVE_FULL = 0x0004u,
    DCI_STATUS_TRANSMIT_UNDERFLOW = 0x0002u,
    DCI_STATUS_TRANSMIT_EMPTY = 0x0001u,
    DCI_PMD_ADDRESS = 0x0760u,
    DCI_PMD = 0x0100u,
    DCI_TRANSFER_IRQ = 60u,
    DCI_ERROR_IRQ = 59u,
    DCI_DMA_REQUEST = 0x3cu,
    DCI_PPS_INPUTS = 0x06d0u,
    DCI_PPS_FRAME = 0x06d2u,
    DCI_PPS_DATA_OUTPUT = 11u,
    DCI_PPS_CLOCK_OUTPUT = 12u,
    DCI_PPS_FRAME_OUTPUT = 13u,
    DCI_EVENT_START = 0u,
    DCI_EVENT_INTERNAL = 1u,
    DCI_EVENT_EXTERNAL = 2u,
    DCI_EVENT_EXTERNAL_FRAME = 3u,
    DCI_EVENT_SAMPLE = 4u,
    DCI_EVENT_FRAME_START = 5u,
    DCI_EVENT_PMD = UINT16_MAX,
    DCI_EVENT_DISABLED = 0x00000001u,
    DCI_EVENT_GENERATION_SHIFT = 1u,
    AUXILIARY_CLOCK_CONTROL = 0x0758u,
    AUXILIARY_CLOCK_DIVISOR = 0x075au,
    REFERENCE_CLOCK_CONTROL = 0x074eu,
    REFERENCE_CLOCK_ENABLE = 0x8000u,
    REFERENCE_CLOCK_DIVISOR = 0x0f00u,
    MAIN_CLOCK_DIVISOR = 0x0744u,
    MAIN_PLL_FEEDBACK = 0x0746u,
    MAIN_OSCILLATOR_TUNING = 0x0748u,
    MAIN_PLL_PRESCALER = 0x001fu,
    MAIN_FRC_DIVISOR = 0x0700u,
    MAIN_PLL_FEEDBACK_DIVISOR = 0x01ffu,
    MAIN_FRC_TUNING = 0x003fu,
    AUXILIARY_PLL_ENABLE = 0x8000u,
    AUXILIARY_PLL_LOCK = 0x4000u,
    AUXILIARY_CLOCK_WRITABLE = 0xbee7u,
    AUXILIARY_CLOCK_DIVISOR_WRITABLE = 0x0007u,
    AUXILIARY_CLOCK_SOURCE_PRIMARY = 0x0400u,
    AUXILIARY_CLOCK_SOURCE_FRC = 0x0200u,
    AUXILIARY_CLOCK_OSCILLATOR_MODE = 0x1800u,
    AUXILIARY_PLL_PRESCALER = 0x0007u,
    AUXILIARY_PLL_LOCK_DELAY = 32u
};

static bool usb_schedule_bus_event(Dspic33* cpu, Dspic33UsbBusEvent event,
                                   uint16_t value, uint64_t delay);

static const Dspic33RegisterMask register_masks[] = {
    {0x0046u, 0xcfffu}, {0x0048u, 0xfffeu}, {0x004au, 0xfffeu}, {0x004cu, 0xfffeu},
    {0x004eu, 0xfffeu}, {0x0050u, 0xffffu}, {0x0104u, 0xa076u}, {0x0110u, 0xa07au},
    {0x0112u, 0xa072u}, {0x011eu, 0xa07au}, {0x0120u, 0xa072u}, {0x012cu, 0xa07au},
    {0x012eu, 0xa072u}, {0x013au, 0xa07au}, {0x013cu, 0xa072u}, {0x01c0u, 0xbf7fu},
    {0x01c2u, 0xfff0u}, {0x01c4u, 0x3fffu}, {0x0280u, 0xafe3u}, {0x0282u, 0x0defu},
    {0x0284u, 0x0fffu}, {0x0286u, 0x0000u}, {0x0288u, 0xffffu}, {0x028cu, 0xffffu},
    {0x0290u, 0x0000u}, {0x0292u, 0x0000u}, {0x0294u, 0x0000u}, {0x0296u, 0x0000u},
    {0x0298u, 0xffffu}, {0x029au, 0xffffu}, {0x029cu, 0xffffu}, {0x029eu, 0xffffu},
    {0x05c0u, 0xbf7fu}, {0x05c2u, 0xfff0u}, {0x05c4u, 0x3fffu}, {0x0600u, 0xbfffu},
    {0x0602u, 0x7fffu}, {0x060eu, 0x4040u}, {0x0620u, 0xffffu}, {0x0622u, 0xffffu},
    {0x0624u, 0xffffu}, {0x0626u, 0xa7ffu}, {0x0640u, 0xa038u}, {0x0642u, 0x1f1fu},
    {0x0644u, 0xfffeu}, {0x0646u, 0xffffu}, {0x0648u, 0xffffu}, {0x064au, 0xffffu},
    {0x064cu, 0xffffu}, {0x064eu, 0xffffu}, {0x0680u, 0x3f3fu}, {0x0682u, 0x3f3fu},
    {0x0684u, 0x3f3fu}, {0x0686u, 0x3f3fu}, {0x0688u, 0x3f3fu}, {0x068au, 0x3f3fu},
    {0x068cu, 0x3f3fu}, {0x068eu, 0x3f3fu}, {0x0690u, 0x3f3fu}, {0x0692u, 0x3f3fu},
    {0x0696u, 0x3f3fu}, {0x0698u, 0x3f3fu}, {0x069au, 0x3f3fu}, {0x069cu, 0x3f3fu},
    {0x069eu, 0x3f3fu}, {0x06a0u, 0x7f00u}, {0x06a2u, 0x7f7fu}, {0x06a4u, 0x7f7fu},
    {0x06a6u, 0x7f7fu}, {0x06a8u, 0x7f7fu}, {0x06aau, 0x7f7fu}, {0x06acu, 0x7f7fu},
    {0x06aeu, 0x7f7fu}, {0x06b0u, 0x7f7fu}, {0x06b2u, 0x7f7fu}, {0x06b4u, 0x7f7fu},
    {0x06b6u, 0x7f7fu}, {0x06b8u, 0x7f7fu}, {0x06bau, 0x7f7fu}, {0x06bcu, 0x7f7fu},
    {0x06beu, 0x7f7fu}, {0x06c0u, 0x7f7fu}, {0x06c2u, 0x7f7fu}, {0x06c4u, 0x7f7fu},
    {0x06c6u, 0x7f7fu}, {0x06c8u, 0x7f7fu}, {0x06cau, 0x007fu}, {0x06ceu, 0x007fu},
    {0x06d0u, 0x7f7fu}, {0x06d2u, 0x007fu}, {0x06d4u, 0x7f7fu}, {0x06d6u, 0x7f7fu},
    {0x06d8u, 0x7f7fu}, {0x06dau, 0x7f7fu}, {0x06dcu, 0x007fu}, {0x06deu, 0x7f7fu},
    {0x06e0u, 0x007fu}, {0x06e2u, 0x7f7fu}, {0x06e4u, 0x7f7fu}, {0x06e6u, 0x7f7fu},
    {0x06e8u, 0x7f7fu}, {0x06eau, 0x7f7fu}, {0x06ecu, 0x7f7fu}, {0x06eeu, 0x7f7fu},
    {0x06f0u, 0x7f7fu}, {0x06f2u, 0x007fu}, {0x06f4u, 0x7f7fu}, {0x06f6u, 0x007fu},
    {0x0728u, 0x700fu}, {0x072cu, 0x00ffu}, {0x072eu, 0x00ffu}, {0x0740u, 0xcbffu},
    {0x0744u, 0xffdfu}, {0x0746u, 0x01ffu}, {0x0748u, 0x003fu}, {0x074eu, 0xbf00u},
    {0x075au, 0x0007u}, {0x0760u, 0xffffu}, {0x0762u, 0xffffu}, {0x0764u, 0xf7abu},
    {0x0766u, 0x0021u}, {0x0768u, 0xffffu}, {0x076au, 0x3f03u}, {0x076cu, 0x00f0u},
    {0x0800u, 0xffffu}, {0x0802u, 0xffffu}, {0x0804u, 0xffffu}, {0x0806u, 0x7fffu},
    {0x0808u, 0x0afeu}, {0x080au, 0xdfeeu}, {0x080cu, 0xc3efu}, {0x080eu, 0xffc0u},
    {0x0810u, 0x7fdfu}, {0x0820u, 0xffffu}, {0x0822u, 0xffffu}, {0x0824u, 0xffffu},
    {0x0826u, 0x7fffu}, {0x0828u, 0x0afeu}, {0x082au, 0xdfeeu}, {0x082cu, 0xffefu},
    {0x082eu, 0xffc0u}, {0x0830u, 0x7fdfu}, {0x0840u, 0x7777u}, {0x0842u, 0x7777u},
    {0x0844u, 0x7777u}, {0x0846u, 0x7777u}, {0x0848u, 0x7777u}, {0x084au, 0x7777u},
    {0x084cu, 0x7777u}, {0x084eu, 0x7777u}, {0x0850u, 0x7777u}, {0x0852u, 0x7777u},
    {0x0854u, 0x7777u}, {0x0856u, 0x7777u}, {0x0858u, 0x7777u}, {0x085au, 0x7777u},
    {0x085cu, 0x7777u}, {0x085eu, 0x0777u}, {0x0860u, 0x7770u}, {0x0862u, 0x7777u},
    {0x0864u, 0x7070u}, {0x0868u, 0x7770u}, {0x086au, 0x7700u}, {0x086cu, 0x7777u},
    {0x086eu, 0x7777u}, {0x0870u, 0x7777u}, {0x087au, 0x7700u}, {0x087cu, 0x7777u},
    {0x087eu, 0x7777u}, {0x0880u, 0x7777u}, {0x0882u, 0x7707u}, {0x0884u, 0x7777u},
    {0x0886u, 0x0777u}, {0x0e00u, 0xc6ffu}, {0x0e04u, 0xc6ffu}, {0x0e06u, 0xc03fu},
    {0x0e08u, 0xc6ffu}, {0x0e0au, 0xc6ffu}, {0x0e0cu, 0xc6ffu}, {0x0e0eu, 0x06c0u},
    {0x0e10u, 0xffffu}, {0x0e14u, 0xffffu}, {0x0e18u, 0xffffu}, {0x0e1au, 0xffffu},
    {0x0e1cu, 0xffffu}, {0x0e1eu, 0xffffu}, {0x0e20u, 0xf01eu}, {0x0e24u, 0xf01eu},
    {0x0e28u, 0xf01eu}, {0x0e2au, 0xf01eu}, {0x0e2cu, 0xf01eu}, {0x0e2eu, 0x601eu},
    {0x0e30u, 0xffffu}, {0x0e34u, 0xffffu}, {0x0e36u, 0xff3fu}, {0x0e38u, 0xffffu},
    {0x0e3au, 0xffffu}, {0x0e3cu, 0xffffu}, {0x0e3eu, 0x00c0u}, {0x0e40u, 0x03ffu},
    {0x0e44u, 0x03ffu}, {0x0e48u, 0x03ffu}, {0x0e4au, 0x03ffu}, {0x0e4cu, 0x03ffu},
    {0x0e4eu, 0x03ffu}, {0x0e50u, 0x313fu}, {0x0e54u, 0x313fu}, {0x0e56u, 0x313fu},
    {0x0e58u, 0x313fu}, {0x0e5au, 0x313fu}, {0x0e5cu, 0x313fu}, {0x0e60u, 0xf3c3u},
    {0x0e64u, 0xf3c3u}, {0x0e66u, 0xf003u}, {0x0e68u, 0xf3cfu}, {0x0e6au, 0xf3c3u},
    {0x0e6cu, 0xf3c3u}, {0x0e6eu, 0x03c0u}, {0x0efeu, 0x0003u}, {0x0f82u, 0x00ffu},
    {0x0f8cu, 0x00ffu}, {0x0fa4u, 0x001fu}};

static const Dspic33ResetValue reset_values[] = {
    {0x004au, 0x0001u}, {0x004eu, 0x0001u}, {0x0102u, 0xffffu}, {0x010cu, 0xffffu},
    {0x010eu, 0xffffu}, {0x011au, 0xffffu}, {0x011cu, 0xffffu}, {0x0128u, 0xffffu},
    {0x012au, 0xffffu}, {0x0136u, 0xffffu}, {0x0138u, 0xffffu}, {0x0142u, 0x000du},
    {0x014au, 0x000du}, {0x0152u, 0x000du}, {0x015au, 0x000du}, {0x0162u, 0x000du},
    {0x016au, 0x000du}, {0x0172u, 0x000du}, {0x017au, 0x000du}, {0x0182u, 0x000du},
    {0x018au, 0x000du}, {0x0192u, 0x000du}, {0x019au, 0x000du}, {0x01a2u, 0x000du},
    {0x01aau, 0x000du}, {0x01b2u, 0x000du}, {0x01bau, 0x000du}, {0x0202u, 0x00ffu},
    {0x0206u, 0x1000u}, {0x0212u, 0x00ffu}, {0x0216u, 0x1000u}, {0x0222u, 0x0110u},
    {0x0232u, 0x0110u}, {0x0252u, 0x0110u}, {0x02b2u, 0x0110u}, {0x0400u, 0x0480u},
    {0x0404u, 0x0040u}, {0x0414u, 0x003fu}, {0x0500u, 0x0480u}, {0x0504u, 0x0040u},
    {0x0514u, 0x003fu}, {0x060eu, 0x008fu}, {0x0640u, 0x0040u}, {0x0740u, 0x0003u},
    {0x0744u, 0x3040u}, {0x0746u, 0x0030u}, {0x0840u, 0x4444u}, {0x0842u, 0x4444u},
    {0x0844u, 0x4444u}, {0x0846u, 0x0444u}, {0x0848u, 0x4444u}, {0x084au, 0x0004u},
    {0x084cu, 0x4444u}, {0x084eu, 0x4444u}, {0x0850u, 0x4444u}, {0x0852u, 0x0444u},
    {0x0858u, 0x4444u}, {0x085au, 0x4444u}, {0x085cu, 0x4004u}, {0x085eu, 0x0044u},
    {0x0860u, 0x0440u}, {0x0862u, 0x4444u}, {0x0864u, 0x4040u}, {0x0868u, 0x4440u},
    {0x086eu, 0x4400u}, {0x0870u, 0x4444u}, {0x0902u, 0x000cu}, {0x090cu, 0x000cu},
    {0x0916u, 0x000cu}, {0x0920u, 0x000cu}, {0x092au, 0x000cu}, {0x0934u, 0x000cu},
    {0x093eu, 0x000cu}, {0x0948u, 0x000cu}, {0x0952u, 0x000cu}, {0x095cu, 0x000cu},
    {0x0966u, 0x000cu}, {0x0970u, 0x000cu}, {0x097au, 0x000cu}, {0x0984u, 0x000cu},
    {0x098eu, 0x000cu}, {0x0998u, 0x000cu}, {0x0c04u, 0xffffu}, {0x0c12u, 0xffffu},
    {0x0e00u, 0xc6ffu}, {0x0e0eu, 0x06c0u}, {0x0e10u, 0xffffu}, {0x0e1eu, 0xffffu},
    {0x0e20u, 0xf01eu}, {0x0e2eu, 0x601eu}, {0x0e30u, 0xffffu}, {0x0e3eu, 0x00c0u},
    {0x0e40u, 0x03ffu}, {0x0e4eu, 0x03ffu}, {0x0e50u, 0x313fu}, {0x0e60u, 0xf3c3u},
    {0x0e6eu, 0x03c0u}};

static uint16_t raw_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] |
                      ((uint16_t)cpu->data[(uint16_t)(address + 1u)] << 8u));
}

static void raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[(uint16_t)(address + 1u)] = (uint8_t)(value >> 8u);
}

static bool register_write_mask(uint16_t address, uint16_t* writable) {
    size_t first = 0u;
    size_t count = sizeof(register_masks) / sizeof(register_masks[0]);

    while (count != 0u) {
        size_t step = count / 2u;
        size_t index = first + step;

        if (register_masks[index].address < address) {
            first = index + 1u;
            count -= step + 1u;
        } else {
            count = step;
        }
    }
    if (first == sizeof(register_masks) / sizeof(register_masks[0]) ||
        register_masks[first].address != address) {
        return false;
    }
    *writable = register_masks[first].writable;
    return true;
}

static bool pps_register_write_mask(uint16_t address, uint16_t* writable) {
    return address >= 0x0680u && address <= 0x06f6u &&
           register_write_mask(address, writable);
}

static void pps_capture_shadow(Dspic33* cpu) {
    uint8_t index;
    for (index = 0u; index < DSPIC33_PPS_REGISTER_COUNT; index++) {
        uint16_t address = (uint16_t)(0x0680u + index * 2u);
        uint16_t writable;
        cpu->io.pps.shadow[index] = pps_register_write_mask(address, &writable)
                                        ? (uint16_t)(raw_word(cpu, address) & writable)
                                        : 0u;
    }
}

static void pps_update_shadow(Dspic33* cpu, uint16_t address) {
    uint16_t writable;
    if (pps_register_write_mask(address, &writable)) {
        cpu->io.pps.shadow[(address - 0x0680u) / 2u] =
            (uint16_t)(raw_word(cpu, address) & writable);
    }
}

static bool pps_shadow_matches(const Dspic33* cpu) {
    uint8_t index;
    for (index = 0u; index < DSPIC33_PPS_REGISTER_COUNT; index++) {
        uint16_t address = (uint16_t)(0x0680u + index * 2u);
        uint16_t writable;
        if (pps_register_write_mask(address, &writable) &&
            (raw_word(cpu, address) & writable) != cpu->io.pps.shadow[index]) {
            return false;
        }
    }
    return true;
}

static bool input_capture_register_write_mask(uint16_t address, uint16_t* writable) {
    uint16_t offset;
    if (address < INPUT_CAPTURE_BASE ||
        address >=
            INPUT_CAPTURE_BASE + DSPIC33_INPUT_CAPTURE_COUNT * INPUT_CAPTURE_STRIDE) {
        return false;
    }
    offset = (uint16_t)((address - INPUT_CAPTURE_BASE) % INPUT_CAPTURE_STRIDE);
    if (offset == 0u) {
        *writable = INPUT_CAPTURE_CON1_WRITABLE;
        return true;
    }
    if (offset == 2u) {
        *writable = INPUT_CAPTURE_CON2_WRITABLE;
        return true;
    }
    if (offset == 4u || offset == 6u) {
        *writable = 0u;
        return true;
    }
    return false;
}

static bool output_compare_register_write_mask(uint16_t address, uint16_t* writable) {
    uint16_t offset;
    if (address < OUTPUT_COMPARE_BASE ||
        address >= OUTPUT_COMPARE_BASE +
                       DSPIC33_OUTPUT_COMPARE_COUNT * OUTPUT_COMPARE_STRIDE) {
        return false;
    }
    offset = (uint16_t)((address - OUTPUT_COMPARE_BASE) % OUTPUT_COMPARE_STRIDE);
    if (offset == 0u) {
        *writable = OUTPUT_COMPARE_CON1_WRITABLE;
        return true;
    }
    if (offset == 2u) {
        *writable = OUTPUT_COMPARE_CON2_WRITABLE;
        return true;
    }
    if (offset == 4u || offset == 6u) {
        *writable = UINT16_MAX;
        return true;
    }
    if (offset == 8u) {
        *writable = 0u;
        return true;
    }
    return false;
}

static bool comparator_register_write_mask(uint16_t address, uint16_t* writable) {
    uint16_t offset;
    if (address == COMPARATOR_STATUS) {
        *writable = COMPARATOR_STOP_IDLE;
        return true;
    }
    if (address == COMPARATOR_REFERENCE) {
        *writable = 0x07ffu;
        return true;
    }
    if (address < COMPARATOR_BASE ||
        address >= COMPARATOR_BASE + DSPIC33_COMPARATOR_COUNT * COMPARATOR_STRIDE) {
        return false;
    }
    offset = (uint16_t)((address - COMPARATOR_BASE) % COMPARATOR_STRIDE);
    if (offset == 0u) {
        *writable = COMPARATOR_CONTROL_WRITABLE;
        return true;
    }
    if (offset == 2u) {
        *writable = 0x0fffu;
        return true;
    }
    if (offset == 4u) {
        *writable = 0xbfffu;
        return true;
    }
    if (offset == 6u) {
        *writable = 0x007fu;
        return true;
    }
    return false;
}

static void update_gpio_latch(Dspic33* cpu, uint16_t address, uint16_t requested) {
    uint16_t port_address = (uint16_t)(address & 0xfffeu);
    uint8_t port;

    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        uint16_t latch_address;
        uint16_t latch;
        uint16_t writable;
        bool word_write;

        if (port_address != gpio_port_addresses[port]) {
            continue;
        }
        latch_address = gpio_latch_addresses[port];
        word_write = cpu->io.dma_transfer_active
                         ? cpu->io.dma_transfer_width == 2u
                         : cpu->io.cpu_write_valid && cpu->io.cpu_write_width == 2u &&
                               cpu->io.cpu_write_address == port_address;
        if (word_write) {
            latch = requested;
        } else if ((address & 1u) == 0u) {
            latch = requested & 0x00ffu;
        } else {
            latch = requested & 0xff00u;
        }
        if (register_write_mask(latch_address, &writable)) {
            latch = (uint16_t)((raw_word(cpu, latch_address) & ~writable) |
                               (latch & writable));
        }
        raw_write_word(cpu, latch_address, latch);
        return;
    }
}

static bool adc_register_write_mask(uint16_t address, uint16_t* writable) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        uint16_t control = adc_controls[module];
        uint16_t buffer = adc_buffers[module];
        if (address >= buffer && address < buffer + 0x20u) {
            *writable = 0u;
            return true;
        }
        if (address == control) {
            *writable = module == 0u ? 0xb7feu : 0xb3feu;
            return true;
        }
        if (address == control + 2u) {
            *writable = module == 0u ? 0xe77fu : 0xe73fu;
            return true;
        }
        if (address == control + 4u) {
            *writable = 0x9fffu;
            return true;
        }
        if (address == control + 6u) {
            *writable = 0x0707u;
            return true;
        }
        if (address == control + 8u) {
            *writable = 0x9f9fu;
            return true;
        }
        if ((module == 0u && (address == 0x032eu || address == 0x0330u)) ||
            (module == 1u && address == 0x0370u)) {
            *writable = 0xffffu;
            return true;
        }
        if ((module == 0u && address == 0x0332u) ||
            (module == 1u && address == 0x0372u)) {
            *writable = 0x0107u;
            return true;
        }
    }
    return false;
}

static bool uart_module_disabled(const Dspic33* cpu, uint8_t channel) {
    static const uint16_t pmd_addresses[DSPIC33_UART_COUNT] = {0x0760u, 0x0760u,
                                                               0x0764u, 0x0766u};
    static const uint16_t pmd_masks[DSPIC33_UART_COUNT] = {0x0020u, 0x0040u, 0x0008u,
                                                           0x0020u};
    return channel >= DSPIC33_UART_COUNT ||
           (raw_word(cpu, pmd_addresses[channel]) & pmd_masks[channel]) != 0u;
}

static bool uart_register_write_mask(const Dspic33* cpu, uint16_t address,
                                     uint16_t* writable) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t offset = (uint16_t)(address - uart_bases[channel]);
        if (offset > 8u || (offset & 1u) != 0u) {
            continue;
        }
        if (uart_module_disabled(cpu, channel)) {
            *writable = 0u;
        } else if (offset == 0u) {
            *writable = 0xbbffu;
        } else if (offset == 2u) {
            *writable = 0xece2u;
        } else if (offset == 4u) {
            *writable = 0x01ffu;
        } else if (offset == 6u) {
            *writable = 0u;
        } else {
            *writable = 0xffffu;
        }
        return true;
    }
    return false;
}

static bool spi_register_write_mask(uint16_t address, uint16_t* writable) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t offset = (uint16_t)(address - spi_bases[channel]);
        if (offset == 0u) {
            *writable = 0xa05cu;
            return true;
        }
        if (offset == 2u) {
            *writable = 0x1fffu;
            return true;
        }
        if (offset == 4u) {
            *writable = 0xe003u;
            return true;
        }
    }
    return false;
}

static bool can_register_write_mask(const Dspic33* cpu, uint16_t address,
                                    uint16_t* writable) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = can_bases[channel];
        uint16_t offset = (uint16_t)(address - base);
        bool window = (raw_word(cpu, base) & CAN_WINDOW) != 0u;
        if (offset > 0x7eu || (offset & 1u) != 0u) {
            continue;
        }
        if (offset == 0u) {
            *writable = 0x3f09u;
        } else if (offset == 2u) {
            *writable = 0u;
        } else if (offset == 4u || offset == 8u || offset == 0x0eu) {
            *writable = 0u;
        } else if (offset == 6u) {
            *writable = 0xe01fu;
        } else if (offset == 0x0au) {
            *writable = 0x00efu;
        } else if (offset == 0x0cu) {
            *writable = 0x00efu;
        } else if (offset == 0x10u) {
            *writable = 0x00ffu;
        } else if (offset == 0x12u) {
            *writable = 0x47ffu;
        } else if (offset == 0x14u || offset == 0x18u || offset == 0x1au) {
            *writable = 0xffffu;
        } else if (window && offset >= 0x20u && offset <= 0x7eu) {
            if (offset >= 0x40u && (offset & 2u) == 0u) {
                *writable = 0xffebu;
            } else if (offset == 0x30u || offset == 0x34u || offset == 0x38u) {
                *writable = 0xffebu;
            } else if (offset == 0x28u || offset == 0x2au || offset == 0x3cu ||
                       offset == 0x3eu) {
                *writable = 0u;
            } else {
                *writable = 0xffffu;
            }
        } else if (!window && (offset == 0x20u || offset == 0x22u || offset == 0x28u ||
                               offset == 0x2au)) {
            *writable = 0xffffu;
        } else if (!window && offset >= 0x30u && offset <= 0x36u) {
            *writable = 0x8f8fu;
        } else if (!window && offset == 0x40u) {
            *writable = 0xffffu;
        } else if (!window && offset == 0x42u) {
            *writable = 0xffffu;
        } else {
            *writable = 0u;
        }
        return true;
    }
    return false;
}

static bool usb_register_write_mask(const Dspic33* cpu, uint16_t address,
                                    uint16_t previous, uint16_t* writable) {
    bool host = ((address == USB_CON ? previous : raw_word(cpu, USB_CON)) &
                 USB_HOST_ENABLE) != 0u;
    if (address == USB_OTGIR || address == USB_OTGSTAT || address == USB_IR ||
        address == USB_EIR || address == USB_STAT || address == USB_FRML ||
        address == USB_FRMH) {
        *writable = 0u;
    } else if (address == USB_OTGIE) {
        *writable = 0x00fdu;
    } else if (address == USB_OTGCON) {
        *writable = 0x00ffu;
    } else if (address == USB_PWRC) {
        *writable = USB_POWER | USB_SUSPEND | USB_SLEEP_GUARD;
    } else if (address == USB_IE) {
        *writable = host ? 0x00ffu : 0x00bfu;
    } else if (address == USB_EIE) {
        *writable = 0x00ffu;
    } else if (address == USB_CON) {
        *writable = host ? 0x001fu : 0x002fu;
    } else if (address == USB_ADDR || address == USB_TOK || address == USB_SOF ||
               address == USB_BDTP2 || address == USB_BDTP3) {
        *writable = 0x00ffu;
    } else if (address == USB_BDTP1) {
        *writable = 0x00feu;
    } else if (address == USB_CNFG1) {
        *writable = 0x00d0u;
    } else if (address == USB_CNFG2) {
        *writable = 0x003fu;
    } else if (address >= USB_EP0 &&
               address < USB_EP0 + DSPIC33_USB_ENDPOINT_COUNT * 2u) {
        *writable = address == USB_EP0 ? 0x00dfu : 0x001fu;
    } else if (address == USB_PWMRRS) {
        *writable = 0xffffu;
    } else if (address == USB_PWMCON) {
        *writable = 0x8300u;
    } else {
        return false;
    }
    return true;
}

static bool pwm_register_write_mask(uint16_t address, uint16_t* writable) {
    static const uint16_t global_masks[16] = {
        0xafffu, 0x0007u, 0xffffu, 0xffffu, 0x0000u, 0xffffu, 0x0000u, 0x0fffu,
        0x0007u, 0xffffu, 0xffffu, 0x0000u, 0x0000u, 0x83ffu, 0x0000u, 0x0000u};
    static const uint16_t generator_masks[16] = {
        0x1fefu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0x3fffu, 0x3fffu, 0xffffu,
        0xffffu, 0xffffu, 0xf03fu, 0x0000u, 0x0000u, 0xfc3fu, 0x0fffu, 0x0f3fu};
    if (address >= PWM_GLOBAL_BASE && address < PWM_GENERATOR_BASE) {
        *writable = global_masks[(address - PWM_GLOBAL_BASE) / 2u];
        return true;
    }
    if (address >= PWM_GENERATOR_BASE &&
        address < PWM_GENERATOR_BASE + DSPIC33_PWM_COUNT * PWM_GENERATOR_STRIDE) {
        *writable = generator_masks[((address - PWM_GENERATOR_BASE) & 0x001fu) / 2u];
        return true;
    }
    return false;
}

static bool byte_queue_push(Dspic33ByteQueue* queue, uint8_t value) {
    uint16_t index;
    if (queue->count == sizeof(queue->bytes)) {
        return false;
    }
    index = (uint16_t)((queue->head + queue->count) % sizeof(queue->bytes));
    queue->bytes[index] = value;
    queue->count++;
    return true;
}

static bool uart_fifo_push(Dspic33UartFifo* fifo, const Dspic33UartFrame* frame) {
    uint8_t index;
    if (fifo->count == DSPIC33_UART_FIFO_SIZE) {
        return false;
    }
    index = (uint8_t)((fifo->head + fifo->count) % DSPIC33_UART_FIFO_SIZE);
    fifo->frames[index] = *frame;
    fifo->count++;
    return true;
}

static bool uart_fifo_front(const Dspic33UartFifo* fifo, Dspic33UartFrame* frame) {
    if (fifo->count == 0u) {
        return false;
    }
    *frame = fifo->frames[fifo->head];
    return true;
}

static bool uart_fifo_pop(Dspic33UartFifo* fifo, Dspic33UartFrame* frame) {
    if (!uart_fifo_front(fifo, frame)) {
        return false;
    }
    fifo->head = (uint8_t)((fifo->head + 1u) % DSPIC33_UART_FIFO_SIZE);
    fifo->count--;
    return true;
}

static bool uart_queue_push(Dspic33UartQueue* queue, const Dspic33UartFrame* frame) {
    uint16_t index;
    if (queue->count == DSPIC33_UART_QUEUE_SIZE) {
        return false;
    }
    index = (uint16_t)((queue->head + queue->count) % DSPIC33_UART_QUEUE_SIZE);
    queue->frames[index] = *frame;
    queue->count++;
    return true;
}

static bool uart_queue_pop(Dspic33UartQueue* queue, Dspic33UartFrame* frame) {
    if (queue->count == 0u) {
        return false;
    }
    *frame = queue->frames[queue->head];
    queue->head = (uint16_t)((queue->head + 1u) % DSPIC33_UART_QUEUE_SIZE);
    queue->count--;
    return true;
}

static bool word_queue_push(Dspic33WordQueue* queue, uint16_t value) {
    uint8_t index;
    if (queue->count == sizeof(queue->words) / sizeof(queue->words[0])) {
        return false;
    }
    index = (uint8_t)((queue->head + queue->count) %
                      (sizeof(queue->words) / sizeof(queue->words[0])));
    queue->words[index] = value;
    queue->count++;
    return true;
}

static bool word_queue_pop(Dspic33WordQueue* queue, uint16_t* value) {
    if (queue->count == 0u) {
        return false;
    }
    *value = queue->words[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) %
                            (sizeof(queue->words) / sizeof(queue->words[0])));
    queue->count--;
    return true;
}

static bool word_queue_front(const Dspic33WordQueue* queue, uint16_t* value) {
    if (queue->count == 0u) {
        return false;
    }
    *value = queue->words[queue->head];
    return true;
}

static uint8_t crc_data_width(const Dspic33* cpu) {
    return (uint8_t)(((raw_word(cpu, CRC_CONFIG) >> 8u) & 0x001fu) + 1u);
}

static uint8_t crc_polynomial_width(const Dspic33* cpu) {
    return (uint8_t)((raw_word(cpu, CRC_CONFIG) & 0x001fu) + 1u);
}

static uint8_t crc_capacity(const Dspic33* cpu) {
    uint8_t width = crc_data_width(cpu);
    return width <= 8u ? 16u : width <= 16u ? 8u : 4u;
}

static uint32_t crc_width_mask(uint8_t width) {
    return width == 32u ? UINT32_MAX : ((uint32_t)1u << width) - 1u;
}

static void crc_refresh_status(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, CRC_CONTROL);
    uint16_t status = (uint16_t)((uint16_t)cpu->io.crc.count << 8u);
    if (cpu->io.crc.count == 0u) {
        status |= CRC_EMPTY;
    }
    if (cpu->io.crc.count >= crc_capacity(cpu)) {
        status |= CRC_FULL;
    }
    raw_write_word(
        cpu, CRC_CONTROL,
        (uint16_t)((control & ~(CRC_WORD_COUNT_MASK | CRC_FULL | CRC_EMPTY)) | status));
}

static void crc_abort(Dspic33* cpu) {
    cpu->io.crc.generation++;
    cpu->io.crc.active = false;
    cpu->io.crc.bits_remaining = 0u;
}

static void crc_reset_runtime(Dspic33* cpu) {
    uint16_t generation = (uint16_t)(cpu->io.crc.generation + 1u);
    uint16_t pmd_generation = cpu->io.crc.pmd_generation;
    uint16_t control = raw_word(cpu, CRC_CONTROL);
    bool pmd_disabled = cpu->io.crc.pmd_disabled;
    memset(&cpu->io.crc, 0, sizeof(cpu->io.crc));
    cpu->io.crc.generation = generation;
    cpu->io.crc.pmd_generation = pmd_generation;
    cpu->io.crc.pmd_disabled = pmd_disabled;
    raw_write_word(cpu, CRC_DATA_LOW, 0u);
    raw_write_word(cpu, CRC_DATA_HIGH, 0u);
    raw_write_word(cpu, CRC_SHIFT_LOW, 0u);
    raw_write_word(cpu, CRC_SHIFT_HIGH, 0u);
    raw_write_word(cpu, CRC_CONTROL, (uint16_t)(control & ~CRC_GO));
    crc_refresh_status(cpu);
}

static bool crc_schedule(Dspic33* cpu) {
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CRC, 0u, cpu->io.crc.generation, 1u)) {
        crc_abort(cpu);
        raw_write_word(cpu, CRC_CONTROL,
                       (uint16_t)(raw_word(cpu, CRC_CONTROL) & ~CRC_GO));
        return false;
    }
    cpu->io.crc.active = true;
    return true;
}

static void crc_start_if_ready(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, CRC_CONTROL);
    if ((control & (CRC_ENABLE | CRC_GO)) == (CRC_ENABLE | CRC_GO) &&
        !cpu->io.crc.active && cpu->io.crc.count != 0u) {
        crc_schedule(cpu);
    }
}

static void crc_push(Dspic33* cpu, uint32_t value) {
    uint8_t capacity = crc_capacity(cpu);
    uint8_t index;
    if (cpu->io.crc.count >= capacity) {
        return;
    }
    index = (uint8_t)((cpu->io.crc.head + cpu->io.crc.count) % 16u);
    cpu->io.crc.words[index] = value & crc_width_mask(crc_data_width(cpu));
    cpu->io.crc.count++;
    crc_refresh_status(cpu);
    crc_start_if_ready(cpu);
}

static uint32_t crc_shift_register(const Dspic33* cpu) {
    return (uint32_t)raw_word(cpu, CRC_SHIFT_LOW) |
           ((uint32_t)raw_word(cpu, CRC_SHIFT_HIGH) << 16u);
}

static void crc_write_shift_register(Dspic33* cpu, uint32_t value) {
    raw_write_word(cpu, CRC_SHIFT_LOW, (uint16_t)value);
    raw_write_word(cpu, CRC_SHIFT_HIGH, (uint16_t)(value >> 16u));
}

static void crc_load_shift_data(Dspic33* cpu) {
    uint32_t polynomial = (uint32_t)raw_word(cpu, CRC_POLYNOMIAL_LOW) |
                          ((uint32_t)raw_word(cpu, CRC_POLYNOMIAL_HIGH) << 16u);
    cpu->io.crc.shift_data = cpu->io.crc.words[cpu->io.crc.head];
    cpu->io.crc.head = (uint8_t)((cpu->io.crc.head + 1u) % 16u);
    cpu->io.crc.count--;
    cpu->io.crc.data_width = crc_data_width(cpu);
    cpu->io.crc.polynomial_width = crc_polynomial_width(cpu);
    cpu->io.crc.bits_remaining = cpu->io.crc.data_width;
    cpu->io.crc.polynomial =
        (polynomial | 1u) & crc_width_mask(cpu->io.crc.polynomial_width);
    cpu->io.crc.little_endian = (raw_word(cpu, CRC_CONTROL) & CRC_LITTLE_ENDIAN) != 0u;
    crc_refresh_status(cpu);
    if (cpu->io.crc.count == 0u &&
        (raw_word(cpu, CRC_CONTROL) & CRC_INTERRUPT_EMPTY) != 0u) {
        dspic33_raise_interrupt(cpu, CRC_IRQ);
    }
}

static void crc_shift_bits(Dspic33* cpu) {
    uint32_t remainder =
        crc_shift_register(cpu) & crc_width_mask(cpu->io.crc.polynomial_width);
    uint8_t shifted;
    for (shifted = 0u; shifted < CRC_BITS_PER_CYCLE && cpu->io.crc.bits_remaining != 0u;
         shifted++) {
        uint8_t source_bit =
            cpu->io.crc.little_endian
                ? (uint8_t)(cpu->io.crc.data_width - cpu->io.crc.bits_remaining)
                : (uint8_t)(cpu->io.crc.bits_remaining - 1u);
        bool feedback = ((remainder >> (cpu->io.crc.polynomial_width - 1u)) & 1u) != 0u;
        feedback = feedback != ((cpu->io.crc.shift_data >> source_bit) & 1u);
        remainder = (remainder << 1u) & crc_width_mask(cpu->io.crc.polynomial_width);
        if (feedback) {
            remainder ^= cpu->io.crc.polynomial;
        }
        cpu->io.crc.bits_remaining--;
    }
    crc_write_shift_register(cpu, remainder);
}

static void run_crc(Dspic33* cpu, uint16_t generation) {
    uint16_t control = raw_word(cpu, CRC_CONTROL);
    if (generation != cpu->io.crc.generation || !cpu->io.crc.active ||
        (control & (CRC_ENABLE | CRC_GO)) != (CRC_ENABLE | CRC_GO)) {
        return;
    }
    if (cpu->io.crc.pmd_disabled || cpu->power_state == DSPIC33_POWER_SLEEP ||
        (cpu->power_state == DSPIC33_POWER_IDLE && (control & CRC_STOP_IDLE) != 0u)) {
        crc_schedule(cpu);
        return;
    }
    if (cpu->io.crc.bits_remaining == 0u) {
        if (cpu->io.crc.count == 0u) {
            cpu->io.crc.active = false;
            raw_write_word(cpu, CRC_CONTROL, (uint16_t)(control & ~CRC_GO));
            if ((control & CRC_INTERRUPT_EMPTY) == 0u) {
                dspic33_raise_interrupt(cpu, CRC_IRQ);
            }
            return;
        }
        crc_load_shift_data(cpu);
    }
    crc_shift_bits(cpu);
    if (cpu->io.crc.bits_remaining == 0u && cpu->io.crc.count == 0u) {
        cpu->io.crc.active = false;
        control = raw_word(cpu, CRC_CONTROL);
        raw_write_word(cpu, CRC_CONTROL, (uint16_t)(control & ~CRC_GO));
        if ((control & CRC_INTERRUPT_EMPTY) == 0u) {
            dspic33_raise_interrupt(cpu, CRC_IRQ);
        }
        return;
    }
    crc_schedule(cpu);
}

static void run_crc_pmd(Dspic33* cpu, uint32_t value) {
    uint16_t generation = (uint16_t)(value >> 1u);
    if (generation != cpu->io.crc.pmd_generation) {
        return;
    }
    cpu->io.crc.pmd_disabled = (value & 1u) != 0u;
    if (!cpu->io.crc.pmd_disabled) {
        crc_start_if_ready(cpu);
    }
}

static void update_crc_pmd(Dspic33* cpu, uint16_t previous) {
    bool disabled = (raw_word(cpu, CRC_PMD_ADDRESS) & CRC_PMD) != 0u;
    if (((previous & CRC_PMD) != 0u) == disabled) {
        return;
    }
    cpu->io.crc.pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CRC, CRC_EVENT_PMD_SOURCE,
                          ((uint32_t)cpu->io.crc.pmd_generation << 1u) |
                              (disabled ? 1u : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        raw_write_word(cpu, CRC_PMD_ADDRESS, previous);
        cpu->io.crc.pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static uint8_t pmp_transfer_width(uint16_t mode) {
    return (mode & PMP_DATA_16_BIT) != 0u ? 2u : 1u;
}

static uint64_t pmp_transfer_cycles(uint16_t control, uint16_t mode) {
    uint8_t address_phases = (uint8_t)((control & PMP_ADDRESS_MUX_MASK) >> 11u);
    uint8_t data_phases = pmp_transfer_width(mode);
    uint8_t beginning = (uint8_t)((mode & PMP_WAIT_BEGIN_MASK) >> 6u);
    uint8_t middle = (uint8_t)((mode & PMP_WAIT_MIDDLE_MASK) >> 2u);
    if (middle == 0u) {
        return (uint64_t)address_phases + data_phases;
    }
    return (uint64_t)address_phases * (beginning + 1u) +
           (uint64_t)data_phases *
               (beginning + 1u + middle + (mode & PMP_WAIT_END_MASK) + 1u);
}

static bool pmp_master_enabled(const Dspic33* cpu) {
    uint16_t control = raw_word(cpu, PMP_CONTROL);
    uint16_t mode = raw_word(cpu, PMP_MODE);
    uint16_t master = mode & PMP_MASTER_MODE_MASK;
    return !cpu->io.pmp.pmd_disabled && cpu->power_state != DSPIC33_POWER_SLEEP &&
           (cpu->power_state != DSPIC33_POWER_IDLE ||
            (control & PMP_STOP_IDLE) == 0u) &&
           (control & PMP_ENABLE) != 0u &&
           (master == PMP_MASTER_MODE_2 || master == PMP_MASTER_MODE_1) &&
           (control & PMP_ADDRESS_MUX_MASK) != PMP_ADDRESS_MUX_MASK &&
           (control & PMP_CHIP_SELECT_FUNCTION_MASK) != PMP_CHIP_SELECT_FUNCTION_MASK;
}

static bool pmp_slave_configured(const Dspic33* cpu) {
    uint16_t control = raw_word(cpu, PMP_CONTROL);
    uint16_t mode = raw_word(cpu, PMP_MODE);
    uint16_t slave = mode & PMP_MASTER_MODE_MASK;
    uint16_t increment = mode & PMP_INCREMENT_MODE_MASK;
    return !cpu->io.pmp.pmd_disabled && (control & PMP_ENABLE) != 0u &&
           ((slave == 0u &&
             (increment == 0u || increment == PMP_INCREMENT_MODE_MASK)) ||
            (slave == PMP_SLAVE_ADDRESSABLE && increment == 0u));
}

static bool pmp_slave_enabled(const Dspic33* cpu, bool reading) {
    uint16_t mode = raw_word(cpu, PMP_MODE);
    uint16_t slave = mode & PMP_MASTER_MODE_MASK;
    uint16_t control = raw_word(cpu, PMP_CONTROL);
    uint16_t enabled_strobe =
        reading ? PMP_READ_STROBE_ENABLE : PMP_WRITE_STROBE_ENABLE;
    if (!pmp_slave_configured(cpu) || (control & enabled_strobe) == 0u ||
        (raw_word(cpu, PMP_ADDRESS_ENABLE_REGISTER) & PMP_CHIP_SELECT_ENABLE) == 0u ||
        (slave != 0u && slave != PMP_SLAVE_ADDRESSABLE)) {
        return false;
    }
    return slave != PMP_SLAVE_ADDRESSABLE ||
           (raw_word(cpu, PMP_ADDRESS_ENABLE_REGISTER) & PMP_ADDRESS_ENABLE) ==
               PMP_ADDRESS_ENABLE;
}

static bool pmp_master_clock_available(const Dspic33* cpu) {
    uint16_t control = raw_word(cpu, PMP_CONTROL);
    return !cpu->io.pmp.pmd_disabled && cpu->power_state != DSPIC33_POWER_SLEEP &&
           (cpu->power_state != DSPIC33_POWER_IDLE || (control & PMP_STOP_IDLE) == 0u);
}

static bool pmp_master_event(const Dspic33Event* event) {
    return event->type == DSPIC33_EVENT_PMP && event->source <= PMP_EVENT_CLEAR_BUSY;
}

static void pmp_pause_master_events(Dspic33* cpu) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (!pmp_master_event(event) || event->paused) {
            continue;
        }
        event->paused_remaining = event->cycle - cpu->device_cycles;
        event->paused = true;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void pmp_resume_master_events(Dspic33* cpu) {
    size_t index;
    bool changed = false;
    if (!pmp_master_clock_available(cpu)) {
        return;
    }
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (!pmp_master_event(event) || !event->paused) {
            continue;
        }
        if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            continue;
        }
        event->cycle = cpu->device_cycles + event->paused_remaining;
        event->paused_remaining = 0u;
        event->paused = false;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void pmp_discard_master_events(Dspic33* cpu) {
    size_t source;
    size_t destination = 0u;
    for (source = 0u; source < cpu->events.count; source++) {
        if (!pmp_master_event(&cpu->events.items[source])) {
            cpu->events.items[destination++] = cpu->events.items[source];
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
}

void dspic33_device_power_state_changed(Dspic33* cpu) {
    if (pmp_master_clock_available(cpu)) {
        pmp_resume_master_events(cpu);
    } else {
        pmp_pause_master_events(cpu);
    }
    output_compare_update_power_state(cpu);
    dci_update_power_state(cpu);
    comparator_update_filter_power(cpu);
    comparator_evaluate_all(cpu);
    dspic33_i2c_refresh_pins(cpu);
}

static void pmp_update_address(Dspic33* cpu, uint16_t control, uint16_t mode) {
    uint16_t increment = mode & PMP_INCREMENT_MODE_MASK;
    uint16_t counter_mask;
    uint16_t address;
    if (increment != PMP_INCREMENT_ADDRESS && increment != PMP_DECREMENT_ADDRESS) {
        return;
    }
    switch (control & PMP_CHIP_SELECT_FUNCTION_MASK) {
    case 0x0000u:
        counter_mask = 0xffffu;
        break;
    case 0x0040u:
        counter_mask = 0x7fffu;
        break;
    case 0x0080u:
        counter_mask = 0x3fffu;
        break;
    default:
        return;
    }
    address = raw_word(cpu, PMP_ADDRESS);
    raw_write_word(
        cpu, PMP_ADDRESS,
        (uint16_t)((address & ~counter_mask) |
                   ((increment == PMP_INCREMENT_ADDRESS ? address + 1u : address - 1u) &
                    counter_mask)));
}

static void pmp_abort(Dspic33* cpu) {
    cpu->io.pmp.generation++;
    pmp_discard_master_events(cpu);
    cpu->io.pmp.active = false;
    cpu->io.pmp.completing_active = false;
    cpu->io.pmp.reading = false;
    cpu->io.pmp.completing_reading = false;
    raw_write_word(cpu, PMP_MODE, (uint16_t)(raw_word(cpu, PMP_MODE) & ~PMP_BUSY));
}

static bool pmp_output_push(Dspic33PmpQueue* queue,
                            const Dspic33PmpTransfer* transfer) {
    uint16_t index;
    if (queue->count == DSPIC33_PMP_QUEUE_SIZE) {
        return false;
    }
    index = (uint16_t)((queue->head + queue->count) % DSPIC33_PMP_QUEUE_SIZE);
    queue->transfers[index] = *transfer;
    queue->count++;
    return true;
}

static bool pmp_output_pop(Dspic33PmpQueue* queue, Dspic33PmpTransfer* transfer) {
    if (queue->count == 0u) {
        return false;
    }
    *transfer = queue->transfers[queue->head];
    queue->head = (uint16_t)((queue->head + 1u) % DSPIC33_PMP_QUEUE_SIZE);
    queue->count--;
    return true;
}

static bool pmp_response_push(Dspic33PmpResponseQueue* queue,
                              const Dspic33PmpResponse* response) {
    uint16_t index;
    uint16_t logical;
    if (queue->count == DSPIC33_PMP_QUEUE_SIZE) {
        return false;
    }
    index = (uint16_t)((queue->head + queue->count) % DSPIC33_PMP_QUEUE_SIZE);
    queue->responses[index] = *response;
    queue->count++;
    logical = (uint16_t)(queue->count - 1u);
    while (logical != 0u) {
        uint16_t current = (uint16_t)((queue->head + logical) % DSPIC33_PMP_QUEUE_SIZE);
        uint16_t prior =
            (uint16_t)((queue->head + logical - 1u) % DSPIC33_PMP_QUEUE_SIZE);
        Dspic33PmpResponse temporary;
        if (queue->responses[prior].cycle <= queue->responses[current].cycle) {
            break;
        }
        temporary = queue->responses[prior];
        queue->responses[prior] = queue->responses[current];
        queue->responses[current] = temporary;
        logical--;
    }
    return true;
}

static bool pmp_response_pop(Dspic33PmpResponseQueue* queue, uint64_t cycle,
                             Dspic33PmpResponse* response) {
    if (queue->count == 0u || queue->responses[queue->head].cycle > cycle) {
        return false;
    }
    *response = queue->responses[queue->head];
    queue->head = (uint16_t)((queue->head + 1u) % DSPIC33_PMP_QUEUE_SIZE);
    queue->count--;
    return true;
}

static bool pmp_buffered_slave(const Dspic33* cpu) {
    uint16_t mode = raw_word(cpu, PMP_MODE);
    return (mode & PMP_MASTER_MODE_MASK) == PMP_SLAVE_ADDRESSABLE ||
           (mode & (PMP_MASTER_MODE_MASK | PMP_INCREMENT_MODE_MASK)) ==
               PMP_BUFFERED_SLAVE;
}

static void pmp_refresh_slave_status(Dspic33* cpu) {
    uint16_t status = raw_word(cpu, PMP_STATUS);
    if (!pmp_buffered_slave(cpu)) {
        return;
    }
    status &= (uint16_t)~(PMP_INPUT_FULL | PMP_OUTPUT_EMPTY);
    if ((status & PMP_INPUT_BUFFER_MASK) == PMP_INPUT_BUFFER_MASK) {
        status |= PMP_INPUT_FULL;
    }
    if ((status & PMP_OUTPUT_BUFFER_MASK) == PMP_OUTPUT_BUFFER_MASK) {
        status |= PMP_OUTPUT_EMPTY;
    }
    raw_write_word(cpu, PMP_STATUS, status);
}

static uint8_t pmp_slave_buffer(Dspic33* cpu, uint8_t address, bool reading) {
    uint16_t mode = raw_word(cpu, PMP_MODE);
    uint8_t index;
    if ((mode & PMP_MASTER_MODE_MASK) == PMP_SLAVE_ADDRESSABLE) {
        return (uint8_t)(address & 3u);
    }
    if (!pmp_buffered_slave(cpu)) {
        return 0u;
    }
    index = reading ? cpu->io.pmp.slave_read_index : cpu->io.pmp.slave_write_index;
    if (reading) {
        cpu->io.pmp.slave_read_index = (uint8_t)((index + 1u) & 3u);
    } else {
        cpu->io.pmp.slave_write_index = (uint8_t)((index + 1u) & 3u);
    }
    return index;
}

static void pmp_slave_interrupt(Dspic33* cpu, uint8_t index) {
    uint16_t mode = raw_word(cpu, PMP_MODE);
    uint16_t request = mode & PMP_INTERRUPT_MODE_MASK;
    if (!pmp_buffered_slave(cpu) || request == PMP_INTERRUPT_EACH ||
        (request == PMP_INTERRUPT_LAST && index == 3u)) {
        dspic33_raise_interrupt(cpu, PMP_IRQ);
    }
}

static void pmp_slave_write_event(Dspic33* cpu, uint8_t address, uint8_t value) {
    uint16_t status;
    uint16_t full;
    uint8_t index;
    if (!pmp_slave_enabled(cpu, false)) {
        return;
    }
    index = pmp_slave_buffer(cpu, address, false);
    status = raw_word(cpu, PMP_STATUS);
    if (pmp_buffered_slave(cpu)) {
        full = (uint16_t)(1u << (8u + index));
    } else {
        full = PMP_INPUT_FULL;
    }
    if ((status & full) != 0u) {
        status |= PMP_INPUT_OVERFLOW;
    } else {
        cpu->data[PMP_DATA + index] = value;
        status |= full;
    }
    raw_write_word(cpu, PMP_STATUS, status);
    pmp_refresh_slave_status(cpu);
    pmp_slave_interrupt(cpu, index);
}

static void pmp_slave_read_event(Dspic33* cpu, uint8_t address) {
    Dspic33PmpTransfer transfer;
    uint16_t status;
    uint16_t empty;
    uint8_t index;
    if (!pmp_slave_enabled(cpu, true)) {
        return;
    }
    index = pmp_slave_buffer(cpu, address, true);
    status = raw_word(cpu, PMP_STATUS);
    empty = pmp_buffered_slave(cpu) ? (uint16_t)(1u << index) : PMP_OUTPUT_EMPTY;
    if ((status & empty) != 0u) {
        status |= PMP_OUTPUT_UNDERFLOW;
    }
    status |= empty;
    raw_write_word(cpu, PMP_STATUS, status);
    pmp_refresh_slave_status(cpu);
    transfer.cycle = cpu->device_cycles;
    transfer.address = index;
    transfer.control = raw_word(cpu, PMP_CONTROL);
    transfer.mode = raw_word(cpu, PMP_MODE);
    transfer.value = cpu->data[PMP_ADDRESS + index];
    transfer.width = 1u;
    if (!pmp_output_push(&cpu->io.pmp.output, &transfer)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    pmp_slave_interrupt(cpu, index);
}

static void pmp_read_slave_buffer(Dspic33* cpu, uint16_t address) {
    uint16_t status;
    uint16_t full;
    uint8_t first;
    uint8_t width;
    if (!pmp_slave_configured(cpu) || address < PMP_DATA ||
        address > PMP_INPUT_2 + 1u) {
        return;
    }
    first = (uint8_t)(address - PMP_DATA);
    width = 1u;
    if (cpu->io.cpu_read_valid && cpu->io.cpu_read_width == 2u &&
        cpu->io.cpu_read_address >= PMP_DATA &&
        cpu->io.cpu_read_address <= PMP_INPUT_2) {
        first = (uint8_t)(cpu->io.cpu_read_address - PMP_DATA);
        width = 2u;
    }
    status = raw_word(cpu, PMP_STATUS);
    full = pmp_buffered_slave(cpu) ? (uint16_t)(((1u << width) - 1u) << (8u + first))
                                   : PMP_INPUT_FULL;
    if (pmp_buffered_slave(cpu) || first == 0u) {
        raw_write_word(cpu, PMP_STATUS, (uint16_t)(status & ~full));
        pmp_refresh_slave_status(cpu);
    }
}

static void pmp_write_slave_buffer(Dspic33* cpu, uint16_t address) {
    uint16_t status;
    uint16_t empty;
    uint8_t first;
    uint8_t width;
    if (!pmp_slave_configured(cpu) || address < PMP_ADDRESS ||
        address > PMP_OUTPUT_2 + 1u) {
        return;
    }
    first = (uint8_t)(address - PMP_ADDRESS);
    width = 1u;
    if (cpu->io.cpu_write_valid && cpu->io.cpu_write_width == 2u &&
        cpu->io.cpu_write_address >= PMP_ADDRESS &&
        cpu->io.cpu_write_address <= PMP_OUTPUT_2) {
        first = (uint8_t)(cpu->io.cpu_write_address - PMP_ADDRESS);
        width = 2u;
    }
    status = raw_word(cpu, PMP_STATUS);
    empty = pmp_buffered_slave(cpu) ? (uint16_t)(((1u << width) - 1u) << first)
                                    : PMP_OUTPUT_EMPTY;
    if (pmp_buffered_slave(cpu) || first == 0u) {
        raw_write_word(cpu, PMP_STATUS, (uint16_t)(status & ~empty));
        pmp_refresh_slave_status(cpu);
    }
}

static void run_pmp_pmd(Dspic33* cpu, uint32_t value) {
    uint16_t generation = (uint16_t)(value >> 1u);
    if (generation != cpu->io.pmp.pmd_generation) {
        return;
    }
    cpu->io.pmp.pmd_disabled = (value & 1u) != 0u;
    dspic33_device_power_state_changed(cpu);
}

static void update_pmp_pmd(Dspic33* cpu, uint16_t previous) {
    bool disabled = (raw_word(cpu, PMP_PMD_ADDRESS) & PMP_PMD) != 0u;
    if (((previous & PMP_PMD) != 0u) == disabled) {
        return;
    }
    cpu->io.pmp.pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_PMD,
                          ((uint32_t)cpu->io.pmp.pmd_generation << 1u) |
                              (disabled ? 1u : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        raw_write_word(cpu, PMP_PMD_ADDRESS, previous);
        cpu->io.pmp.pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void pmp_start_transfer(Dspic33* cpu, bool reading) {
    uint64_t delay;
    cpu->io.pmp.generation++;
    cpu->io.pmp.address = raw_word(cpu, PMP_ADDRESS);
    cpu->io.pmp.control = raw_word(cpu, PMP_CONTROL);
    cpu->io.pmp.mode = (uint16_t)(raw_word(cpu, PMP_MODE) & ~PMP_BUSY);
    cpu->io.pmp.width = pmp_transfer_width(cpu->io.pmp.mode);
    cpu->io.pmp.value = reading
                            ? 0u
                            : (uint16_t)(raw_word(cpu, PMP_DATA) &
                                         (cpu->io.pmp.width == 2u ? 0xffffu : 0x00ffu));
    cpu->io.pmp.reading = reading;
    cpu->io.pmp.active = true;
    delay = pmp_transfer_cycles(cpu->io.pmp.control, cpu->io.pmp.mode);
    if (delay > 1u) {
        raw_write_word(cpu, PMP_MODE, (uint16_t)(raw_word(cpu, PMP_MODE) | PMP_BUSY));
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_COMPLETE,
                          cpu->io.pmp.generation, delay) ||
        (delay > 1u && !dspic33_schedule(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_CLEAR_BUSY,
                                         cpu->io.pmp.generation, delay - 1u))) {
        pmp_abort(cpu);
    }
}

static void pmp_clear_busy(Dspic33* cpu, uint16_t generation) {
    if (!cpu->io.pmp.active || generation != cpu->io.pmp.generation) {
        return;
    }
    cpu->io.pmp.completing.cycle = 0u;
    cpu->io.pmp.completing.address = cpu->io.pmp.address;
    cpu->io.pmp.completing.control = cpu->io.pmp.control;
    cpu->io.pmp.completing.mode = cpu->io.pmp.mode;
    cpu->io.pmp.completing.value = cpu->io.pmp.value;
    cpu->io.pmp.completing.width = cpu->io.pmp.width;
    cpu->io.pmp.completing_generation = generation;
    cpu->io.pmp.completing_reading = cpu->io.pmp.reading;
    cpu->io.pmp.completing_active = true;
    cpu->io.pmp.active = false;
    cpu->io.pmp.reading = false;
    pmp_update_address(cpu, cpu->io.pmp.control, cpu->io.pmp.mode);
    raw_write_word(cpu, PMP_MODE, (uint16_t)(raw_word(cpu, PMP_MODE) & ~PMP_BUSY));
}

static void run_pmp(Dspic33* cpu, uint16_t generation) {
    Dspic33PmpResponse response;
    Dspic33PmpTransfer transfer;
    bool reading;
    if (cpu->io.pmp.completing_active &&
        generation == cpu->io.pmp.completing_generation) {
        transfer = cpu->io.pmp.completing;
        reading = cpu->io.pmp.completing_reading;
        cpu->io.pmp.completing_active = false;
        cpu->io.pmp.completing_reading = false;
    } else if (cpu->io.pmp.active && generation == cpu->io.pmp.generation) {
        transfer.address = cpu->io.pmp.address;
        transfer.control = cpu->io.pmp.control;
        transfer.mode = cpu->io.pmp.mode;
        transfer.value = cpu->io.pmp.value;
        transfer.width = cpu->io.pmp.width;
        reading = cpu->io.pmp.reading;
        cpu->io.pmp.active = false;
        cpu->io.pmp.reading = false;
        pmp_update_address(cpu, cpu->io.pmp.control, cpu->io.pmp.mode);
        raw_write_word(cpu, PMP_MODE, (uint16_t)(raw_word(cpu, PMP_MODE) & ~PMP_BUSY));
    } else {
        return;
    }
    transfer.cycle = cpu->device_cycles;
    if (reading) {
        transfer.value = 0u;
        if (pmp_response_pop(&cpu->io.pmp.input, cpu->device_cycles, &response)) {
            transfer.value = response.value;
        }
        transfer.value &= transfer.width == 2u ? 0xffffu : 0x00ffu;
        raw_write_word(
            cpu, PMP_DATA,
            transfer.width == 2u
                ? transfer.value
                : (uint16_t)((raw_word(cpu, PMP_DATA) & 0xff00u) | transfer.value));
        cpu->io.pmp.last_read = transfer;
        cpu->io.pmp.last_read_valid = true;
    } else {
        if (!pmp_output_push(&cpu->io.pmp.output, &transfer)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
    }
    if ((transfer.mode & PMP_INTERRUPT_MODE_MASK) == PMP_INTERRUPT_EACH) {
        dspic33_raise_interrupt(cpu, PMP_IRQ);
        if (!dspic33_dma_request(cpu, PMP_DMA_REQUEST, 0u, 0u)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
}

static bool pmp_initiating_write(const Dspic33* cpu, uint16_t address) {
    return address == PMP_DATA ||
           (address == PMP_DATA + 1u && cpu->io.cpu_write_valid &&
            cpu->io.cpu_write_width == 2u && cpu->io.cpu_write_address == PMP_DATA);
}

static void pmp_read_register(Dspic33* cpu, uint16_t address) {
    pmp_read_slave_buffer(cpu, address);
    if (address == PMP_DATA && !cpu->io.pmp.active && pmp_master_enabled(cpu)) {
        pmp_start_transfer(cpu, true);
    }
}

static void update_pmp_register(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    if (base < PMP_CONTROL || base > PMP_STATUS) {
        return;
    }
    if (cpu->io.pmp.pmd_disabled) {
        raw_write_word(cpu, base, previous);
        return;
    }
    if (base == PMP_CONTROL) {
        uint16_t current = raw_word(cpu, PMP_CONTROL);
        bool was_enabled = (previous & PMP_ENABLE) != 0u;
        bool enabled = (current & PMP_ENABLE) != 0u;
        if (was_enabled && !enabled) {
            pmp_abort(cpu);
        } else if (!was_enabled && enabled) {
            raw_write_word(cpu, PMP_STATUS, 0x008fu);
            cpu->io.pmp.slave_read_index = 0u;
            cpu->io.pmp.slave_write_index = 0u;
        }
        if (((previous ^ current) & PMP_STOP_IDLE) != 0u &&
            cpu->power_state == DSPIC33_POWER_IDLE) {
            dspic33_device_power_state_changed(cpu);
        }
        return;
    }
    if (base == PMP_ADDRESS || base == PMP_OUTPUT_2) {
        pmp_write_slave_buffer(cpu, address);
        return;
    }
    if (base != PMP_DATA || !pmp_initiating_write(cpu, address)) {
        return;
    }
    if (cpu->io.pmp.active) {
        raw_write_word(cpu, PMP_DATA, previous);
        return;
    }
    if (pmp_master_enabled(cpu)) {
        pmp_start_transfer(cpu, false);
    }
}

static uint16_t input_capture_base(uint8_t channel) {
    return (uint16_t)(INPUT_CAPTURE_BASE + channel * INPUT_CAPTURE_STRIDE);
}

static bool input_capture_pmd_disabled(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.input_capture.pmd_disabled & (uint16_t)(1u << channel)) != 0u;
}

static bool input_capture_pair_configured(const Dspic33* cpu, uint8_t channel) {
    uint8_t first = (uint8_t)(channel & 0xfeu);
    if (first + 1u >= DSPIC33_INPUT_CAPTURE_COUNT) {
        return false;
    }
    return (raw_word(cpu, (uint16_t)(input_capture_base(first) + 2u)) &
            INPUT_CAPTURE_32_BIT) != 0u &&
           (raw_word(cpu, (uint16_t)(input_capture_base((uint8_t)(first + 1u)) + 2u)) &
            INPUT_CAPTURE_32_BIT) != 0u;
}

static bool input_capture_event_belongs_to_channel(const Dspic33Event* event,
                                                   uint8_t channel) {
    uint32_t kind = event->value & INPUT_CAPTURE_EVENT_KIND_MASK;
    if (event->source == channel) {
        return kind == INPUT_CAPTURE_EVENT_CAPTURE ||
               kind == INPUT_CAPTURE_EVENT_INTERRUPT;
    }
    return kind == INPUT_CAPTURE_EVENT_CAPTURE &&
           (event->value & INPUT_CAPTURE_EVENT_PAIRED) != 0u &&
           event->source == (uint16_t)(channel & 0xfeu);
}

static bool input_capture_event_can_resume(const Dspic33* cpu,
                                           const Dspic33Event* event) {
    uint32_t kind = event->value & INPUT_CAPTURE_EVENT_KIND_MASK;
    if (kind == INPUT_CAPTURE_EVENT_CAPTURE &&
        (event->value & INPUT_CAPTURE_EVENT_PAIRED) != 0u) {
        return !input_capture_pmd_disabled(cpu, (uint8_t)event->source) &&
               !input_capture_pmd_disabled(cpu, (uint8_t)(event->source + 1u));
    }
    return !input_capture_pmd_disabled(cpu, (uint8_t)event->source);
}

static void input_capture_pause_events(Dspic33* cpu, uint8_t channel) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (event->type != DSPIC33_EVENT_INPUT_CAPTURE || event->paused ||
            !input_capture_event_belongs_to_channel(event, channel)) {
            continue;
        }
        event->paused_remaining = event->cycle - cpu->device_cycles;
        event->paused = true;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void input_capture_resume_events(Dspic33* cpu, uint8_t channel) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (event->type != DSPIC33_EVENT_INPUT_CAPTURE || !event->paused ||
            !input_capture_event_belongs_to_channel(event, channel) ||
            !input_capture_event_can_resume(cpu, event)) {
            continue;
        }
        if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            continue;
        }
        event->cycle = cpu->device_cycles + event->paused_remaining;
        event->paused_remaining = 0u;
        event->paused = false;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static bool input_capture_fifo_push(Dspic33InputCaptureFifo* fifo, uint16_t value) {
    uint8_t index;
    if (fifo->count == DSPIC33_INPUT_CAPTURE_FIFO_SIZE) {
        return false;
    }
    index = (uint8_t)((fifo->head + fifo->count) % DSPIC33_INPUT_CAPTURE_FIFO_SIZE);
    fifo->words[index] = value;
    fifo->count++;
    return true;
}

static bool input_capture_fifo_pop(Dspic33InputCaptureFifo* fifo, uint16_t* value) {
    if (fifo->count == 0u) {
        return false;
    }
    *value = fifo->words[fifo->head];
    fifo->head = (uint8_t)((fifo->head + 1u) % DSPIC33_INPUT_CAPTURE_FIFO_SIZE);
    fifo->count--;
    return true;
}

static uint16_t input_capture_fifo_front(const Dspic33InputCaptureFifo* fifo) {
    return fifo->count == 0u ? 0u : fifo->words[fifo->head];
}

static void input_capture_refresh(Dspic33* cpu, uint8_t channel) {
    uint16_t base = input_capture_base(channel);
    uint16_t control = raw_word(cpu, base);
    control &= (uint16_t)~INPUT_CAPTURE_NOT_EMPTY;
    if (cpu->io.input_capture.fifo[channel].count != 0u) {
        control |= INPUT_CAPTURE_NOT_EMPTY;
    }
    raw_write_word(cpu, base, control);
    raw_write_word(cpu, (uint16_t)(base + 4u),
                   input_capture_fifo_front(&cpu->io.input_capture.fifo[channel]));
}

static void input_capture_flush(Dspic33* cpu, uint8_t channel) {
    Dspic33InputCaptureFifo* fifo = &cpu->io.input_capture.fifo[channel];
    uint16_t base = input_capture_base(channel);
    fifo->head = 0u;
    fifo->count = 0u;
    cpu->io.input_capture.interrupt_count[channel] = 0u;
    cpu->io.input_capture.prescaler_count[channel] = 0u;
    cpu->io.input_capture.timer[channel] = 0u;
    cpu->io.input_capture.generation[channel]++;
    raw_write_word(cpu, base,
                   (uint16_t)(raw_word(cpu, base) &
                              ~(INPUT_CAPTURE_NOT_EMPTY | INPUT_CAPTURE_OVERFLOW)));
    raw_write_word(cpu, (uint16_t)(base + 4u), 0u);
    raw_write_word(cpu, (uint16_t)(base + 6u), 0u);
}

static bool input_capture_sync_source_valid(uint8_t channel, uint16_t control2) {
    uint8_t source = (uint8_t)(control2 & INPUT_CAPTURE_SYNC_MASK);
    if (source == INPUT_CAPTURE_SYNC_RESERVED ||
        (channel < 8u && source == INPUT_CAPTURE_SYNC_IC_FIRST + channel)) {
        return false;
    }
    return (control2 & INPUT_CAPTURE_TRIGGER) != 0u ||
           source < INPUT_CAPTURE_SYNC_COMPARATOR_FIRST || source >= 29u;
}

static bool input_capture_configuration_supported(const Dspic33* cpu, uint8_t channel) {
    uint16_t base = input_capture_base(channel);
    uint16_t control1 = raw_word(cpu, base);
    uint16_t control2 = raw_word(cpu, (uint16_t)(base + 2u));
    uint16_t mode = control1 & INPUT_CAPTURE_MODE_MASK;
    uint16_t timer_source = control1 & INPUT_CAPTURE_TIMER_SOURCE_MASK;
    return mode != 0u && mode != 6u && mode != INPUT_CAPTURE_MODE_INTERRUPT &&
           timer_source != 0x1400u && timer_source != 0x1800u &&
           input_capture_sync_source_valid(channel, control2);
}

static bool input_capture_operating(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, input_capture_base(channel));
    if (input_capture_pmd_disabled(cpu, channel) ||
        (input_capture_pair_configured(cpu, channel) &&
         input_capture_pmd_disabled(cpu, (uint8_t)(channel ^ 1u))) ||
        !input_capture_configuration_supported(cpu, channel) ||
        cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE ||
           (control & INPUT_CAPTURE_STOP_IDLE) == 0u;
}

static bool input_capture_timer_running(const Dspic33* cpu, uint8_t channel) {
    uint16_t control2;
    if (!input_capture_operating(cpu, channel)) {
        return false;
    }
    control2 = raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
    return (control2 & INPUT_CAPTURE_TRIGGER) == 0u ||
           (control2 & INPUT_CAPTURE_TRIGGER_STATUS) != 0u;
}

static bool input_capture_pair_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t first;
    uint16_t second;
    if ((channel & 1u) != 0u || channel + 1u >= DSPIC33_INPUT_CAPTURE_COUNT ||
        !input_capture_operating(cpu, channel) ||
        !input_capture_operating(cpu, (uint8_t)(channel + 1u))) {
        return false;
    }
    first = raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
    second =
        raw_word(cpu, (uint16_t)(input_capture_base((uint8_t)(channel + 1u)) + 2u));
    return (first & INPUT_CAPTURE_32_BIT) != 0u &&
           (second & INPUT_CAPTURE_32_BIT) != 0u &&
           (raw_word(cpu, input_capture_base(channel)) &
            INPUT_CAPTURE_TIMER_SOURCE_MASK) ==
               (raw_word(cpu, input_capture_base((uint8_t)(channel + 1u))) &
                INPUT_CAPTURE_TIMER_SOURCE_MASK);
}

static bool input_capture_pair_timer_running(const Dspic33* cpu, uint8_t channel) {
    return input_capture_pair_enabled(cpu, channel) &&
           input_capture_timer_running(cpu, channel) &&
           input_capture_timer_running(cpu, (uint8_t)(channel + 1u));
}

static bool input_capture_schedule_interrupt(Dspic33* cpu, uint8_t channel) {
    uint32_t value = INPUT_CAPTURE_EVENT_INTERRUPT |
                     ((uint32_t)cpu->io.input_capture.generation[channel]
                      << INPUT_CAPTURE_EVENT_GENERATION_SHIFT);
    return dspic33_schedule(cpu, DSPIC33_EVENT_INPUT_CAPTURE, channel, value, 2u);
}

static bool input_capture_request_dma(Dspic33* cpu, uint8_t channel) {
    uint16_t base = input_capture_base(channel);
    if (channel >= 4u || (raw_word(cpu, base) & INPUT_CAPTURE_INTERRUPT_MASK) != 0u) {
        return true;
    }
    return dspic33_dma_request(cpu, input_capture_irqs[channel], (uint16_t)(base + 4u),
                               0u);
}

static void input_capture_reset_timer(Dspic33* cpu, uint8_t channel) {
    cpu->io.input_capture.timer[channel] = 0u;
    raw_write_word(cpu, (uint16_t)(input_capture_base(channel) + 6u), 0u);
}

static void input_capture_refresh_sync_outputs(Dspic33* cpu);

static void input_capture_trigger_source(Dspic33* cpu, uint8_t source) {
    uint8_t channel = 0u;
    while (channel < DSPIC33_INPUT_CAPTURE_COUNT) {
        uint16_t control2 = raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
        bool paired = input_capture_pair_enabled(cpu, channel);
        if (input_capture_operating(cpu, channel) &&
            (control2 & INPUT_CAPTURE_TRIGGER) != 0u &&
            (control2 & INPUT_CAPTURE_SYNC_MASK) == source) {
            raw_write_word(cpu, (uint16_t)(input_capture_base(channel) + 2u),
                           (uint16_t)(control2 | INPUT_CAPTURE_TRIGGER_STATUS));
        }
        if (paired) {
            uint16_t second = raw_word(
                cpu, (uint16_t)(input_capture_base((uint8_t)(channel + 1u)) + 2u));
            if (input_capture_operating(cpu, (uint8_t)(channel + 1u)) &&
                (second & INPUT_CAPTURE_TRIGGER) != 0u &&
                (second & INPUT_CAPTURE_SYNC_MASK) == source) {
                raw_write_word(
                    cpu, (uint16_t)(input_capture_base((uint8_t)(channel + 1u)) + 2u),
                    (uint16_t)(second | INPUT_CAPTURE_TRIGGER_STATUS));
            }
            channel += 2u;
        } else {
            channel++;
        }
    }
}

static void input_capture_pulse_source(Dspic33* cpu, uint8_t source) {
    uint8_t channel;
    input_capture_trigger_source(cpu, source);
    input_capture_refresh_sync_outputs(cpu);
    for (channel = 0u; channel < DSPIC33_INPUT_CAPTURE_COUNT; channel++) {
        uint16_t control2 = raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
        if (input_capture_operating(cpu, channel) &&
            (control2 & INPUT_CAPTURE_TRIGGER) == 0u &&
            (control2 & INPUT_CAPTURE_SYNC_MASK) == source) {
            cpu->io.input_capture.sync_reset_pending |= (uint16_t)(1u << channel);
        }
    }
}

static bool input_capture_source_awaited(const Dspic33* cpu, uint8_t source) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_INPUT_CAPTURE_COUNT; channel++) {
        uint16_t control2 = raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
        if (input_capture_operating(cpu, channel) &&
            (control2 & INPUT_CAPTURE_SYNC_MASK) == source &&
            ((control2 & INPUT_CAPTURE_TRIGGER) == 0u ||
             (control2 & INPUT_CAPTURE_TRIGGER_STATUS) == 0u)) {
            return true;
        }
    }
    return false;
}

static bool input_capture_sync_output_base_high(const Dspic33* cpu, uint8_t channel) {
    uint16_t control1 = raw_word(cpu, input_capture_base(channel));
    uint16_t control2 = raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
    if (input_capture_pmd_disabled(cpu, channel) ||
        (control1 & INPUT_CAPTURE_MODE_MASK) == 0u ||
        (control1 & INPUT_CAPTURE_MODE_MASK) == 6u) {
        return true;
    }
    if ((control2 & INPUT_CAPTURE_TRIGGER) != 0u &&
        (control2 & INPUT_CAPTURE_TRIGGER_STATUS) == 0u) {
        return true;
    }
    return cpu->io.input_capture.timer[channel] == UINT16_MAX;
}

static void input_capture_refresh_sync_outputs(Dspic33* cpu) {
    uint8_t pass;
    for (pass = 0u; pass < 8u; pass++) {
        uint16_t previous = cpu->io.input_capture.sync_output_high & 0x00ffu;
        uint16_t current = 0u;
        uint16_t rising;
        uint8_t channel;
        for (channel = 0u; channel < 8u; channel++) {
            if (input_capture_sync_output_base_high(cpu, channel)) {
                current |= (uint16_t)(1u << channel);
            }
        }
        for (;;) {
            uint16_t expanded = current;
            for (channel = 0u; channel < 8u; channel++) {
                uint16_t control2 =
                    raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
                uint8_t source = (uint8_t)(control2 & INPUT_CAPTURE_SYNC_MASK);
                if ((control2 & INPUT_CAPTURE_TRIGGER) == 0u &&
                    source >= INPUT_CAPTURE_SYNC_IC_FIRST &&
                    source < INPUT_CAPTURE_SYNC_COMPARATOR_FIRST &&
                    (current &
                     (uint16_t)(1u << (source - INPUT_CAPTURE_SYNC_IC_FIRST))) != 0u) {
                    expanded |= (uint16_t)(1u << channel);
                }
            }
            if (expanded == current) {
                break;
            }
            current = expanded;
        }
        cpu->io.input_capture.sync_output_high = current;
        rising = (uint16_t)(current & ~previous);
        if (rising == 0u) {
            break;
        }
        for (channel = 0u; channel < 8u; channel++) {
            if ((rising & (uint16_t)(1u << channel)) != 0u) {
                input_capture_trigger_source(
                    cpu, (uint8_t)(INPUT_CAPTURE_SYNC_IC_FIRST + channel));
            }
        }
    }
}

static void input_capture_record(Dspic33* cpu, uint8_t channel, uint16_t value) {
    Dspic33InputCaptureFifo* fifo = &cpu->io.input_capture.fifo[channel];
    uint16_t base = input_capture_base(channel);
    uint16_t control = raw_word(cpu, base);
    uint16_t mode = control & INPUT_CAPTURE_MODE_MASK;
    uint8_t interrupt_interval =
        mode == INPUT_CAPTURE_MODE_EVERY_EDGE
            ? 1u
            : (uint8_t)(((control & INPUT_CAPTURE_INTERRUPT_MASK) >> 5u) + 1u);
    bool interrupt = false;
    if (!input_capture_fifo_push(fifo, value)) {
        if (mode != INPUT_CAPTURE_MODE_EVERY_EDGE &&
            (control & INPUT_CAPTURE_INTERRUPT_MASK) != 0u) {
            raw_write_word(cpu, base, (uint16_t)(control | INPUT_CAPTURE_OVERFLOW));
            return;
        }
        interrupt = true;
    } else {
        cpu->io.input_capture.interrupt_count[channel]++;
        if (cpu->io.input_capture.interrupt_count[channel] == interrupt_interval) {
            cpu->io.input_capture.interrupt_count[channel] = 0u;
            interrupt = true;
        }
        input_capture_refresh(cpu, channel);
    }
    if (interrupt && !input_capture_schedule_interrupt(cpu, channel)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    if (!input_capture_request_dma(cpu, channel)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void input_capture_snapshot(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint16_t generation = (uint16_t)(value >> INPUT_CAPTURE_EVENT_GENERATION_SHIFT);
    bool paired = (value & INPUT_CAPTURE_EVENT_PAIRED) != 0u;
    if (generation != cpu->io.input_capture.generation[channel] ||
        !input_capture_operating(cpu, channel)) {
        return;
    }
    if (paired) {
        if (!input_capture_pair_enabled(cpu, channel)) {
            return;
        }
        input_capture_record(cpu, channel, cpu->io.input_capture.timer[channel]);
        if (cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR) {
            return;
        }
        input_capture_record(cpu, (uint8_t)(channel + 1u),
                             cpu->io.input_capture.timer[channel + 1u]);
        return;
    }
    input_capture_record(cpu, channel, cpu->io.input_capture.timer[channel]);
}

static bool input_capture_edge_matches(Dspic33* cpu, uint8_t channel, bool was_high,
                                       bool high) {
    uint16_t mode =
        raw_word(cpu, input_capture_base(channel)) & INPUT_CAPTURE_MODE_MASK;
    if (was_high == high) {
        return false;
    }
    if (mode == INPUT_CAPTURE_MODE_EVERY_EDGE) {
        return true;
    }
    if (mode == INPUT_CAPTURE_MODE_FALLING) {
        return was_high && !high;
    }
    if (mode == INPUT_CAPTURE_MODE_RISING) {
        return !was_high && high;
    }
    if (!was_high && high &&
        (mode == INPUT_CAPTURE_MODE_EVERY_FOURTH_RISING ||
         mode == INPUT_CAPTURE_MODE_EVERY_SIXTEENTH_RISING)) {
        uint8_t interval = mode == INPUT_CAPTURE_MODE_EVERY_FOURTH_RISING ? 4u : 16u;
        cpu->io.input_capture.prescaler_count[channel] =
            (uint8_t)((cpu->io.input_capture.prescaler_count[channel] + 1u) & 0x0fu);
        if ((cpu->io.input_capture.prescaler_count[channel] & (interval - 1u)) == 0u) {
            return true;
        }
    }
    return false;
}

static bool input_capture_interrupt_mode(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, input_capture_base(channel));
    return !input_capture_pmd_disabled(cpu, channel) &&
           (control & INPUT_CAPTURE_MODE_MASK) == INPUT_CAPTURE_MODE_INTERRUPT &&
           (cpu->power_state == DSPIC33_POWER_SLEEP ||
            cpu->power_state == DSPIC33_POWER_IDLE);
}

static void input_capture_level(Dspic33* cpu, uint8_t channel, bool high) {
    uint16_t bit = (uint16_t)(1u << channel);
    bool was_high = (cpu->io.input_capture.input_high & bit) != 0u;
    uint32_t value;
    if (high) {
        cpu->io.input_capture.input_high |= bit;
    } else {
        cpu->io.input_capture.input_high &= (uint16_t)~bit;
    }
    if (input_capture_interrupt_mode(cpu, channel)) {
        if (!was_high && high) {
            dspic33_raise_interrupt(cpu, input_capture_irqs[channel]);
        }
        return;
    }
    if (!input_capture_operating(cpu, channel) ||
        !input_capture_edge_matches(cpu, channel, was_high, high) ||
        ((channel & 1u) != 0u &&
         input_capture_pair_enabled(cpu, (uint8_t)(channel - 1u)))) {
        return;
    }
    value = INPUT_CAPTURE_EVENT_CAPTURE |
            ((uint32_t)cpu->io.input_capture.generation[channel]
             << INPUT_CAPTURE_EVENT_GENERATION_SHIFT);
    if (input_capture_pair_enabled(cpu, channel)) {
        value |= INPUT_CAPTURE_EVENT_PAIRED;
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_INPUT_CAPTURE, channel, value, 1u)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static uint8_t input_capture_pps_pin(const Dspic33* cpu, uint8_t channel) {
    uint16_t mapping = raw_word(cpu, input_capture_pps_registers[channel / 2u]);
    return (channel & 1u) == 0u ? (uint8_t)(mapping & 0x007fu)
                                : (uint8_t)((mapping >> 8u) & 0x007fu);
}

static const Dspic33PpsPin* pps_pin(uint8_t pin) {
    size_t first = 0u;
    size_t count = sizeof(pps_pins) / sizeof(pps_pins[0]);
    while (count != 0u) {
        size_t step = count / 2u;
        size_t index = first + step;
        if (pps_pins[index].pin < pin) {
            first = index + 1u;
            count -= step + 1u;
        } else {
            count = step;
        }
    }
    if (first == sizeof(pps_pins) / sizeof(pps_pins[0]) || pps_pins[first].pin != pin) {
        return NULL;
    }
    return &pps_pins[first];
}

static bool pps_physical_input_enabled(const Dspic33* cpu, uint8_t pin) {
    const Dspic33PpsPin* mapping = pps_pin(pin);
    uint16_t bit;
    if (mapping == NULL) {
        return false;
    }
    bit = (uint16_t)(1u << mapping->bit);
    return (raw_word(cpu, gpio_tris_addresses[mapping->port]) & bit) != 0u &&
           (gpio_analog_addresses[mapping->port] == 0u ||
            (raw_word(cpu, gpio_analog_addresses[mapping->port]) & bit) == 0u);
}

static void input_capture_pps_source(Dspic33* cpu, uint8_t source, bool high) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_INPUT_CAPTURE_COUNT; channel++) {
        if (input_capture_pps_pin(cpu, channel) == source) {
            input_capture_level(cpu, channel, high);
        }
    }
}

static void run_input_capture(Dspic33* cpu, uint16_t source, uint32_t value) {
    uint32_t kind = value & INPUT_CAPTURE_EVENT_KIND_MASK;
    if (kind == INPUT_CAPTURE_EVENT_PIN) {
        if (pps_physical_input_enabled(cpu, (uint8_t)source)) {
            input_capture_pps_source(cpu, (uint8_t)source,
                                     (value & INPUT_CAPTURE_EVENT_HIGH) != 0u);
        }
        return;
    }
    if (source >= DSPIC33_INPUT_CAPTURE_COUNT) {
        return;
    }
    if (kind == INPUT_CAPTURE_EVENT_PMD) {
        uint16_t generation = (uint16_t)(value >> INPUT_CAPTURE_EVENT_GENERATION_SHIFT);
        uint16_t bit = (uint16_t)(1u << source);
        if (generation != cpu->io.input_capture.pmd_generation[source]) {
            return;
        }
        if ((value & INPUT_CAPTURE_EVENT_PMD_DISABLED) != 0u) {
            cpu->io.input_capture.pmd_disabled |= bit;
            input_capture_pause_events(cpu, (uint8_t)source);
        } else {
            cpu->io.input_capture.pmd_disabled &= (uint16_t)~bit;
            input_capture_resume_events(cpu, (uint8_t)source);
        }
        input_capture_refresh_sync_outputs(cpu);
    } else if (kind == INPUT_CAPTURE_EVENT_INPUT) {
        input_capture_level(cpu, (uint8_t)source,
                            (value & INPUT_CAPTURE_EVENT_HIGH) != 0u);
    } else if (kind == INPUT_CAPTURE_EVENT_CAPTURE) {
        input_capture_snapshot(cpu, (uint8_t)source, value);
    } else if (kind == INPUT_CAPTURE_EVENT_INTERRUPT &&
               (uint16_t)(value >> INPUT_CAPTURE_EVENT_GENERATION_SHIFT) ==
                   cpu->io.input_capture.generation[source] &&
               (input_capture_operating(cpu, (uint8_t)source) ||
                input_capture_interrupt_mode(cpu, (uint8_t)source))) {
        dspic33_raise_interrupt(cpu, input_capture_irqs[source]);
        if (source < 8u) {
            output_compare_pulse_source(
                cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_IC_FIRST + source));
        }
    }
}

static void update_input_capture_register(Dspic33* cpu, uint16_t address,
                                          uint16_t previous) {
    uint16_t base;
    uint16_t offset;
    uint16_t current;
    uint8_t channel;
    if (address < INPUT_CAPTURE_BASE ||
        address >=
            INPUT_CAPTURE_BASE + DSPIC33_INPUT_CAPTURE_COUNT * INPUT_CAPTURE_STRIDE) {
        return;
    }
    channel = (uint8_t)((address - INPUT_CAPTURE_BASE) / INPUT_CAPTURE_STRIDE);
    base = input_capture_base(channel);
    offset = (uint16_t)(address - base);
    current = raw_word(cpu, base + offset);
    if (input_capture_pmd_disabled(cpu, channel)) {
        raw_write_word(cpu, (uint16_t)(base + offset), previous);
        return;
    }
    if (offset == 0u) {
        if ((previous & INPUT_CAPTURE_MODE_MASK) != 0u &&
            (current & INPUT_CAPTURE_MODE_MASK) == 0u) {
            input_capture_flush(cpu, channel);
        } else if ((previous & INPUT_CAPTURE_CON1_WRITABLE) !=
                   (current & INPUT_CAPTURE_CON1_WRITABLE)) {
            cpu->io.input_capture.generation[channel]++;
        }
        input_capture_refresh_sync_outputs(cpu);
        return;
    }
    if (offset == 2u && (previous & INPUT_CAPTURE_CON2_WRITABLE) !=
                            (current & INPUT_CAPTURE_CON2_WRITABLE)) {
        if ((previous &
             (INPUT_CAPTURE_CON2_WRITABLE & ~INPUT_CAPTURE_TRIGGER_STATUS)) !=
            (current & (INPUT_CAPTURE_CON2_WRITABLE & ~INPUT_CAPTURE_TRIGGER_STATUS))) {
            cpu->io.input_capture.generation[channel]++;
            cpu->io.input_capture.sync_reset_pending &=
                (uint16_t)~(uint16_t)(1u << channel);
        }
        if ((current & (INPUT_CAPTURE_TRIGGER | INPUT_CAPTURE_TRIGGER_STATUS)) ==
            INPUT_CAPTURE_TRIGGER) {
            input_capture_reset_timer(cpu, channel);
        }
        input_capture_refresh_sync_outputs(cpu);
    }
}

static void update_input_capture_pmd(Dspic33* cpu, uint16_t address,
                                     uint16_t previous) {
    uint8_t first_channel;
    uint8_t channel;
    uint16_t changed;
    uint16_t current;
    if (address == 0x0762u) {
        first_channel = 0u;
    } else if (address == 0x0768u) {
        first_channel = 8u;
    } else {
        return;
    }
    current = raw_word(cpu, address);
    changed = (uint16_t)((previous ^ current) & 0xff00u);
    for (channel = first_channel; channel < first_channel + 8u; channel++) {
        uint16_t register_mask = (uint16_t)(1u << (8u + channel - first_channel));
        if ((changed & register_mask) == 0u) {
            continue;
        }
        cpu->io.input_capture.pmd_generation[channel]++;
        if (!dspic33_schedule(
                cpu, DSPIC33_EVENT_INPUT_CAPTURE, channel,
                INPUT_CAPTURE_EVENT_PMD |
                    ((current & register_mask) != 0u ? INPUT_CAPTURE_EVENT_PMD_DISABLED
                                                     : 0u) |
                    ((uint32_t)cpu->io.input_capture.pmd_generation[channel]
                     << INPUT_CAPTURE_EVENT_GENERATION_SHIFT),
                dspic33_device_instruction_cycles(cpu, 1u))) {
            uint8_t invalidate;
            raw_write_word(cpu, address, previous);
            for (invalidate = first_channel; invalidate < first_channel + 8u;
                 invalidate++) {
                if ((changed & (uint16_t)(1u << (8u + invalidate - first_channel))) !=
                    0u) {
                    cpu->io.input_capture.pmd_generation[invalidate]++;
                }
            }
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
    }
}

static void input_capture_read_complete(Dspic33* cpu, uint8_t channel) {
    uint16_t discarded;
    if (!input_capture_fifo_pop(&cpu->io.input_capture.fifo[channel], &discarded)) {
        return;
    }
    if (cpu->io.input_capture.fifo[channel].count == 0u) {
        cpu->io.input_capture.interrupt_count[channel] = 0u;
        raw_write_word(cpu, input_capture_base(channel),
                       (uint16_t)(raw_word(cpu, input_capture_base(channel)) &
                                  ~INPUT_CAPTURE_OVERFLOW));
    }
    input_capture_refresh(cpu, channel);
}

static bool input_capture_sync_held(const Dspic33* cpu, uint8_t channel) {
    uint16_t control2 = raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
    uint8_t source = (uint8_t)(control2 & INPUT_CAPTURE_SYNC_MASK);
    return (control2 & INPUT_CAPTURE_TRIGGER) == 0u &&
           source >= INPUT_CAPTURE_SYNC_IC_FIRST &&
           source < INPUT_CAPTURE_SYNC_COMPARATOR_FIRST &&
           (cpu->io.input_capture.sync_output_high &
            (uint16_t)(1u << (source - INPUT_CAPTURE_SYNC_IC_FIRST))) != 0u;
}

static uint64_t input_capture_sync_transition(const Dspic33* cpu, uint8_t channel,
                                              uint16_t timer_source) {
    uint16_t control1 = raw_word(cpu, input_capture_base(channel));
    if (channel >= 8u || !input_capture_timer_running(cpu, channel) ||
        (control1 & INPUT_CAPTURE_TIMER_SOURCE_MASK) != timer_source ||
        input_capture_sync_held(cpu, channel)) {
        return UINT64_MAX;
    }
    if (input_capture_pair_enabled(cpu, (uint8_t)(channel & 0xfeu))) {
        uint8_t first = (uint8_t)(channel & 0xfeu);
        uint32_t timer = (uint32_t)cpu->io.input_capture.timer[first] |
                         ((uint32_t)cpu->io.input_capture.timer[first + 1u] << 16u);
        if (channel == first) {
            uint16_t low = (uint16_t)timer;
            return low == UINT16_MAX ? 1u : UINT16_MAX - low;
        }
        {
            uint16_t low = (uint16_t)timer;
            uint16_t high = (uint16_t)(timer >> 16u);
            return high == UINT16_MAX ? UINT16_MAX + 1ull - low
                                      : ((uint64_t)(UINT16_MAX - high) << 16u) - low;
        }
    }
    return cpu->io.input_capture.timer[channel] == UINT16_MAX
               ? 1u
               : UINT16_MAX - cpu->io.input_capture.timer[channel];
}

static void input_capture_advance_step(Dspic33* cpu, uint8_t channel, uint64_t cycles) {
    uint16_t bit = (uint16_t)(1u << channel);
    if ((cpu->io.input_capture.sync_reset_pending & bit) != 0u) {
        cpu->io.input_capture.sync_reset_pending &= (uint16_t)~bit;
        input_capture_reset_timer(cpu, channel);
        cycles--;
    }
    if (cycles == 0u || input_capture_sync_held(cpu, channel) ||
        !input_capture_timer_running(cpu, channel)) {
        if (input_capture_sync_held(cpu, channel)) {
            input_capture_reset_timer(cpu, channel);
        }
        return;
    }
    cpu->io.input_capture.timer[channel] =
        (uint16_t)(cpu->io.input_capture.timer[channel] + cycles);
    raw_write_word(cpu, (uint16_t)(input_capture_base(channel) + 6u),
                   cpu->io.input_capture.timer[channel]);
}

static void input_capture_advance_pair(Dspic33* cpu, uint8_t channel, uint64_t cycles) {
    uint16_t bits = (uint16_t)(3u << channel);
    bool reset = (cpu->io.input_capture.sync_reset_pending & bits) != 0u;
    bool held = input_capture_sync_held(cpu, channel) ||
                input_capture_sync_held(cpu, (uint8_t)(channel + 1u));
    uint32_t timer;
    if (reset) {
        cpu->io.input_capture.sync_reset_pending &= (uint16_t)~bits;
        input_capture_reset_timer(cpu, channel);
        input_capture_reset_timer(cpu, (uint8_t)(channel + 1u));
        cycles--;
    }
    if (cycles == 0u || held || !input_capture_pair_timer_running(cpu, channel)) {
        if (held) {
            input_capture_reset_timer(cpu, channel);
            input_capture_reset_timer(cpu, (uint8_t)(channel + 1u));
        }
        return;
    }
    timer = (uint32_t)cpu->io.input_capture.timer[channel] |
            ((uint32_t)cpu->io.input_capture.timer[channel + 1u] << 16u);
    timer += (uint32_t)cycles;
    cpu->io.input_capture.timer[channel] = (uint16_t)timer;
    cpu->io.input_capture.timer[channel + 1u] = (uint16_t)(timer >> 16u);
    raw_write_word(cpu, (uint16_t)(input_capture_base(channel) + 6u), (uint16_t)timer);
    raw_write_word(cpu, (uint16_t)(input_capture_base((uint8_t)(channel + 1u)) + 6u),
                   (uint16_t)(timer >> 16u));
}

static void input_capture_advance_clock(Dspic33* cpu, uint16_t timer_source,
                                        uint64_t cycles) {
    uint64_t remaining = cycles;
    input_capture_refresh_sync_outputs(cpu);
    while (remaining != 0u) {
        uint64_t step = remaining;
        uint8_t source;
        uint8_t channel = 0u;
        for (source = 0u; source < 8u; source++) {
            uint64_t transition =
                input_capture_sync_transition(cpu, source, timer_source);
            if (transition < step) {
                step = transition;
            }
        }
        if (step == 0u) {
            step = 1u;
        }
        while (channel < DSPIC33_INPUT_CAPTURE_COUNT) {
            bool paired = input_capture_pair_configured(cpu, channel);
            if (paired && (raw_word(cpu, input_capture_base(channel)) &
                           INPUT_CAPTURE_TIMER_SOURCE_MASK) == timer_source) {
                input_capture_advance_pair(cpu, channel, step);
                channel += 2u;
                continue;
            }
            if (!paired && (raw_word(cpu, input_capture_base(channel)) &
                            INPUT_CAPTURE_TIMER_SOURCE_MASK) == timer_source) {
                input_capture_advance_step(cpu, channel, step);
            }
            channel += paired ? 2u : 1u;
        }
        remaining -= step;
        input_capture_refresh_sync_outputs(cpu);
    }
}

static void advance_input_capture(Dspic33* cpu, uint64_t cycles) {
    input_capture_advance_clock(cpu, INPUT_CAPTURE_TIMER_SOURCE_FP, cycles);
}

static uint16_t output_compare_base(uint8_t channel) {
    return (uint16_t)(OUTPUT_COMPARE_BASE + channel * OUTPUT_COMPARE_STRIDE);
}

static uint8_t output_compare_pair_low(uint8_t channel) {
    return (uint8_t)(channel & 0xfeu);
}

static uint8_t output_compare_pair_high(uint8_t channel) {
    return (uint8_t)(output_compare_pair_low(channel) + 1u);
}

static bool output_compare_cascade_requested(const Dspic33* cpu, uint8_t channel) {
    uint16_t control2 = raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
    return (control2 & OUTPUT_COMPARE_32_BIT) != 0u;
}

static bool output_compare_clock_supported(uint16_t control1) {
    uint16_t source = control1 & OUTPUT_COMPARE_TIMER_SOURCE_MASK;
    return source <= OUTPUT_COMPARE_TIMER_SOURCE_TIMER1 ||
           source == OUTPUT_COMPARE_TIMER_SOURCE_FP;
}

static bool output_compare_configuration_supported(uint8_t channel, uint16_t control1,
                                                   uint16_t control2) {
    uint16_t mode = control1 & OUTPUT_COMPARE_MODE_MASK;
    uint16_t synchronization = control2 & OUTPUT_COMPARE_SYNC_MASK;
    bool trigger = (control2 & OUTPUT_COMPARE_TRIGGER) != 0u;
    bool own_source =
        synchronization == OUTPUT_COMPARE_SYNC_SELF ||
        (channel < 9u && synchronization == OUTPUT_COMPARE_SYNC_OC_FIRST + channel);
    return output_compare_clock_supported(control1) &&
           (control1 & OUTPUT_COMPARE_CON1_UNSUPPORTED) == 0u && mode != 0u &&
           (control2 & OUTPUT_COMPARE_CON2_UNSUPPORTED) == 0u &&
           (control2 & OUTPUT_COMPARE_32_BIT) == 0u &&
           synchronization != OUTPUT_COMPARE_SYNC_RESERVED && !(trigger && own_source);
}

static bool output_compare_cascade_controls_supported(uint8_t channel,
                                                      uint16_t control1,
                                                      uint16_t control2) {
    uint16_t synchronization = control2 & OUTPUT_COMPARE_SYNC_MASK;
    bool trigger = (control2 & OUTPUT_COMPARE_TRIGGER) != 0u;
    bool own_source =
        synchronization == OUTPUT_COMPARE_SYNC_SELF ||
        (channel < 9u && synchronization == OUTPUT_COMPARE_SYNC_OC_FIRST + channel);
    return output_compare_clock_supported(control1) &&
           (control1 & OUTPUT_COMPARE_CON1_UNSUPPORTED) == 0u &&
           (control1 & OUTPUT_COMPARE_MODE_MASK) != 0u &&
           (control2 & OUTPUT_COMPARE_CON2_UNSUPPORTED) == 0u &&
           (control2 & OUTPUT_COMPARE_32_BIT) != 0u &&
           synchronization != OUTPUT_COMPARE_SYNC_RESERVED && !(trigger && own_source);
}

static bool output_compare_cascade_supported(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    uint16_t low_base = output_compare_base(low);
    uint16_t high_base = output_compare_base(high);
    uint16_t low_control1 = raw_word(cpu, low_base);
    uint16_t low_control2 = raw_word(cpu, (uint16_t)(low_base + 2u));
    uint16_t high_control1 = raw_word(cpu, high_base);
    uint16_t high_control2 = raw_word(cpu, (uint16_t)(high_base + 2u));
    return output_compare_cascade_controls_supported(low, low_control1, low_control2) &&
           output_compare_cascade_controls_supported(high, high_control1,
                                                     high_control2) &&
           (low_control1 &
            (OUTPUT_COMPARE_TIMER_SOURCE_MASK | OUTPUT_COMPARE_MODE_MASK)) ==
               (high_control1 &
                (OUTPUT_COMPARE_TIMER_SOURCE_MASK | OUTPUT_COMPARE_MODE_MASK)) &&
           (low_control2 & OUTPUT_COMPARE_SYNC_MASK) ==
               (high_control2 & OUTPUT_COMPARE_SYNC_MASK) &&
           (low_control2 & OUTPUT_COMPARE_TRISTATE) != 0u &&
           (high_control2 & (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRISTATE)) == 0u;
}

static bool output_compare_supported(const Dspic33* cpu, uint8_t channel) {
    uint16_t base = output_compare_base(channel);
    if (output_compare_cascade_requested(cpu, channel)) {
        return output_compare_cascade_supported(cpu, channel);
    }
    return output_compare_configuration_supported(channel, raw_word(cpu, base),
                                                  raw_word(cpu, (uint16_t)(base + 2u)));
}

static bool output_compare_cascade_owner(const Dspic33* cpu, uint8_t channel) {
    return output_compare_cascade_supported(cpu, channel) &&
           channel == output_compare_pair_low(channel);
}

static bool output_compare_timer_owner(const Dspic33* cpu, uint8_t channel) {
    return !output_compare_cascade_requested(cpu, channel) ||
           output_compare_cascade_owner(cpu, channel);
}

static uint8_t output_compare_output_channel(const Dspic33* cpu, uint8_t channel) {
    return output_compare_cascade_supported(cpu, channel)
               ? output_compare_pair_high(channel)
               : channel;
}

static bool output_compare_pmd_disabled(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.output_compare.pmd_disabled & (uint16_t)(1u << channel)) != 0u;
}

static bool output_compare_operating(const Dspic33* cpu, uint8_t channel) {
    uint16_t control1 = raw_word(cpu, output_compare_base(channel));
    if (!output_compare_supported(cpu, channel) ||
        cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    if (output_compare_cascade_requested(cpu, channel)) {
        uint8_t low = output_compare_pair_low(channel);
        uint8_t high = output_compare_pair_high(channel);
        return !output_compare_pmd_disabled(cpu, low) &&
               !output_compare_pmd_disabled(cpu, high) &&
               (cpu->power_state != DSPIC33_POWER_IDLE ||
                ((raw_word(cpu, output_compare_base(low)) |
                  raw_word(cpu, output_compare_base(high))) &
                 OUTPUT_COMPARE_STOP_IDLE) == 0u);
    }
    return !output_compare_pmd_disabled(cpu, channel) &&
           (cpu->power_state != DSPIC33_POWER_IDLE ||
            (control1 & OUTPUT_COMPARE_STOP_IDLE) == 0u);
}

static bool output_compare_internal_event(const Dspic33Event* event, uint8_t channel) {
    return event->type == DSPIC33_EVENT_OUTPUT_COMPARE && event->source == channel &&
           (event->value & OUTPUT_COMPARE_EVENT_KIND_MASK) != OUTPUT_COMPARE_EVENT_PMD;
}

static void output_compare_pause_events(Dspic33* cpu, uint8_t channel) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (!output_compare_internal_event(event, channel) || event->paused) {
            continue;
        }
        event->paused_remaining = event->cycle - cpu->device_cycles;
        event->paused = true;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void output_compare_resume_events(Dspic33* cpu, uint8_t channel) {
    size_t index;
    bool changed = false;
    if (!output_compare_operating(cpu, channel)) {
        return;
    }
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (!output_compare_internal_event(event, channel) || !event->paused) {
            continue;
        }
        if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            continue;
        }
        event->cycle = cpu->device_cycles + event->paused_remaining;
        event->paused_remaining = 0u;
        event->paused = false;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void output_compare_update_power_state(Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        if (output_compare_operating(cpu, channel)) {
            output_compare_resume_events(cpu, channel);
        } else {
            output_compare_pause_events(cpu, channel);
        }
    }
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        uint16_t pending = cpu->io.output_compare.fault_interrupt_pending;
        cpu->io.output_compare.fault_interrupt_pending = 0u;
        for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
            if ((pending & (uint16_t)(1u << channel)) != 0u) {
                output_compare_raise(cpu, channel);
            }
        }
    }
}

static bool output_compare_fp_clocked(const Dspic33* cpu, uint8_t channel) {
    return (raw_word(cpu, output_compare_base(channel)) &
            OUTPUT_COMPARE_TIMER_SOURCE_MASK) == OUTPUT_COMPARE_TIMER_SOURCE_FP;
}

static uint8_t output_compare_timer_source(const Dspic33* cpu, uint8_t channel) {
    static const uint8_t timers[] = {1u, 2u, 3u, 4u, 0u};
    uint16_t selection =
        raw_word(cpu, output_compare_base(channel)) & OUTPUT_COMPARE_TIMER_SOURCE_MASK;
    return timers[selection >> 10u];
}

static void output_compare_set_high(Dspic33* cpu, uint8_t channel, bool high) {
    uint16_t bit = (uint16_t)(1u << channel);
    if (high) {
        cpu->io.output_compare.output_high |= bit;
    } else {
        cpu->io.output_compare.output_high &= (uint16_t)~bit;
    }
}

static bool output_compare_high(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.output_compare.output_high & (uint16_t)(1u << channel)) != 0u;
}

static uint16_t output_compare_fault_enable(uint8_t source) {
    return (uint16_t)(0x0080u << source);
}

static uint16_t output_compare_fault_status(uint8_t source) {
    return (uint16_t)(0x0010u << source);
}

static bool output_compare_fault_capable(const Dspic33* cpu, uint8_t channel) {
    uint16_t mode =
        raw_word(cpu, output_compare_base(channel)) & OUTPUT_COMPARE_MODE_MASK;
    return output_compare_supported(cpu, channel) &&
           !output_compare_pmd_disabled(cpu, channel) &&
           (mode == OUTPUT_COMPARE_MODE_EDGE_PWM ||
            mode == OUTPUT_COMPARE_MODE_CENTER_PWM);
}

static uint16_t output_compare_active_fault_status(const Dspic33* cpu,
                                                   uint8_t channel) {
    uint16_t control1 = raw_word(cpu, output_compare_base(channel));
    uint16_t status = 0u;
    uint8_t source;
    if (!output_compare_fault_capable(cpu, channel)) {
        return 0u;
    }
    for (source = 0u; source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT; source++) {
        uint8_t input = (uint8_t)(1u << source);
        if ((control1 & output_compare_fault_enable(source)) != 0u &&
            (cpu->io.output_compare.fault_inputs & input) == 0u) {
            status |= output_compare_fault_status(source);
        }
    }
    return status;
}

static void output_compare_enter_fault(Dspic33* cpu, uint8_t channel, uint16_t active) {
    uint16_t base = output_compare_base(channel);
    uint16_t bit = (uint16_t)(1u << channel);
    raw_write_word(cpu, base, (uint16_t)(raw_word(cpu, base) | active));
    if ((cpu->io.output_compare.fault_held & bit) != 0u) {
        return;
    }
    cpu->io.output_compare.fault_held |= bit;
    if (cpu->power_state == DSPIC33_POWER_SLEEP) {
        cpu->io.output_compare.fault_interrupt_pending |= bit;
    } else {
        output_compare_raise(cpu, channel);
    }
}

static void output_compare_refresh_fault(Dspic33* cpu, uint8_t channel) {
    uint16_t active = output_compare_active_fault_status(cpu, channel);
    uint16_t bit = (uint16_t)(1u << channel);
    if (!output_compare_fault_capable(cpu, channel)) {
        cpu->io.output_compare.fault_held &= (uint16_t)~bit;
        cpu->io.output_compare.fault_interrupt_pending &= (uint16_t)~bit;
    } else if (active != 0u) {
        output_compare_enter_fault(cpu, channel, active);
    }
}

static void output_compare_fault_boundary(Dspic33* cpu, uint8_t channel) {
    uint16_t base = output_compare_base(channel);
    uint16_t control1 = raw_word(cpu, base);
    uint16_t control2 = raw_word(cpu, (uint16_t)(base + 2u));
    uint16_t active = output_compare_active_fault_status(cpu, channel);
    uint16_t status = control1 & OUTPUT_COMPARE_FAULT_STATUS_MASK;
    uint16_t bit = (uint16_t)(1u << channel);
    if ((control2 & OUTPUT_COMPARE_FAULT_INACTIVE) == 0u) {
        status = active;
        raw_write_word(
            cpu, base,
            (uint16_t)((control1 & ~OUTPUT_COMPARE_FAULT_STATUS_MASK) | status));
    } else if (active != 0u) {
        status |= active;
        raw_write_word(cpu, base, (uint16_t)(control1 | active));
    }
    if (status == 0u) {
        cpu->io.output_compare.fault_held &= (uint16_t)~bit;
        cpu->io.output_compare.fault_interrupt_pending &= (uint16_t)~bit;
    } else {
        cpu->io.output_compare.fault_held |= bit;
    }
}

static void output_compare_set_fault_input(Dspic33* cpu, uint8_t source, bool high) {
    uint8_t channel;
    uint8_t bit = (uint8_t)(1u << source);
    if (high) {
        cpu->io.output_compare.fault_inputs |= bit;
    } else {
        cpu->io.output_compare.fault_inputs &= (uint8_t)~bit;
    }
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        output_compare_refresh_fault(cpu, channel);
    }
}

static void output_compare_fault_input(Dspic33* cpu, uint8_t source, bool high) {
    cpu->io.output_compare.fault_direct_mask |= (uint8_t)(1u << source);
    output_compare_set_fault_input(cpu, source, high);
}

static uint8_t output_compare_fault_pps_pin(const Dspic33* cpu, uint8_t source) {
    uint16_t mapping;
    if (source < 2u) {
        mapping = raw_word(cpu, OUTPUT_COMPARE_FAULT_PPS_AB);
        return source == 0u ? (uint8_t)(mapping & 0x007fu)
                            : (uint8_t)((mapping >> 8u) & 0x007fu);
    }
    return (uint8_t)(raw_word(cpu, OUTPUT_COMPARE_FAULT_PPS_C) & 0x007fu);
}

static bool output_compare_fault_selected_high(const Dspic33* cpu, uint8_t source,
                                               bool* high) {
    uint8_t selection = output_compare_fault_pps_pin(cpu, source);
    const Dspic33PpsPin* mapping;
    uint16_t bit;
    if (selection == 0u) {
        *high = false;
        return true;
    }
    if (selection <= DSPIC33_COMPARATOR_COUNT) {
        *high =
            (cpu->io.comparator.output_high & (uint8_t)(1u << (selection - 1u))) != 0u;
        return true;
    }
    mapping = pps_pin(selection);
    if (mapping == NULL || !pps_physical_input_enabled(cpu, selection)) {
        return false;
    }
    bit = (uint16_t)(1u << mapping->bit);
    *high = (gpio_pin_values(cpu, mapping->port) & bit) != 0u;
    return true;
}

static void output_compare_refresh_fault_pps_inputs(Dspic33* cpu) {
    uint8_t source;
    for (source = 0u; source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT; source++) {
        bool high;
        if ((cpu->io.output_compare.fault_direct_mask & (uint8_t)(1u << source)) ==
                0u &&
            output_compare_fault_selected_high(cpu, source, &high)) {
            output_compare_set_fault_input(cpu, source, high);
        }
    }
}

static void output_compare_fault_pin_input(Dspic33* cpu, uint8_t pin, bool high) {
    const Dspic33PpsPin* mapping = pps_pin(pin);
    uint16_t bit;
    if (mapping == NULL) {
        return;
    }
    bit = (uint16_t)(1u << mapping->bit);
    cpu->io.gpio[mapping->port] =
        (uint16_t)((cpu->io.gpio[mapping->port] & ~bit) | (high ? bit : 0u));
    cpu->io.gpio_driven[mapping->port] |= bit;
    refresh_gpio_change_notification(cpu);
    refresh_external_interrupts(cpu);
    output_compare_refresh_fault_pps_inputs(cpu);
}

static void output_compare_abort(Dspic33* cpu, uint8_t channel) {
    if (output_compare_cascade_owner(cpu, channel)) {
        uint8_t low = output_compare_pair_low(channel);
        uint8_t high = output_compare_pair_high(channel);
        uint16_t pair_bits = (uint16_t)((1u << low) | (1u << high));
        uint8_t member;
        for (member = low; member <= high; member++) {
            uint16_t member_base = output_compare_base(member);
            cpu->io.output_compare.generation[member]++;
            cpu->io.output_compare.timer_generation[member]++;
            raw_write_word(
                cpu, member_base,
                (uint16_t)(raw_word(cpu, member_base) & ~OUTPUT_COMPARE_MODE_MASK));
            raw_write_word(cpu, (uint16_t)(member_base + 8u), 0u);
            output_compare_set_high(cpu, member, false);
            cpu->io.output_compare.phase[member] = 0u;
            cpu->io.output_compare.sync_emitted[member] = false;
            cpu->io.output_compare.activation_cycle[member] = 0u;
        }
        cpu->io.output_compare.sync_reset_pending &= (uint16_t)~pair_bits;
        cpu->io.output_compare.activation_pending &= (uint16_t)~pair_bits;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    uint16_t base = output_compare_base(channel);
    uint16_t bit = (uint16_t)(1u << channel);
    cpu->io.output_compare.generation[channel]++;
    cpu->io.output_compare.timer_generation[channel]++;
    raw_write_word(cpu, base,
                   (uint16_t)(raw_word(cpu, base) & ~OUTPUT_COMPARE_MODE_MASK));
    raw_write_word(cpu, (uint16_t)(base + 8u), 0u);
    output_compare_set_high(cpu, channel, false);
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~bit;
    cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
    cpu->io.output_compare.phase[channel] = 0u;
    cpu->io.output_compare.sync_emitted[channel] = false;
    cpu->io.output_compare.activation_cycle[channel] = 0u;
    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
}

static bool output_compare_schedule(Dspic33* cpu, uint8_t channel, uint32_t kind,
                                    uint64_t delay) {
    bool timer_event = kind == OUTPUT_COMPARE_EVENT_PRIMARY ||
                       kind == OUTPUT_COMPARE_EVENT_SECONDARY ||
                       kind == OUTPUT_COMPARE_EVENT_BOUNDARY ||
                       kind == OUTPUT_COMPARE_EVENT_SYNC ||
                       kind == OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY ||
                       kind == OUTPUT_COMPARE_EVENT_SYNC_PRIMARY ||
                       kind == OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC;
    uint16_t generation = timer_event ? cpu->io.output_compare.timer_generation[channel]
                                      : cpu->io.output_compare.generation[channel];
    uint32_t value =
        kind | ((uint32_t)generation << OUTPUT_COMPARE_EVENT_GENERATION_SHIFT);
    if (dspic33_schedule(cpu, DSPIC33_EVENT_OUTPUT_COMPARE, channel, value, delay)) {
        return true;
    }
    output_compare_abort(cpu, channel);
    return false;
}

static bool output_compare_self_synchronized(const Dspic33* cpu, uint8_t channel) {
    uint16_t control2 = raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
    uint16_t source = control2 & OUTPUT_COMPARE_SYNC_MASK;
    return (control2 & OUTPUT_COMPARE_TRIGGER) == 0u &&
           (source == OUTPUT_COMPARE_SYNC_SELF ||
            (channel < 9u && source == OUTPUT_COMPARE_SYNC_OC_FIRST + channel));
}

static bool output_compare_internal_period(const Dspic33* cpu, uint8_t channel) {
    return output_compare_self_synchronized(cpu, channel) ||
           (raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u)) &
            OUTPUT_COMPARE_TRIGGER) != 0u;
}

static void output_compare_pulse_source(Dspic33* cpu, uint8_t source);

static uint32_t output_compare_cascade_timer(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    return (uint32_t)raw_word(cpu, (uint16_t)(output_compare_base(low) + 8u)) |
           ((uint32_t)raw_word(cpu, (uint16_t)(output_compare_base(high) + 8u)) << 16u);
}

static void output_compare_write_cascade_timer(Dspic33* cpu, uint8_t channel,
                                               uint32_t timer) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    raw_write_word(cpu, (uint16_t)(output_compare_base(low) + 8u), (uint16_t)timer);
    raw_write_word(cpu, (uint16_t)(output_compare_base(high) + 8u),
                   (uint16_t)(timer >> 16u));
}

static uint32_t output_compare_input_capture_timer(const Dspic33* cpu, uint8_t source) {
    uint8_t channel = (uint8_t)(source - OUTPUT_COMPARE_SYNC_IC_FIRST);
    uint32_t timer = cpu->io.input_capture.timer[channel];
    if ((channel & 1u) == 0u && input_capture_pair_configured(cpu, channel)) {
        timer |= (uint32_t)cpu->io.input_capture.timer[channel + 1u] << 16u;
    }
    return timer;
}

static void output_compare_adopt_input_capture_timer(Dspic33* cpu, uint8_t channel,
                                                     uint8_t source) {
    uint32_t timer = output_compare_input_capture_timer(cpu, source);
    if (output_compare_cascade_owner(cpu, channel)) {
        output_compare_write_cascade_timer(cpu, channel, timer);
    } else {
        raw_write_word(cpu, (uint16_t)(output_compare_base(channel) + 8u),
                       (uint16_t)timer);
    }
}

static uint32_t output_compare_cascade_r(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    return (uint32_t)cpu->io.output_compare.active_r[low] |
           ((uint32_t)cpu->io.output_compare.active_r[high] << 16u);
}

static uint32_t output_compare_cascade_rs(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    return (uint32_t)cpu->io.output_compare.active_rs[low] |
           ((uint32_t)cpu->io.output_compare.active_rs[high] << 16u);
}

static bool output_compare_cascade_pwm_mode(uint16_t mode) {
    return mode == OUTPUT_COMPARE_MODE_EDGE_PWM ||
           mode == OUTPUT_COMPARE_MODE_CENTER_PWM;
}

static bool output_compare_cascade_pwm_degenerate(const Dspic33* cpu, uint8_t channel,
                                                  uint16_t mode) {
    uint8_t high = output_compare_pair_high(channel);
    return output_compare_cascade_pwm_mode(mode) &&
           (cpu->io.output_compare.active_r[high] == 0u ||
            cpu->io.output_compare.active_rs[high] == 0u);
}

static uint64_t output_compare_boundary_delay(const Dspic33* cpu, uint8_t channel) {
    if (output_compare_cascade_owner(cpu, channel)) {
        uint8_t high = output_compare_pair_high(channel);
        uint16_t mode =
            raw_word(cpu, output_compare_base(channel)) & OUTPUT_COMPARE_MODE_MASK;
        uint32_t timer = output_compare_cascade_timer(cpu, channel);
        uint64_t delay = UINT64_C(0x100000000) - timer;
        uint32_t rs = output_compare_cascade_rs(cpu, channel);
        if (output_compare_cascade_pwm_degenerate(cpu, channel, mode)) {
            uint16_t high_r = cpu->io.output_compare.active_r[high];
            uint16_t high_rs = cpu->io.output_compare.active_rs[high];
            uint16_t high_timer = (uint16_t)(timer >> 16u);
            uint16_t low_timer = (uint16_t)timer;
            if (high_r == 0u && high_rs == 0u &&
                cpu->io.output_compare.phase[channel] == 1u && high_timer == 0u &&
                low_timer <= cpu->io.output_compare.active_rs[channel]) {
                return (uint32_t)cpu->io.output_compare.active_rs[channel] + 1u -
                       low_timer;
            }
            if (high_r == 0u && high_rs != 0u && high_timer < high_rs) {
                return ((uint64_t)(high_rs - high_timer) << 16u) - low_timer;
            }
            return delay;
        }
        if (output_compare_internal_period(cpu, channel) && timer <= rs) {
            delay = (uint64_t)rs + 1u - timer;
        }
        return delay;
    }
    uint16_t base = output_compare_base(channel);
    uint16_t timer = raw_word(cpu, (uint16_t)(base + 8u));
    uint64_t delay = UINT32_C(0x10000) - timer;
    if (output_compare_internal_period(cpu, channel) &&
        timer <= cpu->io.output_compare.active_rs[channel]) {
        delay = (uint32_t)cpu->io.output_compare.active_rs[channel] + 1u - timer;
    }
    return delay;
}

static uint64_t output_compare_next_timer_event(const Dspic33* cpu, uint8_t channel,
                                                uint32_t* kind) {
    uint16_t base = output_compare_base(channel);
    uint16_t mode = raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK;
    uint16_t r = cpu->io.output_compare.active_r[channel];
    uint16_t rs = cpu->io.output_compare.active_rs[channel];
    uint16_t timer = raw_word(cpu, (uint16_t)(base + 8u));
    uint64_t boundary = output_compare_boundary_delay(cpu, channel);
    uint32_t target = r;
    uint32_t next_kind = OUTPUT_COMPARE_EVENT_PRIMARY;
    uint64_t delay = boundary;
    bool eligible = false;
    if ((cpu->io.output_compare.sync_reset_pending & (uint16_t)(1u << channel)) != 0u) {
        *kind = OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC;
        return 1u;
    }
    if ((raw_word(cpu, (uint16_t)(base + 2u)) &
         (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRIGGER_STATUS)) ==
        OUTPUT_COMPARE_TRIGGER) {
        *kind = OUTPUT_COMPARE_EVENT_BOUNDARY;
        return UINT64_MAX;
    }
    if (output_compare_cascade_owner(cpu, channel)) {
        uint32_t cascade_r = output_compare_cascade_r(cpu, channel);
        uint32_t cascade_rs = output_compare_cascade_rs(cpu, channel);
        uint32_t cascade_timer = output_compare_cascade_timer(cpu, channel);
        target = cascade_r;
        if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM) {
            eligible = cascade_r != 0u && cascade_timer < cascade_r &&
                       (output_compare_cascade_pwm_degenerate(cpu, channel, mode) ||
                        !output_compare_internal_period(cpu, channel) ||
                        cascade_r < cascade_rs);
        } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_HIGH ||
                   mode == OUTPUT_COMPARE_MODE_SINGLE_LOW ||
                   mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
            eligible = cpu->io.output_compare.phase[channel] == 0u &&
                       cascade_timer < cascade_r;
        } else if (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ||
                   mode == OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS ||
                   mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
            if (cpu->io.output_compare.phase[channel] == 0u) {
                eligible = cascade_timer < cascade_r;
            } else if (cpu->io.output_compare.phase[channel] == 1u &&
                       !output_compare_cascade_pwm_degenerate(cpu, channel, mode)) {
                target = cascade_rs;
                next_kind = OUTPUT_COMPARE_EVENT_SECONDARY;
                eligible = cascade_timer < cascade_rs;
            }
        }
        if (eligible && (uint64_t)(target - cascade_timer) < delay) {
            delay = target - cascade_timer;
        } else {
            next_kind = OUTPUT_COMPARE_EVENT_BOUNDARY;
        }
        if (!output_compare_internal_period(cpu, channel) &&
            !cpu->io.output_compare.sync_emitted[channel] &&
            cascade_timer <= cascade_rs) {
            uint64_t sync_delay = (uint64_t)cascade_rs + 1u - cascade_timer;
            if (sync_delay < delay) {
                next_kind = OUTPUT_COMPARE_EVENT_SYNC;
                delay = sync_delay;
            } else if (sync_delay == delay &&
                       next_kind == OUTPUT_COMPARE_EVENT_PRIMARY) {
                next_kind = OUTPUT_COMPARE_EVENT_SYNC_PRIMARY;
            } else if (sync_delay == delay &&
                       next_kind == OUTPUT_COMPARE_EVENT_BOUNDARY) {
                next_kind = OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY;
            }
        }
        *kind = next_kind;
        return delay;
    }
    if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM) {
        eligible = r != 0u && timer < r;
        if (output_compare_internal_period(cpu, channel)) {
            eligible = eligible && r < rs;
        }
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_HIGH ||
               mode == OUTPUT_COMPARE_MODE_SINGLE_LOW) {
        eligible = cpu->io.output_compare.phase[channel] == 0u && timer < r;
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
        eligible = cpu->io.output_compare.phase[channel] == 0u && timer < r;
    } else if (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ||
               mode == OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS ||
               mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
        if (cpu->io.output_compare.phase[channel] == 0u) {
            eligible = timer < r;
        } else if (cpu->io.output_compare.phase[channel] == 1u) {
            target = rs;
            next_kind = OUTPUT_COMPARE_EVENT_SECONDARY;
            eligible = timer < rs;
        }
    }
    if (eligible && (uint32_t)((uint16_t)target - timer) < delay) {
        delay = (uint32_t)((uint16_t)target - timer);
    } else {
        next_kind = OUTPUT_COMPARE_EVENT_BOUNDARY;
    }
    if (!output_compare_internal_period(cpu, channel) &&
        !cpu->io.output_compare.sync_emitted[channel] && timer <= rs) {
        uint32_t sync_delay = (uint32_t)rs + 1u - timer;
        if (sync_delay < delay) {
            next_kind = OUTPUT_COMPARE_EVENT_SYNC;
            delay = sync_delay;
        } else if (sync_delay == delay && next_kind == OUTPUT_COMPARE_EVENT_PRIMARY) {
            next_kind = OUTPUT_COMPARE_EVENT_SYNC_PRIMARY;
        } else if (sync_delay == delay && next_kind == OUTPUT_COMPARE_EVENT_BOUNDARY) {
            next_kind = OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY;
        }
    }
    *kind = next_kind;
    return delay;
}

static bool output_compare_schedule_next(Dspic33* cpu, uint8_t channel,
                                         uint64_t initial_delay) {
    uint32_t kind;
    uint64_t delay;
    if (!output_compare_fp_clocked(cpu, channel)) {
        return true;
    }
    delay = output_compare_next_timer_event(cpu, channel, &kind);
    return delay == UINT64_MAX ||
           output_compare_schedule(cpu, channel, kind, initial_delay + delay);
}

static void output_compare_start(Dspic33* cpu, uint8_t channel) {
    uint16_t base = output_compare_base(channel);
    uint16_t bit = (uint16_t)(1u << channel);
    uint64_t activation_delay =
        cpu->instruction_active
            ? dspic33_device_instruction_cycles(cpu, cpu->current_instruction_cycles)
            : 0u;
    cpu->io.output_compare.generation[channel]++;
    cpu->io.output_compare.timer_generation[channel]++;
    cpu->io.output_compare.active_rs[channel] = raw_word(cpu, (uint16_t)(base + 4u));
    cpu->io.output_compare.active_r[channel] = raw_word(cpu, (uint16_t)(base + 6u));
    cpu->io.output_compare.phase[channel] = 0u;
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~bit;
    if (cpu->instruction_active) {
        cpu->io.output_compare.activation_pending |= bit;
    } else {
        cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
    }
    cpu->io.output_compare.sync_emitted[channel] = false;
    cpu->io.output_compare.activation_cycle[channel] =
        cpu->device_cycles + activation_delay;
    raw_write_word(cpu, (uint16_t)(base + 8u), 0u);
    output_compare_set_high(cpu, channel,
                            (raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK) ==
                                    OUTPUT_COMPARE_MODE_SINGLE_LOW ||
                                ((raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK) ==
                                     OUTPUT_COMPARE_MODE_EDGE_PWM &&
                                 cpu->io.output_compare.active_r[channel] != 0u));
    output_compare_schedule_next(cpu, channel, activation_delay);
}

static void output_compare_start_cascade(Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    uint16_t low_base = output_compare_base(low);
    uint16_t high_base = output_compare_base(high);
    uint16_t pair_bits = (uint16_t)((1u << low) | (1u << high));
    uint64_t activation_delay =
        cpu->instruction_active
            ? dspic33_device_instruction_cycles(cpu, cpu->current_instruction_cycles)
            : 0u;
    cpu->io.output_compare.generation[low]++;
    cpu->io.output_compare.generation[high]++;
    cpu->io.output_compare.timer_generation[low]++;
    cpu->io.output_compare.timer_generation[high]++;
    cpu->io.output_compare.active_rs[low] = raw_word(cpu, (uint16_t)(low_base + 4u));
    cpu->io.output_compare.active_rs[high] = raw_word(cpu, (uint16_t)(high_base + 4u));
    cpu->io.output_compare.active_r[low] = raw_word(cpu, (uint16_t)(low_base + 6u));
    cpu->io.output_compare.active_r[high] = raw_word(cpu, (uint16_t)(high_base + 6u));
    cpu->io.output_compare.phase[low] = 0u;
    cpu->io.output_compare.phase[high] = 0u;
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~pair_bits;
    if (cpu->instruction_active) {
        cpu->io.output_compare.activation_pending |= pair_bits;
    } else {
        cpu->io.output_compare.activation_pending &= (uint16_t)~pair_bits;
    }
    cpu->io.output_compare.sync_emitted[low] = false;
    cpu->io.output_compare.sync_emitted[high] = false;
    cpu->io.output_compare.activation_cycle[low] =
        cpu->device_cycles + activation_delay;
    cpu->io.output_compare.activation_cycle[high] =
        cpu->device_cycles + activation_delay;
    output_compare_write_cascade_timer(cpu, low, 0u);
    output_compare_set_high(cpu, low, false);
    output_compare_set_high(cpu, high,
                            (raw_word(cpu, low_base) & OUTPUT_COMPARE_MODE_MASK) ==
                                OUTPUT_COMPARE_MODE_SINGLE_LOW);
    output_compare_schedule_next(cpu, low, activation_delay);
}

static void output_compare_stop(Dspic33* cpu, uint8_t channel) {
    uint16_t bit = (uint16_t)(1u << channel);
    cpu->io.output_compare.generation[channel]++;
    cpu->io.output_compare.timer_generation[channel]++;
    raw_write_word(cpu, (uint16_t)(output_compare_base(channel) + 8u), 0u);
    output_compare_set_high(cpu, channel, false);
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~bit;
    cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
    cpu->io.output_compare.phase[channel] = 0u;
    cpu->io.output_compare.sync_emitted[channel] = false;
    cpu->io.output_compare.activation_cycle[channel] = 0u;
    if (channel < 9u) {
        output_compare_pulse_source(cpu,
                                    (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + channel));
    }
}

static void output_compare_stop_cascade(Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    uint16_t pair_bits = (uint16_t)((1u << low) | (1u << high));
    cpu->io.output_compare.generation[low]++;
    cpu->io.output_compare.generation[high]++;
    cpu->io.output_compare.timer_generation[low]++;
    cpu->io.output_compare.timer_generation[high]++;
    output_compare_write_cascade_timer(cpu, low, 0u);
    output_compare_set_high(cpu, low, false);
    output_compare_set_high(cpu, high, false);
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~pair_bits;
    cpu->io.output_compare.activation_pending &= (uint16_t)~pair_bits;
    cpu->io.output_compare.phase[low] = 0u;
    cpu->io.output_compare.phase[high] = 0u;
    cpu->io.output_compare.sync_emitted[low] = false;
    cpu->io.output_compare.sync_emitted[high] = false;
    cpu->io.output_compare.activation_cycle[low] = 0u;
    cpu->io.output_compare.activation_cycle[high] = 0u;
    if (low < 9u) {
        output_compare_pulse_source(cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + low));
    }
}

static void output_compare_raise(Dspic33* cpu, uint8_t channel) {
    uint8_t output = output_compare_output_channel(cpu, channel);
    if (output < 4u) {
        dspic33_dma_request(cpu, output_compare_irqs[output], 0u, 0u);
    }
    dspic33_raise_interrupt(cpu, output_compare_irqs[output]);
}

static void output_compare_pulse_source(Dspic33* cpu, uint8_t source) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = output_compare_base(channel);
        uint16_t control2 = raw_word(cpu, (uint16_t)(base + 2u));
        uint16_t bit = (uint16_t)(1u << channel);
        if (!output_compare_timer_owner(cpu, channel) ||
            !output_compare_operating(cpu, channel) ||
            (control2 & OUTPUT_COMPARE_SYNC_MASK) != source ||
            output_compare_self_synchronized(cpu, channel)) {
            continue;
        }
        if ((control2 & OUTPUT_COMPARE_TRIGGER) != 0u) {
            if ((control2 & OUTPUT_COMPARE_TRIGGER_STATUS) != 0u) {
                continue;
            }
            raw_write_word(cpu, (uint16_t)(base + 2u),
                           (uint16_t)(control2 | OUTPUT_COMPARE_TRIGGER_STATUS));
        } else {
            if ((cpu->io.output_compare.sync_reset_pending & bit) != 0u) {
                continue;
            }
            cpu->io.output_compare.sync_reset_pending |= bit;
        }
        cpu->io.output_compare.timer_generation[channel]++;
        output_compare_schedule_next(cpu, channel, 0u);
    }
}

static bool output_compare_source_awaited(const Dspic33* cpu, uint8_t source) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t control2 =
            raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
        if (output_compare_timer_owner(cpu, channel) &&
            output_compare_operating(cpu, channel) &&
            (control2 & OUTPUT_COMPARE_SYNC_MASK) == source &&
            ((control2 & OUTPUT_COMPARE_TRIGGER) == 0u ||
             (control2 & OUTPUT_COMPARE_TRIGGER_STATUS) == 0u)) {
            return true;
        }
    }
    return false;
}

static void output_compare_pulse_sync_source(Dspic33* cpu, uint8_t channel) {
    if (channel < 9u) {
        uint8_t source = (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + channel);
        input_capture_pulse_source(cpu, source);
        if (cpu->io.output_compare.clock_advancing) {
            cpu->io.output_compare.deferred_sync_pulses |= (uint16_t)(1u << channel);
        } else {
            output_compare_pulse_source(cpu, source);
        }
    }
}

static bool output_compare_primary_match(Dspic33* cpu, uint8_t channel, uint16_t mode) {
    uint8_t output = output_compare_output_channel(cpu, channel);
    if (mode == OUTPUT_COMPARE_MODE_SINGLE_HIGH) {
        cpu->io.output_compare.phase[channel] = 2u;
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_LOW) {
        cpu->io.output_compare.phase[channel] = 2u;
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
        cpu->io.output_compare.phase[channel] = 1u;
    } else if (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ||
               mode == OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS ||
               mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
        cpu->io.output_compare.phase[channel] = 1u;
    } else if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM) {
        if (output_compare_cascade_owner(cpu, channel) &&
            output_compare_cascade_pwm_degenerate(cpu, channel, mode)) {
            cpu->io.output_compare.phase[channel] = 1u;
        }
        output_compare_set_high(cpu, output,
                                output_compare_cascade_owner(cpu, channel));
        return true;
    }
    if (mode <= OUTPUT_COMPARE_MODE_SINGLE_TOGGLE &&
        !output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_INTERRUPT, 3u)) {
        return false;
    }
    return output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_APPLY_PRIMARY,
                                   1u);
}

static bool output_compare_secondary_match(Dspic33* cpu, uint8_t channel,
                                           uint16_t mode) {
    cpu->io.output_compare.phase[channel] =
        mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ? 2u : 0u;
    return output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_INTERRUPT, 3u) &&
           output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_APPLY_SECONDARY,
                                   1u);
}

static bool output_compare_boundary(Dspic33* cpu, uint8_t channel, uint16_t mode) {
    uint16_t base = output_compare_base(channel);
    uint8_t output = output_compare_output_channel(cpu, channel);
    bool cascade = output_compare_cascade_owner(cpu, channel);
    bool degenerate =
        cascade && output_compare_cascade_pwm_degenerate(cpu, channel, mode);
    bool first_low_period = degenerate &&
                            cpu->io.output_compare.active_r[output] == 0u &&
                            cpu->io.output_compare.active_rs[output] == 0u &&
                            cpu->io.output_compare.phase[channel] == 1u &&
                            (output_compare_cascade_timer(cpu, channel) >> 16u) == 0u;
    if (cascade) {
        output_compare_write_cascade_timer(cpu, channel,
                                           first_low_period ? UINT32_C(0x10000) : 0u);
    } else {
        raw_write_word(cpu, (uint16_t)(base + 8u), 0u);
    }
    cpu->io.output_compare.sync_emitted[channel] = false;
    if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM ||
        mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
        if (cascade) {
            uint8_t high = output_compare_pair_high(channel);
            uint16_t high_base = output_compare_base(high);
            cpu->io.output_compare.active_rs[channel] =
                raw_word(cpu, (uint16_t)(base + 4u));
            cpu->io.output_compare.active_rs[high] =
                raw_word(cpu, (uint16_t)(high_base + 4u));
            cpu->io.output_compare.active_r[channel] =
                raw_word(cpu, (uint16_t)(base + 6u));
            cpu->io.output_compare.active_r[high] =
                raw_word(cpu, (uint16_t)(high_base + 6u));
        } else {
            cpu->io.output_compare.active_rs[channel] =
                raw_word(cpu, (uint16_t)(base + 4u));
            cpu->io.output_compare.active_r[channel] =
                raw_word(cpu, (uint16_t)(base + 6u));
        }
    }
    if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM) {
        output_compare_set_high(
            cpu, output,
            degenerate || (!cascade && cpu->io.output_compare.active_r[channel] != 0u));
        output_compare_raise(cpu, channel);
        if (degenerate) {
            cpu->io.output_compare.phase[channel] = 0u;
        }
    } else if (mode == OUTPUT_COMPARE_MODE_CENTER_PWM && degenerate) {
        output_compare_set_high(cpu, output, true);
        cpu->io.output_compare.phase[channel] = 0u;
        output_compare_raise(cpu, channel);
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
        cpu->io.output_compare.phase[channel] = 0u;
        if ((output_compare_cascade_owner(cpu, channel)
                 ? output_compare_cascade_r(cpu, channel) == 0u
                 : cpu->io.output_compare.active_r[channel] == 0u) &&
            !output_compare_self_synchronized(cpu, channel) &&
            !output_compare_primary_match(cpu, channel, mode)) {
            return false;
        }
    } else if ((mode == OUTPUT_COMPARE_MODE_SINGLE_HIGH ||
                mode == OUTPUT_COMPARE_MODE_SINGLE_LOW) &&
               cpu->io.output_compare.phase[channel] == 0u &&
               (output_compare_cascade_owner(cpu, channel)
                    ? output_compare_cascade_r(cpu, channel) == 0u
                    : cpu->io.output_compare.active_r[channel] == 0u) &&
               !output_compare_self_synchronized(cpu, channel) &&
               !output_compare_primary_match(cpu, channel, mode)) {
        return false;
    } else if (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ||
               mode == OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS ||
               mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
        uint32_t r = output_compare_cascade_owner(cpu, channel)
                         ? output_compare_cascade_r(cpu, channel)
                         : cpu->io.output_compare.active_r[channel];
        uint32_t rs = output_compare_cascade_owner(cpu, channel)
                          ? output_compare_cascade_rs(cpu, channel)
                          : cpu->io.output_compare.active_rs[channel];
        if (r == 0u && rs == 0u) {
            output_compare_set_high(cpu, output, false);
            cpu->io.output_compare.phase[channel] = 0u;
        } else if (cpu->io.output_compare.phase[channel] == 1u && rs == 0u) {
            if (!output_compare_secondary_match(cpu, channel, mode)) {
                return false;
            }
        } else if (cpu->io.output_compare.phase[channel] == 0u && r == 0u) {
            if (!output_compare_primary_match(cpu, channel, mode)) {
                return false;
            }
        }
    }
    if (output_compare_internal_period(cpu, channel)) {
        output_compare_pulse_sync_source(cpu, channel);
    }
    if ((raw_word(cpu, base) & OUTPUT_COMPARE_TRIGGER_ONESHOT) != 0u &&
        (raw_word(cpu, (uint16_t)(base + 2u)) & OUTPUT_COMPARE_TRIGGER) != 0u) {
        raw_write_word(cpu, (uint16_t)(base + 2u),
                       (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) &
                                  ~OUTPUT_COMPARE_TRIGGER_STATUS));
    }
    output_compare_fault_boundary(cpu, channel);
    if (cascade) {
        output_compare_fault_boundary(cpu, output_compare_pair_high(channel));
    }
    return true;
}

static void run_output_compare(Dspic33* cpu, uint16_t source, uint32_t value) {
    uint16_t generation;
    uint8_t channel;
    uint32_t kind;
    bool timer_event;
    if (source >= DSPIC33_OUTPUT_COMPARE_COUNT) {
        return;
    }
    channel = (uint8_t)source;
    kind = value & OUTPUT_COMPARE_EVENT_KIND_MASK;
    if (kind == OUTPUT_COMPARE_EVENT_PMD) {
        uint16_t generation = (uint16_t)((value & ~OUTPUT_COMPARE_EVENT_PMD_DISABLED) >>
                                         OUTPUT_COMPARE_EVENT_PMD_GENERATION_SHIFT);
        uint16_t bit = (uint16_t)(1u << channel);
        if (generation != cpu->io.output_compare.pmd_generation[channel]) {
            return;
        }
        if ((value & OUTPUT_COMPARE_EVENT_PMD_DISABLED) != 0u) {
            cpu->io.output_compare.pmd_disabled |= bit;
        } else {
            cpu->io.output_compare.pmd_disabled &= (uint16_t)~bit;
        }
        output_compare_refresh_fault(cpu, channel);
        if (output_compare_cascade_requested(cpu, channel)) {
            output_compare_refresh_fault(cpu, output_compare_pair_low(channel));
            output_compare_refresh_fault(cpu, output_compare_pair_high(channel));
        }
        output_compare_update_power_state(cpu);
        return;
    }
    timer_event = kind == OUTPUT_COMPARE_EVENT_PRIMARY ||
                  kind == OUTPUT_COMPARE_EVENT_SECONDARY ||
                  kind == OUTPUT_COMPARE_EVENT_BOUNDARY ||
                  kind == OUTPUT_COMPARE_EVENT_SYNC ||
                  kind == OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY ||
                  kind == OUTPUT_COMPARE_EVENT_SYNC_PRIMARY ||
                  kind == OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC;
    generation = (uint16_t)(value >> OUTPUT_COMPARE_EVENT_GENERATION_SHIFT);
    if (generation != (timer_event ? cpu->io.output_compare.timer_generation[channel]
                                   : cpu->io.output_compare.generation[channel]) ||
        !output_compare_operating(cpu, channel)) {
        return;
    }
    {
        uint16_t base = output_compare_base(channel);
        uint16_t mode = raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK;
        if (kind == OUTPUT_COMPARE_EVENT_PRIMARY) {
            if (!output_compare_primary_match(cpu, channel, mode)) {
                return;
            }
        } else if (kind == OUTPUT_COMPARE_EVENT_SECONDARY) {
            if (!output_compare_secondary_match(cpu, channel, mode)) {
                return;
            }
        } else if (kind == OUTPUT_COMPARE_EVENT_BOUNDARY) {
            if (!output_compare_boundary(cpu, channel, mode)) {
                return;
            }
        } else if (kind == OUTPUT_COMPARE_EVENT_SYNC) {
            cpu->io.output_compare.sync_emitted[channel] = true;
            output_compare_pulse_sync_source(cpu, channel);
        } else if (kind == OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY) {
            if (!output_compare_boundary(cpu, channel, mode)) {
                return;
            }
            output_compare_pulse_sync_source(cpu, channel);
        } else if (kind == OUTPUT_COMPARE_EVENT_SYNC_PRIMARY) {
            cpu->io.output_compare.sync_emitted[channel] = true;
            output_compare_pulse_sync_source(cpu, channel);
            if (!output_compare_primary_match(cpu, channel, mode)) {
                return;
            }
        } else if (kind == OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC) {
            uint8_t synchronization = (uint8_t)(raw_word(cpu, (uint16_t)(base + 2u)) &
                                                OUTPUT_COMPARE_SYNC_MASK);
            cpu->io.output_compare.sync_reset_pending &=
                (uint16_t)~(uint16_t)(1u << channel);
            if (!output_compare_boundary(cpu, channel, mode)) {
                return;
            }
            if (synchronization >= OUTPUT_COMPARE_SYNC_IC_FIRST &&
                synchronization < OUTPUT_COMPARE_SYNC_COMPARATOR_FIRST) {
                output_compare_adopt_input_capture_timer(cpu, channel, synchronization);
            }
        } else if (kind == OUTPUT_COMPARE_EVENT_APPLY_PRIMARY) {
            uint8_t output = output_compare_output_channel(cpu, channel);
            if (mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
                output_compare_set_high(cpu, output, !output_compare_high(cpu, output));
            } else {
                output_compare_set_high(cpu, output,
                                        mode != OUTPUT_COMPARE_MODE_SINGLE_LOW);
            }
            return;
        } else if (kind == OUTPUT_COMPARE_EVENT_APPLY_SECONDARY) {
            output_compare_set_high(cpu, output_compare_output_channel(cpu, channel),
                                    false);
            return;
        } else {
            output_compare_raise(cpu, channel);
            return;
        }
        output_compare_schedule_next(cpu, channel, 0u);
    }
}

static uint16_t output_compare_previous_word(const Dspic33* cpu,
                                             uint16_t changed_address,
                                             uint16_t previous, uint16_t address) {
    return address == changed_address ? previous : raw_word(cpu, address);
}

static bool output_compare_cascade_supported_before(const Dspic33* cpu, uint8_t channel,
                                                    uint16_t changed_address,
                                                    uint16_t previous) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    uint16_t low_base = output_compare_base(low);
    uint16_t high_base = output_compare_base(high);
    uint16_t low_control1 =
        output_compare_previous_word(cpu, changed_address, previous, low_base);
    uint16_t low_control2 = output_compare_previous_word(cpu, changed_address, previous,
                                                         (uint16_t)(low_base + 2u));
    uint16_t high_control1 =
        output_compare_previous_word(cpu, changed_address, previous, high_base);
    uint16_t high_control2 = output_compare_previous_word(
        cpu, changed_address, previous, (uint16_t)(high_base + 2u));
    return output_compare_cascade_controls_supported(low, low_control1, low_control2) &&
           output_compare_cascade_controls_supported(high, high_control1,
                                                     high_control2) &&
           (low_control1 &
            (OUTPUT_COMPARE_TIMER_SOURCE_MASK | OUTPUT_COMPARE_MODE_MASK)) ==
               (high_control1 &
                (OUTPUT_COMPARE_TIMER_SOURCE_MASK | OUTPUT_COMPARE_MODE_MASK)) &&
           (low_control2 & OUTPUT_COMPARE_SYNC_MASK) ==
               (high_control2 & OUTPUT_COMPARE_SYNC_MASK) &&
           (low_control2 & OUTPUT_COMPARE_TRISTATE) != 0u &&
           (high_control2 & (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRISTATE)) == 0u;
}

static bool output_compare_update_cascade(Dspic33* cpu, uint8_t channel,
                                          uint16_t address, uint16_t previous) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    uint16_t low_base = output_compare_base(low);
    uint16_t high_base = output_compare_base(high);
    uint16_t offset = (uint16_t)(address - output_compare_base(channel));
    bool was_supported =
        output_compare_cascade_supported_before(cpu, channel, address, previous);
    bool is_supported = output_compare_cascade_supported(cpu, channel);
    bool cascade_involved = was_supported || is_supported ||
                            output_compare_cascade_requested(cpu, low) ||
                            output_compare_cascade_requested(cpu, high) ||
                            (offset == 2u && (previous & OUTPUT_COMPARE_32_BIT) != 0u);
    if (!cascade_involved) {
        return false;
    }
    if (!was_supported && is_supported) {
        output_compare_start_cascade(cpu, low);
    } else if (was_supported && !is_supported) {
        output_compare_stop_cascade(cpu, low);
    } else if (was_supported && is_supported) {
        uint16_t mode = raw_word(cpu, low_base) & OUTPUT_COMPARE_MODE_MASK;
        if (offset == 0u || offset == 2u) {
            uint16_t changed = (uint16_t)(raw_word(cpu, address) ^ previous);
            if ((offset == 0u &&
                 (changed &
                  ~(OUTPUT_COMPARE_STOP_IDLE | OUTPUT_COMPARE_FAULT_ENABLE_MASK |
                    OUTPUT_COMPARE_FAULT_STATUS_MASK)) == 0u) ||
                (offset == 2u &&
                 (changed &
                  ~(OUTPUT_COMPARE_INVERT | OUTPUT_COMPARE_TRISTATE |
                    OUTPUT_COMPARE_FAULT_INACTIVE | OUTPUT_COMPARE_FAULT_OUTPUT |
                    OUTPUT_COMPARE_FAULT_TRISTATE)) == 0u)) {
                output_compare_update_power_state(cpu);
            } else {
                output_compare_start_cascade(cpu, low);
            }
        } else if (offset == 4u || offset == 6u) {
            if (mode != OUTPUT_COMPARE_MODE_EDGE_PWM &&
                mode != OUTPUT_COMPARE_MODE_CENTER_PWM) {
                uint16_t value = raw_word(cpu, address);
                if (offset == 4u) {
                    cpu->io.output_compare.active_rs[channel] = value;
                    cpu->io.output_compare.sync_emitted[low] = false;
                } else {
                    cpu->io.output_compare.active_r[channel] = value;
                }
                cpu->io.output_compare.timer_generation[low]++;
                output_compare_schedule_next(cpu, low, 0u);
            }
        }
    }
    if (was_supported && !is_supported) {
        uint16_t control1 = raw_word(cpu, output_compare_base(channel));
        uint16_t control2 =
            raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
        if (output_compare_configuration_supported(channel, control1, control2)) {
            output_compare_start(cpu, channel);
        }
    }
    if (address == low_base || address == high_base) {
        output_compare_update_power_state(cpu);
    }
    output_compare_refresh_fault(cpu, low);
    output_compare_refresh_fault(cpu, high);
    return true;
}

static void update_output_compare_register(Dspic33* cpu, uint16_t address,
                                           uint16_t previous) {
    uint16_t base;
    uint16_t control1;
    uint16_t control2;
    uint16_t previous1;
    uint16_t previous2;
    uint16_t offset;
    uint8_t channel;
    bool was_supported;
    bool is_supported;
    if (address < OUTPUT_COMPARE_BASE ||
        address >= OUTPUT_COMPARE_BASE +
                       DSPIC33_OUTPUT_COMPARE_COUNT * OUTPUT_COMPARE_STRIDE) {
        return;
    }
    channel = (uint8_t)((address - OUTPUT_COMPARE_BASE) / OUTPUT_COMPARE_STRIDE);
    base = output_compare_base(channel);
    offset = (uint16_t)(address - base);
    if (output_compare_pmd_disabled(cpu, channel) && offset <= 8u) {
        raw_write_word(cpu, (uint16_t)(base + offset), previous);
        return;
    }
    if (offset != 0u && offset != 2u && offset != 4u && offset != 6u) {
        return;
    }
    if (output_compare_update_cascade(cpu, channel, address, previous)) {
        return;
    }
    control1 = raw_word(cpu, base);
    control2 = raw_word(cpu, (uint16_t)(base + 2u));
    previous1 = offset == 0u ? previous : control1;
    previous2 = offset == 2u ? previous : control2;
    if (offset == 2u && (control1 & OUTPUT_COMPARE_TRIGGER_ONESHOT) != 0u &&
        (control1 & OUTPUT_COMPARE_CON1_UNSUPPORTED) == 0u &&
        (control1 & OUTPUT_COMPARE_MODE_MASK) != 0u &&
        (control2 & OUTPUT_COMPARE_TRIGGER) != 0u &&
        ((control2 ^ previous2) & OUTPUT_COMPARE_TRIGGER_STATUS) != 0u) {
        control2 = (uint16_t)((control2 & ~OUTPUT_COMPARE_TRIGGER_STATUS) |
                              (previous2 & OUTPUT_COMPARE_TRIGGER_STATUS));
        raw_write_word(cpu, (uint16_t)(base + 2u), control2);
    }
    was_supported =
        output_compare_configuration_supported(channel, previous1, previous2);
    is_supported = output_compare_configuration_supported(channel, control1, control2);
    if (!was_supported && is_supported) {
        output_compare_start(cpu, channel);
    } else if (was_supported && !is_supported) {
        output_compare_stop(cpu, channel);
    } else if (was_supported && is_supported) {
        uint16_t mode = control1 & OUTPUT_COMPARE_MODE_MASK;
        bool mode_changed = (previous1 & OUTPUT_COMPARE_MODE_MASK) != mode;
        bool trigger_changed = ((previous2 ^ control2) & OUTPUT_COMPARE_TRIGGER) != 0u;
        bool trigger_status_changed =
            ((previous2 ^ control2) & OUTPUT_COMPARE_TRIGGER_STATUS) != 0u;
        bool clock_changed =
            ((previous1 ^ control1) & OUTPUT_COMPARE_TIMER_SOURCE_MASK) != 0u;
        bool low_control_written =
            offset == 0u &&
            (!cpu->io.cpu_write_valid ||
             (cpu->io.cpu_write_address <= base &&
              cpu->io.cpu_write_address + cpu->io.cpu_write_width > base));
        if (mode_changed || trigger_changed ||
            (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE && low_control_written)) {
            output_compare_start(cpu, channel);
        } else if (clock_changed || trigger_status_changed ||
                   (previous2 & OUTPUT_COMPARE_SYNC_MASK) !=
                       (control2 & OUTPUT_COMPARE_SYNC_MASK) ||
                   ((offset == 4u || offset == 6u) &&
                    mode != OUTPUT_COMPARE_MODE_EDGE_PWM &&
                    mode != OUTPUT_COMPARE_MODE_CENTER_PWM)) {
            if ((previous2 & OUTPUT_COMPARE_SYNC_MASK) !=
                    (control2 & OUTPUT_COMPARE_SYNC_MASK) ||
                trigger_status_changed) {
                cpu->io.output_compare.sync_emitted[channel] = false;
            }
            if ((previous2 & OUTPUT_COMPARE_SYNC_MASK) !=
                    (control2 & OUTPUT_COMPARE_SYNC_MASK) ||
                (trigger_status_changed &&
                 (control2 & OUTPUT_COMPARE_TRIGGER_STATUS) != 0u)) {
                cpu->io.output_compare.sync_reset_pending &=
                    (uint16_t)~(uint16_t)(1u << channel);
            } else if (trigger_status_changed) {
                cpu->io.output_compare.sync_reset_pending |= (uint16_t)(1u << channel);
            }
            if (offset == 4u) {
                cpu->io.output_compare.active_rs[channel] =
                    raw_word(cpu, (uint16_t)(base + 4u));
                cpu->io.output_compare.sync_emitted[channel] = false;
            } else if (offset == 6u) {
                cpu->io.output_compare.active_r[channel] =
                    raw_word(cpu, (uint16_t)(base + 6u));
            }
            if (trigger_status_changed) {
                uint64_t activation_delay =
                    cpu->instruction_active ? dspic33_device_instruction_cycles(
                                                  cpu, cpu->current_instruction_cycles)
                                            : 0u;
                uint16_t bit = (uint16_t)(1u << channel);
                cpu->io.output_compare.activation_cycle[channel] =
                    cpu->device_cycles + activation_delay;
                if (cpu->instruction_active) {
                    cpu->io.output_compare.activation_pending |= bit;
                } else {
                    cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
                }
                cpu->io.output_compare.timer_generation[channel]++;
                output_compare_schedule_next(cpu, channel, activation_delay);
                return;
            }
            cpu->io.output_compare.timer_generation[channel]++;
            output_compare_schedule_next(cpu, channel, 0u);
        }
    }
    if (offset == 0u && ((previous1 ^ control1) & OUTPUT_COMPARE_STOP_IDLE) != 0u &&
        cpu->power_state == DSPIC33_POWER_IDLE) {
        output_compare_update_power_state(cpu);
    }
    output_compare_refresh_fault(cpu, channel);
}

static void update_output_compare_pmd(Dspic33* cpu, uint16_t address,
                                      uint16_t previous) {
    uint8_t first_channel;
    uint8_t channel;
    uint16_t changed;
    uint16_t current;
    if (address == 0x0762u) {
        first_channel = 0u;
    } else if (address == 0x0768u) {
        first_channel = 8u;
    } else {
        return;
    }
    current = raw_word(cpu, address);
    changed = (uint16_t)((previous ^ current) & 0x00ffu);
    for (channel = first_channel; channel < first_channel + 8u; channel++) {
        uint16_t register_mask = (uint16_t)(1u << (channel - first_channel));
        if ((changed & register_mask) == 0u) {
            continue;
        }
        cpu->io.output_compare.pmd_generation[channel]++;
        if (!dspic33_schedule(
                cpu, DSPIC33_EVENT_OUTPUT_COMPARE, channel,
                OUTPUT_COMPARE_EVENT_PMD |
                    ((current & register_mask) != 0u ? OUTPUT_COMPARE_EVENT_PMD_DISABLED
                                                     : 0u) |
                    ((uint32_t)cpu->io.output_compare.pmd_generation[channel]
                     << OUTPUT_COMPARE_EVENT_PMD_GENERATION_SHIFT),
                dspic33_device_instruction_cycles(cpu, 1u))) {
            uint8_t invalidate;
            raw_write_word(cpu, address, previous);
            for (invalidate = first_channel; invalidate < first_channel + 8u;
                 invalidate++) {
                if ((changed & (uint16_t)(1u << (invalidate - first_channel))) != 0u) {
                    cpu->io.output_compare.pmd_generation[invalidate]++;
                }
            }
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
    }
}

static void advance_output_compare(Dspic33* cpu, uint64_t cycles) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        if (output_compare_timer_owner(cpu, channel) &&
            output_compare_operating(cpu, channel) &&
            output_compare_fp_clocked(cpu, channel)) {
            uint16_t address = (uint16_t)(output_compare_base(channel) + 8u);
            uint16_t control2 =
                raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
            uint64_t interval_start = cpu->device_cycles - cycles;
            uint64_t active_start = cpu->io.output_compare.activation_cycle[channel];
            uint64_t elapsed_start =
                interval_start > active_start ? interval_start : active_start;
            if (cpu->device_cycles > elapsed_start &&
                (control2 & (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRIGGER_STATUS)) !=
                    OUTPUT_COMPARE_TRIGGER) {
                if (output_compare_cascade_owner(cpu, channel)) {
                    output_compare_write_cascade_timer(
                        cpu, channel,
                        output_compare_cascade_timer(cpu, channel) +
                            (uint32_t)(cpu->device_cycles - elapsed_start));
                } else {
                    raw_write_word(
                        cpu, address,
                        (uint16_t)(raw_word(cpu, address) +
                                   (uint16_t)(cpu->device_cycles - elapsed_start)));
                }
            }
        }
    }
}

static bool output_compare_uses_timer(const Dspic33* cpu, uint8_t channel,
                                      uint8_t timer) {
    return output_compare_timer_owner(cpu, channel) &&
           output_compare_operating(cpu, channel) &&
           !output_compare_fp_clocked(cpu, channel) &&
           output_compare_timer_source(cpu, channel) == timer &&
           (timer != 0u || (raw_word(cpu, timer_controls[0]) &
                            (TIMER_EXTERNAL | TIMER_SYNC)) != TIMER_EXTERNAL);
}

static uint64_t output_compare_clock_boundary_ticks(const Dspic33* cpu, uint8_t timer) {
    uint64_t boundary = UINT64_MAX;
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint32_t kind;
        uint64_t delay;
        if (!output_compare_uses_timer(cpu, channel, timer)) {
            continue;
        }
        delay = output_compare_next_timer_event(cpu, channel, &kind);
        if (delay < boundary) {
            boundary = delay;
        }
    }
    return boundary;
}

static void output_compare_advance_clock(Dspic33* cpu, uint8_t timer, uint64_t ticks) {
    uint8_t channel;
    uint16_t pulses;
    cpu->io.output_compare.clock_advancing = true;
    cpu->io.output_compare.deferred_sync_pulses = 0u;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base;
        uint16_t bit = (uint16_t)(1u << channel);
        uint16_t timer_address;
        uint32_t kind;
        uint64_t delay;
        if (!output_compare_uses_timer(cpu, channel, timer) ||
            ((cpu->io.output_compare.activation_pending & bit) != 0u &&
             cpu->device_cycles <= cpu->io.output_compare.activation_cycle[channel])) {
            continue;
        }
        cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
        base = output_compare_base(channel);
        timer_address = (uint16_t)(base + 8u);
        delay = output_compare_next_timer_event(cpu, channel, &kind);
        if (delay == UINT64_MAX) {
            continue;
        }
        if (output_compare_cascade_owner(cpu, channel)) {
            output_compare_write_cascade_timer(
                cpu, channel,
                output_compare_cascade_timer(cpu, channel) + (uint32_t)ticks);
        } else {
            raw_write_word(cpu, timer_address,
                           (uint16_t)(raw_word(cpu, timer_address) + (uint16_t)ticks));
        }
        if (ticks == delay) {
            run_output_compare(
                cpu, channel,
                kind | ((uint32_t)cpu->io.output_compare.timer_generation[channel]
                        << OUTPUT_COMPARE_EVENT_GENERATION_SHIFT));
        }
    }
    cpu->io.output_compare.clock_advancing = false;
    pulses = cpu->io.output_compare.deferred_sync_pulses;
    cpu->io.output_compare.deferred_sync_pulses = 0u;
    for (channel = 0u; channel < 9u; channel++) {
        if ((pulses & (uint16_t)(1u << channel)) != 0u) {
            output_compare_pulse_source(
                cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + channel));
        }
    }
}

static bool output_compare_function_channel(uint8_t function, uint8_t* channel) {
    if (function >= 0x10u && function <= 0x17u) {
        *channel = (uint8_t)(function - 0x10u);
        return true;
    }
    if (function >= 0x25u && function <= 0x2cu) {
        *channel = (uint8_t)(function - 0x25u + 8u);
        return true;
    }
    return false;
}

static bool output_compare_pin_channel(const Dspic33* cpu, uint8_t pin,
                                       uint8_t* channel) {
    size_t index;
    for (index = 0u; index < sizeof(pps_outputs) / sizeof(pps_outputs[0]); index++) {
        if (pps_outputs[index].pin == pin) {
            uint8_t function = (uint8_t)((raw_word(cpu, pps_outputs[index].address) >>
                                          pps_outputs[index].shift) &
                                         0x003fu);
            return output_compare_function_channel(function, channel);
        }
    }
    return false;
}

static uint16_t comparator_base(uint8_t comparator) {
    return (uint16_t)(COMPARATOR_BASE + comparator * COMPARATOR_STRIDE);
}

static bool comparator_reference_level(const Dspic33* cpu, uint16_t control,
                                       bool positive, uint16_t* level) {
    uint16_t reference = raw_word(cpu, COMPARATOR_REFERENCE);
    if (positive) {
        uint32_t source;
        uint8_t tap;
        if ((control & COMPARATOR_REFERENCE_INTERNAL) == 0u) {
            return false;
        }
        if ((reference & COMPARATOR_REFERENCE_EXTERNAL) != 0u) {
            *level = cpu->io.comparator
                         .reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE];
            return true;
        }
        if ((reference & COMPARATOR_REFERENCE_ENABLE) == 0u) {
            return false;
        }
        if ((reference & COMPARATOR_REFERENCE_SOURCE_EXTERNAL) != 0u) {
            uint16_t positive_reference =
                cpu->io.comparator
                    .reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE];
            uint16_t negative_reference =
                cpu->io.comparator
                    .reference[DSPIC33_COMPARATOR_REFERENCE_VREF_NEGATIVE];
            if (positive_reference < negative_reference) {
                return false;
            }
            source = (uint32_t)(positive_reference - negative_reference);
        } else {
            source = cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_AVDD];
        }
        tap = (uint8_t)(reference & 0x000fu);
        *level = (uint16_t)((reference & COMPARATOR_REFERENCE_LOW_RANGE) != 0u
                                ? source * tap / 24u
                                : source * (8u + tap) / 32u);
        return true;
    }
    if ((control & COMPARATOR_CHANNEL_MASK) != COMPARATOR_CHANNEL_MASK) {
        return false;
    }
    switch (reference & COMPARATOR_REFERENCE_BAND_GAP_MASK) {
    case 0x0000u:
        *level = 2200u;
        return true;
    case 0x0100u:
        *level = 600u;
        return true;
    case 0x0200u:
        *level = 200u;
        return true;
    default:
        if ((reference & COMPARATOR_REFERENCE_SOURCE_EXTERNAL) != 0u) {
            return false;
        }
        *level =
            cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE];
        return true;
    }
}

static bool comparator_configuration_supported(const Dspic33* cpu, uint8_t comparator) {
    uint16_t control = raw_word(cpu, comparator_base(comparator));
    uint16_t level;
    if ((control & COMPARATOR_ENABLE) == 0u) {
        return false;
    }
    if ((control & COMPARATOR_REFERENCE_INTERNAL) != 0u &&
        !comparator_reference_level(cpu, control, true, &level)) {
        return false;
    }
    return (control & COMPARATOR_CHANNEL_MASK) != COMPARATOR_CHANNEL_MASK ||
           comparator_reference_level(cpu, control, false, &level);
}

static bool comparator_operating(const Dspic33* cpu, uint8_t comparator) {
    if (cpu->io.comparator.pmd_disabled ||
        !comparator_configuration_supported(cpu, comparator)) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE ||
           (raw_word(cpu, COMPARATOR_STATUS) & COMPARATOR_STOP_IDLE) == 0u;
}

static void comparator_refresh_status(Dspic33* cpu) {
    uint16_t status = raw_word(cpu, COMPARATOR_STATUS) & COMPARATOR_STOP_IDLE;
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint16_t control = raw_word(cpu, comparator_base(comparator));
        if ((control & COMPARATOR_EVENT) != 0u) {
            status |= (uint16_t)(0x0100u << comparator);
        }
        if ((control & COMPARATOR_OUTPUT) != 0u) {
            status |= (uint16_t)(1u << comparator);
        }
    }
    raw_write_word(cpu, COMPARATOR_STATUS, status);
}

static void comparator_set_output(Dspic33* cpu, uint8_t comparator, bool high) {
    uint16_t base = comparator_base(comparator);
    uint16_t control = raw_word(cpu, base);
    uint8_t bit = (uint8_t)(1u << comparator);
    bool rising = (control & COMPARATOR_OUTPUT) == 0u && high;
    if (high) {
        control |= COMPARATOR_OUTPUT;
        cpu->io.comparator.output_high |= bit;
    } else {
        control &= (uint16_t)~COMPARATOR_OUTPUT;
        cpu->io.comparator.output_high &= (uint8_t)~bit;
    }
    raw_write_word(cpu, base, control);
    comparator_refresh_status(cpu);
    refresh_external_interrupts(cpu);
    output_compare_refresh_fault_pps_inputs(cpu);
    if (rising) {
        input_capture_pulse_source(
            cpu, (uint8_t)(INPUT_CAPTURE_SYNC_COMPARATOR_FIRST + comparator));
    }
}

static void comparator_raise_event(Dspic33* cpu, uint8_t comparator) {
    uint16_t base = comparator_base(comparator);
    raw_write_word(cpu, base, (uint16_t)(raw_word(cpu, base) | COMPARATOR_EVENT));
    comparator_refresh_status(cpu);
    dspic33_raise_interrupt(cpu, COMPARATOR_IRQ);
    output_compare_pulse_source(
        cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_COMPARATOR_FIRST + comparator));
}

static bool comparator_transition_matches(uint16_t control, bool previous,
                                          bool current) {
    uint16_t polarity = control & COMPARATOR_EVENT_POLARITY_MASK;
    bool rising = !previous && current;
    bool falling = previous && !current;
    if (polarity == COMPARATOR_EVENT_POLARITY_MASK) {
        return rising || falling;
    }
    if (polarity == 0x0040u) {
        return rising;
    }
    if (polarity == 0x0080u) {
        return falling;
    }
    return false;
}

static bool comparator_mask_source(const Dspic33* cpu, uint8_t selection) {
    if (selection < 14u) {
        return dspic33_pwm_output(cpu, (uint8_t)(selection / 2u),
                                  (selection & 1u) != 0u);
    }
    return (cpu->io.pwm_fault_inputs &
            ((uint32_t)1u << (selection == 14u ? 1u : 3u))) != 0u;
}

static bool comparator_mask_gate(bool source, uint16_t control, uint16_t positive,
                                 uint16_t negative) {
    return ((control & positive) != 0u && source) ||
           ((control & negative) != 0u && !source);
}

static bool comparator_mask_and(bool source, uint16_t control, uint16_t positive,
                                uint16_t negative, bool result) {
    if ((control & positive) != 0u) {
        result &= source;
    }
    if ((control & negative) != 0u) {
        result &= !source;
    }
    return result;
}

static bool comparator_mask_active(const Dspic33* cpu, uint8_t comparator) {
    uint16_t base = comparator_base(comparator);
    uint16_t selections = raw_word(cpu, (uint16_t)(base + 2u));
    uint16_t control = raw_word(cpu, (uint16_t)(base + 4u));
    bool source_a = comparator_mask_source(cpu, (uint8_t)(selections & 0x000fu));
    bool source_b =
        comparator_mask_source(cpu, (uint8_t)((selections >> 4u) & 0x000fu));
    bool source_c =
        comparator_mask_source(cpu, (uint8_t)((selections >> 8u) & 0x000fu));
    bool and_result = true;
    bool mask = comparator_mask_gate(source_a, control, 0x0200u, 0x0100u) ||
                comparator_mask_gate(source_b, control, 0x0800u, 0x0400u) ||
                comparator_mask_gate(source_c, control, 0x2000u, 0x1000u);
    and_result = comparator_mask_and(source_a, control, 0x0002u, 0x0001u, and_result);
    and_result = comparator_mask_and(source_b, control, 0x0008u, 0x0004u, and_result);
    and_result = comparator_mask_and(source_c, control, 0x0020u, 0x0010u, and_result);
    if ((control & 0x0040u) != 0u && and_result) {
        mask = true;
    }
    if ((control & 0x0080u) != 0u && !and_result) {
        mask = true;
    }
    return mask;
}

static bool comparator_filter_enabled(const Dspic33* cpu, uint8_t comparator) {
    return (raw_word(cpu, (uint16_t)(comparator_base(comparator) + 6u)) &
            COMPARATOR_FILTER_ENABLE) != 0u;
}

static uint8_t comparator_filter_source(const Dspic33* cpu, uint8_t comparator) {
    return (uint8_t)((raw_word(cpu, (uint16_t)(comparator_base(comparator) + 6u)) &
                      COMPARATOR_FILTER_SOURCE_MASK) >>
                     4u);
}

static uint16_t comparator_filter_divider(const Dspic33* cpu, uint8_t comparator) {
    uint16_t control = raw_word(cpu, (uint16_t)(comparator_base(comparator) + 6u));
    return (uint16_t)(1u << (control & COMPARATOR_FILTER_DIVIDER_MASK));
}

static void comparator_publish_output(Dspic33* cpu, uint8_t comparator, bool current) {
    uint16_t control = raw_word(cpu, comparator_base(comparator));
    bool previous =
        (cpu->io.comparator.last_read_cout & (uint8_t)(1u << comparator)) != 0u;
    comparator_set_output(cpu, comparator, current);
    if ((control & COMPARATOR_EVENT) == 0u &&
        cpu->device_cycles >= cpu->io.comparator.rearm_cycle[comparator] &&
        comparator_transition_matches(control, previous, current)) {
        comparator_raise_event(cpu, comparator);
    }
}

static void comparator_filter_sample(Dspic33* cpu, uint8_t comparator) {
    uint8_t bit = (uint8_t)(1u << comparator);
    bool sample = (cpu->io.comparator.raw_high & bit) != 0u;
    bool output = (cpu->io.comparator.output_high & bit) != 0u;
    bool candidate = (cpu->io.comparator.filter_candidate_high & bit) != 0u;
    if (sample == output) {
        cpu->io.comparator.filter_count[comparator] = 0u;
        comparator_publish_output(cpu, comparator, output);
        return;
    }
    if (cpu->io.comparator.filter_count[comparator] == 0u || sample != candidate) {
        cpu->io.comparator.filter_count[comparator] = 1u;
        if (sample) {
            cpu->io.comparator.filter_candidate_high |= bit;
        } else {
            cpu->io.comparator.filter_candidate_high &= (uint8_t)~bit;
        }
        return;
    }
    cpu->io.comparator.filter_count[comparator]++;
    if (cpu->io.comparator.filter_count[comparator] >= 3u) {
        cpu->io.comparator.filter_count[comparator] = 0u;
        comparator_publish_output(cpu, comparator, sample);
    }
}

static void comparator_filter_samples(Dspic33* cpu, uint8_t comparator,
                                      uint64_t samples) {
    while (samples-- != 0u) {
        comparator_filter_sample(cpu, comparator);
        if (cpu->io.comparator.filter_count[comparator] == 0u &&
            ((cpu->io.comparator.raw_high ^ cpu->io.comparator.output_high) &
             (uint8_t)(1u << comparator)) == 0u) {
            break;
        }
    }
}

static bool comparator_internal_filter_clock_available(const Dspic33* cpu,
                                                       uint8_t comparator) {
    return comparator_operating(cpu, comparator) &&
           comparator_filter_enabled(cpu, comparator) &&
           comparator_filter_source(cpu, comparator) < 2u &&
           cpu->power_state != DSPIC33_POWER_SLEEP;
}

static uint64_t comparator_internal_filter_period(const Dspic33* cpu,
                                                  uint8_t comparator) {
    uint16_t divider = comparator_filter_divider(cpu, comparator);
    return comparator_filter_source(cpu, comparator) == 0u
               ? divider
               : (uint64_t)(divider > 1u ? divider / 2u : 1u);
}

static bool comparator_schedule_filter(Dspic33* cpu, uint8_t comparator,
                                       uint32_t generation) {
    return !comparator_internal_filter_clock_available(cpu, comparator) ||
           dspic33_schedule(cpu, DSPIC33_EVENT_COMPARATOR,
                            (uint16_t)(COMPARATOR_EVENT_FILTER_FIRST + comparator),
                            generation,
                            comparator_internal_filter_period(cpu, comparator));
}

static void comparator_remove_filter_events(Dspic33* cpu, uint8_t comparator,
                                            uint32_t generation, bool retain) {
    size_t source;
    size_t destination = 0u;
    uint16_t event_source = (uint16_t)(COMPARATOR_EVENT_FILTER_FIRST + comparator);
    for (source = 0u; source < cpu->events.count; source++) {
        Dspic33Event* event = &cpu->events.items[source];
        if (event->type == DSPIC33_EVENT_COMPARATOR && event->source == event_source &&
            (!retain || event->value != generation)) {
            continue;
        }
        cpu->events.items[destination++] = *event;
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
}

static bool comparator_reconfigure_filter(Dspic33* cpu, uint8_t comparator) {
    uint32_t generation = cpu->io.comparator.filter_generation[comparator] + 1u;
    if (!comparator_schedule_filter(cpu, comparator, generation)) {
        return false;
    }
    cpu->io.comparator.filter_generation[comparator] = generation;
    cpu->io.comparator.filter_fraction[comparator] = 0u;
    cpu->io.comparator.filter_count[comparator] = 0u;
    comparator_remove_filter_events(
        cpu, comparator, generation,
        comparator_internal_filter_clock_available(cpu, comparator));
    return true;
}

static bool comparator_filter_event(const Dspic33* cpu, const Dspic33Event* event,
                                    uint8_t comparator) {
    return event->type == DSPIC33_EVENT_COMPARATOR &&
           event->source == COMPARATOR_EVENT_FILTER_FIRST + comparator &&
           event->value == cpu->io.comparator.filter_generation[comparator];
}

static void comparator_update_filter_power(Dspic33* cpu) {
    uint8_t comparator;
    bool changed = false;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        bool available = comparator_internal_filter_clock_available(cpu, comparator);
        bool found = false;
        size_t index;
        for (index = 0u; index < cpu->events.count; index++) {
            Dspic33Event* event = &cpu->events.items[index];
            if (!comparator_filter_event(cpu, event, comparator)) {
                continue;
            }
            found = true;
            if (!available && !event->paused) {
                event->paused_remaining = event->cycle - cpu->device_cycles;
                event->paused = true;
                changed = true;
            } else if (available && event->paused) {
                if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
                    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
                    continue;
                }
                event->cycle = cpu->device_cycles + event->paused_remaining;
                event->paused_remaining = 0u;
                event->paused = false;
                changed = true;
            }
        }
        if (available && !found &&
            !comparator_schedule_filter(
                cpu, comparator, cpu->io.comparator.filter_generation[comparator])) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void comparator_filter_clock(Dspic33* cpu, uint8_t source, uint64_t clocks) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint64_t accumulated;
        uint16_t divider;
        if (!comparator_operating(cpu, comparator) ||
            !comparator_filter_enabled(cpu, comparator) ||
            comparator_filter_source(cpu, comparator) != source) {
            continue;
        }
        divider = comparator_filter_divider(cpu, comparator);
        accumulated = cpu->io.comparator.filter_fraction[comparator] + clocks;
        cpu->io.comparator.filter_fraction[comparator] =
            (uint16_t)(accumulated % divider);
        comparator_filter_samples(cpu, comparator, accumulated / divider);
    }
}

static void comparator_evaluate(Dspic33* cpu, uint8_t comparator) {
    uint16_t base = comparator_base(comparator);
    uint16_t control = raw_word(cpu, base);
    uint8_t bit = (uint8_t)(1u << comparator);
    uint16_t positive;
    uint16_t negative;
    bool current;
    if (!comparator_operating(cpu, comparator)) {
        if (!comparator_configuration_supported(cpu, comparator) ||
            cpu->io.comparator.pmd_disabled) {
            comparator_set_output(cpu, comparator, false);
            cpu->io.comparator.raw_high &= (uint8_t)~bit;
            cpu->io.comparator.filter_count[comparator] = 0u;
        }
        return;
    }
    if (!comparator_reference_level(cpu, control, true, &positive)) {
        positive =
            cpu->io.comparator.input[comparator][DSPIC33_COMPARATOR_INPUT_POSITIVE];
    }
    if (!comparator_reference_level(cpu, control, false, &negative)) {
        negative = cpu->io.comparator
                       .input[comparator][(control & COMPARATOR_CHANNEL_MASK) + 1u];
    }
    current = positive > negative;
    if ((control & COMPARATOR_POLARITY) != 0u) {
        current = !current;
    }
    if (comparator_mask_active(cpu, comparator)) {
        current = (raw_word(cpu, (uint16_t)(base + 4u)) & 0x8000u) != 0u;
    }
    if (current) {
        cpu->io.comparator.raw_high |= bit;
    } else {
        cpu->io.comparator.raw_high &= (uint8_t)~bit;
    }
    if (!comparator_filter_enabled(cpu, comparator)) {
        cpu->io.comparator.filter_count[comparator] = 0u;
        comparator_publish_output(cpu, comparator, current);
    }
}

static void comparator_evaluate_all(Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        comparator_evaluate(cpu, comparator);
    }
}

static void run_comparator(Dspic33* cpu, uint16_t source, uint32_t value) {
    if (source == COMPARATOR_EVENT_PMD_SOURCE) {
        uint16_t generation = (uint16_t)(value >> 1u);
        if (generation != cpu->io.comparator.pmd_generation) {
            return;
        }
        cpu->io.comparator.pmd_disabled = (value & 1u) != 0u;
        comparator_evaluate_all(cpu);
        comparator_update_filter_power(cpu);
        return;
    }
    if (source < COMPARATOR_EVENT_INPUT_COUNT) {
        uint8_t comparator = (uint8_t)(source / DSPIC33_COMPARATOR_INPUT_COUNT);
        uint8_t input = (uint8_t)(source % DSPIC33_COMPARATOR_INPUT_COUNT);
        cpu->io.comparator.input[comparator][input] = (uint16_t)value;
        comparator_evaluate(cpu, comparator);
        return;
    }
    if (source >= COMPARATOR_EVENT_REFERENCE_FIRST &&
        source <
            COMPARATOR_EVENT_REFERENCE_FIRST + DSPIC33_COMPARATOR_REFERENCE_COUNT) {
        cpu->io.comparator.reference[source - COMPARATOR_EVENT_REFERENCE_FIRST] =
            (uint16_t)value;
        comparator_evaluate_all(cpu);
        return;
    }
    if (source >= COMPARATOR_EVENT_FILTER_FIRST &&
        source < COMPARATOR_EVENT_FILTER_FIRST + DSPIC33_COMPARATOR_COUNT) {
        uint8_t comparator = (uint8_t)(source - COMPARATOR_EVENT_FILTER_FIRST);
        uint64_t samples = comparator_filter_source(cpu, comparator) == 1u &&
                                   comparator_filter_divider(cpu, comparator) == 1u
                               ? 2u
                               : 1u;
        if (value != cpu->io.comparator.filter_generation[comparator] ||
            !comparator_internal_filter_clock_available(cpu, comparator)) {
            return;
        }
        comparator_filter_samples(cpu, comparator, samples);
        if (!comparator_schedule_filter(cpu, comparator, value)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
}

static void update_comparator_pmd(Dspic33* cpu, uint16_t previous) {
    bool disabled = (raw_word(cpu, COMPARATOR_PMD_ADDRESS) & COMPARATOR_PMD) != 0u;
    if (((previous & COMPARATOR_PMD) != 0u) == disabled) {
        return;
    }
    cpu->io.comparator.pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_COMPARATOR, COMPARATOR_EVENT_PMD_SOURCE,
                          ((uint32_t)cpu->io.comparator.pmd_generation << 1u) |
                              (disabled ? 1u : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        raw_write_word(cpu, COMPARATOR_PMD_ADDRESS, previous);
        cpu->io.comparator.pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void update_comparator_register(Dspic33* cpu, uint16_t address,
                                       uint16_t previous, uint16_t requested) {
    uint16_t offset;
    uint8_t comparator;
    if (address == COMPARATOR_PMD_ADDRESS) {
        update_comparator_pmd(cpu, previous);
        return;
    }
    if (!comparator_register_write_mask(address, &offset)) {
        return;
    }
    if (cpu->io.comparator.pmd_disabled) {
        raw_write_word(cpu, address, previous);
        return;
    }
    if (address == COMPARATOR_STATUS) {
        comparator_evaluate_all(cpu);
        comparator_update_filter_power(cpu);
        return;
    }
    if (address == COMPARATOR_REFERENCE) {
        comparator_evaluate_all(cpu);
        return;
    }
    comparator = (uint8_t)((address - COMPARATOR_BASE) / COMPARATOR_STRIDE);
    offset = (uint16_t)((address - COMPARATOR_BASE) % COMPARATOR_STRIDE);
    if (((offset == 0u &&
          ((previous ^ raw_word(cpu, address)) & COMPARATOR_ENABLE) != 0u) ||
         (offset == 6u && previous != raw_word(cpu, address))) &&
        !comparator_reconfigure_filter(cpu, comparator)) {
        raw_write_word(cpu, address, previous);
        comparator_evaluate(cpu, comparator);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    if (offset == 0u) {
        bool previous_event = (previous & COMPARATOR_EVENT) != 0u;
        bool requested_event = (requested & COMPARATOR_EVENT) != 0u;
        if (!previous_event && requested_event) {
            comparator_raise_event(cpu, comparator);
        } else if (previous_event && !requested_event) {
            cpu->io.comparator.rearm_cycle[comparator] = cpu->device_cycles + 1u;
            comparator_refresh_status(cpu);
        }
    }
    comparator_evaluate(cpu, comparator);
}

static const uint16_t rtcc_calendar_masks[4] = {0x7f7fu, 0x073fu, 0x1f3fu, 0x00ffu};
static const uint16_t rtcc_alarm_masks[3] = {0x7f7fu, 0x073fu, 0x1f3fu};

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
    return (value & 0x0fu) <= 9u && (value >> 4u) <= 9u && decoded >= minimum &&
           decoded <= maximum;
}

static uint8_t rtcc_month_days(uint8_t year, uint8_t month) {
    static const uint8_t days[] = {31u, 28u, 31u, 30u, 31u, 30u,
                                   31u, 31u, 30u, 31u, 30u, 31u};
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
        !rtcc_bcd_valid(hour, 0u, 23u) || weekday > 6u ||
        !rtcc_bcd_valid(month, 1u, 12u) || !rtcc_bcd_valid(year, 0u, 99u)) {
        return false;
    }
    return rtcc_bcd_valid(
        day, 1u, rtcc_month_days(rtcc_bcd_decode(year), rtcc_bcd_decode(month)));
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
    rtcc->calendar[2] =
        (uint16_t)(((uint16_t)rtcc_bcd_encode(month) << 8u) | rtcc_bcd_encode(day));
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
    return !cpu->io.rtcc.pmd_disabled &&
           (raw_word(cpu, RTCC_CONTROL) & RTCC_ENABLE) != 0u &&
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

static void rtcc_decrement_pointer(Dspic33* cpu, uint16_t control_address,
                                   uint16_t pointer_mask) {
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
    uint16_t pointer =
        (uint16_t)((raw_word(cpu, control_address) & pointer_mask) >> 8u);
    uint16_t mask = alarm ? (pointer < 3u ? rtcc_alarm_masks[pointer] : 0u)
                          : rtcc_calendar_masks[pointer];
    uint16_t value = alarm ? (pointer < 3u ? cpu->io.rtcc.alarm[pointer] : 0u)
                           : cpu->io.rtcc.calendar[pointer];
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

static uint16_t rtcc_window_write_value(const Dspic33* cpu, uint16_t address,
                                        uint16_t previous) {
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
    uint16_t pointer =
        (uint16_t)((raw_word(cpu, control_address) & pointer_mask) >> 8u);
    uint16_t previous = alarm ? (pointer < 3u ? cpu->io.rtcc.alarm[pointer] : 0u)
                              : cpu->io.rtcc.calendar[pointer];
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
                          ((uint32_t)cpu->io.rtcc.pmd_generation << 1u) |
                              (disabled ? 1u : 0u),
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

static bool qei_register(uint16_t address, uint8_t* channel, uint16_t* offset) {
    uint8_t index;
    uint16_t base = (uint16_t)(address & 0xfffeu);
    for (index = 0u; index < DSPIC33_QEI_COUNT; index++) {
        if (base >= qei_bases[index] &&
            base <= qei_bases[index] + QEI_LESS_EQUAL_HIGH) {
            *channel = index;
            *offset = (uint16_t)(base - qei_bases[index]);
            return true;
        }
    }
    return false;
}

static uint32_t qei_read_counter(const Dspic33* cpu, uint8_t channel,
                                 uint16_t low_offset) {
    uint16_t base = qei_bases[channel];
    return (uint32_t)raw_word(cpu, (uint16_t)(base + low_offset)) |
           ((uint32_t)raw_word(cpu, (uint16_t)(base + low_offset + 2u)) << 16u);
}

static void qei_write_counter(Dspic33* cpu, uint8_t channel, uint16_t low_offset,
                              uint32_t value) {
    uint16_t base = qei_bases[channel];
    raw_write_word(cpu, (uint16_t)(base + low_offset), (uint16_t)value);
    raw_write_word(cpu, (uint16_t)(base + low_offset + 2u), (uint16_t)(value >> 16u));
}

static uint64_t qei_divider(uint16_t control) {
    static const uint16_t divisors[8] = {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u};
    return divisors[(control & QEI_CONTROL_DIVIDER_MASK) >> QEI_CONTROL_DIVIDER_SHIFT];
}

static uint64_t qei_filter_divider(uint16_t io_control) {
    uint8_t selection = (uint8_t)((io_control & QEI_IO_FILTER_DIVIDER_MASK) >>
                                  QEI_IO_FILTER_DIVIDER_SHIFT);
    return selection == 7u ? 256u : 1ull << selection;
}

static bool qei_filter_clock_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, qei_bases[channel]);
    if (cpu->io.qei.pmd_disabled[channel] || cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE ||
           (control & QEI_CONTROL_STOP_IDLE) == 0u;
}

static bool qei_clock_enabled(const Dspic33* cpu, uint8_t channel) {
    return qei_filter_clock_enabled(cpu, channel) &&
           (raw_word(cpu, qei_bases[channel]) & QEI_CONTROL_ENABLE) != 0u;
}

static void qei_raise_status(Dspic33* cpu, uint8_t channel, uint16_t flag) {
    uint16_t address = (uint16_t)(qei_bases[channel] + 4u);
    uint16_t status = (uint16_t)(raw_word(cpu, address) | flag);
    raw_write_word(cpu, address, status);
    if ((status & (flag >> 1u)) != 0u) {
        dspic33_raise_interrupt(cpu, qei_irqs[channel]);
    }
}

static void qei_refresh_interrupt(Dspic33* cpu, uint8_t channel) {
    uint16_t status = raw_word(cpu, (uint16_t)(qei_bases[channel] + 4u));
    if (((status & QEI_STATUS_FLAG_MASK) >> 1u) & status & QEI_STATUS_ENABLE_MASK) {
        dspic33_raise_interrupt(cpu, qei_irqs[channel]);
    }
}

static void qei_refresh_comparisons(Dspic33* cpu, uint8_t channel) {
    int32_t position = (int32_t)qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    int32_t greater_equal =
        (int32_t)qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
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
    return (raw_word(cpu, qei_bases[channel]) & QEI_CONTROL_DIRECTION_INVERT) != 0u ? -1
                                                                                    : 1;
}

static void qei_update_position(Dspic33* cpu, uint8_t channel, int8_t direction) {
    uint16_t control = raw_word(cpu, qei_bases[channel]);
    uint8_t mode = (uint8_t)((control & QEI_CONTROL_POSITION_MODE_MASK) >>
                             QEI_CONTROL_POSITION_MODE_SHIFT);
    uint32_t position = qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    uint32_t greater_equal = qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
    uint32_t less_equal = qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW);
    uint32_t lower = less_equal;
    uint32_t upper = greater_equal;
    if ((control & QEI_CONTROL_COUNT_MODE_MASK) >= 2u) {
        mode = 0u;
    }
    if (mode == 6u && (control & QEI_CONTROL_DIRECTION_INVERT) != 0u) {
        lower = greater_equal;
        upper = less_equal;
    }
    if (mode == 6u && direction > 0 && position == upper) {
        position = lower;
    } else if (mode == 6u && direction < 0 && position == lower) {
        position = upper;
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
    uint16_t address = (uint16_t)(qei_bases[channel] + QEI_VELOCITY);
    uint16_t velocity = raw_word(cpu, address);
    if ((direction > 0 && velocity == 0x7fffu) ||
        (direction < 0 && velocity == 0x8000u)) {
        qei_raise_status(cpu, channel, QEI_STATUS_VELOCITY_OVERFLOW);
    }
    raw_write_word(cpu, address,
                   direction > 0 ? (uint16_t)(velocity + 1u)
                                 : (uint16_t)(velocity - 1u));
}

static void qei_update_index_counter(Dspic33* cpu, uint8_t channel, int8_t direction) {
    uint32_t value = qei_read_counter(cpu, channel, QEI_INDEX_LOW);
    qei_write_counter(cpu, channel, QEI_INDEX_LOW,
                      direction > 0 ? value + 1u : value - 1u);
}

static void qei_update_interval(Dspic33* cpu, uint8_t channel, uint64_t ticks) {
    uint32_t value = qei_read_counter(cpu, channel, QEI_INTERVAL_LOW);
    qei_write_counter(cpu, channel, QEI_INTERVAL_LOW, value + (uint32_t)ticks);
}

static void qei_capture_interval(Dspic33* cpu, uint8_t channel) {
    uint16_t base = qei_bases[channel];
    if (!cpu->io.qei.interval_armed[channel]) {
        cpu->io.qei.interval_armed[channel] = true;
        qei_write_counter(cpu, channel, QEI_INTERVAL_LOW, 0u);
        return;
    }
    if (!cpu->io.qei.interval_hold_locked[channel]) {
        raw_write_word(cpu, (uint16_t)(base + QEI_INTERVAL_HOLD_LOW),
                       raw_word(cpu, (uint16_t)(base + QEI_INTERVAL_LOW)));
        raw_write_word(cpu, (uint16_t)(base + QEI_INTERVAL_HOLD_HIGH),
                       raw_word(cpu, (uint16_t)(base + QEI_INTERVAL_HIGH)));
    }
    qei_write_counter(cpu, channel, QEI_INTERVAL_LOW, 0u);
}

static void qei_count_pulse(Dspic33* cpu, uint8_t channel, int8_t direction,
                            bool interval_measurement) {
    qei_update_position(cpu, channel, direction);
    qei_update_velocity(cpu, channel, direction);
    if (interval_measurement) {
        qei_capture_interval(cpu, channel);
    } else {
        qei_update_index_counter(cpu, channel, direction);
        qei_update_interval(cpu, channel, 1u);
    }
}

static void qei_timer_pulse(Dspic33* cpu, uint8_t channel, int8_t direction,
                            bool position_gate, uint8_t logical) {
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

static bool qei_ranges_intersect(uint32_t first_low, uint32_t first_high,
                                 uint32_t second_low, uint32_t second_high) {
    return first_low <= second_high && second_low <= first_high;
}

static bool qei_path_hits_range(uint32_t start, int8_t direction, uint64_t ticks,
                                uint32_t low, uint32_t high) {
    uint32_t distance;
    uint32_t end;
    if (ticks == 0u) {
        return false;
    }
    if (ticks > UINT32_MAX) {
        return true;
    }
    distance = (uint32_t)ticks;
    end = direction > 0 ? start + distance : start - distance;
    if (direction > 0) {
        if (end > start) {
            return qei_ranges_intersect(start + 1u, end, low, high);
        }
        return (start != UINT32_MAX &&
                qei_ranges_intersect(start + 1u, UINT32_MAX, low, high)) ||
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
    if (ticks == 0u) {
        return false;
    }
    if (ticks > UINT16_MAX) {
        return true;
    }
    end = direction > 0 ? (uint16_t)(start + (uint16_t)ticks)
                        : (uint16_t)(start - (uint16_t)ticks);
    if (direction > 0) {
        return end > start ? value > start && value <= end
                           : value > start || value <= end;
    }
    return end < start ? value >= end && value < start : value < start || value >= end;
}

static void qei_advance_timer_ticks(Dspic33* cpu, uint8_t channel, int8_t direction,
                                    bool position_gate, uint8_t logical,
                                    uint64_t ticks) {
    uint16_t base = qei_bases[channel];
    uint32_t delta = (uint32_t)ticks;
    if (ticks == 0u) {
        return;
    }
    if (position_gate) {
        uint32_t position = qei_read_counter(cpu, channel, QEI_POSITION_LOW);
        uint32_t position_key = position ^ 0x80000000u;
        uint32_t greater_equal =
            qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW) ^ 0x80000000u;
        uint32_t less_equal =
            qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW) ^ 0x80000000u;
        uint16_t velocity = raw_word(cpu, (uint16_t)(base + QEI_VELOCITY));
        if (qei_path_crosses_value(position, direction, ticks,
                                   direction > 0 ? 0x80000000u : 0x7fffffffu)) {
            qei_raise_status(cpu, channel, QEI_STATUS_POSITION_OVERFLOW);
        }
        if (qei_path_hits_range(position_key, direction, ticks, greater_equal,
                                UINT32_MAX)) {
            qei_raise_status(cpu, channel, QEI_STATUS_HIGH_COMPARE);
        }
        if (qei_path_hits_range(position_key, direction, ticks, 0u, less_equal)) {
            qei_raise_status(cpu, channel, QEI_STATUS_LOW_COMPARE);
        }
        if (qei_path_crosses_word(velocity, direction, ticks,
                                  direction > 0 ? 0x8000u : 0x7fffu)) {
            qei_raise_status(cpu, channel, QEI_STATUS_VELOCITY_OVERFLOW);
        }
        qei_write_counter(cpu, channel, QEI_POSITION_LOW,
                          direction > 0 ? position + delta : position - delta);
        raw_write_word(cpu, (uint16_t)(base + QEI_VELOCITY),
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
    uint16_t io_control = raw_word(cpu, (uint16_t)(qei_bases[channel] + 2u));
    uint8_t inputs = cpu->io.qei.filtered_inputs[channel] & QEI_IO_INPUT_MASK;
    if ((io_control & QEI_IO_SWAP) != 0u) {
        inputs =
            (uint8_t)((inputs & 0x0cu) | ((inputs & 1u) << 1u) | ((inputs & 2u) >> 1u));
    }
    return (uint8_t)(inputs ^ ((io_control & QEI_IO_POLARITY_MASK) >> 4u));
}

static void qei_index_event(Dspic33* cpu, uint8_t channel, uint8_t logical) {
    uint16_t base = qei_bases[channel];
    uint16_t control = raw_word(cpu, base);
    uint8_t match = (uint8_t)((control & QEI_CONTROL_INDEX_MATCH_MASK) >>
                              QEI_CONTROL_INDEX_MATCH_SHIFT);
    uint8_t mode = (uint8_t)((control & QEI_CONTROL_POSITION_MODE_MASK) >>
                             QEI_CONTROL_POSITION_MODE_SHIFT);
    uint8_t count_mode = (uint8_t)(control & QEI_CONTROL_COUNT_MODE_MASK);
    if (count_mode >= 2u) {
        mode = 0u;
    }
    if ((logical & 3u) != match) {
        return;
    }
    qei_raise_status(cpu, channel, QEI_STATUS_INDEX);
    if (count_mode < 2u) {
        qei_update_index_counter(cpu, channel, qei_current_direction(cpu, channel));
    }
    if (mode == 4u && cpu->io.qei.home_index_count[channel] == 1u) {
        cpu->io.qei.home_index_count[channel] = 2u;
    } else if (mode == 1u) {
        qei_write_counter(cpu, channel, QEI_POSITION_LOW, 0u);
    } else if (mode == 2u ||
               (mode == 3u && cpu->io.qei.home_index_count[channel] >= 1u) ||
               (mode == 4u && cpu->io.qei.home_index_count[channel] >= 2u)) {
        qei_write_counter(cpu, channel, QEI_POSITION_LOW,
                          qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW));
        raw_write_word(cpu, base,
                       (uint16_t)(control & ~QEI_CONTROL_POSITION_MODE_MASK));
        if (mode == 3u || mode == 4u) {
            qei_raise_status(cpu, channel, QEI_STATUS_INITIALIZED);
        }
    }
    qei_refresh_comparisons(cpu, channel);
}

static void qei_apply_filtered_inputs(Dspic33* cpu, uint8_t channel) {
    static const int8_t actions[16] = {0, 1, -1, 0,  -1, 0,  0, 1,
                                       1, 0, 0,  -1, 0,  -1, 1, 0};
    uint16_t control = raw_word(cpu, qei_bases[channel]);
    uint8_t index_match = (uint8_t)((control & QEI_CONTROL_INDEX_MATCH_MASK) >>
                                    QEI_CONTROL_INDEX_MATCH_SHIFT);
    uint8_t previous = cpu->io.qei.logical_inputs[channel];
    uint8_t logical = qei_logical_inputs(cpu, channel);
    uint8_t mode = (uint8_t)(control & QEI_CONTROL_COUNT_MODE_MASK);
    cpu->io.qei.logical_inputs[channel] = logical;
    if ((logical & 4u) == 0u) {
        cpu->io.qei.index_latched[channel] = false;
    }
    if (!qei_clock_enabled(cpu, channel)) {
        return;
    }
    if (mode == 0u && (previous & 3u) != (logical & 3u)) {
        int8_t direction = actions[((previous & 3u) << 2u) | (logical & 3u)];
        if ((control & QEI_CONTROL_DIRECTION_INVERT) != 0u) {
            direction = (int8_t)-direction;
        }
        if (direction != 0) {
            qei_count_pulse(cpu, channel, direction, true);
        }
    } else if ((mode == 1u || mode == 2u) && (previous & 1u) == 0u &&
               (logical & 1u) != 0u) {
        bool gate_allows =
            (control & QEI_CONTROL_GATE_ENABLE) == 0u || (logical & 2u) != 0u;
        if (mode == 1u) {
            int8_t direction = mode == 1u ? ((logical & 2u) != 0u ? 1 : -1) : 1;
            if ((control & QEI_CONTROL_DIRECTION_INVERT) != 0u) {
                direction = (int8_t)-direction;
            }
            qei_count_pulse(cpu, channel, direction, true);
        } else {
            int8_t direction = (control & QEI_CONTROL_DIRECTION_INVERT) != 0u ? -1 : 1;
            qei_timer_pulse(cpu, channel, direction, gate_allows, logical);
        }
    }
    if ((previous & 8u) == 0u && (logical & 8u) != 0u) {
        cpu->io.qei.home_index_count[channel] = 1u;
        qei_raise_status(cpu, channel, QEI_STATUS_HOME);
        if ((raw_word(cpu, (uint16_t)(qei_bases[channel] + 2u)) &
             QEI_IO_CAPTURE_HOME) != 0u) {
            qei_write_counter(cpu, channel, QEI_GREATER_EQUAL_LOW,
                              qei_read_counter(cpu, channel, QEI_POSITION_LOW));
        }
    }
    if (!cpu->io.qei.index_latched[channel] && (logical & 4u) != 0u &&
        (logical & 3u) == index_match) {
        cpu->io.qei.index_latched[channel] = true;
        qei_index_event(cpu, channel, logical);
    }
}

static void qei_set_physical_input(Dspic33* cpu, uint8_t channel, uint8_t input,
                                   bool high) {
    uint8_t bit = (uint8_t)(1u << input);
    uint16_t io_control = raw_word(cpu, (uint16_t)(qei_bases[channel] + 2u));
    if (high) {
        cpu->qei_inputs[channel] |= bit;
    } else {
        cpu->qei_inputs[channel] &= (uint8_t)~bit;
    }
    if ((io_control & QEI_IO_FILTER_ENABLE) == 0u &&
        !cpu->io.qei.pmd_disabled[channel]) {
        cpu->io.qei.filtered_inputs[channel] = cpu->qei_inputs[channel];
        qei_apply_filtered_inputs(cpu, channel);
    }
}

static void qei_filter_ticks(Dspic33* cpu, uint8_t channel, uint64_t ticks) {
    uint8_t input;
    bool changed = false;
    if (ticks == 0u) {
        return;
    }
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

static uint64_t qei_accumulate_ticks(uint64_t* fraction, uint64_t cycles,
                                     uint64_t divider) {
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

static void qei_advance_counters(Dspic33* cpu, uint8_t channel, uint16_t control,
                                 uint64_t cycles) {
    uint64_t ticks = qei_accumulate_ticks(&cpu->io.qei.counter_fraction[channel],
                                          cycles, qei_divider(control));
    if ((control & QEI_CONTROL_COUNT_MODE_MASK) == 3u) {
        uint8_t logical = cpu->io.qei.logical_inputs[channel];
        bool gate_allows =
            (control & QEI_CONTROL_GATE_ENABLE) == 0u || (logical & 2u) != 0u;
        int8_t direction = (control & QEI_CONTROL_DIRECTION_INVERT) != 0u ? -1 : 1;
        qei_advance_timer_ticks(cpu, channel, direction, gate_allows, logical, ticks);
    } else if ((control & QEI_CONTROL_COUNT_MODE_MASK) < 2u) {
        qei_update_interval(cpu, channel, ticks);
    }
}

static void qei_advance_channel(Dspic33* cpu, uint8_t channel, uint64_t cycles) {
    uint16_t control = raw_word(cpu, qei_bases[channel]);
    uint16_t io_control = raw_word(cpu, (uint16_t)(qei_bases[channel] + 2u));
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
        if (cpu->io.qei.filtered_inputs[channel] == cpu->qei_inputs[channel] &&
            remaining != 0u) {
            if (counters_enabled) {
                qei_advance_counters(cpu, channel, control, remaining);
            }
            qei_accumulate_ticks(&cpu->io.qei.filter_fraction[channel], remaining,
                                 divider);
            memset(cpu->io.qei.filter_stability[channel], 0,
                   sizeof(cpu->io.qei.filter_stability[channel]));
            remaining = 0u;
        }
    }
}

static void advance_qei(Dspic33* cpu, uint64_t cycles) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        qei_advance_channel(cpu, channel, cycles);
    }
}

static void run_qei(Dspic33* cpu, uint16_t source, uint32_t value) {
    if (source >= QEI_PMD_EVENT_BASE) {
        uint8_t channel = (uint8_t)(source - QEI_PMD_EVENT_BASE);
        uint16_t generation = (uint16_t)(value >> 1u);
        if (channel < DSPIC33_QEI_COUNT &&
            generation == cpu->io.qei.pmd_generation[channel]) {
            cpu->io.qei.pmd_disabled[channel] = (value & 1u) != 0u;
            if (!cpu->io.qei.pmd_disabled[channel]) {
                uint16_t io_control =
                    raw_word(cpu, (uint16_t)(qei_bases[channel] + 2u));
                if ((io_control & QEI_IO_FILTER_ENABLE) == 0u) {
                    uint8_t match = (uint8_t)((raw_word(cpu, qei_bases[channel]) &
                                               QEI_CONTROL_INDEX_MATCH_MASK) >>
                                              QEI_CONTROL_INDEX_MATCH_SHIFT);
                    cpu->io.qei.filtered_inputs[channel] = cpu->qei_inputs[channel];
                    cpu->io.qei.logical_inputs[channel] =
                        qei_logical_inputs(cpu, channel);
                    if ((cpu->io.qei.logical_inputs[channel] & 4u) == 0u) {
                        cpu->io.qei.index_latched[channel] = false;
                    } else if ((cpu->io.qei.logical_inputs[channel] & 3u) == match) {
                        cpu->io.qei.index_latched[channel] = true;
                    }
                }
            }
        }
        return;
    }
    if (source < DSPIC33_QEI_COUNT * 4u) {
        qei_set_physical_input(cpu, (uint8_t)(source / 4u), (uint8_t)(source % 4u),
                               value != 0u);
    }
}

static void qei_update_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    static const uint16_t addresses[DSPIC33_QEI_COUNT] = {0x0760u, 0x0764u};
    static const uint16_t masks[DSPIC33_QEI_COUNT] = {0x0400u, 0x0020u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        bool disabled;
        if (address != addresses[channel] ||
            ((previous ^ raw_word(cpu, address)) & masks[channel]) == 0u) {
            continue;
        }
        disabled = (raw_word(cpu, address) & masks[channel]) != 0u;
        cpu->io.qei.pmd_generation[channel]++;
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_QEI,
                              (uint16_t)(QEI_PMD_EVENT_BASE + channel),
                              ((uint32_t)cpu->io.qei.pmd_generation[channel] << 1u) |
                                  (disabled ? 1u : 0u),
                              dspic33_device_instruction_cycles(cpu, 1u))) {
            raw_write_word(cpu, address, previous);
            cpu->io.qei.pmd_generation[channel]++;
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
}

static void update_qei_register(Dspic33* cpu, uint16_t address, uint16_t previous,
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
        raw_write_word(cpu, (uint16_t)(address & 0xfffeu), previous);
        return;
    }
    if (offset == 4u) {
        uint16_t status = raw_word(cpu, (uint16_t)(qei_bases[channel] + 4u));
        status = (uint16_t)((status & QEI_STATUS_ENABLE_MASK) |
                            (previous & requested & QEI_STATUS_FLAG_MASK));
        raw_write_word(cpu, (uint16_t)(qei_bases[channel] + 4u), status);
        qei_refresh_interrupt(cpu, channel);
    } else if (offset == 0u || offset == 2u) {
        memset(cpu->io.qei.filter_stability[channel], 0,
               sizeof(cpu->io.qei.filter_stability[channel]));
        cpu->io.qei.counter_fraction[channel] = 0u;
        cpu->io.qei.filter_fraction[channel] = 0u;
        if (offset == 2u && (raw_word(cpu, (uint16_t)(qei_bases[channel] + 2u)) &
                             QEI_IO_FILTER_ENABLE) == 0u) {
            cpu->io.qei.filtered_inputs[channel] = cpu->qei_inputs[channel];
        }
        cpu->io.qei.logical_inputs[channel] = qei_logical_inputs(cpu, channel);
        if (offset == 0u) {
            qei_refresh_comparisons(cpu, channel);
        }
    } else if (offset == QEI_POSITION_LOW) {
        raw_write_word(
            cpu, (uint16_t)(qei_bases[channel] + QEI_POSITION_HIGH),
            raw_word(cpu, (uint16_t)(qei_bases[channel] + QEI_POSITION_HOLD)));
        qei_refresh_comparisons(cpu, channel);
    } else if (offset == QEI_POSITION_HIGH) {
        qei_refresh_comparisons(cpu, channel);
    } else if (offset == QEI_INDEX_LOW) {
        raw_write_word(cpu, (uint16_t)(qei_bases[channel] + QEI_INDEX_HIGH),
                       raw_word(cpu, (uint16_t)(qei_bases[channel] + QEI_INDEX_HOLD)));
    } else if (offset == QEI_GREATER_EQUAL_LOW || offset == QEI_GREATER_EQUAL_HIGH ||
               offset == QEI_LESS_EQUAL_LOW || offset == QEI_LESS_EQUAL_HIGH) {
        qei_refresh_comparisons(cpu, channel);
    }
}

static bool qei_read_complete(const Dspic33* cpu, uint16_t address) {
    return !cpu->io.cpu_read_valid || cpu->io.cpu_read_width == 1u ||
           address == cpu->io.cpu_read_address + 1u;
}

static bool qei_read_register(Dspic33* cpu, uint16_t address, uint8_t* value) {
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
        uint16_t io_control = raw_word(cpu, (uint16_t)(qei_bases[channel] + 2u));
        io_control = (uint16_t)((io_control & ~QEI_IO_INPUT_MASK) |
                                cpu->io.qei.logical_inputs[channel]);
        *value = (uint8_t)(io_control >> ((address & 1u) * 8u));
    }
    if (offset == QEI_POSITION_LOW && (address & 1u) == 0u) {
        raw_write_word(
            cpu, (uint16_t)(qei_bases[channel] + QEI_POSITION_HOLD),
            raw_word(cpu, (uint16_t)(qei_bases[channel] + QEI_POSITION_HIGH)));
    } else if (offset == QEI_INDEX_LOW && (address & 1u) == 0u) {
        raw_write_word(cpu, (uint16_t)(qei_bases[channel] + QEI_INDEX_HOLD),
                       raw_word(cpu, (uint16_t)(qei_bases[channel] + QEI_INDEX_HIGH)));
    } else if (offset == QEI_INTERVAL_HOLD_LOW && (address & 1u) == 0u) {
        cpu->io.qei.interval_hold_locked[channel] = true;
    } else if (offset == QEI_INTERVAL_HOLD_HIGH && qei_read_complete(cpu, address)) {
        cpu->io.qei.interval_hold_locked[channel] = false;
    } else if (offset == QEI_VELOCITY && qei_read_complete(cpu, address)) {
        raw_write_word(cpu, (uint16_t)(qei_bases[channel] + QEI_VELOCITY), 0u);
    }
    return true;
}

static uint8_t dci_buffer_count(const Dspic33* cpu) {
    return (uint8_t)(((raw_word(cpu, DCI_CONTROL2) & DCI_CONTROL2_BUFFER_MASK) >> 10u) +
                     1u);
}

static uint8_t dci_mode(const Dspic33* cpu) {
    return (uint8_t)(raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_MODE_MASK);
}

static uint8_t dci_frame_count(const Dspic33* cpu) {
    uint8_t mode = dci_mode(cpu);
    if (mode == 2u) {
        return 13u;
    }
    if (mode == 3u) {
        return 16u;
    }
    return (uint8_t)(((raw_word(cpu, DCI_CONTROL2) & DCI_CONTROL2_FRAME_MASK) >> 5u) +
                     1u);
}

static uint8_t dci_word_width(const Dspic33* cpu) {
    if (dci_mode(cpu) >= 2u) {
        return 16u;
    }
    return (uint8_t)((raw_word(cpu, DCI_CONTROL2) & DCI_CONTROL2_WORD_MASK) + 1u);
}

static uint8_t dci_slot_width(const Dspic33* cpu, uint8_t slot) {
    return dci_mode(cpu) == 2u && slot != 0u ? 20u : dci_word_width(cpu);
}

static uint16_t dci_slot_mask(const Dspic33* cpu) {
    return dci_mode(cpu) == 2u ? 0x1fffu : UINT16_MAX;
}

static uint16_t dci_word_mask(const Dspic33* cpu) {
    uint8_t width = dci_word_width(cpu);
    return width == 16u ? UINT16_MAX : (uint16_t)(UINT16_MAX << (16u - width));
}

static uint8_t dci_active_transmit_buffers(const Dspic33* cpu) {
    uint16_t slot_mask = dci_slot_mask(cpu);
    uint16_t transmit_slots = (uint16_t)(raw_word(cpu, DCI_TRANSMIT_SLOTS) & slot_mask);
    uint16_t active_slots =
        (uint16_t)(transmit_slots | (raw_word(cpu, DCI_RECEIVE_SLOTS) & slot_mask));
    uint8_t count = dci_buffer_count(cpu);
    uint8_t buffer = 0u;
    uint8_t active = 0u;
    uint8_t frame;
    for (frame = 0u; frame < count; frame++) {
        uint8_t slot;
        for (slot = 0u; slot < dci_frame_count(cpu); slot++) {
            uint16_t bit = (uint16_t)(1u << slot);
            if ((transmit_slots & bit) != 0u) {
                active |= (uint8_t)(1u << buffer);
            }
            if ((active_slots & bit) != 0u) {
                buffer = (uint8_t)((buffer + 1u) % count);
            }
        }
    }
    return active;
}

static bool dci_configuration_supported(const Dspic33* cpu) {
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    uint16_t divider = raw_word(cpu, DCI_CONTROL3);
    return (control & (uint16_t)~DCI_CONTROL_SUPPORTED_MASK) == 0u &&
           dci_word_width(cpu) >= 4u &&
           (((control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u && divider == 0u) ||
            ((control & DCI_CONTROL_EXTERNAL_CLOCK) == 0u && divider != 0u));
}

static uint64_t dci_bit_cycles(const Dspic33* cpu) {
    return ((uint64_t)(raw_word(cpu, DCI_CONTROL3) & 0x0fffu) + 1u) * 2u;
}

static uint64_t dci_word_cycles(const Dspic33* cpu) {
    uint64_t bit_cycles = dci_bit_cycles(cpu);
    uint8_t width = dci_slot_width(cpu, cpu->io.dci.slot);
    return bit_cycles > UINT64_MAX / width ? UINT64_MAX : bit_cycles * width;
}

static bool dci_bcg_running(const Dspic33* cpu) {
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    return raw_word(cpu, DCI_CONTROL3) != 0u && !cpu->io.dci.pmd_disabled &&
           cpu->power_state != DSPIC33_POWER_SLEEP &&
           (cpu->power_state != DSPIC33_POWER_IDLE ||
            (control & DCI_CONTROL_STOP_IDLE) == 0u);
}

static uint64_t dci_bcg_phase(const Dspic33* cpu) {
    uint64_t period = dci_bit_cycles(cpu);
    uint64_t phase = cpu->io.dci.bcg_phase % period;
    if (!cpu->io.dci.bcg_paused) {
        phase =
            (phase + (cpu->device_cycles - cpu->io.dci.bcg_cycle) % period) % period;
    }
    return phase;
}

static void dci_update_bcg(Dspic33* cpu, bool reset) {
    Dspic33Dci* dci = &cpu->io.dci;
    bool running = dci_bcg_running(cpu);
    if (reset) {
        dci->bcg_cycle = cpu->device_cycles;
        dci->bcg_phase = 0u;
        dci->bcg_paused = !running;
        return;
    }
    if (!dci->bcg_paused && !running) {
        dci->bcg_phase = dci_bcg_phase(cpu);
        dci->bcg_cycle = cpu->device_cycles;
        dci->bcg_paused = true;
    } else if (dci->bcg_paused && running) {
        dci->bcg_cycle = cpu->device_cycles;
        dci->bcg_paused = false;
    }
}

static bool dci_output_push(Dspic33* cpu, uint16_t value, uint8_t slot, bool driven) {
    Dspic33DciQueue* queue = &cpu->io.dci.output;
    uint8_t tail;
    if (queue->count == DSPIC33_DCI_QUEUE_SIZE) {
        return false;
    }
    tail = (uint8_t)((queue->head + queue->count) % DSPIC33_DCI_QUEUE_SIZE);
    queue->transfers[tail].cycle = cpu->device_cycles;
    queue->transfers[tail].value = value;
    queue->transfers[tail].slot = slot;
    queue->transfers[tail].driven = driven;
    queue->count++;
    return true;
}

static bool dci_output_pop(Dspic33DciQueue* queue, Dspic33DciTransfer* transfer) {
    if (queue->count == 0u) {
        return false;
    }
    *transfer = queue->transfers[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % DSPIC33_DCI_QUEUE_SIZE);
    queue->count--;
    return true;
}

static void dci_refresh_status(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t status = (uint16_t)((uint16_t)dci->slot << 8u);
    if (dci->receive_overflow != 0u) {
        status |= DCI_STATUS_RECEIVE_OVERFLOW;
    }
    if (dci->receive_unread != 0u) {
        status |= DCI_STATUS_RECEIVE_FULL;
    }
    if (dci->transmit_underflow != 0u) {
        status |= DCI_STATUS_TRANSMIT_UNDERFLOW;
    }
    if (dci->transmit_empty) {
        status |= DCI_STATUS_TRANSMIT_EMPTY;
    }
    raw_write_word(cpu, DCI_STATUS, status);
}

static void dci_abort(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    dci_discard_internal_events(cpu);
    dci->generation++;
    dci->started = false;
    dci->initialized = false;
    dci->disable_pending = false;
    dci->internal_scheduled = false;
    dci->disable_frames = 0u;
    dci->buffer = 0u;
    dci->slot = 0u;
    dci->serial_input = 0u;
    dci->serial_bits = 0u;
    dci->serial_startup_bits = 0u;
    dci->serial_frame_bits = 0u;
    dci->serial_output_high = false;
    dci->serial_output_driven = false;
    dci->serial_delay = false;
    dci->pps_frame_pending = false;
    dci->receive_buffered = 0u;
    dci->transmit_buffered = 0u;
    raw_write_word(cpu, DCI_CONTROL1,
                   (uint16_t)(raw_word(cpu, DCI_CONTROL1) & ~DCI_CONTROL_ENABLE));
    dci_refresh_status(cpu);
}

static bool dci_schedule_internal(Dspic33* cpu, uint16_t source, uint64_t delay) {
    Dspic33Dci* dci = &cpu->io.dci;
    if (dci->internal_scheduled) {
        return true;
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_DCI, source, dci->generation, delay)) {
        dci_abort(cpu);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    dci->internal_scheduled = true;
    return true;
}

static bool dci_schedule_sample(Dspic33* cpu, uint64_t delay) {
    if (dspic33_schedule(cpu, DSPIC33_EVENT_DCI, DCI_EVENT_SAMPLE,
                         cpu->io.dci.generation, delay)) {
        return true;
    }
    dci_abort(cpu);
    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    return false;
}

static bool dci_begin_internal_word(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint64_t word_cycles = dci_word_cycles(cpu);
    uint64_t sample_delay =
        (raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_SAMPLE_RISING) != 0u
            ? 0u
            : dci_bit_cycles(cpu) / 2u;
    if (word_cycles == UINT64_MAX) {
        dci_abort(cpu);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    dci->serial_input = 0u;
    dci->serial_bits = 0u;
    if (!dci_schedule_internal(cpu, DCI_EVENT_INTERNAL, word_cycles)) {
        return false;
    }
    return !dci->pps_input_configured || dci_schedule_sample(cpu, sample_delay);
}

static bool dci_clock_running(const Dspic33* cpu) {
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    if (cpu->io.dci.pmd_disabled || cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE ||
           (control & DCI_CONTROL_STOP_IDLE) == 0u;
}

static bool dci_internal_event(const Dspic33Event* event) {
    return event->type == DSPIC33_EVENT_DCI &&
           (event->source == DCI_EVENT_START || event->source == DCI_EVENT_INTERNAL ||
            event->source == DCI_EVENT_SAMPLE ||
            event->source == DCI_EVENT_FRAME_START);
}

static void dci_pause_events(Dspic33* cpu) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (!dci_internal_event(event) || event->paused) {
            continue;
        }
        event->paused_remaining = event->cycle - cpu->device_cycles;
        event->paused = true;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void dci_resume_events(Dspic33* cpu) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (!dci_internal_event(event) || !event->paused) {
            continue;
        }
        if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            continue;
        }
        event->cycle = cpu->device_cycles + event->paused_remaining;
        event->paused_remaining = 0u;
        event->paused = false;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void dci_discard_internal_events(Dspic33* cpu) {
    size_t source;
    size_t destination = 0u;
    for (source = 0u; source < cpu->events.count; source++) {
        if (!dci_internal_event(&cpu->events.items[source])) {
            cpu->events.items[destination++] = cpu->events.items[source];
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
}

static void dci_update_power_state(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    bool internal = (control & DCI_CONTROL_EXTERNAL_CLOCK) == 0u;
    bool clocked =
        !internal || (raw_word(cpu, DCI_CONTROL3) != 0u && dci_clock_running(cpu));
    if (clocked) {
        dci_resume_events(cpu);
    } else {
        dci_pause_events(cpu);
    }
    dci_update_bcg(cpu, false);
}

static bool dci_dma_request(Dspic33* cpu) {
    if (dci_buffer_count(cpu) != 1u) {
        return true;
    }
    if (dspic33_dma_request(cpu, DCI_DMA_REQUEST, 0u, 0u)) {
        return true;
    }
    dci_abort(cpu);
    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    return false;
}

static bool dci_transfer_buffers(Dspic33* cpu, bool receive, bool transmit) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint8_t count = dci_buffer_count(cpu);
    uint8_t index;
    bool error = false;
    for (index = 0u; index < count; index++) {
        uint8_t bit = (uint8_t)(1u << index);
        if (receive && (dci->receive_buffered & bit) != 0u) {
            if ((dci->receive_unread & bit) != 0u) {
                dci->receive_overflow |= bit;
                error = true;
            }
            raw_write_word(cpu, (uint16_t)(DCI_RECEIVE_BASE + index * 2u),
                           dci->receive[index]);
            dci->receive_unread |= bit;
        }
        if (transmit && (dci->transmit_buffered & bit) != 0u) {
            if ((dci->transmit_written & bit) != 0u) {
                uint16_t value =
                    raw_word(cpu, (uint16_t)(DCI_TRANSMIT_BASE + index * 2u));
                dci->transmit[index] = value;
                dci->last_transmit[index] = value;
            } else {
                dci->transmit_underflow |= bit;
                dci->transmit[index] =
                    (raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_UNDERFLOW_LAST) != 0u
                        ? dci->last_transmit[index]
                        : 0u;
                error = true;
            }
            dci->transmit_written &= (uint8_t)~bit;
        }
    }
    dci->receive_buffered = 0u;
    dci->transmit_buffered = 0u;
    if (transmit) {
        dci->transmit_empty = true;
    }
    dci_refresh_status(cpu);
    if (receive || transmit) {
        dspic33_raise_interrupt(cpu, DCI_TRANSFER_IRQ);
        if (!dci_dma_request(cpu)) {
            return false;
        }
    }
    if (error) {
        dspic33_raise_interrupt(cpu, DCI_ERROR_IRQ);
    }
    return true;
}

static bool dci_startup_transfer(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    uint8_t active_transmit = dci_active_transmit_buffers(cpu);
    dci->started = true;
    dci->initialized = true;
    dci->buffer = 0u;
    dci->slot = 0u;
    dci->serial_input = 0u;
    dci->serial_bits = 0u;
    dci->serial_startup_bits = 0u;
    dci->serial_frame_bits = 0u;
    dci->serial_delay = (control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u &&
                        (control & DCI_CONTROL_EXTERNAL_FRAME) == 0u &&
                        dci_mode(cpu) < DCI_MODE_AC_LINK_16 &&
                        (control & DCI_CONTROL_DATA_JUSTIFY) == 0u;
    dci->output_frame_high = true;
    dci->transmit_buffered = active_transmit;
    return active_transmit == 0u || dci_transfer_buffers(cpu, false, true);
}

static bool dci_finish_frame(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    bool complete = true;
    if (dci->disable_pending && dci->disable_frames > 1u) {
        dci->disable_frames--;
        return true;
    }
    if (dci->disable_pending &&
        (dci->receive_buffered != 0u || dci->transmit_buffered != 0u)) {
        complete = dci_transfer_buffers(cpu, dci->receive_buffered != 0u,
                                        dci->transmit_buffered != 0u);
    }
    if (dci->disable_pending) {
        dci->generation++;
        dci->started = false;
        dci->initialized = false;
        dci->disable_pending = false;
        dci->internal_scheduled = false;
        dci->disable_frames = 0u;
        dci->buffer = 0u;
        dci->slot = 0u;
        dci_refresh_status(cpu);
    }
    return complete;
}

static bool dci_process_word(Dspic33* cpu, uint16_t input) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    uint16_t slot_mask = dci_slot_mask(cpu);
    uint16_t transmit_slots = (uint16_t)(raw_word(cpu, DCI_TRANSMIT_SLOTS) & slot_mask);
    uint16_t receive_slots = (uint16_t)(raw_word(cpu, DCI_RECEIVE_SLOTS) & slot_mask);
    uint8_t bit = (uint8_t)(1u << dci->buffer);
    uint16_t slot_bit = (uint16_t)(1u << dci->slot);
    bool transmit = (transmit_slots & slot_bit) != 0u;
    bool receive = (receive_slots & slot_bit) != 0u;
    bool driven = transmit || (control & DCI_CONTROL_TRISTATE) == 0u;
    uint16_t output =
        transmit ? (uint16_t)(dci->transmit[dci->buffer] & dci_word_mask(cpu)) : 0u;
    if (!dci_output_push(cpu, output, dci->slot, driven)) {
        dci_abort(cpu);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    if (transmit) {
        dci->transmit_buffered |= bit;
    }
    if (receive) {
        dci->receive[dci->buffer] = (control & DCI_CONTROL_LOOPBACK) != 0u
                                        ? output
                                        : (uint16_t)(input & dci_word_mask(cpu));
        dci->receive_buffered |= bit;
    }
    if (transmit || receive) {
        dci->buffer++;
        if (dci->buffer == dci_buffer_count(cpu) &&
            !dci_transfer_buffers(cpu, dci->receive_buffered != 0u,
                                  dci->transmit_buffered != 0u)) {
            return false;
        }
        if (dci->buffer == dci_buffer_count(cpu)) {
            dci->buffer = 0u;
        }
    }
    dci->slot++;
    if (dci->slot == dci_frame_count(cpu)) {
        dci->slot = 0u;
        dci->serial_frame_bits = 0u;
        if (dci_mode(cpu) == 1u && (control & DCI_CONTROL_EXTERNAL_FRAME) == 0u) {
            dci->output_frame_high = !dci->output_frame_high;
        }
        if (!dci_finish_frame(cpu)) {
            return false;
        }
        if ((control & DCI_CONTROL_EXTERNAL_FRAME) != 0u) {
            dci->started = false;
        } else if ((control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u &&
                   dci_mode(cpu) < DCI_MODE_AC_LINK_16 &&
                   (control & DCI_CONTROL_DATA_JUSTIFY) == 0u) {
            dci->serial_delay = true;
        }
    }
    dci_refresh_status(cpu);
    return true;
}

static uint8_t dci_pps_selection(const Dspic33* cpu, uint16_t address, uint8_t shift) {
    return (uint8_t)((raw_word(cpu, address) >> shift) & 0x007fu);
}

static bool dci_pps_input_high(const Dspic33* cpu, uint8_t selection) {
    const Dspic33PpsPin* mapping;
    uint16_t bit;
    if (selection == 0u) {
        return false;
    }
    mapping = pps_pin(selection);
    if (mapping == NULL || !pps_physical_input_enabled(cpu, selection)) {
        return false;
    }
    bit = (uint16_t)(1u << mapping->bit);
    return (gpio_pin_values(cpu, mapping->port) & bit) != 0u;
}

static void dci_refresh_serial_output(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    uint16_t slot_bit = (uint16_t)(1u << dci->slot);
    bool transmit = (raw_word(cpu, DCI_TRANSMIT_SLOTS) & slot_bit) != 0u;
    uint16_t output = transmit ? dci->transmit[dci->buffer] : 0u;
    dci->serial_output_driven = transmit || (control & DCI_CONTROL_TRISTATE) == 0u;
    dci->serial_output_high = dci->initialized && dci->started && !dci->serial_delay &&
                              dci->serial_bits < 16u &&
                              (output & (uint16_t)(0x8000u >> dci->serial_bits)) != 0u;
}

static void dci_begin_serial_frame(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    if (!dci->initialized && !dci_startup_transfer(cpu)) {
        return;
    }
    dci->slot = 0u;
    dci->started = true;
    dci->serial_input = 0u;
    dci->serial_bits = 0u;
    dci->serial_frame_bits = 0u;
    dci->serial_delay = dci_mode(cpu) < DCI_MODE_AC_LINK_16 &&
                        (raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_DATA_JUSTIFY) == 0u;
    dci_refresh_serial_output(cpu);
    dci_refresh_status(cpu);
}

static void dci_sample_serial_input(Dspic33* cpu, bool high) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint8_t width = dci_slot_width(cpu, dci->slot);
    if (dci->serial_bits < 16u && high) {
        dci->serial_input |= (uint16_t)(0x8000u >> dci->serial_bits);
    }
    dci->serial_bits++;
    dci->serial_frame_bits++;
    if (dci->serial_bits == width) {
        uint16_t input = dci->serial_input;
        dci->serial_input = 0u;
        dci->serial_bits = 0u;
        dci_process_word(cpu, input);
    }
}

static void dci_refresh_pps_inputs(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    bool external_clock = (control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u;
    bool operating = ((control & DCI_CONTROL_ENABLE) != 0u || dci->disable_pending) &&
                     dci_configuration_supported(cpu) && !dci->pmd_disabled &&
                     (cpu->power_state != DSPIC33_POWER_IDLE ||
                      (control & DCI_CONTROL_STOP_IDLE) == 0u);
    bool clock_high =
        dci_pps_input_high(cpu, dci_pps_selection(cpu, DCI_PPS_INPUTS, 8u));
    bool frame_high =
        dci_pps_input_high(cpu, dci_pps_selection(cpu, DCI_PPS_FRAME, 0u));
    bool clock_changed = clock_high != dci->pps_clock_high;
    bool frame_changed = frame_high != dci->pps_frame_high;
    bool sample_edge =
        clock_changed && (((control & DCI_CONTROL_SAMPLE_RISING) != 0u) == clock_high);
    bool frame_edge = (control & DCI_CONTROL_EXTERNAL_FRAME) != 0u &&
                      ((dci_mode(cpu) == DCI_MODE_I2S && frame_changed) ||
                       (dci_mode(cpu) != DCI_MODE_I2S && frame_changed && frame_high));
    dci->pps_clock_high = clock_high;
    dci->pps_frame_high = frame_high;
    if (operating && dci->initialized && frame_edge && !dci->started) {
        if (external_clock) {
            dci->pps_frame_pending = true;
        } else if (dci_clock_running(cpu) && !dci->internal_scheduled) {
            uint64_t delay = dci_mode(cpu) < DCI_MODE_AC_LINK_16 &&
                                     (control & DCI_CONTROL_DATA_JUSTIFY) != 0u
                                 ? 0u
                                 : dci_bit_cycles(cpu);
            dci->pps_frame_pending = true;
            if (delay == 0u) {
                dci->pps_frame_pending = false;
                dci_begin_serial_frame(cpu);
                dci_begin_internal_word(cpu);
            } else {
                dci_schedule_internal(cpu, DCI_EVENT_FRAME_START, delay);
            }
        }
    }
    if (!clock_changed || !external_clock || !operating) {
        return;
    }
    if (!sample_edge) {
        dci_refresh_serial_output(cpu);
        return;
    }
    if (!dci->initialized && dci->serial_startup_bits != 0u) {
        dci->serial_startup_bits--;
        if (dci->serial_startup_bits != 0u || !dci_startup_transfer(cpu)) {
            return;
        }
        if ((control & DCI_CONTROL_EXTERNAL_FRAME) != 0u) {
            dci->started = false;
        }
        dci_refresh_serial_output(cpu);
        return;
    }
    if ((control & DCI_CONTROL_EXTERNAL_FRAME) != 0u) {
        if (dci->pps_frame_pending) {
            dci->pps_frame_pending = false;
            if (!dci->started) {
                dci_begin_serial_frame(cpu);
            }
        }
        if (!dci->started) {
            return;
        }
    } else if (!dci->initialized) {
        if (!dci_startup_transfer(cpu)) {
            return;
        }
        dci_refresh_serial_output(cpu);
    }
    if (dci->serial_delay) {
        dci->serial_delay = false;
        dci->serial_frame_bits++;
        dci_refresh_serial_output(cpu);
        return;
    }
    dci_sample_serial_input(
        cpu, dci_pps_input_high(cpu, dci_pps_selection(cpu, DCI_PPS_INPUTS, 0u)));
}

static bool dci_internal_event_phase(const Dspic33* cpu, uint16_t* source,
                                     uint64_t* elapsed) {
    size_t index;
    for (index = 0u; index < cpu->events.count; index++) {
        const Dspic33Event* event = &cpu->events.items[index];
        uint64_t total;
        uint64_t remaining;
        if (event->type != DSPIC33_EVENT_DCI ||
            (event->source != DCI_EVENT_START && event->source != DCI_EVENT_INTERNAL) ||
            (uint16_t)event->value != cpu->io.dci.generation) {
            continue;
        }
        total = event->source == DCI_EVENT_START ? dci_bit_cycles(cpu) * 3u
                                                 : dci_word_cycles(cpu);
        remaining =
            event->paused ? event->paused_remaining : event->cycle - cpu->device_cycles;
        if (remaining > total) {
            return false;
        }
        *source = event->source;
        *elapsed = total - remaining;
        return true;
    }
    return false;
}

static bool dci_internal_clock_high(const Dspic33* cpu, bool* high) {
    uint64_t half_period = dci_bit_cycles(cpu) / 2u;
    if (raw_word(cpu, DCI_CONTROL3) == 0u || cpu->io.dci.pmd_disabled ||
        (raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_EXTERNAL_CLOCK) != 0u ||
        half_period == 0u) {
        return false;
    }
    *high = dci_bcg_phase(cpu) < half_period;
    return true;
}

static bool dci_data_output(const Dspic33* cpu, bool* high) {
    const Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    uint16_t slot_bit = (uint16_t)(1u << dci->slot);
    bool transmit = (raw_word(cpu, DCI_TRANSMIT_SLOTS) & slot_bit) != 0u;
    bool driven = transmit || (control & DCI_CONTROL_TRISTATE) == 0u;
    uint8_t bit = dci->serial_bits;
    uint16_t output = transmit ? dci->transmit[dci->buffer] : 0u;
    if ((control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        *high = dci->serial_output_high;
        return dci->serial_output_driven;
    }
    if (!driven) {
        return false;
    }
    {
        uint16_t source;
        uint64_t elapsed;
        if (dci_internal_event_phase(cpu, &source, &elapsed) &&
            source == DCI_EVENT_INTERNAL) {
            if ((control & DCI_CONTROL_SAMPLE_RISING) != 0u) {
                elapsed += dci_bit_cycles(cpu) / 2u;
            }
            bit = (uint8_t)(elapsed / dci_bit_cycles(cpu));
        }
    }
    *high = dci->initialized && dci->started && bit < 16u &&
            (output & (uint16_t)(0x8000u >> bit)) != 0u;
    return true;
}

static bool dci_frame_output(const Dspic33* cpu, bool* high) {
    uint8_t mode = dci_mode(cpu);
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    uint16_t source;
    uint64_t elapsed;
    uint64_t bit_cycles = dci_bit_cycles(cpu);
    bool immediate = (control & DCI_CONTROL_DATA_JUSTIFY) != 0u;
    if ((control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        if (!cpu->io.dci.initialized || !cpu->io.dci.started) {
            *high = false;
        } else if (mode == DCI_MODE_I2S) {
            *high = cpu->io.dci.output_frame_high;
        } else if (mode >= DCI_MODE_AC_LINK_16) {
            *high = cpu->io.dci.serial_frame_bits < 16u;
        } else {
            *high = cpu->io.dci.serial_delay || (immediate && cpu->io.dci.slot == 0u &&
                                                 cpu->io.dci.serial_bits == 0u);
        }
        return true;
    }
    if (!dci_internal_event_phase(cpu, &source, &elapsed)) {
        *high = false;
        return true;
    }
    if (mode == DCI_MODE_I2S) {
        *high = source == DCI_EVENT_START ? !immediate && elapsed >= bit_cycles * 2u
                                          : cpu->io.dci.output_frame_high;
        if (!immediate && source == DCI_EVENT_INTERNAL &&
            cpu->io.dci.slot + 1u == dci_frame_count(cpu) &&
            elapsed + bit_cycles >= dci_word_cycles(cpu)) {
            *high = !*high;
        }
        return true;
    }
    if (mode >= DCI_MODE_AC_LINK_16) {
        *high = (source == DCI_EVENT_START && elapsed >= bit_cycles * 2u) ||
                (source == DCI_EVENT_INTERNAL && cpu->io.dci.slot == 0u &&
                 elapsed < bit_cycles * 15u);
        return true;
    }
    if (immediate) {
        *high = source == DCI_EVENT_INTERNAL && cpu->io.dci.slot == 0u &&
                elapsed < bit_cycles;
    } else {
        *high = (source == DCI_EVENT_START && elapsed >= bit_cycles * 2u) ||
                (source == DCI_EVENT_INTERNAL &&
                 cpu->io.dci.slot + 1u == dci_frame_count(cpu) &&
                 elapsed + bit_cycles >= dci_word_cycles(cpu));
    }
    return true;
}

static void dci_run_internal(Dspic33* cpu, uint16_t generation) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    if (generation != dci->generation) {
        return;
    }
    dci->internal_scheduled = false;
    if (((control & DCI_CONTROL_ENABLE) == 0u && !dci->disable_pending) ||
        (control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        return;
    }
    if (!dci_configuration_supported(cpu) || !dci_clock_running(cpu) || !dci->started) {
        dci_begin_internal_word(cpu);
        return;
    }
    if (dci_process_word(cpu,
                         dci->pps_input_configured ? dci->serial_input : dci->input) &&
        ((raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_ENABLE) != 0u ||
         dci->disable_pending) &&
        (!(raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_EXTERNAL_FRAME) || dci->started)) {
        dci_begin_internal_word(cpu);
    }
}

static void dci_run_sample(Dspic33* cpu, uint16_t generation) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    uint8_t width = dci_slot_width(cpu, dci->slot);
    uint8_t selection;
    bool high;
    if (generation != dci->generation ||
        ((control & DCI_CONTROL_ENABLE) == 0u && !dci->disable_pending) ||
        (control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u ||
        !dci_configuration_supported(cpu) || !dci_clock_running(cpu) || !dci->started ||
        dci->serial_bits >= width) {
        return;
    }
    selection = dci_pps_selection(cpu, DCI_PPS_INPUTS, 0u);
    high = !dci->pps_input_configured
               ? dci->serial_bits < 16u &&
                     (dci->input & (uint16_t)(0x8000u >> dci->serial_bits)) != 0u
               : dci_pps_input_high(cpu, selection);
    if (dci->serial_bits < 16u && high) {
        dci->serial_input |= (uint16_t)(0x8000u >> dci->serial_bits);
    }
    dci->serial_bits++;
    if (dci->serial_bits < width) {
        dci_schedule_sample(cpu, dci_bit_cycles(cpu));
    }
}

static void dci_run_external(Dspic33* cpu, uint16_t value, bool frame_sync) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    if ((control & DCI_CONTROL_ENABLE) == 0u && !dci->disable_pending) {
        return;
    }
    if (!dci_configuration_supported(cpu) || dci->pmd_disabled ||
        (cpu->power_state == DSPIC33_POWER_IDLE &&
         (control & DCI_CONTROL_STOP_IDLE) != 0u)) {
        return;
    }
    dci->input = value;
    frame_sync = frame_sync && (control & DCI_CONTROL_EXTERNAL_FRAME) != 0u;
    if ((control & DCI_CONTROL_EXTERNAL_CLOCK) == 0u && !frame_sync) {
        return;
    }
    if (!dci->initialized) {
        if ((control & DCI_CONTROL_EXTERNAL_CLOCK) == 0u) {
            return;
        }
        dci->serial_startup_bits = 0u;
        if (!dci_startup_transfer(cpu)) {
            return;
        }
    }
    if ((control & DCI_CONTROL_EXTERNAL_FRAME) != 0u && !frame_sync &&
        dci->slot == 0u) {
        dci->started = false;
    }
    if ((control & DCI_CONTROL_EXTERNAL_FRAME) != 0u && frame_sync && !dci->started) {
        dci->slot = 0u;
        dci->started = true;
    }
    if (!dci->started) {
        if ((control & DCI_CONTROL_EXTERNAL_FRAME) != 0u) {
            return;
        }
        dci->started = true;
    }
    if ((control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        if (dci_process_word(cpu, value)) {
            dci_refresh_serial_output(cpu);
        }
    } else if (frame_sync && !dci->internal_scheduled) {
        dci_begin_internal_word(cpu);
    }
}

static void run_dci(Dspic33* cpu, uint16_t source, uint32_t value) {
    Dspic33Dci* dci = &cpu->io.dci;
    if (source == DCI_EVENT_PMD) {
        uint16_t generation = (uint16_t)(value >> DCI_EVENT_GENERATION_SHIFT);
        if (generation == dci->pmd_generation) {
            dci->pmd_disabled = (value & DCI_EVENT_DISABLED) != 0u;
            dci_update_power_state(cpu);
        }
        return;
    }
    if (source == DCI_EVENT_START) {
        if ((uint16_t)value != dci->generation) {
            return;
        }
        dci->internal_scheduled = false;
        if (dci->pmd_disabled ||
            (raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_ENABLE) == 0u) {
            return;
        }
        if (dci_startup_transfer(cpu)) {
            if ((raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_EXTERNAL_FRAME) != 0u) {
                dci->started = false;
            } else {
                dci_begin_internal_word(cpu);
            }
        }
        return;
    }
    if (source == DCI_EVENT_INTERNAL) {
        dci_run_internal(cpu, (uint16_t)value);
        return;
    }
    if (source == DCI_EVENT_SAMPLE) {
        dci_run_sample(cpu, (uint16_t)value);
        return;
    }
    if (source == DCI_EVENT_FRAME_START) {
        if ((uint16_t)value != dci->generation) {
            return;
        }
        dci->internal_scheduled = false;
        if (!dci->pps_frame_pending || dci->started ||
            !dci_configuration_supported(cpu) || !dci_clock_running(cpu)) {
            return;
        }
        dci->pps_frame_pending = false;
        dci_begin_serial_frame(cpu);
        dci_begin_internal_word(cpu);
        return;
    }
    if (source == DCI_EVENT_EXTERNAL || source == DCI_EVENT_EXTERNAL_FRAME) {
        dci_run_external(cpu, (uint16_t)value, source == DCI_EVENT_EXTERNAL_FRAME);
    }
}

static void dci_start(Dspic33* cpu) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t control = raw_word(cpu, DCI_CONTROL1);
    uint16_t clock = raw_word(cpu, DCI_CONTROL3);
    uint64_t start_delay;
    dci_discard_internal_events(cpu);
    dci->generation++;
    dci->started = false;
    dci->initialized = false;
    dci->disable_pending = false;
    dci->internal_scheduled = false;
    dci->disable_frames = 0u;
    dci->buffer = 0u;
    dci->slot = 0u;
    dci->serial_input = 0u;
    dci->serial_bits = 0u;
    dci->serial_startup_bits = (control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u ? 3u : 0u;
    dci->serial_frame_bits = 0u;
    dci->serial_output_high = false;
    dci->serial_output_driven = false;
    dci->serial_delay = false;
    dci->pps_frame_pending = false;
    dci->output_frame_high = true;
    dci->receive_buffered = 0u;
    dci->transmit_buffered = 0u;
    if (!dci_configuration_supported(cpu)) {
        return;
    }
    if ((control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        return;
    }
    if (clock == 0u) {
        return;
    }
    start_delay = dci_bit_cycles(cpu) * 3u;
    dci_schedule_internal(cpu, DCI_EVENT_START, start_delay);
}

static bool dci_frame_remaining_cycles(const Dspic33* cpu, uint64_t* remaining) {
    size_t index;
    uint8_t slot;
    uint64_t bit_cycles = dci_bit_cycles(cpu);
    for (index = 0u; index < cpu->events.count; index++) {
        const Dspic33Event* event = &cpu->events.items[index];
        if (event->type != DSPIC33_EVENT_DCI || event->source != DCI_EVENT_INTERNAL ||
            (uint16_t)event->value != cpu->io.dci.generation) {
            continue;
        }
        *remaining =
            event->paused ? event->paused_remaining : event->cycle - cpu->device_cycles;
        for (slot = (uint8_t)(cpu->io.dci.slot + 1u); slot < dci_frame_count(cpu);
             slot++) {
            *remaining += bit_cycles * dci_slot_width(cpu, slot);
        }
        return true;
    }
    return false;
}

static void dci_disable(Dspic33* cpu, uint16_t control) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint64_t remaining;
    if (!dci->initialized || !dci->started) {
        dci_abort(cpu);
        return;
    }
    dci->disable_pending = true;
    dci->disable_frames = 1u;
    if ((control & DCI_CONTROL_EXTERNAL_CLOCK) != 0u) {
        uint8_t slot;
        remaining = dci_slot_width(cpu, dci->slot) - dci->serial_bits;
        for (slot = (uint8_t)(dci->slot + 1u); slot < dci_frame_count(cpu); slot++) {
            remaining += dci_slot_width(cpu, slot);
        }
        if (remaining < 3u) {
            dci->disable_frames = 2u;
        }
    } else {
        if (!dci_frame_remaining_cycles(cpu, &remaining)) {
            dci_abort(cpu);
            return;
        }
        if (remaining < dci_bit_cycles(cpu) * 3u) {
            dci->disable_frames = 2u;
        }
    }
}

static void dci_update_pmd(Dspic33* cpu, uint16_t previous) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t current = raw_word(cpu, DCI_PMD_ADDRESS);
    bool disabled;
    if (((previous ^ current) & DCI_PMD) == 0u) {
        return;
    }
    disabled = (current & DCI_PMD) != 0u;
    dci->pmd_generation++;
    if (!dspic33_schedule(
            cpu, DSPIC33_EVENT_DCI, DCI_EVENT_PMD,
            ((uint32_t)dci->pmd_generation << DCI_EVENT_GENERATION_SHIFT) |
                (disabled ? DCI_EVENT_DISABLED : 0u),
            dspic33_device_instruction_cycles(cpu, 1u))) {
        raw_write_word(cpu, DCI_PMD_ADDRESS, previous);
        dci->pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void update_dci_register(Dspic33* cpu, uint16_t address, uint16_t previous) {
    Dspic33Dci* dci = &cpu->io.dci;
    if (address == DCI_PMD_ADDRESS) {
        dci_update_pmd(cpu, previous);
        return;
    }
    if (address == DCI_PPS_INPUTS) {
        dci->pps_input_configured = true;
        return;
    }
    if (address < DCI_BASE || address > DCI_TRANSMIT_BASE + 6u) {
        return;
    }
    if (dci->pmd_disabled) {
        raw_write_word(cpu, address, previous);
        return;
    }
    if (address == DCI_CONTROL1) {
        bool was_enabled = (previous & DCI_CONTROL_ENABLE) != 0u;
        bool enabled = (raw_word(cpu, DCI_CONTROL1) & DCI_CONTROL_ENABLE) != 0u;
        if (!was_enabled && enabled) {
            dci_start(cpu);
        } else if (was_enabled && !enabled) {
            dci_disable(cpu, previous);
        }
        dci_update_power_state(cpu);
        return;
    }
    if (address == DCI_CONTROL3) {
        dci_update_bcg(cpu, true);
        dci_update_power_state(cpu);
        return;
    }
    if (address >= DCI_TRANSMIT_BASE && address <= DCI_TRANSMIT_BASE + 6u) {
        uint8_t index = (uint8_t)((address - DCI_TRANSMIT_BASE) / 2u);
        uint8_t bit = (uint8_t)(1u << index);
        if (index < dci_buffer_count(cpu)) {
            dci->transmit_written |= bit;
            dci->transmit_underflow &= (uint8_t)~bit;
        }
        if ((dci_active_transmit_buffers(cpu) & bit) != 0u) {
            dci->transmit_empty = false;
        }
        dci_refresh_status(cpu);
    }
}

static bool dci_read_register(Dspic33* cpu, uint16_t address, uint8_t* value) {
    Dspic33Dci* dci = &cpu->io.dci;
    uint16_t base = (uint16_t)(address & 0xfffeu);
    if (base < DCI_BASE || base > DCI_TRANSMIT_BASE + 6u || base == 0x028au ||
        base == 0x028eu) {
        return false;
    }
    if (dci->pmd_disabled) {
        *value = 0u;
        return true;
    }
    if (base >= DCI_TRANSMIT_BASE) {
        *value = 0u;
        return true;
    }
    if (base >= DCI_RECEIVE_BASE && base <= DCI_RECEIVE_BASE + 6u &&
        ((!cpu->io.cpu_read_valid && !cpu->io.dma_transfer_active) ||
         (cpu->io.dma_transfer_active && cpu->io.dma_transfer_width == 1u) ||
         (!cpu->io.dma_transfer_active && cpu->io.cpu_read_width == 1u) ||
         (cpu->io.dma_transfer_active && cpu->io.dma_transfer_width == 2u &&
          (address & 1u) != 0u) ||
         (!cpu->io.dma_transfer_active && cpu->io.cpu_read_valid &&
          address == cpu->io.cpu_read_address + 1u))) {
        uint8_t index = (uint8_t)((base - DCI_RECEIVE_BASE) / 2u);
        uint8_t bit = (uint8_t)(1u << index);
        dci->receive_unread &= (uint8_t)~bit;
        dci->receive_overflow &= (uint8_t)~bit;
        dci_refresh_status(cpu);
    }
    return true;
}

static bool comparator_pin_channel(const Dspic33* cpu, uint8_t pin,
                                   uint8_t* comparator) {
    size_t index;
    for (index = 0u; index < sizeof(pps_outputs) / sizeof(pps_outputs[0]); index++) {
        if (pps_outputs[index].pin == pin) {
            uint8_t function = (uint8_t)((raw_word(cpu, pps_outputs[index].address) >>
                                          pps_outputs[index].shift) &
                                         0x003fu);
            if (function >= COMPARATOR_PPS_FUNCTION &&
                function < COMPARATOR_PPS_FUNCTION + DSPIC33_COMPARATOR_COUNT) {
                *comparator = (uint8_t)(function - COMPARATOR_PPS_FUNCTION);
                return true;
            }
            return false;
        }
    }
    return false;
}

static bool can_queue_push(Dspic33CanQueue* queue, const Dspic33CanFrame* frame) {
    uint8_t index;
    if (queue->count == 64u) {
        return false;
    }
    index = (uint8_t)((queue->head + queue->count) % 64u);
    queue->frames[index] = *frame;
    queue->count++;
    return true;
}

static bool can_queue_pop(Dspic33CanQueue* queue, Dspic33CanFrame* frame) {
    if (queue->count == 0u) {
        return false;
    }
    *frame = queue->frames[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % 64u);
    queue->count--;
    return true;
}

static bool event_less(const Dspic33Event* left, const Dspic33Event* right) {
    bool left_dma_completion;
    bool right_dma_completion;
    if (left->paused != right->paused) {
        return !left->paused;
    }
    if (left->cycle != right->cycle) {
        return left->cycle < right->cycle;
    }
    if (left->type == DSPIC33_EVENT_DMA && right->type == DSPIC33_EVENT_DMA &&
        left->source != right->source) {
        left_dma_completion = left->source >= DSPIC33_DMA_COUNT;
        right_dma_completion = right->source >= DSPIC33_DMA_COUNT;
        if (left_dma_completion != right_dma_completion) {
            return left_dma_completion;
        }
        return left->source % DSPIC33_DMA_COUNT < right->source % DSPIC33_DMA_COUNT;
    }
    return left->sequence < right->sequence;
}

static bool event_reserve(Dspic33EventQueue* queue) {
    Dspic33Event* items;
    size_t capacity;
    if (queue->count < queue->capacity) {
        return true;
    }
    capacity = queue->capacity == 0u ? 64u : queue->capacity * 2u;
    items = realloc(queue->items, capacity * sizeof(*items));
    if (items == NULL) {
        return false;
    }
    queue->items = items;
    queue->capacity = capacity;
    return true;
}

bool dspic33_schedule(Dspic33* cpu, Dspic33EventType type, uint16_t source,
                      uint32_t value, uint64_t delay) {
    Dspic33Event event;
    size_t index;
    size_t parent;
    if (delay > UINT64_MAX - cpu->device_cycles) {
        return false;
    }
    if (!event_reserve(&cpu->events)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    event.cycle = cpu->device_cycles + delay;
    event.sequence = cpu->events.sequence++;
    event.paused_remaining = 0u;
    event.value = value;
    event.source = source;
    event.type = type;
    event.paused = false;
    index = cpu->events.count++;
    while (index != 0u) {
        parent = (index - 1u) / 2u;
        if (!event_less(&event, &cpu->events.items[parent])) {
            break;
        }
        cpu->events.items[index] = cpu->events.items[parent];
        index = parent;
    }
    cpu->events.items[index] = event;
    return true;
}

void dspic33_reorder_events(Dspic33* cpu) {
    size_t parent;
    if (cpu->events.count < 2u) {
        return;
    }
    for (parent = cpu->events.count / 2u; parent != 0u; parent--) {
        Dspic33Event event = cpu->events.items[parent - 1u];
        size_t index = parent - 1u;
        size_t child = index * 2u + 1u;
        while (child < cpu->events.count) {
            if (child + 1u < cpu->events.count &&
                event_less(&cpu->events.items[child + 1u], &cpu->events.items[child])) {
                child++;
            }
            if (!event_less(&cpu->events.items[child], &event)) {
                break;
            }
            cpu->events.items[index] = cpu->events.items[child];
            index = child;
            child = index * 2u + 1u;
        }
        cpu->events.items[index] = event;
    }
}

static Dspic33Event event_pop(Dspic33EventQueue* queue) {
    Dspic33Event result = queue->items[0];
    Dspic33Event tail = queue->items[--queue->count];
    size_t index = 0u;
    while (index * 2u + 1u < queue->count) {
        size_t child = index * 2u + 1u;
        if (child + 1u < queue->count &&
            event_less(&queue->items[child + 1u], &queue->items[child])) {
            child++;
        }
        if (!event_less(&queue->items[child], &tail)) {
            break;
        }
        queue->items[index] = queue->items[child];
        index = child;
    }
    if (queue->count != 0u) {
        queue->items[index] = tail;
    }
    return result;
}

static void update_nested_do_interrupt_request(Dspic33* cpu, uint16_t irq);

void dspic33_raise_interrupt(Dspic33* cpu, uint16_t irq) {
    uint16_t address;
    uint16_t mask;
    uint16_t value;
    if (irq >= DSPIC33_IRQ_COUNT) {
        return;
    }
    address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    mask = (uint16_t)(1u << (irq % 16u));
    value = raw_word(cpu, address);
    raw_write_word(cpu, address, (uint16_t)(value | mask));
    if ((value & mask) == 0u) {
        update_nested_do_interrupt_request(cpu, irq);
    }
}

static void raise_external_interrupt(Dspic33* cpu, uint8_t channel) {
    if (channel == 0u) {
        uint8_t module;
        dspic33_dma_request(cpu, 0u, 0u, 0u);
        for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
            dspic33_adc_trigger(cpu, module, 1u, 0u);
        }
    } else if (channel == 1u) {
        output_compare_pulse_source(cpu, OUTPUT_COMPARE_SYNC_INT1);
    } else if (channel == 2u) {
        output_compare_pulse_source(cpu, OUTPUT_COMPARE_SYNC_INT2);
    }
    dspic33_raise_interrupt(cpu, external_interrupt_irqs[channel]);
}

static void raise_scheduled_interrupt(Dspic33* cpu, uint16_t irq) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_EXTERNAL_INTERRUPT_COUNT; channel++) {
        if (external_interrupt_irqs[channel] == irq) {
            raise_external_interrupt(cpu, channel);
            return;
        }
    }
    dspic33_raise_interrupt(cpu, irq);
}

static uint8_t interrupt_priority(const Dspic33* cpu, uint16_t irq) {
    uint16_t value = raw_word(cpu, (uint16_t)(0x0840u + (irq / 4u) * 2u));
    return (uint8_t)((value >> ((irq % 4u) * 4u)) & 0x07u);
}

static bool interrupt_enabled(const Dspic33* cpu, uint16_t irq) {
    uint16_t mask = (uint16_t)(1u << (irq % 16u));
    uint16_t offset = (uint16_t)((irq / 16u) * 2u);
    return (raw_word(cpu, (uint16_t)(0x0800u + offset)) & mask) != 0u &&
           (raw_word(cpu, (uint16_t)(0x0820u + offset)) & mask) != 0u;
}

static bool interrupt_deferred(const Dspic33* cpu, uint16_t irq) {
    return (cpu->interrupt_deferred[irq / 16u] & (uint16_t)(1u << (irq % 16u))) != 0u;
}

void dspic33_device_latch_interrupt(Dspic33* cpu, uint8_t vector, uint8_t priority) {
    raw_write_word(cpu, 0x08c8u, (uint16_t)(((uint16_t)priority << 8u) | vector));
}

void dspic33_device_latch_math_error(Dspic33* cpu, uint16_t cause) {
    raw_write_word(cpu, 0x08c0u, (uint16_t)(raw_word(cpu, 0x08c0u) | cause | 0x0010u));
    dspic33_set_math_error_source(cpu, true);
}

static bool select_interrupt(const Dspic33* cpu, uint16_t* selected_irq,
                             uint8_t* selected_priority) {
    uint8_t current = (uint8_t)((cpu->sr >> 5u) & 0x07u);
    uint8_t best_priority = current;
    uint16_t best_irq = DSPIC33_IRQ_COUNT;
    uint16_t irq;
    uint16_t group;
    if (!cpu->async_events_enabled ||
        ((raw_word(cpu, 0x08c2u) & 0x8000u) == 0u && cpu->gie_disable_deferred == 0u) ||
        (cpu->corcon & 0x0008u) != 0u) {
        return false;
    }
    for (group = 0u; group < (DSPIC33_IRQ_COUNT + 15u) / 16u; group++) {
        uint16_t offset = (uint16_t)(group * 2u);
        if ((raw_word(cpu, (uint16_t)(0x0800u + offset)) &
             raw_word(cpu, (uint16_t)(0x0820u + offset))) != 0u) {
            break;
        }
    }
    if (group == (DSPIC33_IRQ_COUNT + 15u) / 16u) {
        return false;
    }
    for (irq = 0u; irq < DSPIC33_IRQ_COUNT; irq++) {
        uint8_t priority;
        if (!interrupt_enabled(cpu, irq) || interrupt_deferred(cpu, irq)) {
            continue;
        }
        priority = interrupt_priority(cpu, irq);
        if (cpu->disicnt != 0u && priority < 7u) {
            continue;
        }
        if (priority > best_priority) {
            best_priority = priority;
            best_irq = irq;
        }
    }
    if (best_irq == DSPIC33_IRQ_COUNT) {
        return false;
    }
    *selected_irq = best_irq;
    *selected_priority = best_priority;
    return true;
}

static void recover_from_doze(Dspic33* cpu) {
    if ((raw_word(cpu, MAIN_CLOCK_DIVISOR) & 0x8000u) != 0u) {
        raw_write_word(cpu, MAIN_CLOCK_DIVISOR,
                       (uint16_t)(raw_word(cpu, MAIN_CLOCK_DIVISOR) & ~0x0800u));
    }
}

static void update_nested_do_interrupt_request(Dspic33* cpu, uint16_t irq) {
    uint8_t depth = cpu->nested_do_interrupt_depth;
    uint8_t priority;
    uint16_t selected_irq;
    if ((raw_word(cpu, 0x08c0u) & 0x8000u) != 0u) {
        cpu->nested_do_interrupt_armed = false;
        return;
    }
    if (!select_interrupt(cpu, &selected_irq, &priority) || selected_irq != irq) {
        return;
    }
    if (cpu->nested_do_interrupt_armed) {
        if (priority > cpu->nested_do_interrupt_priority &&
            cpu->device_cycles - cpu->nested_do_interrupt_cycle ==
                dspic33_device_instruction_cycles(cpu, 4u) &&
            depth != 0u && cpu->do_depth >= depth &&
            cpu->do_end[depth - 1u] == cpu->nested_do_interrupt_end) {
            cpu->nested_do_extra_decrement_depth = depth;
            cpu->nested_do_extra_decrement_end = cpu->nested_do_interrupt_end;
        }
        cpu->nested_do_interrupt_armed = false;
        return;
    }
    if (cpu->interrupt_entry_active) {
        return;
    }
    depth = cpu->do_depth;
    if (depth != 0u) {
        uint32_t end = cpu->do_end[depth - 1u];
        uint32_t previous = (end - 2u) & 0x007ffffeu;
        uint32_t origin =
            cpu->instruction_advancing ? cpu->current_instruction_pc : cpu->pc;
        if (origin == end || origin == previous) {
            cpu->nested_do_interrupt_cycle = cpu->device_cycles;
            cpu->nested_do_interrupt_end = end;
            cpu->nested_do_interrupt_depth = depth;
            cpu->nested_do_interrupt_priority = priority;
            cpu->nested_do_interrupt_armed = true;
        }
    }
}

static bool service_interrupt(Dspic33* cpu) {
    uint8_t best_priority;
    uint16_t best_irq;
    uint16_t next_priority;
    size_t log_index;
    uint16_t stacked_high;
    uint64_t entry_device_cycles;
    uint32_t origin;
    uint32_t target;
    if (!select_interrupt(cpu, &best_irq, &best_priority)) {
        return false;
    }
    origin = cpu->pc;
    target = dspic33_read_program_word(cpu, origin >= DSPIC33_AUXILIARY_PROGRAM_BASE
                                                ? 0x007ffffau
                                                : 0x0014u + best_irq * 2u) &
             0x007ffffeu;
    if (!dspic33_program_range_implemented(target, 2u)) {
        dspic33_raise_program_vector_error(cpu, origin);
        return true;
    }
    if (!dspic33_codeguard_admit_program_flow(cpu, origin, target)) {
        return true;
    }
    recover_from_doze(cpu);
    entry_device_cycles = dspic33_device_instruction_cycles(cpu, 9u);
    cpu->interrupt_entry_active = true;
    if (!dspic33_device_advance_instruction(cpu, 9u, entry_device_cycles)) {
        cpu->interrupt_entry_active = false;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return true;
    }
    cpu->interrupt_entry_active = false;
    if (cpu->illegal_reset) {
        return true;
    }
    dspic33_check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu, 2u);
    dspic33_write_word(cpu, cpu->w[15],
                       (uint16_t)((cpu->pc & 0xfffeu) | ((cpu->corcon >> 2u) & 1u)));
    dspic33_set_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    stacked_high = (uint16_t)(((cpu->sr & 0x00ffu) << 8u) |
                              ((cpu->corcon & 0x0008u) != 0u ? 0x0080u : 0u) |
                              ((cpu->pc >> 16u) & 0x007fu));
    dspic33_check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu, 2u);
    dspic33_write_word(cpu, cpu->w[15], stacked_high);
    dspic33_set_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    cpu->corcon &= (uint16_t)~0x0004u;
    next_priority = (raw_word(cpu, 0x08c0u) & 0x8000u) != 0u
                        ? UINT16_C(0x00e0)
                        : (uint16_t)((uint16_t)best_priority << 5u);
    cpu->sr = (uint16_t)((cpu->sr & ~0x00e0u) | next_priority);
    log_index = (size_t)(cpu->interrupt_count % 16u);
    cpu->interrupt_log_irq[log_index] = best_irq;
    cpu->interrupt_log_entry[log_index] = cpu->pc;
    cpu->interrupt_log_return[log_index] = 0u;
    cpu->pc = target;
    cpu->last_interrupt = best_irq;
    cpu->interrupt_count++;
    cpu->interrupt_depth++;
    dspic33_device_latch_interrupt(cpu, (uint8_t)(best_irq + 8u), best_priority);
    cpu->repeat_active = 0u;
    cpu->repeat_pc = 0u;
    cpu->sr &= (uint16_t)~0x0010u;
    return true;
}

bool dspic33_device_interrupt_pending(const Dspic33* cpu) {
    uint8_t priority;
    uint16_t irq;
    return select_interrupt(cpu, &irq, &priority);
}

bool dspic33_device_service_interrupt(Dspic33* cpu) { return service_interrupt(cpu); }

bool dspic33_device_wake(Dspic33* cpu) {
    uint16_t irq;
    if (!cpu->async_events_enabled) {
        return false;
    }
    for (irq = 0u; irq < DSPIC33_IRQ_COUNT; irq++) {
        if (interrupt_enabled(cpu, irq) && !interrupt_deferred(cpu, irq) &&
            interrupt_priority(cpu, irq) != 0u) {
            service_interrupt(cpu);
            if (cpu->illegal_reset) {
                return true;
            }
            recover_from_doze(cpu);
            return true;
        }
    }
    return false;
}

void dspic33_device_return_interrupt(Dspic33* cpu) {
    uint16_t high;
    uint16_t low;
    dspic33_check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u, 2u);
    dspic33_set_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
    high = dspic33_read_word(cpu, cpu->w[15]);
    dspic33_check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u, 2u);
    dspic33_set_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
    low = dspic33_read_word(cpu, cpu->w[15]);
    cpu->pc = ((uint32_t)(high & 0x007fu) << 16u) | (low & 0xfffeu);
    cpu->last_interrupt_return = cpu->pc;
    if (cpu->interrupt_count != 0u) {
        cpu->interrupt_log_return[(cpu->interrupt_count - 1u) % 16u] = cpu->pc;
    }
    cpu->sr = (uint16_t)((cpu->sr & 0xff00u) | (high >> 8u));
    if ((high & 0x0080u) != 0u) {
        cpu->corcon |= 0x0008u;
    } else {
        cpu->corcon &= (uint16_t)~0x0008u;
    }
    cpu->corcon = (uint16_t)((cpu->corcon & ~0x0004u) | ((low & 1u) << 2u));
    cpu->repeat_active = (cpu->sr & 0x0010u) != 0u;
    cpu->repeat_pc = cpu->repeat_active != 0u ? cpu->pc : 0u;
    cpu->repeat_psv_reentry = cpu->repeat_active != 0u;
    if (cpu->interrupt_depth != 0u) {
        cpu->interrupt_depth--;
    }
}

static uint16_t dma_channel_base(uint8_t channel) {
    return (uint16_t)(DMA_CHANNEL_BASE + channel * DMA_CHANNEL_STRIDE);
}

static uint16_t dma_channel_bit(uint8_t channel) { return (uint16_t)(1u << channel); }

static uint32_t dma_start_address(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.dma_bank & dma_channel_bit(channel)) != 0u
               ? cpu->io.dma_start_b[channel]
               : cpu->io.dma_start_a[channel];
}

static uint32_t dma_transfer_address(const Dspic33* cpu, uint8_t channel,
                                     uint16_t control, uint16_t indirect_address) {
    uint32_t start = dma_start_address(cpu, channel);
    uint16_t mode = control & DMA_CON_AMODE_MASK;
    if (mode == DMA_CON_AMODE_PERIPHERAL) {
        return start | indirect_address;
    }
    return cpu->io.dma_address[channel];
}

static bool dma_memory_address_valid(uint32_t address, uint8_t width) {
    return address < DSPIC33_DATA_SIZE &&
           (width == 1u || address + 1u < DSPIC33_DATA_SIZE);
}

static bool dma_write_cycle_matches(const Dspic33* cpu) {
    return cpu->io.cpu_write_valid && (cpu->io.cpu_write_cycle == cpu->cycles ||
                                       (cpu->io.cpu_write_cycle != UINT64_MAX &&
                                        cpu->io.cpu_write_cycle + 1u == cpu->cycles));
}

static bool dma_cpu_wrote_byte(const Dspic33* cpu, uint32_t address) {
    return dma_write_cycle_matches(cpu) && address >= cpu->io.cpu_write_address &&
           address < cpu->io.cpu_write_address + cpu->io.cpu_write_width;
}

static uint8_t dma_read_memory_byte(const Dspic33* cpu, uint32_t address) {
    if (dma_cpu_wrote_byte(cpu, address)) {
        uint8_t shift = (uint8_t)((address - cpu->io.cpu_write_address) * 8u);
        return (uint8_t)(cpu->io.cpu_write_previous >> shift);
    }
    return cpu->data[address];
}

static uint16_t dma_read_memory(const Dspic33* cpu, uint32_t address, uint8_t width) {
    if (!dma_memory_address_valid(address, width)) {
        return 0u;
    }
    if (width == 1u) {
        return dma_read_memory_byte(cpu, address);
    }
    return (uint16_t)(dma_read_memory_byte(cpu, address) |
                      ((uint16_t)dma_read_memory_byte(cpu, address + 1u) << 8u));
}

static void dma_write_memory(Dspic33* cpu, uint32_t address, uint8_t width,
                             uint16_t value) {
    if (!dma_memory_address_valid(address, width)) {
        return;
    }
    if (!dma_cpu_wrote_byte(cpu, address)) {
        cpu->data[address] = (uint8_t)value;
    }
    if (width == 2u && !dma_cpu_wrote_byte(cpu, address + 1u)) {
        cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    }
}

static void dma_record_transfer(Dspic33* cpu, uint8_t channel, uint16_t control,
                                uint32_t address) {
    uint16_t bit = dma_channel_bit(channel);
    uint16_t base = dma_channel_base(channel);
    uint16_t start_offset = (cpu->io.dma_bank & bit) != 0u ? 8u : 4u;
    uint16_t ping_pong = raw_word(cpu, DMA_PPS);
    if ((cpu->io.dma_bank & bit) != 0u) {
        ping_pong |= bit;
    } else {
        ping_pong &= (uint16_t)~bit;
    }
    raw_write_word(cpu, DMA_PPS, (uint16_t)(ping_pong & DMA_CHANNEL_MASK));
    raw_write_word(cpu, DMA_LCA, channel);
    raw_write_word(cpu, DMA_SADRL, (uint16_t)address);
    raw_write_word(cpu, DMA_SADRH, (uint16_t)((address >> 16u) & 0x00ffu));
    if ((control & DMA_CON_AMODE_MASK) == DMA_CON_AMODE_PERIPHERAL) {
        raw_write_word(cpu, (uint16_t)(base + start_offset), (uint16_t)address);
        raw_write_word(cpu, (uint16_t)(base + start_offset + 2u),
                       (uint16_t)((address >> 16u) & 0x00ffu));
    }
}

static bool dma_write_collision(const Dspic33* cpu, uint16_t pad, uint8_t width) {
    uint32_t previous_end;
    uint32_t dma_end;
    if (!dma_write_cycle_matches(cpu)) {
        return false;
    }
    previous_end = cpu->io.cpu_write_address + cpu->io.cpu_write_width;
    dma_end = (uint32_t)pad + width;
    return cpu->io.cpu_write_address < dma_end && pad < previous_end;
}

static bool dma_can_write_pad(uint16_t pad) { return pad == 0x0442u || pad == 0x0542u; }

static void dma_peripheral_write_collision(Dspic33* cpu, uint8_t channel) {
    uint16_t status = raw_word(cpu, DMA_PWC);
    uint16_t bit = dma_channel_bit(channel);
    if ((status & bit) == 0u) {
        raw_write_word(cpu, DMA_PWC, (uint16_t)(status | bit));
        dspic33_raise_dma_collision_trap(cpu);
    }
}

static uint16_t uart_dma_error_bits(const Dspic33* cpu, uint16_t address) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        if (address == uart_bases[channel] + 6u) {
            uint16_t status = raw_word(cpu, (uint16_t)(uart_bases[channel] + 2u));
            return (
                uint16_t)(((status & UART_STATUS_PARITY_ERROR) != 0u ? 0x0800u : 0u) |
                          ((status & UART_STATUS_FRAMING_ERROR) != 0u ? 0x0400u : 0u));
        }
    }
    return 0u;
}

static bool dma_pad_in_set(uint16_t pad, const uint16_t* pads, size_t count) {
    size_t index;
    for (index = 0u; index < count; index++) {
        if (pad == pads[index]) {
            return true;
        }
    }
    return false;
}

static bool dma_pad_readable(uint16_t pad) {
    static const uint16_t pads[] = {0x0144u, 0x014cu, 0x0154u, 0x015cu, 0x0226u,
                                    0x0236u, 0x0248u, 0x0256u, 0x0268u, 0x0290u,
                                    0x02a8u, 0x02b6u, 0x02c8u, 0x0300u, 0x0340u,
                                    0x0440u, 0x0540u, 0x0608u};
    return dma_pad_in_set(pad, pads, sizeof(pads) / sizeof(pads[0]));
}

static bool dma_pad_writable(uint16_t pad) {
    static const uint16_t pads[] = {0x0224u, 0x0234u, 0x0248u, 0x0254u, 0x0268u,
                                    0x0298u, 0x02a8u, 0x02b4u, 0x02c8u, 0x0442u,
                                    0x0542u, 0x0608u, 0x0904u, 0x0906u, 0x090eu,
                                    0x0910u, 0x0918u, 0x091au, 0x0922u, 0x0924u};
    return dma_pad_in_set(pad, pads, sizeof(pads) / sizeof(pads[0]));
}

bool dspic33_device_dma_pad_valid(uint16_t pad, bool write) {
    return write ? dma_pad_writable(pad) : dma_pad_readable(pad);
}

static void dma_disable_channel(Dspic33* cpu, uint8_t channel, uint16_t control) {
    uint16_t base = dma_channel_base(channel);
    uint16_t bit = dma_channel_bit(channel);
    raw_write_word(cpu, base, (uint16_t)(control & ~DMA_CON_CHEN));
    raw_write_word(cpu, (uint16_t)(base + 2u),
                   (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
    cpu->io.dma_enabled &= (uint16_t)~bit;
    cpu->io.dma_forced_pending &= (uint16_t)~bit;
    cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
    cpu->io.dma_active &= (uint16_t)~bit;
    cpu->io.dma_generation[channel]++;
}

static void dma_complete_block(Dspic33* cpu, uint8_t channel, uint16_t control) {
    uint16_t bit = dma_channel_bit(channel);
    uint16_t mode = control & DMA_CON_MODE_MASK;
    bool secondary = (cpu->io.dma_bank & bit) != 0u;
    if ((control & DMA_CON_HALF) == 0u) {
        dspic33_raise_interrupt(cpu, dma_irqs[channel]);
    }
    cpu->io.dma_index[channel] = 0u;
    cpu->io.dma_half_raised &= (uint16_t)~bit;
    if ((mode & DMA_CON_MODE_ONE_SHOT) != 0u &&
        ((mode & DMA_CON_MODE_PING_PONG) == 0u || secondary)) {
        dma_disable_channel(cpu, channel, control);
        return;
    }
    if ((mode & DMA_CON_MODE_PING_PONG) != 0u) {
        cpu->io.dma_bank ^= bit;
    }
    cpu->io.dma_address[channel] = dma_start_address(cpu, channel);
}

static void complete_dma_transfer(Dspic33* cpu, uint8_t channel, uint32_t event_value) {
    uint16_t base = dma_channel_base(channel);
    uint16_t bit = dma_channel_bit(channel);
    uint16_t control = raw_word(cpu, base);
    uint16_t count = raw_word(cpu, (uint16_t)(base + 0x0eu));
    uint16_t index = cpu->io.dma_index[channel];
    uint16_t transferred = (uint16_t)(index + 1u);
    uint16_t half = (uint16_t)(((uint32_t)count + 2u) / 2u);
    uint8_t width = (control & DMA_CON_SIZE_BYTE) != 0u ? 1u : 2u;
    bool forced = (event_value & DMA_EVENT_FORCE) != 0u;
    if ((cpu->io.dma_active & bit) == 0u) {
        return;
    }
    if ((control & DMA_CON_AMODE_MASK) == 0u) {
        cpu->io.dma_address[channel] += width;
    }
    if ((control & DMA_CON_HALF) != 0u && transferred == half &&
        (cpu->io.dma_half_raised & bit) == 0u) {
        cpu->io.dma_half_raised |= bit;
        dspic33_raise_interrupt(cpu, dma_irqs[channel]);
    }
    if (index >= count) {
        dma_complete_block(cpu, channel, control);
    } else {
        cpu->io.dma_index[channel]++;
    }
    cpu->io.dma_active &= (uint16_t)~bit;
    if (forced) {
        cpu->io.dma_forced_pending &= (uint16_t)~bit;
        raw_write_word(
            cpu, (uint16_t)(base + 2u),
            (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
    }
}

static void run_dma(Dspic33* cpu, uint16_t source, uint32_t event_value) {
    uint16_t base;
    uint16_t bit;
    uint16_t control;
    uint16_t pad;
    uint16_t value;
    uint16_t uart_errors;
    uint16_t generation;
    uint32_t address;
    uint8_t width;
    uint8_t channel;
    bool forced;
    bool completion;
    if (source >= DSPIC33_DMA_COUNT * 2u) {
        return;
    }
    completion = source >= DSPIC33_DMA_COUNT;
    channel = (uint8_t)(source % DSPIC33_DMA_COUNT);
    bit = dma_channel_bit(channel);
    forced = (event_value & DMA_EVENT_FORCE) != 0u;
    generation = (uint16_t)((event_value >> DMA_EVENT_GENERATION_SHIFT) &
                            DMA_EVENT_GENERATION_MASK);
    if (generation != (cpu->io.dma_generation[channel] & DMA_EVENT_GENERATION_MASK)) {
        return;
    }
    base = dma_channel_base(channel);
    if (completion) {
        complete_dma_transfer(cpu, channel, event_value);
        return;
    }
    if ((cpu->io.dma_active & bit) != 0u) {
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_DMA, source, event_value, 1u)) {
            if (forced) {
                cpu->io.dma_forced_pending &= (uint16_t)~bit;
            } else {
                cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
            }
        }
        return;
    }
    if (!forced) {
        cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
    }
    control = raw_word(cpu, base);
    if ((control & DMA_CON_CHEN) == 0u || (raw_word(cpu, DMA_PWC) & bit) != 0u) {
        if (forced) {
            cpu->io.dma_forced_pending &= (uint16_t)~bit;
            raw_write_word(
                cpu, (uint16_t)(base + 2u),
                (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
        }
        return;
    }
    pad = raw_word(cpu, (uint16_t)(base + 0x0cu));
    width = (control & DMA_CON_SIZE_BYTE) != 0u ? 1u : 2u;
    address = dma_transfer_address(cpu, channel, control, (uint16_t)event_value);
    if (!dma_memory_address_valid(address, width)) {
        dspic33_raise_dma_address_trap(cpu);
        if (forced) {
            cpu->io.dma_forced_pending &= (uint16_t)~bit;
            raw_write_word(
                cpu, (uint16_t)(base + 2u),
                (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
        }
        return;
    }
    dma_record_transfer(cpu, channel, control, address);
    cpu->io.dma_active |= bit;
    cpu->io.dma_transfer_width = width;
    cpu->io.dma_transfer_active = true;
    if ((control & DMA_CON_RAM_TO_PERIPHERAL) != 0u) {
        value = dma_read_memory(cpu, address, width);
        if (dma_pad_writable(pad)) {
            if (dma_write_collision(cpu, pad, width)) {
                if (!dma_can_write_pad(pad)) {
                    dma_peripheral_write_collision(cpu, channel);
                }
            } else if (width == 1u) {
                dspic33_write_byte(cpu, pad, (uint8_t)value);
            } else {
                dspic33_write_word(cpu, pad, value);
            }
        }
    } else {
        value = 0u;
        if (dma_pad_readable(pad)) {
            uart_errors = width == 2u ? uart_dma_error_bits(cpu, pad) : 0u;
            value =
                width == 1u ? dspic33_read_byte(cpu, pad) : dspic33_read_word(cpu, pad);
            value |= uart_errors;
        }
        dma_write_memory(cpu, address, width, value);
        if ((control & DMA_CON_NULL_WRITE) != 0u && dma_pad_writable(pad)) {
            if (dma_write_collision(cpu, pad, width)) {
                if (!dma_can_write_pad(pad)) {
                    dma_peripheral_write_collision(cpu, channel);
                }
            } else if (width == 1u) {
                dspic33_write_byte(cpu, pad, 0u);
            } else {
                dspic33_write_word(cpu, pad, 0u);
            }
        }
    }
    cpu->io.dma_transfer_active = false;
    cpu->io.dma_transfer_width = 0u;
    if ((cpu->io.dma_active & bit) != 0u &&
        !dspic33_schedule(cpu, DSPIC33_EVENT_DMA,
                          (uint16_t)(channel + DSPIC33_DMA_COUNT), event_value, 1u)) {
        complete_dma_transfer(cpu, channel, event_value);
    }
}

static uint32_t dma_event_value(const Dspic33* cpu, uint8_t channel,
                                uint16_t indirect_address, bool forced) {
    uint32_t value = indirect_address;
    value |= ((uint32_t)cpu->io.dma_generation[channel] & DMA_EVENT_GENERATION_MASK)
             << DMA_EVENT_GENERATION_SHIFT;
    if (forced) {
        value |= DMA_EVENT_FORCE;
    }
    return value;
}

static bool schedule_dma_channel(Dspic33* cpu, uint8_t channel,
                                 uint16_t indirect_address, bool forced,
                                 uint64_t delay) {
    uint16_t bit = dma_channel_bit(channel);
    uint16_t* pending =
        forced ? &cpu->io.dma_forced_pending : &cpu->io.dma_peripheral_pending;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_DMA, channel,
                          dma_event_value(cpu, channel, indirect_address, forced),
                          delay)) {
        return false;
    }
    *pending |= bit;
    return true;
}

static void dma_request_collision(Dspic33* cpu, uint8_t channel) {
    uint16_t status = raw_word(cpu, DMA_RQC);
    uint16_t bit = dma_channel_bit(channel);
    if ((status & bit) == 0u) {
        raw_write_word(cpu, DMA_RQC, (uint16_t)(status | bit));
        dspic33_raise_dma_collision_trap(cpu);
    }
}

static uint8_t uart_transmit_interrupt_mode(const Dspic33* cpu, uint8_t channel) {
    uint16_t status = raw_word(cpu, (uint16_t)(uart_bases[channel] + 2u));
    return (uint8_t)(((status & UART_STATUS_TX_INTERRUPT_LOW) != 0u ? 1u : 0u) |
                     ((status & UART_STATUS_TX_INTERRUPT_HIGH) != 0u ? 2u : 0u));
}

static uint8_t uart_receive_interrupt_threshold(const Dspic33* cpu, uint8_t channel) {
    uint16_t mode = raw_word(cpu, (uint16_t)(uart_bases[channel] + 2u)) &
                    UART_STATUS_RX_INTERRUPT_MASK;
    if (mode == 0x0080u) {
        return 3u;
    }
    if (mode == 0x00c0u) {
        return 4u;
    }
    return 1u;
}

static Dspic33UartParity uart_parity(uint16_t mode) {
    uint16_t selection = mode & UART_MODE_DATA_MASK;
    if (selection == 0x0002u) {
        return DSPIC33_UART_PARITY_EVEN;
    }
    if (selection == 0x0004u) {
        return DSPIC33_UART_PARITY_ODD;
    }
    return DSPIC33_UART_PARITY_NONE;
}

static void uart_refresh_status(Dspic33* cpu, uint8_t channel) {
    uint16_t base = uart_bases[channel];
    uint16_t status = raw_word(cpu, (uint16_t)(base + 2u));
    uint8_t bit = (uint8_t)(1u << channel);
    Dspic33UartFrame frame;
    status &= (uint16_t)~(UART_STATUS_TX_FULL | UART_STATUS_TX_EMPTY |
                          UART_STATUS_RX_IDLE | UART_STATUS_PARITY_ERROR |
                          UART_STATUS_FRAMING_ERROR | UART_STATUS_RX_AVAILABLE);
    if (cpu->io.uart_tx_fifo[channel].count == DSPIC33_UART_FIFO_SIZE) {
        status |= UART_STATUS_TX_FULL;
    }
    if ((cpu->io.uart_tx_active & bit) == 0u &&
        cpu->io.uart_tx_fifo[channel].count == 0u) {
        status |= UART_STATUS_TX_EMPTY;
    }
    status |= UART_STATUS_RX_IDLE;
    if (uart_fifo_front(&cpu->io.uart_rx_fifo[channel], &frame)) {
        status |= UART_STATUS_RX_AVAILABLE;
        if (frame.parity_error) {
            status |= UART_STATUS_PARITY_ERROR;
        }
        if (frame.framing_error) {
            status |= UART_STATUS_FRAMING_ERROR;
        }
        raw_write_word(cpu, (uint16_t)(base + 6u), frame.value & 0x01ffu);
    } else {
        raw_write_word(cpu, (uint16_t)(base + 6u), 0u);
    }
    raw_write_word(cpu, (uint16_t)(base + 2u), status);
}

static void uart_clear_receive(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    memset(&cpu->io.uart_rx_fifo[channel], 0, sizeof(cpu->io.uart_rx_fifo[channel]));
    memset(&cpu->io.uart_rx_hold[channel], 0, sizeof(cpu->io.uart_rx_hold[channel]));
    cpu->io.uart_rx_hold_valid &= (uint8_t)~bit;
    raw_write_word(cpu, (uint16_t)(uart_bases[channel] + 2u),
                   (uint16_t)(raw_word(cpu, (uint16_t)(uart_bases[channel] + 2u)) &
                              ~UART_STATUS_OVERRUN));
    uart_refresh_status(cpu, channel);
}

static void uart_clear_transmit(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    memset(&cpu->io.uart_tx_fifo[channel], 0, sizeof(cpu->io.uart_tx_fifo[channel]));
    memset(&cpu->io.uart_tx_shift[channel], 0, sizeof(cpu->io.uart_tx_shift[channel]));
    cpu->io.uart_tx_active &= (uint8_t)~bit;
    cpu->io.uart_tx_scheduled &= (uint8_t)~bit;
    cpu->io.uart_generation[channel]++;
    uart_refresh_status(cpu, channel);
}

static void uart_reset_runtime(Dspic33* cpu, uint8_t channel) {
    uint16_t base = uart_bases[channel];
    uint16_t status = raw_word(cpu, (uint16_t)(base + 2u));
    uint16_t controls =
        status & (UART_STATUS_TX_INTERRUPT_HIGH | UART_STATUS_TX_INVERT |
                  UART_STATUS_TX_INTERRUPT_LOW | UART_STATUS_RX_INTERRUPT_MASK |
                  UART_STATUS_ADDRESS_DETECT);
    uart_clear_transmit(cpu, channel);
    uart_clear_receive(cpu, channel);
    raw_write_word(cpu, (uint16_t)(base + 2u),
                   (uint16_t)(controls | UART_STATUS_TX_EMPTY | UART_STATUS_RX_IDLE));
}

static void uart_disable_module(Dspic33* cpu, uint8_t channel) {
    uint16_t base = uart_bases[channel];
    uint16_t status = raw_word(cpu, (uint16_t)(base + 2u));
    bool preserve_write_pointer = (status & UART_STATUS_TX_ENABLE) == 0u;
    Dspic33UartFifo fifo = cpu->io.uart_tx_fifo[channel];
    uart_reset_runtime(cpu, channel);
    if (preserve_write_pointer) {
        cpu->io.uart_tx_fifo[channel] = fifo;
        uart_refresh_status(cpu, channel);
    }
}

static bool uart_transmitter_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t base = uart_bases[channel];
    return !uart_module_disabled(cpu, channel) &&
           (raw_word(cpu, base) & UART_MODE_ENABLE) != 0u &&
           (raw_word(cpu, (uint16_t)(base + 2u)) & UART_STATUS_TX_ENABLE) != 0u;
}

static bool uart_cts_allows(const Dspic33* cpu, uint8_t channel) {
    uint16_t mode = raw_word(cpu, uart_bases[channel]);
    return (mode & UART_MODE_UEN_MASK) != 0x0200u ||
           (cpu->io.uart_cts & (uint8_t)(1u << channel)) != 0u;
}

static uint64_t uart_frame_cycles(const Dspic33* cpu, uint8_t channel,
                                  const Dspic33UartFrame* frame) {
    uint16_t mode = raw_word(cpu, uart_bases[channel]);
    uint64_t clocks = (mode & UART_MODE_HIGH_SPEED) != 0u ? 4u : 16u;
    uint64_t bits =
        frame->break_signal
            ? 14u
            : (uint64_t)(1u + frame->data_bits + frame->stop_bits +
                         (frame->parity == DSPIC33_UART_PARITY_NONE ? 0u : 1u));
    return ((uint64_t)raw_word(cpu, (uint16_t)(uart_bases[channel] + 8u)) + 1u) *
           clocks * bits;
}

static void uart_raise_transmit(Dspic33* cpu, uint8_t channel, bool dma) {
    dspic33_raise_interrupt(cpu, uart_tx_irqs[channel]);
    if (dma) {
        dspic33_dma_request(cpu, uart_tx_irqs[channel],
                            (uint16_t)(uart_bases[channel] + 4u), 0u);
    }
}

static void uart_schedule_transmit(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    if ((cpu->io.uart_tx_active & bit) == 0u ||
        (cpu->io.uart_tx_scheduled & bit) != 0u || !uart_cts_allows(cpu, channel)) {
        return;
    }
    if (dspic33_schedule(
            cpu, DSPIC33_EVENT_UART, channel,
            UART_EVENT_TRANSMIT | cpu->io.uart_generation[channel],
            uart_frame_cycles(cpu, channel, &cpu->io.uart_tx_shift[channel]))) {
        cpu->io.uart_tx_scheduled |= bit;
    }
}

static void uart_start_transmit(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t interrupt_mode;
    Dspic33UartFrame* frame;
    uint16_t mode;
    uint16_t status;
    if (!uart_transmitter_enabled(cpu, channel) ||
        (cpu->io.uart_tx_active & bit) != 0u ||
        !uart_fifo_pop(&cpu->io.uart_tx_fifo[channel],
                       &cpu->io.uart_tx_shift[channel])) {
        uart_refresh_status(cpu, channel);
        return;
    }
    frame = &cpu->io.uart_tx_shift[channel];
    mode = raw_word(cpu, uart_bases[channel]);
    status = raw_word(cpu, (uint16_t)(uart_bases[channel] + 2u));
    frame->break_signal = (status & UART_STATUS_BREAK) != 0u;
    frame->data_bits = (mode & UART_MODE_NINE_BIT) == UART_MODE_NINE_BIT ? 9u : 8u;
    frame->value &= frame->data_bits == 9u ? 0x01ffu : 0x00ffu;
    frame->stop_bits = (mode & UART_MODE_TWO_STOP_BITS) != 0u ? 2u : 1u;
    frame->parity = uart_parity(mode);
    frame->inverted = (status & UART_STATUS_TX_INVERT) != 0u;
    frame->irda = (mode & UART_MODE_IREN) != 0u;
    frame->baud_period = raw_word(cpu, (uint16_t)(uart_bases[channel] + 8u));
    if (frame->break_signal) {
        frame->value = 0u;
        frame->data_bits = 12u;
        frame->stop_bits = 1u;
        frame->parity = DSPIC33_UART_PARITY_NONE;
    }
    cpu->io.uart_tx_active |= bit;
    interrupt_mode = uart_transmit_interrupt_mode(cpu, channel);
    if (!frame->break_signal && interrupt_mode == 0u) {
        uart_raise_transmit(cpu, channel, true);
    } else if (!frame->break_signal && interrupt_mode == 2u &&
               cpu->io.uart_tx_fifo[channel].count == 0u) {
        uart_raise_transmit(cpu, channel, false);
    }
    uart_refresh_status(cpu, channel);
    uart_schedule_transmit(cpu, channel);
}

static void uart_receive_complete(Dspic33* cpu, uint8_t channel,
                                  const Dspic33UartFrame* incoming) {
    uint16_t base = uart_bases[channel];
    uint16_t mode = raw_word(cpu, base);
    uint16_t status = raw_word(cpu, (uint16_t)(base + 2u));
    uint8_t bit = (uint8_t)(1u << channel);
    Dspic33UartFrame frame = *incoming;
    if (uart_module_disabled(cpu, channel) || (mode & UART_MODE_ENABLE) == 0u) {
        return;
    }
    if ((mode & UART_MODE_WAKE) != 0u) {
        raw_write_word(cpu, base, (uint16_t)(mode & ~UART_MODE_WAKE));
        dspic33_raise_interrupt(cpu, uart_rx_irqs[channel]);
        return;
    }
    if ((mode & UART_MODE_AUTO_BAUD) != 0u) {
        if (frame.baud_period != 0u) {
            raw_write_word(cpu, (uint16_t)(base + 8u), frame.baud_period);
        }
        raw_write_word(cpu, base, (uint16_t)(mode & ~UART_MODE_AUTO_BAUD));
        dspic33_raise_interrupt(cpu, uart_rx_irqs[channel]);
        return;
    }
    frame.data_bits = (mode & UART_MODE_NINE_BIT) == UART_MODE_NINE_BIT ? 9u : 8u;
    frame.value &= frame.data_bits == 9u ? 0x01ffu : 0x00ffu;
    frame.parity = uart_parity(mode);
    if (frame.data_bits == 9u) {
        frame.parity_error = false;
    }
    if ((status & UART_STATUS_ADDRESS_DETECT) != 0u && frame.data_bits == 9u &&
        (frame.value & 0x0100u) == 0u) {
        return;
    }
    if ((status & UART_STATUS_OVERRUN) != 0u) {
        return;
    }
    if (!uart_fifo_push(&cpu->io.uart_rx_fifo[channel], &frame)) {
        cpu->io.uart_rx_hold[channel] = frame;
        cpu->io.uart_rx_hold_valid |= bit;
        raw_write_word(cpu, (uint16_t)(base + 2u),
                       (uint16_t)(status | UART_STATUS_OVERRUN));
        uart_refresh_status(cpu, channel);
        dspic33_raise_interrupt(cpu, uart_error_irqs[channel]);
        return;
    }
    uart_refresh_status(cpu, channel);
    if (cpu->io.uart_rx_fifo[channel].count >=
        uart_receive_interrupt_threshold(cpu, channel)) {
        dspic33_raise_interrupt(cpu, uart_rx_irqs[channel]);
    }
    if (uart_receive_interrupt_threshold(cpu, channel) == 1u) {
        dspic33_dma_request(cpu, uart_rx_irqs[channel], (uint16_t)(base + 6u), 0u);
    }
    if (frame.parity_error || frame.framing_error) {
        dspic33_raise_interrupt(cpu, uart_error_irqs[channel]);
    }
}

static void uart_transmit_complete(Dspic33* cpu, uint8_t channel, uint16_t generation) {
    uint8_t bit = (uint8_t)(1u << channel);
    Dspic33UartFrame frame;
    if (generation != cpu->io.uart_generation[channel] ||
        (cpu->io.uart_tx_active & bit) == 0u ||
        (cpu->io.uart_tx_scheduled & bit) == 0u) {
        return;
    }
    frame = cpu->io.uart_tx_shift[channel];
    cpu->io.uart_tx_active &= (uint8_t)~bit;
    cpu->io.uart_tx_scheduled &= (uint8_t)~bit;
    uart_queue_push(&cpu->io.uart_tx[channel], &frame);
    if ((raw_word(cpu, uart_bases[channel]) & UART_MODE_LOOPBACK) != 0u) {
        Dspic33UartFrame received = frame;
        received.framing_error = frame.break_signal;
        received.parity_error = false;
        received.break_signal = false;
        uart_receive_complete(cpu, channel, &received);
    }
    if (frame.break_signal) {
        raw_write_word(cpu, (uint16_t)(uart_bases[channel] + 2u),
                       (uint16_t)(raw_word(cpu, (uint16_t)(uart_bases[channel] + 2u)) &
                                  ~UART_STATUS_BREAK));
    }
    uart_start_transmit(cpu, channel);
    if ((cpu->io.uart_tx_active & bit) == 0u &&
        uart_transmit_interrupt_mode(cpu, channel) == 1u) {
        uart_raise_transmit(cpu, channel, false);
    }
    uart_refresh_status(cpu, channel);
}

static void run_uart(Dspic33* cpu, uint8_t channel, uint32_t event_value) {
    uint32_t kind = event_value & UART_EVENT_KIND_MASK;
    if (channel >= DSPIC33_UART_COUNT) {
        return;
    }
    if (kind == UART_EVENT_TRANSMIT) {
        uart_transmit_complete(cpu, channel, (uint16_t)event_value);
    } else if (kind == UART_EVENT_CTS) {
        uint8_t bit = (uint8_t)(1u << channel);
        if ((event_value & 1u) != 0u) {
            cpu->io.uart_cts |= bit;
            uart_schedule_transmit(cpu, channel);
        } else {
            cpu->io.uart_cts &= (uint8_t)~bit;
        }
    } else {
        Dspic33UartFrame frame;
        memset(&frame, 0, sizeof(frame));
        frame.value = (uint16_t)(event_value & 0x01ffu);
        frame.parity_error = (event_value & UART_EVENT_PARITY_ERROR) != 0u;
        frame.framing_error = (event_value & UART_EVENT_FRAMING_ERROR) != 0u;
        frame.baud_period =
            (uint16_t)((event_value & UART_EVENT_BAUD_MASK) >> UART_EVENT_BAUD_SHIFT);
        uart_receive_complete(cpu, channel, &frame);
    }
}

static bool spi_module_disabled(const Dspic33* cpu, uint8_t channel) {
    if (channel < 2u) {
        return (raw_word(cpu, 0x0760u) & (uint16_t)(0x0008u << channel)) != 0u;
    }
    return (raw_word(cpu, 0x076au) & (uint16_t)(1u << (channel - 2u))) != 0u;
}

static bool spi_master(const Dspic33* cpu, uint8_t channel) {
    return (raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u)) & SPI_MASTER) != 0u;
}

static bool spi_enhanced(const Dspic33* cpu, uint8_t channel) {
    return (raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u)) & SPI_ENHANCED) != 0u;
}

static void spi_refresh_status(Dspic33* cpu, uint8_t channel) {
    uint16_t base = spi_bases[channel];
    uint16_t status = raw_word(cpu, base);
    uint8_t pending;
    status &= (uint16_t)~(SPI_BUFFER_COUNT_MASK | SPI_SHIFT_EMPTY | SPI_RX_EMPTY |
                          SPI_TX_FULL | SPI_RX_FULL);
    if (spi_enhanced(cpu, channel)) {
        if ((cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u) {
            status |= SPI_SHIFT_EMPTY;
        }
        if (cpu->io.spi_rx_fifo[channel].count == 0u) {
            status |= SPI_RX_EMPTY;
        }
        if (cpu->io.spi_tx_fifo[channel].count == 8u) {
            status |= SPI_TX_FULL;
        }
        if (cpu->io.spi_rx_fifo[channel].count == 8u) {
            status |= SPI_RX_FULL;
        }
        pending = spi_master(cpu, channel) ? cpu->io.spi_tx_fifo[channel].count
                                           : cpu->io.spi_rx_fifo[channel].count;
        if (pending > 7u) {
            pending = 7u;
        }
        status |= (uint16_t)pending << 8u;
    } else {
        if (cpu->io.spi_tx_fifo[channel].count != 0u) {
            status |= SPI_TX_FULL;
        }
        if (cpu->io.spi_rx_fifo[channel].count != 0u) {
            status |= SPI_RX_FULL;
        }
    }
    raw_write_word(cpu, base, status);
}

static void spi_clear_buffers(Dspic33* cpu, uint8_t channel) {
    memset(&cpu->io.spi_tx_fifo[channel], 0, sizeof(cpu->io.spi_tx_fifo[channel]));
    memset(&cpu->io.spi_rx_fifo[channel], 0, sizeof(cpu->io.spi_rx_fifo[channel]));
    cpu->io.spi_busy &= (uint8_t)~(1u << channel);
    cpu->io.spi_frame_active &= (uint8_t)~(1u << channel);
    cpu->io.spi_shift[channel] = 0u;
    cpu->io.spi_generation[channel] =
        (uint16_t)((cpu->io.spi_generation[channel] + 1u) & SPI_EVENT_GENERATION_MASK);
    raw_write_word(cpu, (uint16_t)(spi_bases[channel] + 8u), 0u);
    spi_refresh_status(cpu, channel);
}

static void spi_begin_frame(Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u));
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.spi_frame_active &= (uint8_t)~bit;
    if ((control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE | SPI_FRAME_ACTIVE_HIGH)) ==
        (SPI_FRAME_ENABLE | SPI_FRAME_ACTIVE_HIGH)) {
        if (spi_master(cpu, channel) && (control & SPI_FRAME_DELAY) != 0u) {
            uint16_t data_control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u));
            uint8_t bits = (data_control & SPI_MODE_16) != 0u ? 16u : 8u;
            uint32_t event_value =
                SPI_EVENT_FRAME | ((uint32_t)cpu->io.spi_generation[channel]
                                   << SPI_EVENT_GENERATION_SHIFT);
            dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, event_value,
                             spi_transfer_cycles(cpu, channel) / bits);
        } else {
            cpu->io.spi_frame_active |= bit;
        }
    }
}

static bool spi_power_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t status = raw_word(cpu, spi_bases[channel]);
    if (!spi_master(cpu, channel)) {
        return true;
    }
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    return cpu->power_state == DSPIC33_POWER_IDLE && (status & SPI_STOP_IDLE) == 0u;
}

static bool spi_selected(const Dspic33* cpu, uint8_t channel) {
    uint16_t control1 = raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u));
    uint16_t control2 = raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u));
    bool required = (control2 & SPI_FRAME_ENABLE) != 0u
                        ? (control2 & SPI_FRAME_SLAVE) != 0u
                        : !spi_master(cpu, channel) && (control1 & 0x0080u) != 0u;
    return !required || (cpu->io.spi_selected & (uint8_t)(1u << channel)) != 0u;
}

static uint64_t spi_transfer_cycles(const Dspic33* cpu, uint8_t channel) {
    static const uint8_t primary[] = {64u, 16u, 4u, 1u};
    uint16_t control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u));
    uint8_t secondary = (uint8_t)(8u - ((control >> 2u) & 7u));
    uint8_t bits = (control & SPI_MODE_16) != 0u ? 16u : 8u;
    return (uint64_t)bits * primary[control & 3u] * secondary;
}

static uint8_t spi_interrupt_mode(const Dspic33* cpu, uint8_t channel) {
    return (uint8_t)((raw_word(cpu, spi_bases[channel]) & SPI_INTERRUPT_MODE_MASK) >>
                     2u);
}

static void spi_raise_mode(Dspic33* cpu, uint8_t channel, uint8_t mode) {
    if (!spi_enhanced(cpu, channel) || spi_interrupt_mode(cpu, channel) == mode) {
        dspic33_raise_interrupt(cpu, spi_irqs[channel]);
    }
}

static void spi_schedule_current(Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 2u));
    uint8_t bit = (uint8_t)(1u << channel);
    uint32_t event_value;
    if ((cpu->io.spi_busy & bit) == 0u || !spi_master(cpu, channel) ||
        (control & SPI_DISABLE_CLOCK) != 0u || !spi_power_enabled(cpu, channel)) {
        return;
    }
    event_value =
        SPI_EVENT_INTERNAL |
        ((uint32_t)cpu->io.spi_generation[channel] << SPI_EVENT_GENERATION_SHIFT) |
        cpu->io.spi_shift[channel];
    dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, event_value,
                     spi_transfer_cycles(cpu, channel));
}

static void spi_start_next(Dspic33* cpu, uint8_t channel) {
    uint16_t base = spi_bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + 2u));
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t previous_count = cpu->io.spi_tx_fifo[channel].count;
    uint16_t value;
    if ((cpu->io.spi_busy & bit) != 0u || previous_count == 0u ||
        (raw_word(cpu, base) & SPI_ENABLE) == 0u || spi_module_disabled(cpu, channel) ||
        !word_queue_pop(&cpu->io.spi_tx_fifo[channel], &value)) {
        spi_refresh_status(cpu, channel);
        return;
    }
    cpu->io.spi_busy |= bit;
    cpu->io.spi_shift[channel] = value;
    spi_begin_frame(cpu, channel);
    byte_queue_push(&cpu->io.spi_tx[channel], (uint8_t)value);
    if ((control & SPI_MODE_16) != 0u) {
        byte_queue_push(&cpu->io.spi_tx[channel], (uint8_t)(value >> 8u));
    }
    spi_refresh_status(cpu, channel);
    if (spi_enhanced(cpu, channel)) {
        if (previous_count == 8u) {
            spi_raise_mode(cpu, channel, 4u);
        }
        if (cpu->io.spi_tx_fifo[channel].count == 0u) {
            spi_raise_mode(cpu, channel, 6u);
        }
    }
    spi_schedule_current(cpu, channel);
}

static void spi_receive_word(Dspic33* cpu, uint8_t channel, uint16_t value) {
    uint16_t base = spi_bases[channel];
    uint16_t status = raw_word(cpu, base);
    uint8_t capacity = spi_enhanced(cpu, channel) ? 8u : 1u;
    if ((raw_word(cpu, (uint16_t)(base + 2u)) & SPI_MODE_16) == 0u) {
        value &= 0x00ffu;
    }
    if ((status & SPI_OVERFLOW) != 0u) {
        return;
    }
    if (cpu->io.spi_rx_fifo[channel].count >= capacity ||
        !word_queue_push(&cpu->io.spi_rx_fifo[channel], value)) {
        raw_write_word(cpu, base, (uint16_t)(status | SPI_OVERFLOW));
        dspic33_raise_interrupt(cpu, spi_error_irqs[channel]);
        dspic33_raise_interrupt(cpu, spi_irqs[channel]);
        return;
    }
    if (cpu->io.spi_rx_fifo[channel].count == 1u) {
        raw_write_word(cpu, (uint16_t)(base + 8u), value);
    }
    spi_refresh_status(cpu, channel);
    if (spi_enhanced(cpu, channel)) {
        spi_raise_mode(cpu, channel, 1u);
        if (cpu->io.spi_rx_fifo[channel].count >= 6u) {
            spi_raise_mode(cpu, channel, 2u);
        }
        if (cpu->io.spi_rx_fifo[channel].count == 8u) {
            spi_raise_mode(cpu, channel, 3u);
        }
    }
}

static void spi_complete_transfer(Dspic33* cpu, uint8_t channel, uint16_t value) {
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.spi_busy &= (uint8_t)~bit;
    cpu->io.spi_frame_active &= (uint8_t)~bit;
    spi_receive_word(cpu, channel, value);
    dspic33_dma_request(cpu, spi_dma_requests[channel],
                        (uint16_t)(spi_bases[channel] + 8u), 0u);
    spi_start_next(cpu, channel);
    if (!spi_enhanced(cpu, channel)) {
        dspic33_raise_interrupt(cpu, spi_irqs[channel]);
    } else if ((cpu->io.spi_busy & bit) == 0u &&
               cpu->io.spi_tx_fifo[channel].count == 0u) {
        spi_raise_mode(cpu, channel, 5u);
    }
    spi_refresh_status(cpu, channel);
}

static void run_spi(Dspic33* cpu, uint8_t channel, uint32_t event_value) {
    uint16_t base;
    uint16_t value = (uint16_t)event_value;
    uint8_t bit;
    uint32_t kind = event_value & SPI_EVENT_KIND_MASK;
    if (channel >= DSPIC33_SPI_COUNT) {
        return;
    }
    base = spi_bases[channel];
    bit = (uint8_t)(1u << channel);
    if (kind == 0u) {
        raw_write_word(cpu, (uint16_t)(base + 8u), value);
        raw_write_word(cpu, base, (uint16_t)(raw_word(cpu, base) | SPI_RX_FULL));
        dspic33_raise_interrupt(cpu, spi_irqs[channel]);
        return;
    }
    if ((raw_word(cpu, base) & SPI_ENABLE) == 0u || spi_module_disabled(cpu, channel)) {
        return;
    }
    if (kind == SPI_EVENT_FRAME) {
        uint16_t generation = (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) &
                                         SPI_EVENT_GENERATION_MASK);
        uint16_t control = raw_word(cpu, (uint16_t)(base + 4u));
        if (generation == cpu->io.spi_generation[channel] &&
            (cpu->io.spi_busy & bit) != 0u && spi_power_enabled(cpu, channel) &&
            (control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE | SPI_FRAME_ACTIVE_HIGH)) ==
                (SPI_FRAME_ENABLE | SPI_FRAME_ACTIVE_HIGH)) {
            cpu->io.spi_frame_active |= bit;
        }
        return;
    }
    if (kind == SPI_EVENT_INTERNAL) {
        uint16_t generation = (uint16_t)((event_value >> SPI_EVENT_GENERATION_SHIFT) &
                                         SPI_EVENT_GENERATION_MASK);
        if (generation != cpu->io.spi_generation[channel] ||
            (cpu->io.spi_busy & bit) == 0u) {
            return;
        }
        if (!spi_power_enabled(cpu, channel)) {
            spi_clear_buffers(cpu, channel);
            return;
        }
        spi_complete_transfer(cpu, channel, value);
        return;
    }
    if (!spi_selected(cpu, channel) ||
        (spi_master(cpu, channel) && (cpu->io.spi_busy & bit) == 0u)) {
        return;
    }
    if (spi_master(cpu, channel)) {
        cpu->io.spi_generation[channel] =
            (uint16_t)((cpu->io.spi_generation[channel] + 1u) &
                       SPI_EVENT_GENERATION_MASK);
    } else if ((cpu->io.spi_busy & bit) == 0u) {
        cpu->io.spi_busy |= bit;
        cpu->io.spi_shift[channel] = raw_word(cpu, (uint16_t)(base + 8u));
    }
    spi_complete_transfer(cpu, channel, value);
}

static void run_spi_select(Dspic33* cpu, uint8_t channel, bool selected) {
    uint8_t bit;
    if (channel >= DSPIC33_SPI_COUNT) {
        return;
    }
    bit = (uint8_t)(1u << channel);
    if (selected) {
        cpu->io.spi_selected |= bit;
    } else {
        cpu->io.spi_selected &= (uint8_t)~bit;
    }
}

static uint16_t adc_register(const Dspic33* cpu, uint8_t module, uint16_t offset) {
    return raw_word(cpu, (uint16_t)(adc_controls[module] + offset));
}

static bool adc_12_bit(const Dspic33* cpu, uint8_t module) {
    return module == 0u && (adc_register(cpu, module, 0u) & ADC_12_BIT) != 0u;
}

static bool adc_power_enabled(const Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_register(cpu, module, 0u);
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    if (cpu->power_state == DSPIC33_POWER_IDLE) {
        return (control & ADC_STOP_IDLE) == 0u;
    }
    return (adc_register(cpu, module, 4u) & 0x8000u) != 0u;
}

static uint8_t adc_channel_count(const Dspic33* cpu, uint8_t module) {
    uint16_t selection;
    if (adc_12_bit(cpu, module)) {
        return 1u;
    }
    selection = adc_register(cpu, module, 2u) & ADC_CHANNELS_MASK;
    if (selection == 0u) {
        return 1u;
    }
    return selection == 0x0100u ? 2u : 4u;
}

static uint8_t adc_next_scan_channel(Dspic33* cpu, uint8_t module) {
    uint8_t limit = module == 0u ? 32u : 16u;
    uint8_t offset;
    uint32_t selected = module == 0u ? ((uint32_t)raw_word(cpu, 0x032eu) << 16u) |
                                           raw_word(cpu, 0x0330u)
                                     : raw_word(cpu, 0x0370u);
    if (selected == 0u) {
        return (uint8_t)(adc_register(cpu, module, 8u) & 0x001fu);
    }
    for (offset = 0u; offset < limit; offset++) {
        uint8_t channel = (uint8_t)((cpu->io.adc_scan_index[module] + offset) % limit);
        if ((selected & ((uint32_t)1u << channel)) != 0u) {
            cpu->io.adc_scan_index[module] = (uint8_t)((channel + 1u) % limit);
            return channel;
        }
    }
    return 0u;
}

static uint8_t adc_positive_channel(Dspic33* cpu, uint8_t module, uint8_t lane,
                                    bool mux_b) {
    uint16_t channels;
    if (lane == 0u) {
        if (!mux_b && (adc_register(cpu, module, 2u) & ADC_SCAN) != 0u) {
            return adc_next_scan_channel(cpu, module);
        }
        channels = adc_register(cpu, module, 8u);
        return (uint8_t)((channels >> (mux_b ? 8u : 0u)) & 0x001fu);
    }
    channels = adc_register(cpu, module, 6u);
    return (uint8_t)(lane - 1u +
                     (((channels >> (mux_b ? 8u : 0u)) & 1u) != 0u ? 3u : 0u));
}

static uint8_t adc_negative_channel(const Dspic33* cpu, uint8_t module, uint8_t lane,
                                    bool mux_b) {
    uint16_t channels;
    uint16_t negative;
    if (lane == 0u) {
        channels = adc_register(cpu, module, 8u);
        return (channels & (mux_b ? 0x8000u : 0x0080u)) != 0u ? 1u : UINT8_MAX;
    }
    channels = adc_register(cpu, module, 6u);
    negative = (channels >> (mux_b ? 9u : 1u)) & 3u;
    if (negative < 2u) {
        return UINT8_MAX;
    }
    return (uint8_t)((negative == 2u ? 6u : 9u) + lane - 1u);
}

static uint16_t adc_input_code(const Dspic33* cpu, uint8_t module, uint8_t positive,
                               uint8_t negative) {
    uint16_t high = positive < DSPIC33_ADC_CHANNEL_COUNT ? cpu->io.adc[positive] : 0u;
    uint16_t low = negative < DSPIC33_ADC_CHANNEL_COUNT ? cpu->io.adc[negative] : 0u;
    uint16_t difference = high > low ? (uint16_t)(high - low) : 0u;
    return adc_12_bit(cpu, module) ? (uint16_t)(difference & 0x0fffu)
                                   : (uint16_t)((difference >> 2u) & 0x03ffu);
}

static uint16_t adc_format_code(const Dspic33* cpu, uint8_t module, uint16_t code) {
    uint8_t bits = adc_12_bit(cpu, module) ? 12u : 10u;
    uint8_t shift = (uint8_t)(16u - bits);
    uint16_t sign = (uint16_t)(1u << (bits - 1u));
    uint16_t mask = (uint16_t)((1u << bits) - 1u);
    uint16_t format = adc_register(cpu, module, 0u) & ADC_FORMAT_MASK;
    if (format == 0u) {
        return code;
    }
    if (format == 0x0200u) {
        return (uint16_t)(code << shift);
    }
    code ^= sign;
    if ((code & sign) != 0u) {
        code |= (uint16_t)~mask;
    }
    return format == 0x0100u ? code : (uint16_t)(code << shift);
}

static uint64_t adc_clock_cycles(const Dspic33* cpu, uint8_t module) {
    uint16_t timing = adc_register(cpu, module, 4u);
    return (timing & 0x8000u) != 0u ? 1u : (uint64_t)(timing & 0x00ffu) + 1u;
}

static uint64_t adc_sample_cycles(const Dspic33* cpu, uint8_t module) {
    uint64_t cycles = ((adc_register(cpu, module, 4u) >> 8u) & 0x001fu) *
                      adc_clock_cycles(cpu, module);
    return cycles == 0u ? 1u : cycles;
}

static uint64_t adc_conversion_cycles(const Dspic33* cpu, uint8_t module) {
    return (adc_12_bit(cpu, module) ? 14u : 12u) * adc_clock_cycles(cpu, module);
}

static uint32_t adc_event_value(const Dspic33* cpu, uint8_t module, uint8_t source,
                                bool complete) {
    uint32_t value = source;
    value |= (uint32_t)cpu->io.adc_generation[module] << ADC_EVENT_GENERATION_SHIFT;
    if (complete) {
        value |= ADC_EVENT_COMPLETE;
    }
    return value;
}

static uint8_t adc_increment_threshold(const Dspic33* cpu, uint8_t module) {
    uint8_t value = (uint8_t)((adc_register(cpu, module, 2u) >> 2u) & 0x001fu);
    uint16_t dma = module == 0u ? raw_word(cpu, 0x0332u) : raw_word(cpu, 0x0372u);
    if (module != 0u || (dma & ADC_DMA_ENABLE) == 0u) {
        value &= 0x0fu;
    }
    return (uint8_t)(value + 1u);
}

static uint16_t adc_dma_address(Dspic33* cpu, uint8_t module, uint8_t channel) {
    uint16_t control = adc_register(cpu, module, 0u);
    uint16_t dma = module == 0u ? raw_word(cpu, 0x0332u) : raw_word(cpu, 0x0372u);
    uint8_t length = (uint8_t)(dma & ADC_DMA_LENGTH_MASK);
    uint8_t slot = cpu->io.adc_dma_sample[module][channel];
    uint16_t address;
    if ((control & ADC_BUFFER_ORDER) != 0u) {
        return (uint16_t)(cpu->io.adc_buffer_index[module] * 2u);
    }
    address = (uint16_t)(channel * ((uint16_t)2u << length) + slot * 2u);
    cpu->io.adc_dma_sample[module][channel] =
        (uint8_t)((slot + 1u) & ((1u << length) - 1u));
    return address;
}

static void adc_increment_boundary(Dspic33* cpu, uint8_t module) {
    uint16_t control2 = adc_register(cpu, module, 2u);
    cpu->io.adc_sample_count[module] = 0u;
    cpu->io.adc_scan_index[module] = 0u;
    if ((control2 & ADC_BUFFER_SPLIT) != 0u) {
        control2 ^= ADC_SECOND_BUFFER;
        cpu->io.adc_buffer_index[module] =
            (control2 & ADC_SECOND_BUFFER) != 0u ? 8u : 0u;
        raw_write_word(cpu, (uint16_t)(adc_controls[module] + 2u), control2);
    } else {
        cpu->io.adc_buffer_index[module] = 0u;
    }
}

static void adc_store_result(Dspic33* cpu, uint8_t module, uint8_t channel,
                             uint16_t result) {
    uint16_t dma = module == 0u ? raw_word(cpu, 0x0332u) : raw_word(cpu, 0x0372u);
    uint16_t indirect = 0u;
    bool increment_boundary;
    if ((dma & ADC_DMA_ENABLE) != 0u) {
        indirect = adc_dma_address(cpu, module, channel);
        raw_write_word(cpu, adc_buffers[module], result);
        dspic33_dma_request(cpu, adc_irqs[module], indirect, 0u);
    } else {
        raw_write_word(cpu,
                       (uint16_t)(adc_buffers[module] +
                                  (cpu->io.adc_buffer_index[module] & 0x0fu) * 2u),
                       result);
    }
    cpu->io.adc_buffer_index[module] =
        (uint8_t)((cpu->io.adc_buffer_index[module] + 1u) & 0x0fu);
    cpu->io.adc_sample_count[module]++;
    increment_boundary =
        cpu->io.adc_sample_count[module] >= adc_increment_threshold(cpu, module);
    if (increment_boundary) {
        adc_increment_boundary(cpu, module);
    }
    if ((dma & ADC_DMA_ENABLE) != 0u || increment_boundary) {
        dspic33_raise_interrupt(cpu, adc_irqs[module]);
        if (module == 0u) {
            output_compare_pulse_source(cpu, OUTPUT_COMPARE_SYNC_ADC1);
        }
    }
}

static void adc_begin_sampling(Dspic33* cpu, uint8_t module);

static void adc_complete_conversion(Dspic33* cpu, uint8_t module, uint8_t source) {
    uint16_t control;
    uint8_t index;
    if (!adc_power_enabled(cpu, module)) {
        return;
    }
    control = adc_register(cpu, module, 0u);
    if ((control & ADC_ON) == 0u) {
        return;
    }
    for (index = 0u; index < cpu->io.adc_latched_count[module]; index++) {
        adc_store_result(cpu, module, cpu->io.adc_latched_channel[module][index],
                         cpu->io.adc_latched[module][index]);
    }
    raw_write_word(cpu, adc_controls[module],
                   source == 1u ? (uint16_t)(control & ~ADC_DONE)
                                : (uint16_t)(control | ADC_DONE));
    if ((control & ADC_AUTO_SAMPLE) != 0u) {
        adc_begin_sampling(cpu, module);
    }
}

static void adc_start_conversion(Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_register(cpu, module, 0u);
    uint16_t control2 = adc_register(cpu, module, 2u);
    uint8_t count;
    uint8_t index;
    bool mux_b;
    if ((control & (ADC_ON | ADC_SAMPLE)) != (ADC_ON | ADC_SAMPLE) ||
        !adc_power_enabled(cpu, module)) {
        return;
    }
    count = adc_channel_count(cpu, module);
    mux_b = (cpu->io.adc_mux_b & (uint8_t)(1u << module)) != 0u;
    for (index = 0u; index < count; index++) {
        uint8_t positive = adc_positive_channel(cpu, module, index, mux_b);
        uint8_t negative = adc_negative_channel(cpu, module, index, mux_b);
        cpu->io.adc_latched_channel[module][index] = positive;
        cpu->io.adc_latched[module][index] = adc_format_code(
            cpu, module, adc_input_code(cpu, module, positive, negative));
    }
    cpu->io.adc_latched_count[module] = count;
    if ((control2 & ADC_ALTERNATE) != 0u) {
        cpu->io.adc_mux_b ^= (uint8_t)(1u << module);
    } else {
        cpu->io.adc_mux_b &= (uint8_t)~(1u << module);
    }
    raw_write_word(cpu, adc_controls[module],
                   (uint16_t)(control & ~(ADC_SAMPLE | ADC_DONE)));
    if (module == 0u) {
        input_capture_pulse_source(cpu, INPUT_CAPTURE_SYNC_ADC1);
    }
    dspic33_schedule(cpu, DSPIC33_EVENT_ADC, module,
                     adc_event_value(cpu, module,
                                     (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u),
                                     true),
                     adc_conversion_cycles(cpu, module));
}

static void adc_begin_sampling(Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_register(cpu, module, 0u);
    uint8_t source;
    if ((control & ADC_ON) == 0u || !adc_power_enabled(cpu, module)) {
        return;
    }
    cpu->io.adc_generation[module]++;
    control |= ADC_SAMPLE;
    raw_write_word(cpu, adc_controls[module], control);
    source = (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u);
    if (source == 7u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_ADC, module,
                         adc_event_value(cpu, module, source, false),
                         adc_sample_cycles(cpu, module));
    }
}

static void run_adc(Dspic33* cpu, uint8_t module, uint32_t event_value) {
    uint16_t generation;
    uint8_t source;
    if (module >= DSPIC33_ADC_COUNT) {
        return;
    }
    generation = (uint16_t)(event_value >> ADC_EVENT_GENERATION_SHIFT);
    source = (uint8_t)(event_value & ADC_EVENT_SOURCE_MASK);
    if ((event_value & ADC_EVENT_COMPLETE) != 0u) {
        if (generation == cpu->io.adc_generation[module]) {
            adc_complete_conversion(cpu, module, source);
        }
        return;
    }
    if (generation != UINT16_MAX && generation != cpu->io.adc_generation[module]) {
        return;
    }
    if (((adc_register(cpu, module, 0u) & ADC_TRIGGER_MASK) >> 4u) == source) {
        adc_start_conversion(cpu, module);
    }
}

static uint16_t pwm_generator_base(uint8_t generator) {
    return (uint16_t)(PWM_GENERATOR_BASE + generator * PWM_GENERATOR_STRIDE);
}

static uint16_t pwm_register(const Dspic33* cpu, uint8_t generator, uint16_t offset) {
    return raw_word(cpu, (uint16_t)(pwm_generator_base(generator) + offset));
}

static bool pwm_power_enabled(const Dspic33* cpu) {
    uint16_t control = raw_word(cpu, PWM_GLOBAL_BASE);
    if ((control & PWM_ENABLE) == 0u || cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE || (control & PWM_STOP_IDLE) == 0u;
}

static uint16_t pwm_divider(const Dspic33* cpu, uint8_t time_base) {
    uint16_t address = (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 2u : 0x10u));
    uint8_t selection = (uint8_t)(raw_word(cpu, address) & 7u);
    return selection < 7u ? (uint16_t)(1u << selection) : 64u;
}

static void pwm_latch_periods(Dspic33* cpu) {
    cpu->io.pwm_active_period[0] = raw_word(cpu, PWM_GLOBAL_BASE + 4u);
    cpu->io.pwm_active_period[1] = raw_word(cpu, PWM_GLOBAL_BASE + 0x12u);
}

static void pwm_latch_generator(Dspic33* cpu, uint8_t generator) {
    uint16_t control = pwm_register(cpu, generator, 0u);
    uint16_t master = raw_word(cpu, PWM_GLOBAL_BASE + 0x0au);
    cpu->io.pwm_active_duty[generator][0] =
        (control & PWM_MASTER_DUTY) != 0u ? master : pwm_register(cpu, generator, 6u);
    cpu->io.pwm_active_duty[generator][1] = (control & PWM_MASTER_DUTY) != 0u
                                                ? master
                                                : pwm_register(cpu, generator, 0x0eu);
    cpu->io.pwm_active_phase[generator][0] = pwm_register(cpu, generator, 8u);
    cpu->io.pwm_active_phase[generator][1] = pwm_register(cpu, generator, 0x10u);
    cpu->io.pwm_active_dead_time[generator][0] = pwm_register(cpu, generator, 0x0au);
    cpu->io.pwm_active_dead_time[generator][1] = pwm_register(cpu, generator, 0x0cu);
}

static uint16_t pwm_output_io(const Dspic33* cpu, uint8_t generator) {
    uint16_t io = pwm_register(cpu, generator, 2u);
    if ((io & PWM_OVERRIDE_SYNCHRONIZED) == 0u) {
        return io;
    }
    return (uint16_t)((io & ~PWM_SYNCHRONIZED_IO) |
                      (cpu->io.pwm_active_io[generator] & PWM_SYNCHRONIZED_IO));
}

static uint16_t pwm_period(const Dspic33* cpu, uint8_t generator, uint8_t output) {
    uint16_t control = pwm_register(cpu, generator, 0u);
    uint16_t mode = pwm_register(cpu, generator, 2u) & PWM_MODE_MASK;
    if ((control & PWM_INDEPENDENT_TIME_BASE) != 0u) {
        return output != 0u && mode == PWM_MODE_INDEPENDENT
                   ? cpu->io.pwm_active_phase[generator][1]
                   : cpu->io.pwm_active_phase[generator][0];
    }
    return cpu->io.pwm_active_period[(control & 0x0008u) != 0u ? 1u : 0u];
}

static uint16_t pwm_shifted_counter(uint16_t counter, uint16_t phase, uint16_t period) {
    uint32_t modulus = (uint32_t)period + 1u;
    return (uint16_t)((counter + modulus - phase % modulus) % modulus);
}

static bool pwm_input_active(uint32_t inputs, uint8_t source, bool inverted) {
    bool high = (inputs & ((uint32_t)1u << source)) != 0u;
    return inverted ? !high : high;
}

static bool pwm_fault_active(const Dspic33* cpu, uint8_t generator) {
    uint16_t fault = pwm_register(cpu, generator, 4u);
    uint8_t source = (uint8_t)((fault >> 3u) & 0x1fu);
    return pwm_input_active(cpu->io.pwm_fault_inputs, source, (fault & 0x0004u) != 0u);
}

static bool pwm_current_limit_active(const Dspic33* cpu, uint8_t generator) {
    uint16_t fault = pwm_register(cpu, generator, 4u);
    uint8_t source = (uint8_t)((fault >> 10u) & 0x1fu);
    return pwm_input_active(cpu->io.pwm_current_limit_inputs, source,
                            (fault & 0x0200u) != 0u);
}

static void pwm_waveform_pair(const Dspic33* cpu, uint8_t generator, bool* high,
                              bool* low);

static bool pwm_state_blanked(const Dspic33* cpu, uint8_t generator,
                              bool current_limit) {
    uint16_t leading = pwm_register(cpu, generator, 0x1au);
    uint16_t auxiliary = pwm_register(cpu, generator, 0x1eu);
    bool high = cpu->io.pwm[generator * 2u] != 0u;
    bool low = cpu->io.pwm[generator * 2u + 1u] != 0u;
    uint8_t source = (uint8_t)((auxiliary >> 8u) & 0x0fu);
    bool blank_source = false;
    if (source != 0u && source <= DSPIC33_PWM_COUNT) {
        bool blank_low;
        pwm_waveform_pair(cpu, (uint8_t)(source - 1u), &blank_source, &blank_low);
    }
    if (current_limit ? (leading & 0x0400u) == 0u : (leading & 0x0800u) == 0u) {
        return false;
    }
    if (cpu->io.pwm_leb_ticks[generator] != 0u) {
        return true;
    }
    return ((leading & 0x0020u) != 0u && blank_source) ||
           ((leading & 0x0010u) != 0u && !blank_source) ||
           ((leading & 0x0008u) != 0u && high) ||
           ((leading & 0x0004u) != 0u && !high) || ((leading & 0x0002u) != 0u && low) ||
           ((leading & 0x0001u) != 0u && !low);
}

static void pwm_refresh_status(Dspic33* cpu, uint8_t generator) {
    uint16_t base = pwm_generator_base(generator);
    uint16_t control = raw_word(cpu, base);
    bool fault = pwm_fault_active(cpu, generator);
    bool current_limit = pwm_current_limit_active(cpu, generator);
    if ((control & PWM_FAULT_INTERRUPT) == 0u) {
        control = fault ? (uint16_t)(control | PWM_FAULT_STATUS)
                        : (uint16_t)(control & ~PWM_FAULT_STATUS);
    }
    if ((control & PWM_CURRENT_LIMIT_INTERRUPT) == 0u) {
        control = current_limit ? (uint16_t)(control | PWM_CURRENT_LIMIT_STATUS)
                                : (uint16_t)(control & ~PWM_CURRENT_LIMIT_STATUS);
    }
    raw_write_word(cpu, base, control);
}

static bool pwm_waveform(uint16_t counter, uint16_t duty) { return counter <= duty; }

static uint16_t pwm_saturated_add(uint16_t value, uint16_t increment) {
    return UINT16_MAX - value > increment ? (uint16_t)(value + increment) : UINT16_MAX;
}

typedef enum {
    PWM_COMPENSATION_ORDINARY,
    PWM_COMPENSATION_ZERO,
    PWM_COMPENSATION_FULL
} PwmCompensationResult;

static uint16_t pwm_compensated_duty(const Dspic33* cpu, uint8_t generator,
                                     uint16_t duty, uint16_t compensation) {
    bool input = (cpu->io.pwm_dead_time_sampled & (uint8_t)(1u << generator)) != 0u;
    bool polarity = (pwm_register(cpu, generator, 0u) & 0x0020u) != 0u;
    if (input == polarity) {
        return duty > compensation ? (uint16_t)(duty - compensation) : 0u;
    }
    return pwm_saturated_add(duty, compensation);
}

static uint16_t pwm_b1_edge_compensated_duty(const Dspic33* cpu, uint8_t generator,
                                             uint16_t duty, uint16_t compensation,
                                             PwmCompensationResult* result) {
    bool input = (cpu->io.pwm_dead_time_sampled & (uint8_t)(1u << generator)) != 0u;
    bool polarity = (pwm_register(cpu, generator, 0u) & 0x0020u) != 0u;
    uint16_t period = pwm_period(cpu, generator, 0u);
    uint32_t threshold = (uint32_t)compensation * 2u;
    *result = PWM_COMPENSATION_ORDINARY;
    if (input == polarity && (uint32_t)duty < threshold) {
        *result = PWM_COMPENSATION_ZERO;
        return 0u;
    }
    if (input != polarity && (uint32_t)duty + threshold >= period) {
        *result = PWM_COMPENSATION_FULL;
        return period;
    }
    return pwm_compensated_duty(cpu, generator, duty, compensation);
}

static void pwm_waveform_pair(const Dspic33* cpu, uint8_t generator, bool* high,
                              bool* low) {
    uint16_t control = pwm_register(cpu, generator, 0u);
    uint16_t io = pwm_register(cpu, generator, 2u);
    uint16_t mode = io & PWM_MODE_MASK;
    uint16_t high_counter = cpu->io.pwm_counter[generator][0];
    uint16_t low_counter = cpu->io.pwm_counter[generator][1];
    uint16_t dead_mode = control & 0x00c0u;
    uint16_t primary_dead = cpu->io.pwm_active_dead_time[generator][0];
    uint16_t alternate_dead = cpu->io.pwm_active_dead_time[generator][1];
    bool primary = pwm_waveform(high_counter, cpu->io.pwm_active_duty[generator][0]);
    if (mode == PWM_MODE_INDEPENDENT) {
        *high = primary;
        *low = pwm_waveform(low_counter, cpu->io.pwm_active_duty[generator][1]);
    } else if (mode == PWM_MODE_REDUNDANT) {
        *high = primary;
        *low = primary;
    } else if (mode == PWM_MODE_PUSH_PULL) {
        bool second = (cpu->io.pwm_push_pull & (uint8_t)(1u << generator)) != 0u;
        *high = primary && !second;
        *low = primary && second;
    } else {
        uint16_t duty = cpu->io.pwm_active_duty[generator][0];
        if (dead_mode == 0x0080u) {
            *high = primary;
            *low = !primary;
        } else if (dead_mode == 0x00c0u) {
            if ((control & PWM_CENTER_ALIGNED) != 0u) {
                uint16_t compensated =
                    pwm_compensated_duty(cpu, generator, duty, primary_dead);
                uint16_t half_dead = (uint16_t)((alternate_dead + 1u) / 2u);
                *high = high_counter <= compensated;
                *low = high_counter > pwm_saturated_add(compensated, half_dead);
            } else {
                PwmCompensationResult result;
                uint16_t compensated = pwm_b1_edge_compensated_duty(
                    cpu, generator, duty, primary_dead, &result);
                if (result == PWM_COMPENSATION_ZERO) {
                    *high = false;
                    *low = true;
                } else if (result == PWM_COMPENSATION_FULL) {
                    *high = true;
                    *low = false;
                } else {
                    *high =
                        high_counter >= alternate_dead && high_counter <= compensated;
                    *low =
                        high_counter > pwm_saturated_add(compensated, alternate_dead);
                }
            }
        } else if (dead_mode == 0x0040u && (control & PWM_CENTER_ALIGNED) == 0u) {
            *high = high_counter <= pwm_saturated_add(duty, alternate_dead);
            *low = (uint32_t)high_counter + primary_dead > duty;
        } else if ((control & PWM_CENTER_ALIGNED) != 0u) {
            uint16_t half_dead = (uint16_t)((alternate_dead + 1u) / 2u);
            *high =
                high_counter <= (duty > half_dead ? (uint16_t)(duty - half_dead) : 0u);
            *low = high_counter > pwm_saturated_add(duty, half_dead);
        } else {
            if (duty < alternate_dead) {
                *high = high_counter <= duty;
                *low = high_counter > duty;
            } else {
                *high = high_counter >= primary_dead && high_counter <= duty;
                *low = high_counter > pwm_saturated_add(duty, alternate_dead);
            }
        }
        return;
    }
    if (dead_mode == 0u) {
        uint16_t dead_low_counter =
            mode == PWM_MODE_INDEPENDENT ? low_counter : high_counter;
        if ((control & PWM_CENTER_ALIGNED) != 0u) {
            uint16_t half_dead = (uint16_t)((alternate_dead + 1u) / 2u);
            uint16_t high_duty = cpu->io.pwm_active_duty[generator][0];
            uint16_t low_duty = mode == PWM_MODE_INDEPENDENT
                                    ? cpu->io.pwm_active_duty[generator][1]
                                    : high_duty;
            *high = *high && high_counter <= (high_duty > half_dead
                                                  ? (uint16_t)(high_duty - half_dead)
                                                  : 0u);
            *low = *low &&
                   dead_low_counter <=
                       (low_duty > half_dead ? (uint16_t)(low_duty - half_dead) : 0u);
        } else {
            *high = *high && high_counter >= primary_dead;
            *low = *low && dead_low_counter >= alternate_dead;
        }
    }
}

static void pwm_apply_protection(const Dspic33* cpu, uint8_t generator, bool* high,
                                 bool* low) {
    uint16_t fault = pwm_register(cpu, generator, 4u);
    uint16_t io = pwm_register(cpu, generator, 2u);
    uint8_t bit = (uint8_t)(1u << generator);
    bool fault_active =
        ((cpu->io.pwm_fault_latched | cpu->io.pwm_fault_cycle) & bit) != 0u ||
        (pwm_fault_active(cpu, generator) &&
         !pwm_state_blanked(cpu, generator, false) &&
         (fault & PWM_FAULT_MODE_MASK) != PWM_FAULT_DISABLED);
    bool current_active = ((cpu->io.pwm_current_cycle & bit) != 0u ||
                           (pwm_current_limit_active(cpu, generator) &&
                            !pwm_state_blanked(cpu, generator, true))) &&
                          (fault & PWM_CURRENT_LIMIT_MODE) != 0u;
    if ((fault & 0x8000u) != 0u) {
        if (current_active) {
            *high = (io & 0x0020u) != 0u;
        }
        if (fault_active) {
            *low = (io & 0x0010u) != 0u;
        }
    } else if (fault_active) {
        *high = (io & 0x0020u) != 0u;
        *low = (io & 0x0010u) != 0u;
    } else if (current_active) {
        *high = (io & 0x0008u) != 0u;
        *low = (io & 0x0004u) != 0u;
    }
}

static void pwm_logical_output(const Dspic33* cpu, uint8_t generator, bool* high,
                               bool* low) {
    uint16_t io = pwm_output_io(cpu, generator);
    pwm_waveform_pair(cpu, generator, high, low);
    pwm_apply_protection(cpu, generator, high, low);
    if ((io & PWM_OVERRIDE_HIGH) != 0u) {
        *high = (io & 0x0080u) != 0u;
    }
    if ((io & PWM_OVERRIDE_LOW) != 0u) {
        *low = (io & 0x0040u) != 0u;
    }
    if ((io & PWM_SWAP) != 0u) {
        bool swap = *high;
        *high = *low;
        *low = swap;
    }
}

static void pwm_update_output(Dspic33* cpu, uint8_t generator) {
    uint16_t io = pwm_output_io(cpu, generator);
    uint16_t auxiliary = pwm_register(cpu, generator, 0x1eu);
    uint16_t leading = pwm_register(cpu, generator, 0x1au);
    bool previous_high = cpu->io.pwm[generator * 2u] != 0u;
    bool previous_low = cpu->io.pwm[generator * 2u + 1u] != 0u;
    bool high;
    bool low;
    pwm_logical_output(cpu, generator, &high, &low);
    if ((auxiliary & 0x0003u) != 0u) {
        uint8_t chop_source = (uint8_t)((auxiliary >> 2u) & 0x0fu);
        uint16_t chop_period =
            (uint16_t)((raw_word(cpu, PWM_GLOBAL_BASE + 0x1au) & 0x03ffu) + 1u);
        bool chop_high = false;
        if (chop_source == 0u) {
            chop_high = (raw_word(cpu, PWM_GLOBAL_BASE + 0x1au) & 0x8000u) != 0u &&
                        cpu->io.pwm_chop_counter < (uint16_t)((chop_period + 1u) / 2u);
        } else if (chop_source <= DSPIC33_PWM_COUNT) {
            bool chop_low;
            pwm_logical_output(cpu, (uint8_t)(chop_source - 1u), &chop_high, &chop_low);
        }
        if ((auxiliary & 0x0002u) != 0u) {
            high = high && chop_high;
        }
        if ((auxiliary & 0x0001u) != 0u) {
            low = low && chop_high;
        }
    }
    if ((io & PWM_POLARITY_HIGH) != 0u) {
        high = !high;
    }
    if ((io & PWM_POLARITY_LOW) != 0u) {
        low = !low;
    }
    if ((io & PWM_PIN_HIGH) == 0u) {
        high = false;
    }
    if ((io & PWM_PIN_LOW) == 0u) {
        low = false;
    }
    cpu->io.pwm[generator * 2u] = high ? 1u : 0u;
    cpu->io.pwm[generator * 2u + 1u] = low ? 1u : 0u;
    if ((previous_high != high || previous_low != low) && !cpu->io.pwm_batch_updating) {
        comparator_evaluate_all(cpu);
    }
    if ((!previous_high && high && (leading & 0x8000u) != 0u) ||
        (previous_high && !high && (leading & 0x4000u) != 0u) ||
        (!previous_low && low && (leading & 0x2000u) != 0u) ||
        (previous_low && !low && (leading & 0x1000u) != 0u)) {
        cpu->io.pwm_leb_ticks[generator] = pwm_register(cpu, generator, 0x1cu);
    }
}

static void pwm_emit_adc_trigger(Dspic33* cpu, uint8_t source) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        dspic33_adc_trigger(cpu, module, source, 0u);
    }
}

static void pwm_special_match(Dspic33* cpu, uint8_t time_base) {
    uint16_t control_address =
        (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu));
    uint16_t control = raw_word(cpu, control_address);
    uint8_t postscale = (uint8_t)(control & 0x000fu);
    cpu->io.pwm_special_count[time_base]++;
    if (cpu->io.pwm_special_count[time_base] <= postscale) {
        return;
    }
    cpu->io.pwm_special_count[time_base] = 0u;
    pwm_emit_adc_trigger(cpu, time_base == 0u ? 3u : 5u);
    if ((control & PWM_SPECIAL_INTERRUPT) != 0u) {
        raw_write_word(cpu, control_address, (uint16_t)(control | PWM_SPECIAL_STATUS));
        dspic33_raise_interrupt(cpu, time_base == 0u ? 57u : 73u);
    }
}

static void pwm_generator_match(Dspic33* cpu, uint8_t generator) {
    uint16_t control = pwm_register(cpu, generator, 0u);
    uint16_t trigger_control = pwm_register(cpu, generator, 0x14u);
    uint8_t start = (uint8_t)(trigger_control & 0x003fu);
    uint8_t divider = (uint8_t)(trigger_control >> 12u);
    uint16_t base = pwm_generator_base(generator);
    if (cpu->io.pwm_cycle_count[generator] < start) {
        return;
    }
    cpu->io.pwm_trigger_count[generator]++;
    if (cpu->io.pwm_trigger_count[generator] <= divider) {
        return;
    }
    cpu->io.pwm_trigger_count[generator] = 0u;
    pwm_emit_adc_trigger(cpu, (uint8_t)(8u + generator));
    if ((control & PWM_TRIGGER_INTERRUPT) != 0u) {
        raw_write_word(cpu, base, (uint16_t)(control | PWM_TRIGGER_STATUS));
        dspic33_raise_interrupt(cpu, pwm_irqs[generator]);
    }
}

static bool pwm_advance_independent_counter(Dspic33* cpu, uint8_t generator,
                                            uint8_t output) {
    uint16_t control = pwm_register(cpu, generator, 0u);
    uint16_t period = pwm_period(cpu, generator, output);
    uint16_t* counter = &cpu->io.pwm_counter[generator][output];
    uint8_t bit = (uint8_t)(1u << generator);
    bool descending = (cpu->io.pwm_direction[output] & bit) != 0u;
    if ((control & PWM_CENTER_ALIGNED) == 0u) {
        if (*counter >= period) {
            *counter = 0u;
            return true;
        }
        (*counter)++;
        return false;
    }
    if (period == 0u) {
        *counter = 0u;
        return true;
    }
    if (!descending) {
        if (*counter >= period) {
            cpu->io.pwm_direction[output] |= bit;
            (*counter)--;
        } else {
            (*counter)++;
        }
        return false;
    }
    if (*counter <= 1u) {
        *counter = 0u;
        cpu->io.pwm_direction[output] &= (uint8_t)~bit;
        return true;
    }
    (*counter)--;
    return false;
}

static bool pwm_advance_master_counter(Dspic33* cpu, uint8_t time_base,
                                       uint64_t cycle) {
    uint16_t* counter = &cpu->io.pwm_master_counter[time_base];
    uint16_t period = cpu->io.pwm_active_period[time_base];
    if (*counter >= period) {
        *counter = 0u;
        if ((raw_word(cpu,
                      (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu))) &
             0x0100u) != 0u) {
            cpu->io.pwm_sync_until[time_base] = cycle + 12u;
            comparator_filter_clock(cpu, (uint8_t)(2u + time_base), 1u);
        }
        return true;
    }
    (*counter)++;
    return false;
}

static void pwm_cycle_boundary(Dspic33* cpu, uint8_t generator, bool period_updated) {
    uint8_t bit = (uint8_t)(1u << generator);
    uint16_t fault = pwm_register(cpu, generator, 4u);
    uint16_t io = pwm_register(cpu, generator, 2u);
    bool immediate = (pwm_register(cpu, generator, 0u) & PWM_IMMEDIATE_UPDATE) != 0u;
    bool delayed =
        period_updated && !immediate && (cpu->io.pwm_timing_update & bit) != 0u;
    if (!delayed) {
        cpu->io.pwm_dead_time_sampled =
            (uint8_t)((cpu->io.pwm_dead_time_sampled & ~bit) |
                      (cpu->io.pwm_dead_time_inputs & bit));
        if (!immediate) {
            pwm_latch_generator(cpu, generator);
        }
        cpu->io.pwm_timing_update &= (uint8_t)~bit;
    }
    cpu->io.pwm_cycle_count[generator]++;
    if ((pwm_register(cpu, generator, 2u) & PWM_MODE_MASK) == PWM_MODE_PUSH_PULL) {
        cpu->io.pwm_push_pull ^= bit;
    }
    if ((io & PWM_OVERRIDE_SYNCHRONIZED) != 0u) {
        cpu->io.pwm_active_io[generator] = io;
    }
    if (!delayed && (cpu->io.pwm_fault_release & bit) != 0u &&
        !pwm_fault_active(cpu, generator)) {
        cpu->io.pwm_fault_latched &= (uint8_t)~bit;
        cpu->io.pwm_fault_release &= (uint8_t)~bit;
    }
    if (!delayed && (!pwm_fault_active(cpu, generator) ||
                     (fault & PWM_FAULT_MODE_MASK) != PWM_FAULT_CYCLE)) {
        cpu->io.pwm_fault_cycle &= (uint8_t)~bit;
    }
    if (!delayed && (!pwm_current_limit_active(cpu, generator) ||
                     (fault & PWM_CURRENT_LIMIT_MODE) == 0u)) {
        cpu->io.pwm_current_cycle &= (uint8_t)~bit;
    }
}

static void pwm_tick(Dspic33* cpu, uint8_t time_base, uint64_t cycle) {
    bool master_boundary;
    bool period_updated = false;
    uint8_t generator;
    uint16_t chop_period =
        (uint16_t)((raw_word(cpu, PWM_GLOBAL_BASE + 0x1au) & 0x03ffu) + 1u);
    master_boundary = pwm_advance_master_counter(cpu, time_base, cycle);
    if (time_base == 0u) {
        cpu->io.pwm_chop_counter =
            (uint16_t)((cpu->io.pwm_chop_counter + 1u) % chop_period);
    }
    if (master_boundary &&
        (raw_word(cpu, (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu))) &
         0x0400u) == 0u) {
        uint16_t period_address =
            (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 4u : 0x12u));
        cpu->io.pwm_active_period[time_base] = raw_word(cpu, period_address);
        period_updated = (cpu->io.pwm_period_update & (uint8_t)(1u << time_base)) != 0u;
        cpu->io.pwm_period_update &= (uint8_t)~(1u << time_base);
    }
    if (cpu->io.pwm_master_counter[time_base] ==
        raw_word(cpu, (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 6u : 0x14u)))) {
        pwm_special_match(cpu, time_base);
    }
    cpu->io.pwm_batch_updating = true;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        uint16_t control = pwm_register(cpu, generator, 0u);
        bool independent = (control & PWM_INDEPENDENT_TIME_BASE) != 0u;
        bool boundary;
        uint8_t master = (control & 0x0008u) != 0u ? 1u : 0u;
        if (master != time_base) {
            continue;
        }
        if (independent) {
            boundary = pwm_advance_independent_counter(cpu, generator, 0u);
            pwm_advance_independent_counter(cpu, generator, 1u);
            if (boundary) {
                pwm_cycle_boundary(cpu, generator, false);
            }
        } else {
            if (master_boundary) {
                pwm_cycle_boundary(cpu, generator, period_updated);
            }
            cpu->io.pwm_counter[generator][0] =
                pwm_shifted_counter(cpu->io.pwm_master_counter[master],
                                    cpu->io.pwm_active_phase[generator][0],
                                    cpu->io.pwm_active_period[master]);
            cpu->io.pwm_counter[generator][1] =
                pwm_shifted_counter(cpu->io.pwm_master_counter[master],
                                    cpu->io.pwm_active_phase[generator][1],
                                    cpu->io.pwm_active_period[master]);
        }
        if (cpu->io.pwm_counter[generator][0] == pwm_register(cpu, generator, 0x12u)) {
            pwm_generator_match(cpu, generator);
        }
    }
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        uint16_t control = pwm_register(cpu, generator, 0u);
        uint8_t master = (control & 0x0008u) != 0u ? 1u : 0u;
        if (master != time_base) {
            continue;
        }
        pwm_refresh_status(cpu, generator);
        pwm_update_output(cpu, generator);
    }
    cpu->io.pwm_batch_updating = false;
    comparator_evaluate_all(cpu);
}

static void advance_pwm(Dspic33* cpu, uint64_t cycles) {
    uint8_t time_base;
    uint64_t subcycles;
    uint8_t generator;
    if (!pwm_power_enabled(cpu) || cycles == 0u) {
        return;
    }
    subcycles = cycles * 2u;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        if (cpu->io.pwm_leb_ticks[generator] > subcycles) {
            cpu->io.pwm_leb_ticks[generator] =
                (uint16_t)(cpu->io.pwm_leb_ticks[generator] - subcycles);
        } else {
            cpu->io.pwm_leb_ticks[generator] = 0u;
        }
    }
    for (time_base = 0u; time_base < 2u; time_base++) {
        uint16_t divider = pwm_divider(cpu, time_base);
        uint64_t previous_fraction = cpu->io.pwm_fraction[time_base];
        uint64_t accumulated = previous_fraction + subcycles;
        uint64_t ticks = accumulated / divider;
        uint64_t elapsed_subcycles = divider - previous_fraction;
        cpu->io.pwm_fraction[time_base] = (uint32_t)(accumulated % divider);
        while (ticks-- != 0u) {
            uint64_t cycle =
                cpu->device_cycles - cycles + (elapsed_subcycles + 1u) / 2u;
            pwm_tick(cpu, time_base, cycle);
            elapsed_subcycles += divider;
        }
    }
}

static void pwm_start(Dspic33* cpu) {
    uint8_t generator;
    memset(cpu->io.pwm_counter, 0, sizeof(cpu->io.pwm_counter));
    memset(cpu->io.pwm_cycle_count, 0, sizeof(cpu->io.pwm_cycle_count));
    memset(cpu->io.pwm_trigger_count, 0, sizeof(cpu->io.pwm_trigger_count));
    memset(cpu->io.pwm_special_count, 0, sizeof(cpu->io.pwm_special_count));
    memset(cpu->io.pwm_direction, 0, sizeof(cpu->io.pwm_direction));
    cpu->io.pwm_master_counter[0] = 0u;
    cpu->io.pwm_master_counter[1] = 0u;
    cpu->io.pwm_push_pull = 0u;
    memset(cpu->io.pwm_fraction, 0, sizeof(cpu->io.pwm_fraction));
    cpu->io.pwm_chop_counter = 0u;
    cpu->io.pwm_dead_time_sampled = cpu->io.pwm_dead_time_inputs;
    cpu->io.pwm_period_update = 0u;
    cpu->io.pwm_timing_update = 0u;
    pwm_latch_periods(cpu);
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        uint8_t bit = (uint8_t)(1u << generator);
        uint16_t fault = pwm_register(cpu, generator, 4u);
        if (!pwm_fault_active(cpu, generator)) {
            cpu->io.pwm_fault_cycle &= (uint8_t)~bit;
            if ((cpu->io.pwm_fault_release & bit) != 0u) {
                cpu->io.pwm_fault_latched &= (uint8_t)~bit;
                cpu->io.pwm_fault_release &= (uint8_t)~bit;
            }
        }
        if (!pwm_current_limit_active(cpu, generator) ||
            (fault & PWM_CURRENT_LIMIT_MODE) == 0u) {
            cpu->io.pwm_current_cycle &= (uint8_t)~bit;
        }
        cpu->io.pwm_active_io[generator] = pwm_register(cpu, generator, 2u);
        pwm_latch_generator(cpu, generator);
        if ((pwm_register(cpu, generator, 0u) & PWM_INDEPENDENT_TIME_BASE) == 0u) {
            uint8_t master =
                (pwm_register(cpu, generator, 0u) & 0x0008u) != 0u ? 1u : 0u;
            cpu->io.pwm_counter[generator][0] =
                pwm_shifted_counter(0u, cpu->io.pwm_active_phase[generator][0],
                                    cpu->io.pwm_active_period[master]);
            cpu->io.pwm_counter[generator][1] =
                pwm_shifted_counter(0u, cpu->io.pwm_active_phase[generator][1],
                                    cpu->io.pwm_active_period[master]);
        }
        pwm_refresh_status(cpu, generator);
    }
    cpu->io.pwm_batch_updating = true;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        pwm_update_output(cpu, generator);
        if (pwm_register(cpu, generator, 0x12u) == 0u) {
            pwm_generator_match(cpu, generator);
        }
    }
    cpu->io.pwm_batch_updating = false;
    comparator_evaluate_all(cpu);
    if (raw_word(cpu, PWM_GLOBAL_BASE + 6u) == 0u) {
        pwm_special_match(cpu, 0u);
    }
    if (raw_word(cpu, PWM_GLOBAL_BASE + 0x14u) == 0u) {
        pwm_special_match(cpu, 1u);
    }
}

static void pwm_input_event(Dspic33* cpu, uint8_t source, bool high,
                            bool current_limit) {
    uint32_t bit = (uint32_t)1u << source;
    uint8_t generator;
    uint32_t* inputs =
        current_limit ? &cpu->io.pwm_current_limit_inputs : &cpu->io.pwm_fault_inputs;
    if (high) {
        *inputs |= bit;
    } else {
        *inputs &= ~bit;
    }
    if (!current_limit && (source == 1u || source == 3u)) {
        comparator_evaluate_all(cpu);
    }
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        uint16_t base = pwm_generator_base(generator);
        uint16_t control = raw_word(cpu, base);
        uint16_t fault = raw_word(cpu, (uint16_t)(base + 4u));
        uint8_t selected = current_limit ? (uint8_t)((fault >> 10u) & 0x1fu)
                                         : (uint8_t)((fault >> 3u) & 0x1fu);
        bool active = current_limit ? pwm_current_limit_active(cpu, generator)
                                    : pwm_fault_active(cpu, generator);
        uint8_t generator_bit = (uint8_t)(1u << generator);
        if (selected != source || pwm_state_blanked(cpu, generator, current_limit)) {
            continue;
        }
        cpu->io.pwm_timing_update |= generator_bit;
        if (current_limit) {
            if (active) {
                if ((fault & PWM_CURRENT_LIMIT_MODE) != 0u) {
                    cpu->io.pwm_current_cycle |= generator_bit;
                }
                raw_write_word(cpu, (uint16_t)(base + 0x18u),
                               cpu->io.pwm_counter[generator][0]);
                if ((control & PWM_CURRENT_LIMIT_INTERRUPT) != 0u) {
                    raw_write_word(cpu, base,
                                   (uint16_t)(control | PWM_CURRENT_LIMIT_STATUS));
                    dspic33_raise_interrupt(cpu, pwm_irqs[generator]);
                }
                if ((control & (PWM_INDEPENDENT_TIME_BASE | PWM_EXTERNAL_RESET)) ==
                        (PWM_INDEPENDENT_TIME_BASE | PWM_EXTERNAL_RESET) &&
                    (fault & PWM_CURRENT_LIMIT_MODE) == 0u) {
                    cpu->io.pwm_counter[generator][0] = 0u;
                }
            }
        } else if (active) {
            if ((fault & PWM_FAULT_MODE_MASK) == 0u) {
                cpu->io.pwm_fault_latched |= generator_bit;
            } else if ((fault & PWM_FAULT_MODE_MASK) == PWM_FAULT_CYCLE) {
                cpu->io.pwm_fault_cycle |= generator_bit;
            }
            if ((control & PWM_FAULT_INTERRUPT) != 0u) {
                raw_write_word(cpu, base, (uint16_t)(control | PWM_FAULT_STATUS));
                dspic33_raise_interrupt(cpu, pwm_irqs[generator]);
            }
        } else {
            cpu->io.pwm_fault_release |= generator_bit;
        }
        pwm_refresh_status(cpu, generator);
        pwm_update_output(cpu, generator);
    }
}

static void pwm_sync_event(Dspic33* cpu, uint8_t input, bool high) {
    uint8_t bit = (uint8_t)(1u << input);
    bool previous = (cpu->io.pwm_sync_inputs & bit) != 0u;
    uint8_t time_base;
    if (high) {
        cpu->io.pwm_sync_inputs |= bit;
    } else {
        cpu->io.pwm_sync_inputs &= (uint8_t)~bit;
    }
    for (time_base = 0u; time_base < 2u; time_base++) {
        uint16_t address = (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu));
        uint16_t control = raw_word(cpu, address);
        bool falling = (control & 0x0200u) != 0u;
        uint8_t selected = (uint8_t)((control >> 4u) & 7u);
        bool edge = falling ? previous && !high : !previous && high;
        if ((control & 0x0080u) != 0u && selected == input && edge) {
            cpu->io.pwm_master_counter[time_base] = 0u;
        }
    }
}

static void pwm_dead_time_event(Dspic33* cpu, uint8_t generator, bool high) {
    uint8_t bit = (uint8_t)(1u << generator);
    if (high) {
        cpu->io.pwm_dead_time_inputs |= bit;
    } else {
        cpu->io.pwm_dead_time_inputs &= (uint8_t)~bit;
    }
    if (!pwm_power_enabled(cpu)) {
        cpu->io.pwm_dead_time_sampled =
            (uint8_t)((cpu->io.pwm_dead_time_sampled & ~bit) |
                      (cpu->io.pwm_dead_time_inputs & bit));
    } else {
        cpu->io.pwm_timing_update |= bit;
    }
}

static uint16_t can_filter_word(const Dspic33* cpu, uint8_t channel, uint16_t offset) {
    return cpu->io.can_filter_window[channel][(offset - 0x20u) / 2u];
}

static uint8_t can_mode(const Dspic33* cpu, uint8_t channel) {
    return (uint8_t)((raw_word(cpu, can_bases[channel]) >> 5u) & 7u);
}

static bool can_power_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, can_bases[channel]);
    if ((raw_word(cpu, 0x0760u) & (uint16_t)(2u << channel)) != 0u) {
        return false;
    }
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    return cpu->power_state == DSPIC33_POWER_IDLE && (control & CAN_STOP_IDLE) == 0u;
}

static uint8_t can_buffer_count(const Dspic33* cpu, uint8_t channel) {
    static const uint8_t counts[] = {4u, 6u, 8u, 12u, 16u, 24u, 32u, 32u};
    uint16_t control = raw_word(cpu, (uint16_t)(can_bases[channel] + 6u));
    return counts[(control >> 13u) & 7u];
}

static uint16_t can_buffer_control(const Dspic33* cpu, uint8_t channel,
                                   uint8_t buffer) {
    uint16_t value =
        raw_word(cpu, (uint16_t)(can_bases[channel] + 0x30u + (buffer / 2u) * 2u));
    return (uint16_t)(value >> ((buffer & 1u) * 8u));
}

static void can_set_buffer_control(Dspic33* cpu, uint8_t channel, uint8_t buffer,
                                   uint16_t value) {
    uint16_t address = (uint16_t)(can_bases[channel] + 0x30u + (buffer / 2u) * 2u);
    uint16_t current = raw_word(cpu, address);
    uint8_t shift = (uint8_t)((buffer & 1u) * 8u);
    current = (uint16_t)((current & ~(uint16_t)(0xffu << shift)) |
                         ((value & 0xffu) << shift));
    raw_write_word(cpu, address, current);
}

static uint16_t can_buffer_flag_address(uint8_t channel, uint8_t buffer,
                                        bool overflow) {
    return (uint16_t)(can_bases[channel] + (overflow ? 0x28u : 0x20u) +
                      (buffer >= 16u ? 2u : 0u));
}

static bool can_buffer_flag(const Dspic33* cpu, uint8_t channel, uint8_t buffer,
                            bool overflow) {
    uint16_t address = can_buffer_flag_address(channel, buffer, overflow);
    return (raw_word(cpu, address) & (uint16_t)(1u << (buffer & 15u))) != 0u;
}

static void can_set_buffer_flag(Dspic33* cpu, uint8_t channel, uint8_t buffer,
                                bool overflow) {
    uint16_t address = can_buffer_flag_address(channel, buffer, overflow);
    raw_write_word(
        cpu, address,
        (uint16_t)(raw_word(cpu, address) | (uint16_t)(1u << (buffer & 15u))));
}

static void can_update_vector(Dspic33* cpu, uint8_t channel) {
    uint16_t base = can_bases[channel];
    uint16_t active = (uint16_t)(raw_word(cpu, (uint16_t)(base + 0x0au)) &
                                 raw_word(cpu, (uint16_t)(base + 0x0cu)));
    uint8_t code = 0x40u;
    if ((active & (CAN_INTERRUPT_TRANSMIT | CAN_INTERRUPT_RECEIVE)) != 0u) {
        code = cpu->io.can_last_buffer[channel];
    } else if ((active & CAN_INTERRUPT_ERROR) != 0u) {
        code = 0x41u;
    } else if ((active & CAN_INTERRUPT_WAKE) != 0u) {
        code = 0x42u;
    } else if ((active & CAN_INTERRUPT_OVERFLOW) != 0u) {
        code = 0x43u;
    } else if ((active & CAN_INTERRUPT_FIFO) != 0u) {
        code = 0x44u;
    }
    raw_write_word(
        cpu, (uint16_t)(base + 4u),
        (uint16_t)(((uint16_t)cpu->io.can_last_filter[channel] << 8u) | code));
    if (active != 0u) {
        dspic33_raise_interrupt(cpu, can_event_irqs[channel]);
    }
}

static void can_raise_event(Dspic33* cpu, uint8_t channel, uint16_t flag,
                            uint8_t buffer, uint8_t filter) {
    uint16_t address = (uint16_t)(can_bases[channel] + 0x0au);
    raw_write_word(cpu, address, (uint16_t)(raw_word(cpu, address) | flag));
    cpu->io.can_last_buffer[channel] = buffer;
    cpu->io.can_last_filter[channel] = filter;
    can_update_vector(cpu, channel);
}

static bool can_dma_ready(const Dspic33* cpu, uint8_t request, uint16_t pad,
                          bool transmit) {
    uint8_t dma;
    for (dma = 0u; dma < DSPIC33_DMA_COUNT; dma++) {
        uint16_t base = dma_channel_base(dma);
        uint16_t control = raw_word(cpu, base);
        if ((control & DMA_CON_CHEN) != 0u &&
            (raw_word(cpu, (uint16_t)(base + 2u)) & DMA_REQ_SOURCE_MASK) == request &&
            raw_word(cpu, (uint16_t)(base + 0x0cu)) == pad &&
            (control & DMA_CON_SIZE_BYTE) == 0u &&
            (control & DMA_CON_AMODE_MASK) == DMA_CON_AMODE_PERIPHERAL &&
            ((control & DMA_CON_RAM_TO_PERIPHERAL) != 0u) == transmit) {
            return true;
        }
    }
    return false;
}

static uint32_t can_identifier_sid(const Dspic33CanFrame* frame) {
    return frame->extended ? (frame->identifier >> 18u) & 0x7ffu
                           : frame->identifier & 0x7ffu;
}

static uint32_t can_identifier_eid(const Dspic33CanFrame* frame) {
    return frame->identifier & 0x3ffffu;
}

static bool can_devicenet_match(const Dspic33CanFrame* frame, uint32_t expected,
                                uint8_t bits) {
    uint32_t data = 0u;
    uint8_t available = (uint8_t)(frame->length * 8u);
    if (bits > 18u) {
        bits = 18u;
    }
    if (bits > available) {
        bits = available;
    }
    if (bits == 0u) {
        return true;
    }
    data = (uint32_t)frame->data[0] << 16u;
    if (frame->length > 1u) {
        data |= (uint32_t)frame->data[1] << 8u;
    }
    if (frame->length > 2u) {
        data |= frame->data[2];
    }
    return (data >> (24u - bits)) == (expected >> (18u - bits));
}

static bool can_filter_matches(const Dspic33* cpu, uint8_t channel, uint8_t filter,
                               const Dspic33CanFrame* frame) {
    uint16_t filter_sid =
        can_filter_word(cpu, channel, (uint16_t)(0x40u + filter * 4u));
    uint16_t filter_eid =
        can_filter_word(cpu, channel, (uint16_t)(0x42u + filter * 4u));
    uint16_t selection =
        raw_word(cpu, (uint16_t)(can_bases[channel] + (filter < 8u ? 0x18u : 0x1au)));
    uint8_t mask_index = (uint8_t)((selection >> ((filter & 7u) * 2u)) & 3u);
    uint16_t mask_sid;
    uint16_t mask_eid;
    uint32_t sid = can_identifier_sid(frame);
    uint32_t eid = can_identifier_eid(frame);
    uint8_t devicenet =
        (uint8_t)(raw_word(cpu, (uint16_t)(can_bases[channel] + 2u)) & 0x001fu);
    if (mask_index >= 3u) {
        return false;
    }
    mask_sid = can_filter_word(cpu, channel, (uint16_t)(0x30u + mask_index * 4u));
    mask_eid = can_filter_word(cpu, channel, (uint16_t)(0x32u + mask_index * 4u));
    if ((mask_sid & 0x0008u) != 0u &&
        frame->extended != ((filter_sid & 0x0008u) != 0u)) {
        return false;
    }
    if ((((sid << 5u) ^ filter_sid) & mask_sid & 0xffe0u) != 0u) {
        return false;
    }
    if (!frame->extended && devicenet != 0u && (mask_sid & 0x0008u) != 0u &&
        (filter_sid & 0x0008u) == 0u) {
        uint32_t expected = ((uint32_t)(filter_sid & 3u) << 16u) | filter_eid;
        return can_devicenet_match(frame, expected, devicenet);
    }
    if (frame->extended) {
        uint32_t expected = ((uint32_t)(filter_sid & 3u) << 16u) | filter_eid;
        uint32_t mask = ((uint32_t)(mask_sid & 3u) << 16u) | mask_eid;
        return ((eid ^ expected) & mask) == 0u;
    }
    return true;
}

static uint8_t can_filter_buffer(const Dspic33* cpu, uint8_t channel, uint8_t filter) {
    uint16_t value =
        can_filter_word(cpu, channel, (uint16_t)(0x20u + (filter / 4u) * 2u));
    return (uint8_t)((value >> ((filter & 3u) * 4u)) & 0x0fu);
}

static uint8_t can_fifo_end(const Dspic33* cpu, uint8_t channel) {
    return (uint8_t)(can_buffer_count(cpu, channel) - 1u);
}

static uint8_t can_next_fifo_buffer(const Dspic33* cpu, uint8_t channel,
                                    uint8_t buffer) {
    uint8_t start =
        (uint8_t)(raw_word(cpu, (uint16_t)(can_bases[channel] + 6u)) & 0x001fu);
    return buffer >= can_fifo_end(cpu, channel) ? start : (uint8_t)(buffer + 1u);
}

static uint8_t can_advance_fifo_write(Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint8_t next = can_next_fifo_buffer(cpu, channel, buffer);
    uint16_t address = (uint16_t)(can_bases[channel] + 8u);
    uint16_t fifo = raw_word(cpu, address);
    cpu->io.can_fifo_write[channel] = next;
    raw_write_word(cpu, address, (uint16_t)((fifo & 0x003fu) | ((uint16_t)next << 8u)));
    return next;
}

static bool can_select_receive_buffer(Dspic33* cpu, uint8_t channel,
                                      const Dspic33CanFrame* frame, uint8_t* buffer,
                                      uint8_t* matched_filter) {
    uint16_t enabled = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x14u));
    uint8_t first_buffer = 0u;
    uint8_t first_filter = 0u;
    bool first_fifo = false;
    bool matched = false;
    uint8_t filter;
    for (filter = 0u; filter < 16u; filter++) {
        bool fifo;
        uint8_t target;
        uint16_t control;
        if ((enabled & (uint16_t)(1u << filter)) == 0u ||
            !can_filter_matches(cpu, channel, filter, frame)) {
            continue;
        }
        target = can_filter_buffer(cpu, channel, filter);
        fifo = target == 15u;
        if (fifo) {
            target = cpu->io.can_fifo_write[channel];
        }
        if (!matched) {
            matched = true;
            first_buffer = target;
            first_filter = filter;
            first_fifo = fifo;
        }
        if (target >= can_buffer_count(cpu, channel) || target > 31u) {
            continue;
        }
        control = target < 8u ? can_buffer_control(cpu, channel, target) : 0u;
        if (target < 8u && (control & CAN_BUFFER_TRANSMIT) != 0u) {
            if (frame->remote && (control & CAN_BUFFER_REMOTE) != 0u) {
                can_set_buffer_control(cpu, channel, target,
                                       (uint16_t)(control | CAN_BUFFER_REQUEST));
                dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel,
                                 CAN_EVENT_TRANSMIT_START, 0u);
                return false;
            }
            continue;
        }
        if (can_buffer_flag(cpu, channel, target, false)) {
            continue;
        }
        *buffer = target;
        *matched_filter = filter;
        return true;
    }
    if (matched && first_buffer < 32u) {
        can_set_buffer_flag(cpu, channel, first_buffer, true);
        can_raise_event(cpu, channel, CAN_INTERRUPT_OVERFLOW, first_buffer,
                        first_filter);
        if (first_fifo) {
            can_advance_fifo_write(cpu, channel, first_buffer);
        }
    }
    return false;
}

static void can_encode_frame(const Dspic33CanFrame* frame, uint8_t filter,
                             uint16_t words[8]) {
    uint32_t sid = can_identifier_sid(frame);
    uint32_t eid = can_identifier_eid(frame);
    uint8_t index;
    memset(words, 0, sizeof(uint16_t) * 8u);
    words[0] = (uint16_t)(sid << 2u);
    if (frame->extended) {
        words[0] |= 3u;
        words[1] = (uint16_t)(eid >> 6u);
        words[2] = (uint16_t)((eid & 0x3fu) << 10u);
        if (frame->remote) {
            words[2] |= 0x0200u;
        }
    } else if (frame->remote) {
        words[0] |= 2u;
    }
    words[2] |= frame->length > 8u ? 8u : frame->length;
    for (index = 0u; index < frame->length && index < 8u; index++) {
        words[3u + index / 2u] |= (uint16_t)frame->data[index] << ((index & 1u) * 8u);
    }
    words[7] = (uint16_t)filter << 8u;
}

static Dspic33CanFrame can_decode_frame(const uint16_t words[8]) {
    Dspic33CanFrame frame;
    uint32_t sid = (words[0] >> 2u) & 0x7ffu;
    uint8_t index;
    memset(&frame, 0, sizeof(frame));
    frame.extended = (words[0] & 1u) != 0u;
    if (frame.extended) {
        frame.identifier = (sid << 18u) | ((uint32_t)(words[1] & 0x0fffu) << 6u) |
                           ((words[2] >> 10u) & 0x3fu);
        frame.remote = (words[2] & 0x0200u) != 0u;
    } else {
        frame.identifier = sid;
        frame.remote = (words[0] & 2u) != 0u;
    }
    frame.length = (uint8_t)(words[2] & 0x0fu);
    if (frame.length > 8u) {
        frame.length = 8u;
    }
    for (index = 0u; index < frame.length; index++) {
        frame.data[index] = (uint8_t)(words[3u + index / 2u] >> ((index & 1u) * 8u));
    }
    return frame;
}

static void can_refresh_error_status(Dspic33* cpu, uint8_t channel) {
    uint16_t base = can_bases[channel];
    uint16_t counts = raw_word(cpu, (uint16_t)(base + 0x0eu));
    uint16_t status = raw_word(cpu, (uint16_t)(base + 0x0au));
    uint8_t receive = (uint8_t)counts;
    uint8_t transmit = (uint8_t)(counts >> 8u);
    bool bus_off = (status & CAN_BUS_OFF) != 0u;
    status &= 0x00ffu;
    if (receive >= 96u || transmit >= 96u) {
        status |= CAN_ERROR_WARNING;
    }
    if (receive >= 96u && receive < 128u) {
        status |= CAN_RECEIVE_WARNING;
    }
    if (transmit >= 96u && transmit < 128u) {
        status |= CAN_TRANSMIT_WARNING;
    }
    if (receive >= 128u) {
        status |= CAN_RECEIVE_PASSIVE;
    }
    if (transmit >= 128u && !bus_off) {
        status |= CAN_TRANSMIT_PASSIVE;
    }
    if (bus_off) {
        status |= CAN_BUS_OFF;
    }
    raw_write_word(cpu, (uint16_t)(base + 0x0au), status);
}

static void can_receive_start(Dspic33* cpu, uint8_t channel) {
    Dspic33CanFrame frame;
    uint8_t buffer;
    uint8_t filter;
    uint8_t bit = (uint8_t)(1u << channel);
    if ((cpu->io.can_rx_busy & bit) != 0u ||
        !can_queue_pop(&cpu->io.can_rx[channel], &frame)) {
        return;
    }
    if (cpu->power_state == DSPIC33_POWER_SLEEP) {
        can_raise_event(cpu, channel, CAN_INTERRUPT_WAKE, 0u, 0u);
        return;
    }
    if (!can_power_enabled(cpu, channel) ||
        can_mode(cpu, channel) == CAN_MODE_DISABLE ||
        can_mode(cpu, channel) == CAN_MODE_CONFIGURATION) {
        return;
    }
    if (can_mode(cpu, channel) == CAN_MODE_LISTEN_ALL) {
        bool fifo;
        bool transmit;
        filter = 0u;
        buffer = can_filter_buffer(cpu, channel, filter);
        fifo = buffer == 15u;
        if (fifo) {
            buffer = cpu->io.can_fifo_write[channel];
        }
        transmit = buffer < 8u && (can_buffer_control(cpu, channel, buffer) &
                                   CAN_BUFFER_TRANSMIT) != 0u;
        if (buffer >= can_buffer_count(cpu, channel) || transmit ||
            can_buffer_flag(cpu, channel, buffer, false)) {
            if (buffer < 32u) {
                can_set_buffer_flag(cpu, channel, buffer, true);
                can_raise_event(cpu, channel, CAN_INTERRUPT_OVERFLOW, buffer, filter);
                if (fifo) {
                    can_advance_fifo_write(cpu, channel, buffer);
                }
            }
            return;
        }
    } else if (!can_select_receive_buffer(cpu, channel, &frame, &buffer, &filter)) {
        return;
    }
    if (!can_dma_ready(cpu, can_rx_requests[channel],
                       (uint16_t)(can_bases[channel] + 0x40u), false)) {
        dspic33_raise_interrupt(cpu, can_rx_irqs[channel]);
        return;
    }
    can_encode_frame(&frame, filter, cpu->io.can_rx_words[channel]);
    cpu->io.can_rx_buffer[channel] = buffer;
    cpu->io.can_rx_filter[channel] = filter;
    cpu->io.can_rx_word[channel] = 0u;
    cpu->io.can_rx_busy |= bit;
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_WORD, 0u);
}

static void can_receive_word(Dspic33* cpu, uint8_t channel) {
    uint8_t word = cpu->io.can_rx_word[channel];
    if (word >= 8u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_FINISH, 0u);
        return;
    }
    cpu->io.can_rx_word[channel]++;
    dspic33_raise_interrupt(cpu, can_rx_irqs[channel]);
    dspic33_dma_request(cpu, can_rx_requests[channel],
                        (uint16_t)(cpu->io.can_rx_buffer[channel] * 16u + word * 2u),
                        0u);
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_WORD, 1u);
}

static void can_receive_finish(Dspic33* cpu, uint8_t channel) {
    uint8_t buffer = cpu->io.can_rx_buffer[channel];
    uint8_t filter = cpu->io.can_rx_filter[channel];
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t next;
    uint16_t control = raw_word(cpu, (uint16_t)(can_bases[channel] + 6u));
    uint16_t fifo;
    can_set_buffer_flag(cpu, channel, buffer, false);
    if (can_filter_buffer(cpu, channel, filter) == 15u) {
        next = can_advance_fifo_write(cpu, channel, buffer);
        fifo = raw_word(cpu, (uint16_t)(can_bases[channel] + 8u));
        if ((fifo & 0x003fu) == next + 1u ||
            (((fifo & 0x003fu) == (control & 0x001fu)) &&
             next == can_fifo_end(cpu, channel))) {
            can_raise_event(cpu, channel, CAN_INTERRUPT_FIFO, buffer, filter);
        }
    }
    can_raise_event(cpu, channel, CAN_INTERRUPT_RECEIVE, buffer, filter);
    cpu->io.can_rx_busy &= (uint8_t)~bit;
    if (cpu->io.can_rx[channel].count != 0u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_START, 0u);
    }
}

static int can_transmit_selection(const Dspic33* cpu, uint8_t channel) {
    int selected = -1;
    uint8_t priority = 0u;
    uint8_t buffer;
    for (buffer = 0u; buffer < 8u; buffer++) {
        uint16_t control = can_buffer_control(cpu, channel, buffer);
        uint8_t current_priority = (uint8_t)(control & 3u);
        if ((control & (CAN_BUFFER_TRANSMIT | CAN_BUFFER_REQUEST)) !=
            (CAN_BUFFER_TRANSMIT | CAN_BUFFER_REQUEST)) {
            continue;
        }
        if (selected < 0 || current_priority > priority ||
            (current_priority == priority && buffer > (uint8_t)selected)) {
            selected = buffer;
            priority = current_priority;
        }
    }
    return selected;
}

static void can_transmit_start(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    int selected;
    if ((cpu->io.can_tx_busy & bit) != 0u || !can_power_enabled(cpu, channel) ||
        (can_mode(cpu, channel) != CAN_MODE_NORMAL &&
         can_mode(cpu, channel) != CAN_MODE_LOOPBACK) ||
        (raw_word(cpu, (uint16_t)(can_bases[channel] + 0x0au)) & CAN_BUS_OFF) != 0u) {
        return;
    }
    selected = can_transmit_selection(cpu, channel);
    if (selected < 0) {
        return;
    }
    if (!can_dma_ready(cpu, can_tx_requests[channel],
                       (uint16_t)(can_bases[channel] + 0x42u), true)) {
        dspic33_raise_interrupt(cpu, can_tx_irqs[channel]);
        return;
    }
    cpu->io.can_tx_buffer[channel] = (uint8_t)selected;
    cpu->io.can_tx_word[channel] = 0u;
    memset(cpu->io.can_tx_words[channel], 0, sizeof(cpu->io.can_tx_words[channel]));
    cpu->io.can_tx_busy |= bit;
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_WORD, 0u);
}

static void can_transmit_word(Dspic33* cpu, uint8_t channel) {
    uint8_t word = cpu->io.can_tx_word[channel];
    if (word >= 8u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_FINISH,
                         0u);
        return;
    }
    cpu->io.can_tx_word[channel]++;
    dspic33_raise_interrupt(cpu, can_tx_irqs[channel]);
    dspic33_dma_request(cpu, can_tx_requests[channel],
                        (uint16_t)(cpu->io.can_tx_buffer[channel] * 16u + word * 2u),
                        0u);
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_WORD, 1u);
}

static void can_transmit_finish(Dspic33* cpu, uint8_t channel) {
    uint8_t buffer = cpu->io.can_tx_buffer[channel];
    uint8_t bit = (uint8_t)(1u << channel);
    uint16_t control = can_buffer_control(cpu, channel, buffer);
    uint16_t counts = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x0eu));
    Dspic33CanFrame frame = can_decode_frame(cpu->io.can_tx_words[channel]);
    can_set_buffer_control(cpu, channel, buffer,
                           (uint16_t)(control & ~CAN_BUFFER_REQUEST));
    if ((counts >> 8u) != 0u) {
        counts = (uint16_t)(counts - 0x0100u);
        raw_write_word(cpu, (uint16_t)(can_bases[channel] + 0x0eu), counts);
        can_refresh_error_status(cpu, channel);
    }
    can_raise_event(cpu, channel, CAN_INTERRUPT_TRANSMIT, buffer, 0u);
    if (can_mode(cpu, channel) == CAN_MODE_LOOPBACK) {
        can_queue_push(&cpu->io.can_rx[channel], &frame);
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_START, 0u);
    } else {
        can_queue_push(&cpu->io.can_tx[channel], &frame);
    }
    cpu->io.can_tx_busy &= (uint8_t)~bit;
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u);
}

static void can_error_event(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint16_t address = (uint16_t)(can_bases[channel] + 0x0eu);
    uint16_t counts = raw_word(cpu, address);
    uint16_t increment = (uint16_t)(value >> CAN_EVENT_ERROR_COUNT_SHIFT);
    if ((value & CAN_EVENT_TRANSMIT_ERROR) != 0u) {
        uint16_t transmit = (uint16_t)(counts >> 8u);
        uint32_t total = (uint32_t)transmit + increment;
        if (total > 0xffu) {
            raw_write_word(
                cpu, (uint16_t)(can_bases[channel] + 0x0au),
                (uint16_t)(raw_word(cpu, (uint16_t)(can_bases[channel] + 0x0au)) |
                           CAN_BUS_OFF));
            transmit = 0xffu;
        } else {
            transmit = (uint16_t)total;
        }
        counts = (uint16_t)((counts & 0x00ffu) | (transmit << 8u));
    } else {
        uint16_t receive = (uint16_t)(counts & 0x00ffu);
        receive = receive + increment > 0xffu ? 0xffu : receive + increment;
        counts = (uint16_t)((counts & 0xff00u) | receive);
    }
    raw_write_word(cpu, address, counts);
    can_refresh_error_status(cpu, channel);
}

static void run_can(Dspic33* cpu, uint8_t channel, uint32_t value) {
    if (channel >= DSPIC33_CAN_COUNT) {
        return;
    }
    switch (value & CAN_EVENT_KIND_MASK) {
    case CAN_EVENT_RECEIVE_START:
        can_receive_start(cpu, channel);
        break;
    case CAN_EVENT_RECEIVE_WORD:
        can_receive_word(cpu, channel);
        break;
    case CAN_EVENT_RECEIVE_FINISH:
        can_receive_finish(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_START:
        can_transmit_start(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_WORD:
        can_transmit_word(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_FINISH:
        can_transmit_finish(cpu, channel);
        break;
    case CAN_EVENT_ERROR:
        can_error_event(cpu, channel, value);
        break;
    }
}

static bool timer_is_type_b(uint8_t timer) { return timer >= 1u && (timer & 1u) != 0u; }

static bool timer_is_type_c(uint8_t timer) { return timer >= 2u && (timer & 1u) == 0u; }

static bool timer_pair_enabled(const Dspic33* cpu, uint8_t timer) {
    return timer_is_type_b(timer) &&
           (raw_word(cpu, timer_controls[timer]) & TIMER_32_BIT) != 0u;
}

static bool timer_is_paired_high(const Dspic33* cpu, uint8_t timer) {
    return timer_is_type_c(timer) && timer_pair_enabled(cpu, (uint8_t)(timer - 1u));
}

static uint32_t timer_prescale(uint16_t control) {
    switch (control & TIMER_PRESCALE_MASK) {
    case 0x0010u:
        return 8u;
    case 0x0020u:
        return 64u;
    case 0x0030u:
        return 256u;
    default:
        return 1u;
    }
}

static bool timer_power_enabled(const Dspic33* cpu, uint8_t timer, bool external) {
    uint16_t control = raw_word(cpu, timer_controls[timer]);
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    if (cpu->power_state == DSPIC33_POWER_IDLE && (control & TIMER_STOP_IDLE) == 0u &&
        (!timer_pair_enabled(cpu, timer) ||
         (raw_word(cpu, timer_controls[timer + 1u]) & TIMER_STOP_IDLE) == 0u)) {
        return true;
    }
    return external && timer == 0u && (control & TIMER_SYNC) == 0u;
}

typedef struct {
    uint64_t value;
    uint64_t matches;
} TimerAdvance;

static TimerAdvance advance_counter(uint64_t current, uint64_t period, uint64_t maximum,
                                    uint64_t ticks) {
    TimerAdvance result = {current, 0u};
    uint64_t cycle = period + 1u;
    uint64_t first_match;
    uint64_t first_reset;
    uint64_t remaining;
    if (ticks == 0u) {
        return result;
    }
    if (current == period) {
        first_match = cycle;
        first_reset = 1u;
    } else if (current < period) {
        first_match = period - current;
        first_reset = first_match + 1u;
    } else {
        first_match = maximum - current + 1u + period;
        first_reset = first_match + 1u;
    }
    if (ticks >= first_match) {
        result.matches = 1u + (ticks - first_match) / cycle;
    }
    if (ticks < first_reset) {
        result.value = (current + ticks) & maximum;
        return result;
    }
    remaining = ticks - first_reset;
    result.value = remaining % cycle;
    return result;
}

static void signal_timer_period(Dspic33* cpu, uint8_t timer, uint64_t matches,
                                bool gated, uint16_t* synchronization_sources) {
    if (matches != 0u) {
        if (timer < 5u) {
            *synchronization_sources |= (uint16_t)(1u << timer);
        }
        if (timer >= 1u && timer <= 4u) {
            dspic33_dma_request(cpu, timer_irqs[timer], 0u, 0u);
        }
        if (!gated) {
            cpu->io.timer_interrupt_pending |= (uint16_t)(1u << timer);
        }
    }
}

static void advance_timer_ticks(Dspic33* cpu, uint8_t timer, uint64_t ticks,
                                uint16_t* synchronization_sources) {
    uint16_t control = raw_word(cpu, timer_controls[timer]);
    bool paired = timer_pair_enabled(cpu, timer);
    bool gated = (control & TIMER_GATE) != 0u && (control & TIMER_EXTERNAL) == 0u;
    if (ticks == 0u) {
        return;
    }
    if (paired) {
        uint64_t current =
            raw_word(cpu, timer_registers[timer]) |
            ((uint64_t)raw_word(cpu, timer_registers[timer + 1u]) << 16u);
        uint64_t period = raw_word(cpu, timer_periods[timer]) |
                          ((uint64_t)raw_word(cpu, timer_periods[timer + 1u]) << 16u);
        TimerAdvance result = advance_counter(current, period, UINT32_MAX, ticks);
        raw_write_word(cpu, timer_registers[timer], (uint16_t)result.value);
        raw_write_word(cpu, timer_registers[timer + 1u],
                       (uint16_t)(result.value >> 16u));
        if (period != 0u) {
            signal_timer_period(cpu, (uint8_t)(timer + 1u), result.matches, gated,
                                synchronization_sources);
        }
    } else {
        uint16_t period = raw_word(cpu, timer_periods[timer]);
        TimerAdvance result = advance_counter(raw_word(cpu, timer_registers[timer]),
                                              period, UINT16_MAX, ticks);
        raw_write_word(cpu, timer_registers[timer], (uint16_t)result.value);
        if (period != 0u) {
            signal_timer_period(cpu, timer, result.matches, gated,
                                synchronization_sources);
        }
    }
}

static void pulse_timer_synchronization_sources(Dspic33* cpu, uint16_t* pending) {
    uint16_t sources = *pending;
    uint8_t timer = 0u;
    *pending = 0u;
    while (sources != 0u) {
        if ((sources & 1u) != 0u) {
            uint8_t source = (uint8_t)(INPUT_CAPTURE_SYNC_TIMER_FIRST + timer);
            input_capture_pulse_source(cpu, source);
            output_compare_pulse_source(cpu, source);
        }
        sources >>= 1u;
        timer++;
    }
}

static uint64_t timer_ticks_until_period(const Dspic33* cpu, uint8_t timer) {
    uint64_t current;
    uint64_t period;
    uint64_t maximum;
    if (timer_pair_enabled(cpu, timer)) {
        current = raw_word(cpu, timer_registers[timer]) |
                  ((uint64_t)raw_word(cpu, timer_registers[timer + 1u]) << 16u);
        period = raw_word(cpu, timer_periods[timer]) |
                 ((uint64_t)raw_word(cpu, timer_periods[timer + 1u]) << 16u);
        maximum = UINT32_MAX;
    } else {
        current = raw_word(cpu, timer_registers[timer]);
        period = raw_word(cpu, timer_periods[timer]);
        maximum = UINT16_MAX;
    }
    if (period == 0u) {
        return UINT64_MAX;
    }
    if (current == period) {
        return period + 1u;
    }
    if (current < period) {
        return period - current;
    }
    return maximum - current + 1u + period;
}

static void clock_timer(Dspic33* cpu, uint8_t timer, uint64_t clocks,
                        uint16_t* synchronization_sources, bool flush_sources) {
    uint16_t control = raw_word(cpu, timer_controls[timer]);
    uint32_t prescale = timer_prescale(control);
    uint64_t accumulated = cpu->io.timer_fraction[timer] + clocks;
    uint64_t ticks = accumulated / prescale;
    uint8_t signal_timer =
        timer_pair_enabled(cpu, timer) ? (uint8_t)(timer + 1u) : timer;
    uint8_t sync_source = (uint8_t)(INPUT_CAPTURE_SYNC_TIMER_FIRST + signal_timer);
    cpu->io.timer_fraction[timer] = (uint32_t)(accumulated % prescale);
    while (ticks != 0u) {
        uint64_t step = ticks;
        if (signal_timer < 5u && (input_capture_source_awaited(cpu, sync_source) ||
                                  output_compare_source_awaited(cpu, sync_source))) {
            uint64_t boundary = timer_ticks_until_period(cpu, timer);
            if (boundary < step) {
                step = boundary;
            }
        }
        if (timer < 5u) {
            uint64_t boundary = output_compare_clock_boundary_ticks(cpu, timer);
            if (boundary < step) {
                step = boundary;
            }
        }
        if (signal_timer != timer && signal_timer < 5u) {
            uint64_t boundary = output_compare_clock_boundary_ticks(cpu, signal_timer);
            if (boundary < step) {
                step = boundary;
            }
        }
        if (timer < 5u) {
            static const uint16_t capture_sources[5] = {0x1000u, 0x0400u, 0x0000u,
                                                        0x0800u, 0x0c00u};
            input_capture_advance_clock(cpu, capture_sources[timer], step);
            output_compare_advance_clock(cpu, timer, step);
            if (signal_timer != timer && signal_timer < 5u) {
                output_compare_advance_clock(cpu, signal_timer, step);
            }
        }
        advance_timer_ticks(cpu, timer, step, synchronization_sources);
        if (timer >= 1u && timer <= 4u) {
            comparator_filter_clock(cpu, (uint8_t)(timer + 3u), step);
        }
        if (flush_sources) {
            pulse_timer_synchronization_sources(cpu, synchronization_sources);
        }
        ticks -= step;
    }
}

static void pulse_timer(Dspic33* cpu, uint8_t timer, uint32_t pulses) {
    uint16_t synchronization_sources = 0u;
    uint16_t bit;
    uint16_t control;
    if (timer >= DSPIC33_TIMER_COUNT || pulses == 0u ||
        timer_is_paired_high(cpu, timer)) {
        return;
    }
    bit = (uint16_t)(1u << timer);
    control = raw_word(cpu, timer_controls[timer]);
    if ((cpu->io.timer_enabled & bit) == 0u || (control & TIMER_EXTERNAL) == 0u ||
        !timer_power_enabled(cpu, timer, true)) {
        return;
    }
    if ((timer == 0u || timer_is_type_b(timer)) &&
        (cpu->io.timer_external_started & bit) == 0u) {
        cpu->io.timer_external_started |= bit;
        pulses--;
    }
    clock_timer(cpu, timer, pulses, &synchronization_sources, true);
    pulse_timer_synchronization_sources(cpu, &synchronization_sources);
}

static void set_timer_gate(Dspic33* cpu, uint8_t timer, bool high) {
    uint16_t bit;
    bool previous;
    uint16_t control;
    if (timer >= DSPIC33_TIMER_COUNT || timer_is_paired_high(cpu, timer)) {
        return;
    }
    bit = (uint16_t)(1u << timer);
    previous = (cpu->io.timer_gate & bit) != 0u;
    if (high) {
        cpu->io.timer_gate |= bit;
    } else {
        cpu->io.timer_gate &= (uint16_t)~bit;
    }
    control = raw_word(cpu, timer_controls[timer]);
    if (previous && !high && (cpu->io.timer_enabled & bit) != 0u &&
        (control & (TIMER_GATE | TIMER_EXTERNAL)) == TIMER_GATE &&
        timer_power_enabled(cpu, timer, false)) {
        uint8_t interrupt_timer =
            timer_pair_enabled(cpu, timer) ? (uint8_t)(timer + 1u) : timer;
        uint32_t period = raw_word(cpu, timer_periods[timer]);
        if (timer_pair_enabled(cpu, timer)) {
            period |= (uint32_t)raw_word(cpu, timer_periods[timer + 1u]) << 16u;
        }
        if (period != 0u) {
            dspic33_raise_interrupt(cpu, timer_irqs[interrupt_timer]);
        }
    }
}

static bool usb_queue_push(Dspic33UsbQueue* queue, const Dspic33UsbPacket* packet) {
    uint8_t index;
    if (queue->count == DSPIC33_USB_PACKET_QUEUE_SIZE) {
        return false;
    }
    index = (uint8_t)((queue->head + queue->count) % DSPIC33_USB_PACKET_QUEUE_SIZE);
    queue->packets[index] = *packet;
    queue->count++;
    return true;
}

static bool usb_queue_pop(Dspic33UsbQueue* queue, Dspic33UsbPacket* packet) {
    if (queue->count == 0u) {
        return false;
    }
    *packet = queue->packets[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % DSPIC33_USB_PACKET_QUEUE_SIZE);
    queue->count--;
    return true;
}

static uint32_t usb_bdt_base(const Dspic33* cpu) {
    return ((uint32_t)(raw_word(cpu, USB_BDTP3) & 0x00ffu) << 24u) |
           ((uint32_t)(raw_word(cpu, USB_BDTP2) & 0x00ffu) << 16u) |
           ((uint32_t)(raw_word(cpu, USB_BDTP1) & 0x00feu) << 8u);
}

static uint32_t usb_descriptor_address(const Dspic33* cpu, uint8_t endpoint,
                                       uint8_t direction, uint8_t bank) {
    uint32_t index = (uint32_t)endpoint * 4u + (uint32_t)direction * 2u + bank;
    return usb_bdt_base(cpu) + index * 8u;
}

static bool usb_memory_word(const Dspic33* cpu, uint32_t address, uint16_t* value) {
    if (address > DSPIC33_DATA_SIZE - 2u) {
        return false;
    }
    *value = (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
    return true;
}

static bool usb_write_memory_word(Dspic33* cpu, uint32_t address, uint16_t value) {
    if (address > DSPIC33_DATA_SIZE - 2u) {
        return false;
    }
    cpu->data[address] = (uint8_t)value;
    cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    return true;
}

static bool usb_descriptor(const Dspic33* cpu, uint8_t endpoint, uint8_t direction,
                           uint8_t bank, uint16_t words[4]) {
    uint32_t address = usb_descriptor_address(cpu, endpoint, direction, bank);
    uint8_t index;
    for (index = 0u; index < 4u; index++) {
        if (!usb_memory_word(cpu, address + index * 2u, &words[index])) {
            return false;
        }
    }
    return true;
}

static bool usb_write_descriptor(Dspic33* cpu, uint8_t endpoint, uint8_t direction,
                                 uint8_t bank, const uint16_t words[4]) {
    uint32_t address = usb_descriptor_address(cpu, endpoint, direction, bank);
    uint8_t index;
    for (index = 0u; index < 4u; index++) {
        if (!usb_write_memory_word(cpu, address + index * 2u, words[index])) {
            return false;
        }
    }
    return true;
}

static void usb_refresh_interrupt(Dspic33* cpu) {
    uint16_t errors = (uint16_t)(raw_word(cpu, USB_EIR) & raw_word(cpu, USB_EIE));
    uint16_t interrupts = raw_word(cpu, USB_IR);
    if (errors != 0u) {
        interrupts |= USB_ERROR_INTERRUPT;
    } else {
        interrupts &= (uint16_t)~USB_ERROR_INTERRUPT;
    }
    raw_write_word(cpu, USB_IR, interrupts);
    if ((interrupts & raw_word(cpu, USB_IE)) != 0u ||
        (raw_word(cpu, USB_OTGIR) & raw_word(cpu, USB_OTGIE)) != 0u) {
        dspic33_raise_interrupt(cpu, USB_IRQ);
    }
}

static void usb_set_error(Dspic33* cpu, uint8_t error) {
    raw_write_word(cpu, USB_EIR,
                   (uint16_t)(raw_word(cpu, USB_EIR) | (error & 0x00ffu)));
    usb_refresh_interrupt(cpu);
}

static void usb_refresh_transaction_status(Dspic33* cpu) {
    uint16_t interrupts = raw_word(cpu, USB_IR);
    if (cpu->io.usb_status_count == 0u) {
        raw_write_word(cpu, USB_STAT, 0u);
        interrupts &= (uint16_t)~USB_TRANSACTION_INTERRUPT;
    } else {
        raw_write_word(cpu, USB_STAT, cpu->io.usb_status[cpu->io.usb_status_head]);
        interrupts |= USB_TRANSACTION_INTERRUPT;
    }
    raw_write_word(cpu, USB_IR, interrupts);
    usb_refresh_interrupt(cpu);
}

static void usb_push_transaction_status(Dspic33* cpu, uint8_t status) {
    uint8_t index;
    if (cpu->io.usb_status_count == sizeof(cpu->io.usb_status)) {
        usb_set_error(cpu, USB_ERROR_DMA);
        return;
    }
    index = (uint8_t)((cpu->io.usb_status_head + cpu->io.usb_status_count) %
                      sizeof(cpu->io.usb_status));
    cpu->io.usb_status[index] = status;
    cpu->io.usb_status_count++;
    usb_refresh_transaction_status(cpu);
}

static void usb_pop_transaction_status(Dspic33* cpu) {
    if (cpu->io.usb_status_count != 0u) {
        cpu->io.usb_status_head =
            (uint8_t)((cpu->io.usb_status_head + 1u) % sizeof(cpu->io.usb_status));
        cpu->io.usb_status_count--;
    }
    usb_refresh_transaction_status(cpu);
}

static void usb_reset_ping_pong(Dspic33* cpu) {
    memset(cpu->io.usb_next_bank, 0, sizeof(cpu->io.usb_next_bank));
}

static void usb_cancel_events(Dspic33* cpu) {
    size_t source;
    size_t destination = 0u;
    for (source = 0u; source < cpu->events.count; source++) {
        if (cpu->events.items[source].type != DSPIC33_EVENT_USB) {
            cpu->events.items[destination++] = cpu->events.items[source];
        }
    }
    cpu->events.count = destination;
}

static void usb_reset_runtime(Dspic33* cpu) {
    usb_cancel_events(cpu);
    memset(cpu->io.usb_pending, 0, sizeof(cpu->io.usb_pending));
    memset(&cpu->io.usb_tx, 0, sizeof(cpu->io.usb_tx));
    memset(cpu->io.usb_status, 0, sizeof(cpu->io.usb_status));
    cpu->io.usb_status_head = 0u;
    cpu->io.usb_status_count = 0u;
    cpu->io.usb_last_endpoint = 0u;
    cpu->io.usb_last_handshake = DSPIC33_USB_HANDSHAKE_NONE;
    cpu->io.usb_host_pending = false;
    cpu->io.usb_host_attached = false;
    cpu->io.usb_bus_idle = false;
    usb_reset_ping_pong(cpu);
}

static void usb_reset_registers(Dspic33* cpu) {
    uint16_t address;
    raw_write_word(cpu, USB_OTGIR, 0u);
    raw_write_word(cpu, USB_OTGIE, 0u);
    raw_write_word(cpu, USB_OTGSTAT, 0u);
    raw_write_word(cpu, USB_OTGCON, 0u);
    raw_write_word(cpu, USB_IR, 0u);
    raw_write_word(cpu, USB_IE, 0u);
    raw_write_word(cpu, USB_EIR, 0u);
    raw_write_word(cpu, USB_EIE, 0u);
    raw_write_word(cpu, USB_STAT, 0u);
    raw_write_word(cpu, USB_CON, 0u);
    raw_write_word(cpu, USB_ADDR, 0u);
    raw_write_word(cpu, USB_BDTP1, 0u);
    raw_write_word(cpu, USB_FRML, 0u);
    raw_write_word(cpu, USB_FRMH, 0u);
    raw_write_word(cpu, USB_TOK, 0u);
    raw_write_word(cpu, USB_SOF, 0u);
    raw_write_word(cpu, USB_BDTP2, 0u);
    raw_write_word(cpu, USB_BDTP3, 0u);
    raw_write_word(cpu, USB_CNFG1, 0u);
    raw_write_word(cpu, USB_CNFG2, 0u);
    for (address = USB_EP0; address < USB_EP0 + DSPIC33_USB_ENDPOINT_COUNT * 2u;
         address += 2u) {
        raw_write_word(cpu, address, 0u);
    }
    raw_write_word(cpu, USB_PWMRRS, 0u);
    raw_write_word(cpu, USB_PWMCON, 0u);
    usb_reset_runtime(cpu);
}

static void usb_set_frame(Dspic33* cpu, uint16_t frame) {
    frame &= 0x07ffu;
    raw_write_word(cpu, USB_FRML, frame & 0x00ffu);
    raw_write_word(cpu, USB_FRMH, frame >> 8u);
}

static uint16_t usb_frame(const Dspic33* cpu) {
    return (uint16_t)(((raw_word(cpu, USB_FRMH) & 7u) << 8u) |
                      (raw_word(cpu, USB_FRML) & 0xffu));
}

static void usb_response(Dspic33* cpu, const Dspic33UsbPacket* token,
                         Dspic33UsbHandshake handshake, const uint8_t* data,
                         uint16_t size, bool data1) {
    Dspic33UsbPacket response;
    memset(&response, 0, sizeof(response));
    response.address = token->address;
    response.endpoint = token->endpoint;
    response.pid = token->pid;
    response.handshake = handshake;
    response.data1 = data1;
    response.low_speed = token->low_speed;
    response.size = size;
    if (size != 0u) {
        memcpy(response.data, data, size);
    }
    cpu->io.usb_last_endpoint = token->endpoint;
    cpu->io.usb_last_handshake = handshake;
    usb_queue_push(&cpu->io.usb_tx, &response);
}

static bool usb_read_memory(const Dspic33* cpu, uint32_t address, uint8_t* data,
                            uint16_t size, bool increment) {
    uint16_t index;
    for (index = 0u; index < size; index++) {
        uint32_t current = address + (increment ? index : 0u);
        if (current >= DSPIC33_DATA_SIZE) {
            return false;
        }
        data[index] = cpu->data[current];
    }
    return true;
}

static bool usb_write_memory(Dspic33* cpu, uint32_t address, const uint8_t* data,
                             uint16_t size, bool increment) {
    uint16_t index;
    for (index = 0u; index < size; index++) {
        uint32_t current = address + (increment ? index : 0u);
        if (current >= DSPIC33_DATA_SIZE) {
            return false;
        }
        cpu->data[current] = data[index];
    }
    return true;
}

static void usb_clear_endpoint_stalls(Dspic33* cpu, uint8_t endpoint) {
    uint8_t direction;
    uint8_t bank;
    uint16_t words[4];
    for (direction = 0u; direction < 2u; direction++) {
        for (bank = 0u; bank < 2u; bank++) {
            if (usb_descriptor(cpu, endpoint, direction, bank, words)) {
                words[0] &= (uint16_t)~USB_DESCRIPTOR_STALL;
                usb_write_descriptor(cpu, endpoint, direction, bank, words);
            }
        }
    }
    raw_write_word(cpu, (uint16_t)(USB_EP0 + endpoint * 2u),
                   (uint16_t)(raw_word(cpu, (uint16_t)(USB_EP0 + endpoint * 2u)) &
                              ~USB_ENDPOINT_STALL));
}

static bool usb_device_active(const Dspic33* cpu) {
    uint16_t control = raw_word(cpu, USB_CON);
    return (raw_word(cpu, 0x0766u) & 1u) == 0u &&
           (raw_word(cpu, USB_PWRC) & USB_POWER) != 0u &&
           (raw_word(cpu, USB_PWRC) & USB_SUSPEND) == 0u &&
           (control & (USB_ENABLE | USB_HOST_ENABLE | USB_PACKET_DISABLE)) ==
               USB_ENABLE;
}

static bool usb_device_ready(const Dspic33* cpu, uint8_t endpoint, uint8_t pid) {
    uint16_t endpoint_control;
    if (endpoint >= DSPIC33_USB_ENDPOINT_COUNT || !usb_device_active(cpu)) {
        return false;
    }
    endpoint_control = raw_word(cpu, (uint16_t)(USB_EP0 + endpoint * 2u));
    if (pid == DSPIC33_USB_PID_IN) {
        return (endpoint_control & USB_ENDPOINT_TX_ENABLE) != 0u;
    }
    if ((endpoint_control & USB_ENDPOINT_RX_ENABLE) == 0u) {
        return false;
    }
    return pid != DSPIC33_USB_PID_SETUP ||
           (endpoint_control & USB_ENDPOINT_CONTROL_DISABLED) == 0u;
}

static bool usb_endpoint_handshake(const Dspic33* cpu, uint8_t endpoint) {
    return (raw_word(cpu, (uint16_t)(USB_EP0 + endpoint * 2u)) &
            USB_ENDPOINT_HANDSHAKE) != 0u;
}

static void usb_complete_descriptor(Dspic33* cpu, uint8_t endpoint, uint8_t direction,
                                    uint8_t bank, uint16_t words[4], uint8_t pid,
                                    bool data1, uint16_t count, bool keep) {
    if (!keep) {
        words[0] =
            (uint16_t)((data1 ? USB_DESCRIPTOR_DATA1 : 0u) | ((uint16_t)pid << 2u));
    }
    words[1] = count & USB_DESCRIPTOR_COUNT_MASK;
    usb_write_descriptor(cpu, endpoint, direction, bank, words);
    if (!keep) {
        cpu->io.usb_next_bank[endpoint][direction] ^= 1u;
    }
    usb_push_transaction_status(
        cpu, (uint8_t)((endpoint << 4u) | (direction << 3u) | (bank << 2u)));
}

static void usb_run_device_token(Dspic33* cpu, const Dspic33UsbPacket* token) {
    uint8_t response_data[DSPIC33_USB_PACKET_SIZE];
    uint8_t direction = token->pid == DSPIC33_USB_PID_IN ? 1u : 0u;
    uint8_t bank = (raw_word(cpu, USB_CON) & USB_PING_PONG_RESET) != 0u
                       ? 0u
                       : cpu->io.usb_next_bank[token->endpoint][direction];
    uint16_t words[4];
    uint16_t descriptor_count;
    uint16_t count;
    uint32_t buffer;
    bool keep;
    bool increment;
    bool expected_data1;
    if (!usb_device_active(cpu)) {
        usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_TIMEOUT, NULL, 0u, false);
        return;
    }
    if (token->address != (raw_word(cpu, USB_ADDR) & 0x007fu)) {
        usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_TIMEOUT, NULL, 0u, false);
        return;
    }
    if (!usb_device_ready(cpu, token->endpoint, token->pid)) {
        usb_response(cpu, token,
                     usb_endpoint_handshake(cpu, token->endpoint)
                         ? DSPIC33_USB_HANDSHAKE_NAK
                         : DSPIC33_USB_HANDSHAKE_TIMEOUT,
                     NULL, 0u, false);
        return;
    }
    if (token->pid == DSPIC33_USB_PID_SETUP) {
        usb_clear_endpoint_stalls(cpu, token->endpoint);
        bank = 0u;
        cpu->io.usb_next_bank[token->endpoint][0] = 0u;
        raw_write_word(cpu, USB_CON,
                       (uint16_t)(raw_word(cpu, USB_CON) | USB_PACKET_DISABLE));
    }
    if (!usb_descriptor(cpu, token->endpoint, direction, bank, words)) {
        usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
        usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_ERROR, NULL, 0u, false);
        return;
    }
    if ((words[0] & USB_DESCRIPTOR_OWNED) == 0u) {
        usb_response(cpu, token,
                     usb_endpoint_handshake(cpu, token->endpoint)
                         ? DSPIC33_USB_HANDSHAKE_NAK
                         : DSPIC33_USB_HANDSHAKE_TIMEOUT,
                     NULL, 0u, false);
        return;
    }
    if ((words[0] & USB_DESCRIPTOR_STALL) != 0u &&
        usb_endpoint_handshake(cpu, token->endpoint)) {
        raw_write_word(
            cpu, (uint16_t)(USB_EP0 + token->endpoint * 2u),
            (uint16_t)(raw_word(cpu, (uint16_t)(USB_EP0 + token->endpoint * 2u)) |
                       USB_ENDPOINT_STALL));
        raw_write_word(cpu, USB_IR,
                       (uint16_t)(raw_word(cpu, USB_IR) | USB_STALL_INTERRUPT));
        usb_refresh_interrupt(cpu);
        usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_STALL, NULL, 0u, false);
        return;
    }
    expected_data1 = (words[0] & USB_DESCRIPTOR_DATA1) != 0u;
    if (token->pid != DSPIC33_USB_PID_SETUP &&
        (words[0] & USB_DESCRIPTOR_DTS_ENABLE) != 0u &&
        token->pid != DSPIC33_USB_PID_IN && token->data1 != expected_data1) {
        usb_response(cpu, token,
                     usb_endpoint_handshake(cpu, token->endpoint)
                         ? DSPIC33_USB_HANDSHAKE_ACK
                         : DSPIC33_USB_HANDSHAKE_NONE,
                     NULL, 0u, false);
        return;
    }
    descriptor_count = words[1] & USB_DESCRIPTOR_COUNT_MASK;
    buffer = ((uint32_t)words[3] << 16u) | words[2];
    keep = (words[0] & USB_DESCRIPTOR_KEEP) != 0u;
    increment = (words[0] & USB_DESCRIPTOR_NO_INCREMENT) == 0u;
    if (token->pid == DSPIC33_USB_PID_IN) {
        count = descriptor_count;
        if (!usb_read_memory(cpu, buffer, response_data, count, increment)) {
            usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
            usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_ERROR, NULL, 0u, false);
            return;
        }
        usb_response(cpu, token,
                     usb_endpoint_handshake(cpu, token->endpoint)
                         ? DSPIC33_USB_HANDSHAKE_ACK
                         : DSPIC33_USB_HANDSHAKE_NONE,
                     response_data, count, expected_data1);
    } else {
        count = token->size < descriptor_count ? token->size : descriptor_count;
        if (!usb_write_memory(cpu, buffer, token->data, count, increment)) {
            usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
            usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_ERROR, NULL, 0u, false);
            return;
        }
        if (token->size > descriptor_count) {
            usb_set_error(cpu, USB_ERROR_DMA);
        }
        usb_response(cpu, token,
                     usb_endpoint_handshake(cpu, token->endpoint)
                         ? DSPIC33_USB_HANDSHAKE_ACK
                         : DSPIC33_USB_HANDSHAKE_NONE,
                     NULL, 0u, token->data1);
    }
    usb_complete_descriptor(
        cpu, token->endpoint, direction, bank, words, token->pid,
        token->pid == DSPIC33_USB_PID_SETUP
            ? false
            : (token->pid == DSPIC33_USB_PID_IN ? expected_data1 : token->data1),
        count, keep);
}

static void usb_run_host_response(Dspic33* cpu, const Dspic33UsbPacket* response) {
    uint8_t direction;
    uint8_t bank;
    uint8_t pid = 0u;
    uint16_t words[4];
    uint16_t count = 0u;
    uint32_t buffer;
    bool keep = false;
    bool increment = true;
    bool complete = false;
    if (!cpu->io.usb_host_pending || (raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u) {
        return;
    }
    direction = cpu->io.usb_host_pid == DSPIC33_USB_PID_IN ? 0u : 1u;
    bank = (raw_word(cpu, USB_CON) & USB_PING_PONG_RESET) != 0u
               ? 0u
               : cpu->io.usb_next_bank[0][direction];
    if (!usb_descriptor(cpu, 0u, direction, bank, words)) {
        usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
        response = NULL;
    } else if ((words[0] & USB_DESCRIPTOR_OWNED) == 0u) {
        usb_set_error(cpu, USB_ERROR_DMA);
        response = NULL;
    }
    if (response != NULL) {
        keep = (words[0] & USB_DESCRIPTOR_KEEP) != 0u;
        increment = (words[0] & USB_DESCRIPTOR_NO_INCREMENT) == 0u;
    }
    if (response != NULL && response->handshake == DSPIC33_USB_HANDSHAKE_ACK) {
        pid = 2u;
        complete = true;
        count = words[1] & USB_DESCRIPTOR_COUNT_MASK;
        if (direction == 0u && response->size < count) {
            count = response->size;
        }
        buffer = ((uint32_t)words[3] << 16u) | words[2];
        if (direction == 0u &&
            !usb_write_memory(cpu, buffer, response->data, count, increment)) {
            usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
            complete = false;
        }
    } else if (response != NULL && response->handshake == DSPIC33_USB_HANDSHAKE_NAK) {
        pid = 0x0au;
        complete = true;
    } else if (response != NULL && response->handshake == DSPIC33_USB_HANDSHAKE_STALL) {
        raw_write_word(cpu, USB_IR,
                       (uint16_t)(raw_word(cpu, USB_IR) | USB_STALL_INTERRUPT));
        pid = 0x0eu;
        complete = true;
    } else if (response != NULL &&
               response->handshake == DSPIC33_USB_HANDSHAKE_TIMEOUT) {
        usb_set_error(cpu, USB_ERROR_BTO);
        complete = true;
    } else if (response != NULL && response->handshake == DSPIC33_USB_HANDSHAKE_ERROR) {
        usb_set_error(cpu, response->error != 0u ? response->error : USB_ERROR_PID);
        complete = true;
    }
    if (complete) {
        usb_complete_descriptor(cpu, 0u, direction, bank, words, pid, response->data1,
                                count, keep);
    }
    cpu->io.usb_last_handshake =
        response != NULL ? response->handshake : DSPIC33_USB_HANDSHAKE_ERROR;
    cpu->io.usb_host_pending = false;
    raw_write_word(cpu, USB_CON, (uint16_t)(raw_word(cpu, USB_CON) & ~USB_TOKEN_BUSY));
    usb_refresh_interrupt(cpu);
}

static void usb_run_bus_event(Dspic33* cpu, Dspic33UsbBusEvent event, uint16_t value) {
    uint16_t status;
    switch (event) {
    case DSPIC33_USB_BUS_RESET:
        cpu->io.usb_bus_idle = false;
        raw_write_word(cpu, USB_ADDR, 0u);
        raw_write_word(cpu, USB_CON,
                       (uint16_t)(raw_word(cpu, USB_CON) & ~USB_PACKET_DISABLE));
        cpu->io.usb_status_head = 0u;
        cpu->io.usb_status_count = 0u;
        usb_reset_ping_pong(cpu);
        if ((raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u) {
            raw_write_word(cpu, USB_IR,
                           (uint16_t)(raw_word(cpu, USB_IR) | USB_RESET_INTERRUPT));
        }
        usb_refresh_transaction_status(cpu);
        break;
    case DSPIC33_USB_BUS_SOF:
        cpu->io.usb_bus_idle = false;
        usb_set_frame(cpu,
                      value == UINT16_MAX ? (uint16_t)(usb_frame(cpu) + 1u) : value);
        raw_write_word(cpu, USB_IR,
                       (uint16_t)(raw_word(cpu, USB_IR) | USB_SOF_INTERRUPT));
        usb_refresh_interrupt(cpu);
        if ((raw_word(cpu, USB_PWRC) & USB_POWER) != 0u &&
            (raw_word(cpu, USB_CON) & (USB_HOST_ENABLE | USB_ENABLE)) ==
                (USB_HOST_ENABLE | USB_ENABLE)) {
            usb_schedule_bus_event(cpu, DSPIC33_USB_BUS_SOF, UINT16_MAX,
                                   USB_FRAME_CYCLES);
        }
        break;
    case DSPIC33_USB_BUS_IDLE:
        if (!cpu->io.usb_bus_idle) {
            cpu->io.usb_bus_idle = true;
            raw_write_word(cpu, USB_IR,
                           (uint16_t)(raw_word(cpu, USB_IR) | USB_IDLE_INTERRUPT));
            usb_refresh_interrupt(cpu);
        }
        break;
    case DSPIC33_USB_BUS_RESUME:
        cpu->io.usb_bus_idle = false;
        raw_write_word(cpu, USB_PWRC,
                       (uint16_t)(raw_word(cpu, USB_PWRC) & ~USB_SUSPEND));
        raw_write_word(cpu, USB_IR,
                       (uint16_t)(raw_word(cpu, USB_IR) | USB_RESUME_INTERRUPT));
        raw_write_word(cpu, USB_OTGIR, (uint16_t)(raw_word(cpu, USB_OTGIR) | 0x0010u));
        usb_refresh_interrupt(cpu);
        break;
    case DSPIC33_USB_BUS_ATTACH:
        raw_write_word(cpu, USB_OTGSTAT,
                       (uint16_t)(raw_word(cpu, USB_OTGSTAT) | USB_OTG_VOLTAGE_STATUS));
        if ((raw_word(cpu, USB_CON) & USB_HOST_ENABLE) != 0u) {
            cpu->io.usb_host_attached = true;
            raw_write_word(cpu, USB_IR,
                           (uint16_t)(raw_word(cpu, USB_IR) | USB_ATTACH_INTERRUPT));
        }
        usb_refresh_interrupt(cpu);
        break;
    case DSPIC33_USB_BUS_DETACH:
        cpu->io.usb_host_attached = false;
        raw_write_word(
            cpu, USB_OTGSTAT,
            (uint16_t)(raw_word(cpu, USB_OTGSTAT) & ~USB_OTG_VOLTAGE_STATUS));
        raw_write_word(cpu, USB_OTGIR, (uint16_t)(raw_word(cpu, USB_OTGIR) | 0x0020u));
        if ((raw_word(cpu, USB_CON) & USB_HOST_ENABLE) != 0u) {
            raw_write_word(cpu, USB_IR,
                           (uint16_t)(raw_word(cpu, USB_IR) | USB_DETACH_INTERRUPT));
        }
        usb_refresh_interrupt(cpu);
        break;
    case DSPIC33_USB_BUS_ERROR:
        cpu->io.usb_bus_idle = false;
        usb_set_error(cpu, (uint8_t)value);
        break;
    case DSPIC33_USB_BUS_OTG_STATE:
        status = raw_word(cpu, USB_OTGSTAT);
        raw_write_word(cpu, USB_OTGSTAT, value & 0x00adu);
        if (status != (value & 0x00adu)) {
            raw_write_word(
                cpu, USB_OTGIR,
                (uint16_t)(raw_word(cpu, USB_OTGIR) | ((status ^ value) & 0x00adu)));
        }
        usb_refresh_interrupt(cpu);
        break;
    }
}

static void run_usb(Dspic33* cpu, uint16_t slot) {
    Dspic33UsbPending* pending;
    if (slot >= DSPIC33_USB_PENDING_COUNT) {
        return;
    }
    pending = &cpu->io.usb_pending[slot];
    if (!pending->active) {
        return;
    }
    if (pending->bus_event) {
        usb_run_bus_event(cpu, pending->event, pending->value);
    } else if ((raw_word(cpu, USB_CON) & USB_HOST_ENABLE) != 0u &&
               pending->packet.handshake != DSPIC33_USB_HANDSHAKE_NONE) {
        usb_run_host_response(cpu, &pending->packet);
    } else {
        usb_run_device_token(cpu, &pending->packet);
    }
    pending->active = false;
}

static void complete_nvm_event(Dspic33* cpu) {
    if (!cpu->nvm.active) {
        return;
    }
    dspic33_complete_nvm(cpu);
    cpu->nvm.active = false;
    raw_write_word(
        cpu, NVM_CONTROL,
        (uint16_t)(raw_word(cpu, NVM_CONTROL) & ~(NVM_WRITE | NVM_WRITE_ERROR)));
    if (dspic33_complete_nvm_reset(cpu)) {
        return;
    }
    if (dspic33_watchdog_complete_nvm(cpu)) {
        return;
    }
    dspic33_raise_interrupt(cpu, 15u);
}

static uint8_t auxiliary_pll_input(uint16_t control) {
    if ((control & AUXILIARY_CLOCK_SOURCE_FRC) != 0u) {
        return 1u;
    }
    if ((control & AUXILIARY_CLOCK_SOURCE_PRIMARY) != 0u) {
        return 2u;
    }
    return (uint8_t)(4u | ((control & AUXILIARY_CLOCK_OSCILLATOR_MODE) >> 11u));
}

static bool auxiliary_pll_input_available(const Dspic33* cpu, uint16_t control) {
    uint8_t input = auxiliary_pll_input(control);
    if (input == 1u) {
        return true;
    }
    if (input == 2u) {
        return (cpu->configuration[8u] & 0x03u) != 0x03u;
    }
    return input != 4u;
}

static bool auxiliary_pll_reconfiguration(uint16_t previous, uint16_t control) {
    return ((previous ^ control) & (AUXILIARY_PLL_ENABLE | AUXILIARY_PLL_PRESCALER)) !=
               0u ||
           auxiliary_pll_input(previous) != auxiliary_pll_input(control);
}

static bool auxiliary_clock_configuration_locked(const Dspic33* cpu) {
    return (raw_word(cpu, OSCILLATOR_CONTROL) & OSCILLATOR_CLOCK_LOCK) != 0u &&
           (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_CLOCK_LOCK) != 0u;
}

static void complete_auxiliary_pll(Dspic33* cpu, uint32_t generation) {
    uint16_t control = raw_word(cpu, AUXILIARY_CLOCK_CONTROL);
    if (generation == cpu->io.auxiliary_pll_generation &&
        (control & AUXILIARY_PLL_ENABLE) != 0u &&
        auxiliary_pll_input_available(cpu, control)) {
        raw_write_word(cpu, AUXILIARY_CLOCK_CONTROL,
                       (uint16_t)(control | AUXILIARY_PLL_LOCK));
    }
}

static void reconfigure_auxiliary_pll(Dspic33* cpu) {
    uint16_t control =
        (uint16_t)(raw_word(cpu, AUXILIARY_CLOCK_CONTROL) & ~AUXILIARY_PLL_LOCK);
    cpu->io.auxiliary_pll_generation++;
    raw_write_word(cpu, AUXILIARY_CLOCK_CONTROL, control);
    if ((control & AUXILIARY_PLL_ENABLE) != 0u &&
        auxiliary_pll_input_available(cpu, control) &&
        !dspic33_schedule(cpu, DSPIC33_EVENT_AUX_PLL, 0u,
                          cpu->io.auxiliary_pll_generation, AUXILIARY_PLL_LOCK_DELAY)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_configuration_changed(Dspic33* cpu, uint32_t address,
                                          uint8_t previous) {
    uint16_t control = raw_word(cpu, AUXILIARY_CLOCK_CONTROL);
    if (address == DSPIC33_CONFIGURATION_BASE + 6u) {
        oscillator_startup_configuration_changed(cpu, previous);
    } else if (address == DSPIC33_CONFIGURATION_BASE + 8u) {
        if (auxiliary_pll_input(control) == 2u &&
            ((previous ^ cpu->configuration[8u]) & 0x03u) != 0u) {
            reconfigure_auxiliary_pll(cpu);
        }
        oscillator_configuration_changed(cpu, previous);
    } else if (address == DSPIC33_CONFIGURATION_BASE + 10u) {
        oscillator_pll_configuration_changed(cpu, previous);
    } else if (address == DSPIC33_CONFIGURATION_BASE + 12u) {
        dspic33_i2c_refresh_pins(cpu);
    }
}

static void remove_nvm_events(Dspic33* cpu) {
    size_t source;
    size_t destination = 0u;
    for (source = 0u; source < cpu->events.count; source++) {
        if (cpu->events.items[source].type != DSPIC33_EVENT_NVM) {
            cpu->events.items[destination++] = cpu->events.items[source];
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
}

static void process_event(Dspic33* cpu, const Dspic33Event* event) {
    switch (event->type) {
    case DSPIC33_EVENT_INTERRUPT:
        raise_scheduled_interrupt(cpu, event->source);
        break;
    case DSPIC33_EVENT_TIMER:
        pulse_timer(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_TIMER_GATE:
        set_timer_gate(cpu, (uint8_t)event->source, event->value != 0u);
        break;
    case DSPIC33_EVENT_DMA:
        run_dma(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_ADC:
        run_adc(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_PWM_FAULT:
        pwm_input_event(cpu, (uint8_t)event->source,
                        (event->value & PWM_INPUT_HIGH) != 0u, false);
        break;
    case DSPIC33_EVENT_PWM_CURRENT_LIMIT:
        pwm_input_event(cpu, (uint8_t)event->source,
                        (event->value & PWM_INPUT_HIGH) != 0u, true);
        break;
    case DSPIC33_EVENT_PWM_DEAD_TIME:
        pwm_dead_time_event(cpu, (uint8_t)event->source,
                            (event->value & PWM_INPUT_HIGH) != 0u);
        break;
    case DSPIC33_EVENT_PWM_SYNC:
        pwm_sync_event(cpu, (uint8_t)event->source,
                       (event->value & PWM_INPUT_HIGH) != 0u);
        break;
    case DSPIC33_EVENT_UART:
        run_uart(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_SPI:
        run_spi(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_SPI_SELECT:
        run_spi_select(cpu, (uint8_t)event->source,
                       (event->value & SPI_SELECT_ACTIVE) != 0u);
        break;
    case DSPIC33_EVENT_I2C:
        dspic33_i2c_process_event(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_CAN:
        run_can(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_USB:
        run_usb(cpu, event->source);
        break;
    case DSPIC33_EVENT_CRC:
        if (event->source == CRC_EVENT_PMD_SOURCE) {
            run_crc_pmd(cpu, event->value);
        } else {
            run_crc(cpu, (uint16_t)event->value);
        }
        break;
    case DSPIC33_EVENT_PMP:
        if (event->source == PMP_EVENT_CLEAR_BUSY) {
            pmp_clear_busy(cpu, (uint16_t)event->value);
        } else if (event->source == PMP_EVENT_COMPLETE) {
            run_pmp(cpu, (uint16_t)event->value);
        } else if (event->source == PMP_EVENT_PMD) {
            run_pmp_pmd(cpu, event->value);
        } else if (event->source == PMP_EVENT_SLAVE_READ) {
            pmp_slave_read_event(cpu, (uint8_t)event->value);
        } else if (event->source == PMP_EVENT_SLAVE_WRITE) {
            pmp_slave_write_event(cpu, (uint8_t)(event->value >> 8u),
                                  (uint8_t)event->value);
        }
        break;
    case DSPIC33_EVENT_INPUT_CAPTURE:
        run_input_capture(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_OUTPUT_COMPARE:
        run_output_compare(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_OUTPUT_COMPARE_FAULT:
        if ((event->value & OUTPUT_COMPARE_FAULT_EVENT_PIN) != 0u) {
            output_compare_fault_pin_input(
                cpu, (uint8_t)event->source,
                (event->value & OUTPUT_COMPARE_FAULT_EVENT_HIGH) != 0u);
        } else {
            output_compare_fault_input(
                cpu, (uint8_t)event->source,
                (event->value & OUTPUT_COMPARE_FAULT_EVENT_HIGH) != 0u);
        }
        break;
    case DSPIC33_EVENT_COMPARATOR:
        run_comparator(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_RTCC:
        run_rtcc(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_QEI:
        run_qei(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_DCI:
        run_dci(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_NVM:
        break;
    case DSPIC33_EVENT_AUX_PLL:
        complete_auxiliary_pll(cpu, event->value);
        break;
    case DSPIC33_EVENT_OSCILLATOR:
        complete_oscillator_event(cpu, event->source, event->value);
        break;
    }
}

static void raise_pending_timer_interrupts(Dspic33* cpu) {
    uint16_t pending = cpu->io.timer_interrupt_pending;
    uint8_t timer = 0u;
    cpu->io.timer_interrupt_pending = 0u;
    while (pending != 0u) {
        if ((pending & 1u) != 0u) {
            dspic33_raise_interrupt(cpu, timer_irqs[timer]);
        }
        pending >>= 1u;
        timer++;
    }
}

static void advance_timers(Dspic33* cpu, uint64_t cycles) {
    uint16_t enabled = cpu->io.timer_enabled;
    uint16_t synchronization_sources = 0u;
    uint8_t timer = 0u;
    while (enabled != 0u) {
        if ((enabled & 1u) != 0u) {
            uint16_t control = raw_word(cpu, timer_controls[timer]);
            bool gated = (control & TIMER_GATE) != 0u;
            bool gate_high = (cpu->io.timer_gate & (uint16_t)(1u << timer)) != 0u;
            if (!timer_is_paired_high(cpu, timer) && (control & TIMER_EXTERNAL) == 0u &&
                timer_power_enabled(cpu, timer, false) && (!gated || gate_high)) {
                clock_timer(cpu, timer, cycles, &synchronization_sources, false);
            }
        }
        enabled >>= 1u;
        timer++;
    }
    pulse_timer_synchronization_sources(cpu, &synchronization_sources);
}

static uint64_t timer_sync_boundary_cycles(const Dspic33* cpu, uint64_t limit) {
    uint64_t boundary = limit;
    uint8_t timer;
    for (timer = 0u; timer < 5u; timer++) {
        uint16_t bit = (uint16_t)(1u << timer);
        uint16_t control = raw_word(cpu, timer_controls[timer]);
        uint64_t ticks;
        uint64_t cycles;
        uint32_t prescale;
        bool gated = (control & TIMER_GATE) != 0u;
        bool gate_high = (cpu->io.timer_gate & bit) != 0u;
        if ((cpu->io.timer_enabled & bit) == 0u || timer_is_paired_high(cpu, timer) ||
            (control & TIMER_EXTERNAL) != 0u ||
            !timer_power_enabled(cpu, timer, false) || (gated && !gate_high)) {
            continue;
        }
        {
            uint8_t signal_timer =
                timer_pair_enabled(cpu, timer) ? (uint8_t)(timer + 1u) : timer;
            uint8_t source = (uint8_t)(INPUT_CAPTURE_SYNC_TIMER_FIRST + signal_timer);
            uint64_t clock_boundary = output_compare_clock_boundary_ticks(cpu, timer);
            if (signal_timer != timer && signal_timer < 5u) {
                uint64_t high_boundary =
                    output_compare_clock_boundary_ticks(cpu, signal_timer);
                if (high_boundary < clock_boundary) {
                    clock_boundary = high_boundary;
                }
            }
            ticks = UINT64_MAX;
            if (signal_timer < 5u && (input_capture_source_awaited(cpu, source) ||
                                      output_compare_source_awaited(cpu, source))) {
                ticks = timer_ticks_until_period(cpu, timer);
            }
            if (clock_boundary < ticks) {
                ticks = clock_boundary;
            }
        }
        if (ticks == UINT64_MAX) {
            continue;
        }
        prescale = timer_prescale(control);
        if (ticks > UINT64_MAX / prescale) {
            continue;
        }
        cycles = ticks * prescale - cpu->io.timer_fraction[timer];
        if (cycles < boundary) {
            boundary = cycles;
        }
    }
    return boundary;
}

static void advance_device_cycles(Dspic33* cpu, uint64_t cycles) {
    cpu->device_cycles += cycles;
    advance_input_capture(cpu, cycles);
    advance_output_compare(cpu, cycles);
    advance_timers(cpu, cycles);
    advance_pwm(cpu, cycles);
    advance_qei(cpu, cycles);
    comparator_evaluate_all(cpu);
}

uint64_t dspic33_device_instruction_cycles(const Dspic33* cpu, uint64_t cycles) {
    uint16_t divisor = raw_word(cpu, MAIN_CLOCK_DIVISOR);
    uint64_t ratio = (divisor & 0x0800u) != 0u
                         ? UINT64_C(1) << ((divisor >> 12u) & 0x07u)
                         : UINT64_C(1);
    return cycles > UINT64_MAX / ratio ? UINT64_MAX : cycles * ratio;
}

bool dspic33_device_advance_instruction(Dspic33* cpu, uint64_t cpu_cycles,
                                        uint64_t device_cycles) {
    uint64_t target;
    size_t group;
    if (!pps_shadow_matches(cpu)) {
        dspic33_configuration_mismatch_reset(cpu);
    }
    if (cpu_cycles > UINT64_MAX - cpu->cycles ||
        (cpu->async_events_enabled &&
         device_cycles > UINT64_MAX - cpu->device_cycles)) {
        return false;
    }
    cpu->cycles += cpu_cycles;
    if (cpu_cycles != 0u) {
        raise_pending_timer_interrupts(cpu);
    }
    if (cpu->disicnt > cpu_cycles) {
        cpu->disicnt = (uint16_t)(cpu->disicnt - cpu_cycles);
    } else {
        cpu->disicnt = 0u;
    }
    if (cpu->async_events_enabled) {
        target = cpu->device_cycles + device_cycles;
        for (;;) {
            uint64_t next_cycle = target;
            uint64_t timer_boundary;
            if (cpu->events.count == 0u || cpu->events.items[0].paused ||
                cpu->events.items[0].cycle > target) {
                next_cycle = target;
            } else if (cpu->events.items[0].cycle < next_cycle) {
                next_cycle = cpu->events.items[0].cycle;
            }
            timer_boundary =
                timer_sync_boundary_cycles(cpu, next_cycle - cpu->device_cycles);
            if (timer_boundary != 0u) {
                advance_device_cycles(cpu, timer_boundary);
                continue;
            }
            if (cpu->device_cycles == target &&
                (cpu->events.count == 0u || cpu->events.items[0].paused ||
                 cpu->events.items[0].cycle > target)) {
                break;
            }
            {
                Dspic33Event event = event_pop(&cpu->events);
                process_event(cpu, &event);
                if (cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR) {
                    return false;
                }
            }
        }
    }
    for (group = 0u; group < DSPIC33_IRQ_GROUP_COUNT; group++) {
        cpu->interrupt_deferred[group] = cpu->interrupt_deferred_next[group];
        cpu->interrupt_deferred_next[group] = 0u;
    }
    cpu->gie_disable_deferred = cpu->gie_disable_deferred_next;
    cpu->gie_disable_deferred_next = 0u;
    if (cpu->nvm.active && cpu->nvm.completion_cycle != 0u &&
        cpu->cycles >= cpu->nvm.completion_cycle) {
        complete_nvm_event(cpu);
        remove_nvm_events(cpu);
    }
    return true;
}

bool dspic33_device_advance(Dspic33* cpu, uint64_t cycles) {
    return dspic33_device_advance_instruction(cpu, cycles, cycles);
}

bool dspic33_device_advance_nvm(Dspic33* cpu) {
    return dspic33_device_advance_instruction(
        cpu, 1u, dspic33_device_instruction_cycles(cpu, 1u));
}

static void update_timer_register(Dspic33* cpu, uint16_t address) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        if ((address & 0xfffeu) == timer_controls[timer]) {
            uint16_t bit = (uint16_t)(1u << timer);
            if ((raw_word(cpu, timer_controls[timer]) & TIMER_ON) != 0u) {
                cpu->io.timer_enabled |= (uint16_t)(1u << timer);
            } else {
                cpu->io.timer_enabled &= (uint16_t)~(1u << timer);
            }
            cpu->io.timer_fraction[timer] = 0u;
            cpu->io.timer_external_started &= (uint16_t)~bit;
            return;
        }
        if ((address & 0xfffeu) == timer_registers[timer]) {
            uint16_t bit = (uint16_t)(1u << timer);
            cpu->io.timer_fraction[timer] = 0u;
            cpu->io.timer_external_started &= (uint16_t)~bit;
            if (timer_pair_enabled(cpu, timer)) {
                raw_write_word(cpu, timer_registers[timer + 1u],
                               raw_word(cpu, timer_holding_registers[timer / 2u]));
            }
            return;
        }
    }
}

static void update_adc_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        uint16_t control_address = adc_controls[module];
        uint16_t control;
        if (address != control_address && address != control_address + 2u &&
            address != control_address + 6u) {
            continue;
        }
        control = raw_word(cpu, control_address);
        if (address == control_address) {
            bool was_on = (previous & ADC_ON) != 0u;
            bool on;
            bool was_sampling = (previous & ADC_SAMPLE) != 0u;
            bool sampling;
            uint8_t source;
            if ((requested & ADC_DONE) == 0u) {
                control &= (uint16_t)~ADC_DONE;
            }
            if (was_on && module == 0u && ((control ^ previous) & ADC_12_BIT) != 0u) {
                control = (uint16_t)((control & ~ADC_12_BIT) | (previous & ADC_12_BIT));
            }
            if (module == 0u && (control & ADC_12_BIT) != 0u) {
                control &= (uint16_t)~ADC_SIMULTANEOUS;
                raw_write_word(
                    cpu, (uint16_t)(control_address + 2u),
                    (uint16_t)(adc_register(cpu, module, 2u) & ~ADC_CHANNELS_MASK));
                raw_write_word(cpu, (uint16_t)(control_address + 6u), 0u);
            }
            raw_write_word(cpu, control_address, control);
            on = (control & ADC_ON) != 0u;
            sampling = (control & ADC_SAMPLE) != 0u;
            source = (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u);
            if (!on) {
                cpu->io.adc_generation[module]++;
                cpu->io.adc_latched_count[module] = 0u;
                raw_write_word(cpu, control_address,
                               (uint16_t)(control & ~(ADC_SAMPLE | ADC_DONE)));
                return;
            }
            if (!was_on) {
                cpu->io.adc_buffer_index[module] = 0u;
                cpu->io.adc_sample_count[module] = 0u;
                cpu->io.adc_scan_index[module] = 0u;
                cpu->io.adc_mux_b &= (uint8_t)~(1u << module);
                memset(cpu->io.adc_dma_sample[module], 0,
                       sizeof(cpu->io.adc_dma_sample[module]));
            }
            if (was_on && was_sampling && !sampling && source == 0u) {
                raw_write_word(cpu, control_address, (uint16_t)(control | ADC_SAMPLE));
                adc_start_conversion(cpu, module);
                return;
            }
            if (sampling && (!was_sampling || !was_on ||
                             ((control ^ previous) & ADC_TRIGGER_MASK) != 0u)) {
                adc_begin_sampling(cpu, module);
                return;
            }
            if ((control & ADC_AUTO_SAMPLE) != 0u &&
                (!was_on || (previous & ADC_AUTO_SAMPLE) == 0u)) {
                adc_begin_sampling(cpu, module);
                return;
            }
            if (was_sampling && !sampling) {
                cpu->io.adc_generation[module]++;
            }
            return;
        }
        if (module == 0u && (control & ADC_12_BIT) != 0u) {
            if (address == control_address + 2u) {
                raw_write_word(cpu, address,
                               (uint16_t)(raw_word(cpu, address) & ~ADC_CHANNELS_MASK));
            } else if (address == control_address + 6u) {
                raw_write_word(cpu, address, 0u);
            }
        }
        return;
    }
}

static void update_pwm_register(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t primary = raw_word(cpu, PWM_GLOBAL_BASE);
    bool enabled = (primary & PWM_ENABLE) != 0u;
    uint8_t generator;
    if (address == PWM_GLOBAL_BASE) {
        if ((primary & PWM_SPECIAL_INTERRUPT) == 0u) {
            raw_write_word(cpu, address, (uint16_t)(primary & ~PWM_SPECIAL_STATUS));
        }
        if ((previous & PWM_ENABLE) == 0u && enabled) {
            pwm_start(cpu);
        } else if ((previous & PWM_ENABLE) != 0u && !enabled) {
            memset(cpu->io.pwm, 0, sizeof(cpu->io.pwm));
            memset(cpu->io.pwm_fraction, 0, sizeof(cpu->io.pwm_fraction));
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 2u && enabled) {
        raw_write_word(cpu, address, previous);
        return;
    }
    if (address == PWM_GLOBAL_BASE + 4u) {
        if (!enabled || (primary & 0x0400u) != 0u) {
            cpu->io.pwm_active_period[0] = raw_word(cpu, address);
            cpu->io.pwm_period_update &= (uint8_t)~1u;
        } else {
            cpu->io.pwm_period_update |= 1u;
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x0eu) {
        uint16_t control = raw_word(cpu, address);
        if ((control & PWM_SPECIAL_INTERRUPT) == 0u) {
            raw_write_word(cpu, address, (uint16_t)(control & ~PWM_SPECIAL_STATUS));
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x10u && enabled) {
        raw_write_word(cpu, address, previous);
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x12u) {
        uint16_t secondary = raw_word(cpu, PWM_GLOBAL_BASE + 0x0eu);
        if (!enabled || (secondary & 0x0400u) != 0u) {
            cpu->io.pwm_active_period[1] = raw_word(cpu, address);
            cpu->io.pwm_period_update &= (uint8_t)~2u;
        } else {
            cpu->io.pwm_period_update |= 2u;
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x0au) {
        for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
            uint8_t bit = (uint8_t)(1u << generator);
            if (!enabled ||
                (pwm_register(cpu, generator, 0u) & PWM_IMMEDIATE_UPDATE) != 0u) {
                pwm_latch_generator(cpu, generator);
                cpu->io.pwm_timing_update &= (uint8_t)~bit;
                if (enabled) {
                    pwm_update_output(cpu, generator);
                }
            } else if ((pwm_register(cpu, generator, 0u) & PWM_MASTER_DUTY) != 0u) {
                cpu->io.pwm_timing_update |= bit;
            }
        }
        return;
    }
    if (address < PWM_GENERATOR_BASE ||
        address >= PWM_GENERATOR_BASE + DSPIC33_PWM_COUNT * PWM_GENERATOR_STRIDE) {
        return;
    }
    generator = (uint8_t)((address - PWM_GENERATOR_BASE) / PWM_GENERATOR_STRIDE);
    {
        uint16_t base = pwm_generator_base(generator);
        uint16_t offset = (uint16_t)(address - base);
        uint16_t control = raw_word(cpu, base);
        uint16_t fault = raw_word(cpu, (uint16_t)(base + 4u));
        uint8_t bit = (uint8_t)(1u << generator);
        bool timing_register = offset == 6u || offset == 8u || offset == 0x0au ||
                               offset == 0x0cu || offset == 0x0eu || offset == 0x10u;
        if (offset == 0u) {
            if ((control & PWM_FAULT_INTERRUPT) == 0u) {
                control &= (uint16_t)~PWM_FAULT_STATUS;
                if (!pwm_fault_active(cpu, generator)) {
                    cpu->io.pwm_fault_release |= bit;
                }
            }
            if ((control & PWM_CURRENT_LIMIT_INTERRUPT) == 0u) {
                control &= (uint16_t)~PWM_CURRENT_LIMIT_STATUS;
            }
            if ((control & PWM_TRIGGER_INTERRUPT) == 0u) {
                control &= (uint16_t)~PWM_TRIGGER_STATUS;
            }
            raw_write_word(cpu, base, control);
        }
        if (offset == 4u && (fault & PWM_FAULT_MODE_MASK) == PWM_FAULT_DISABLED) {
            cpu->io.pwm_fault_release |= bit;
        }
        if (offset == 2u && (!enabled || (pwm_register(cpu, generator, 2u) &
                                          PWM_OVERRIDE_SYNCHRONIZED) == 0u)) {
            cpu->io.pwm_active_io[generator] = pwm_register(cpu, generator, 2u);
        }
        if (!enabled || (control & PWM_IMMEDIATE_UPDATE) != 0u) {
            pwm_latch_generator(cpu, generator);
            cpu->io.pwm_timing_update &= (uint8_t)~bit;
        } else if (timing_register) {
            cpu->io.pwm_timing_update |= bit;
        }
        pwm_refresh_status(cpu, generator);
        if (enabled) {
            pwm_update_output(cpu, generator);
        } else {
            cpu->io.pwm[generator * 2u] = 0u;
            cpu->io.pwm[generator * 2u + 1u] = 0u;
        }
    }
}

static void update_uart_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                 uint16_t requested) {
    static const uint16_t pmd_addresses[DSPIC33_UART_COUNT] = {0x0760u, 0x0760u,
                                                               0x0764u, 0x0766u};
    static const uint16_t pmd_masks[DSPIC33_UART_COUNT] = {0x0020u, 0x0040u, 0x0008u,
                                                           0x0020u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t base = uart_bases[channel];
        uint16_t offset = (uint16_t)(address - base);
        if (offset > 8u || (offset & 1u) != 0u) {
            continue;
        }
        if (offset == 0u) {
            uint16_t mode = raw_word(cpu, base);
            if ((previous & UART_MODE_ENABLE) != 0u &&
                (mode & UART_MODE_ENABLE) == 0u) {
                uart_disable_module(cpu, channel);
            } else {
                uart_refresh_status(cpu, channel);
            }
            return;
        }
        if (offset == 2u) {
            uint16_t status = raw_word(cpu, (uint16_t)(base + 2u));
            bool transmitter_was_enabled = (previous & UART_STATUS_TX_ENABLE) != 0u;
            bool transmitter_enabled;
            status = (uint16_t)((status & ~UART_STATUS_OVERRUN) |
                                (previous & requested & UART_STATUS_OVERRUN));
            if ((raw_word(cpu, base) & UART_MODE_ENABLE) == 0u) {
                status &= (uint16_t)~(UART_STATUS_TX_ENABLE | UART_STATUS_BREAK);
            }
            raw_write_word(cpu, (uint16_t)(base + 2u), status);
            transmitter_enabled = (status & UART_STATUS_TX_ENABLE) != 0u;
            if ((previous & UART_STATUS_OVERRUN) != 0u &&
                (requested & UART_STATUS_OVERRUN) == 0u) {
                uart_clear_receive(cpu, channel);
            }
            if (transmitter_was_enabled && !transmitter_enabled) {
                uart_clear_transmit(cpu, channel);
            } else if (!transmitter_was_enabled && transmitter_enabled) {
                uart_raise_transmit(cpu, channel,
                                    uart_transmit_interrupt_mode(cpu, channel) == 0u);
                uart_start_transmit(cpu, channel);
            }
            uart_refresh_status(cpu, channel);
            return;
        }
        if (offset == 4u) {
            Dspic33UartFrame frame;
            memset(&frame, 0, sizeof(frame));
            frame.value = requested & 0x01ffu;
            raw_write_word(cpu, (uint16_t)(base + 4u), 0u);
            if (!uart_module_disabled(cpu, channel) &&
                (raw_word(cpu, base) & UART_MODE_ENABLE) != 0u &&
                uart_fifo_push(&cpu->io.uart_tx_fifo[channel], &frame)) {
                uart_start_transmit(cpu, channel);
            }
            uart_refresh_status(cpu, channel);
            return;
        }
        if (offset == 6u) {
            uart_refresh_status(cpu, channel);
            return;
        }
        return;
    }
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        if (address == pmd_addresses[channel]) {
            bool was_disabled = (previous & pmd_masks[channel]) != 0u;
            bool disabled = uart_module_disabled(cpu, channel);
            if (!was_disabled && disabled) {
                uart_reset_runtime(cpu, channel);
            } else if (was_disabled && !disabled) {
                uart_refresh_status(cpu, channel);
            }
        }
    }
}

static void uart_read_complete(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    Dspic33UartFrame discarded;
    if (!uart_fifo_pop(&cpu->io.uart_rx_fifo[channel], &discarded)) {
        return;
    }
    if ((cpu->io.uart_rx_hold_valid & bit) != 0u &&
        uart_fifo_push(&cpu->io.uart_rx_fifo[channel],
                       &cpu->io.uart_rx_hold[channel])) {
        memset(&cpu->io.uart_rx_hold[channel], 0,
               sizeof(cpu->io.uart_rx_hold[channel]));
        cpu->io.uart_rx_hold_valid &= (uint8_t)~bit;
    }
    uart_refresh_status(cpu, channel);
}

static void spi_restore_buffer(Dspic33* cpu, uint8_t channel, uint16_t fallback) {
    uint16_t value;
    if (!word_queue_front(&cpu->io.spi_rx_fifo[channel], &value)) {
        value = fallback;
    }
    raw_write_word(cpu, (uint16_t)(spi_bases[channel] + 8u), value);
}

static void update_spi_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = spi_bases[channel];
        uint16_t offset;
        uint16_t control;
        uint16_t value;
        uint8_t capacity;
        if (address < base || address > base + 8u) {
            continue;
        }
        offset = (uint16_t)(address - base);
        if (offset == 0u) {
            uint16_t status = raw_word(cpu, base);
            if ((requested & SPI_OVERFLOW) != 0u) {
                status =
                    (uint16_t)((status & ~SPI_OVERFLOW) | (previous & SPI_OVERFLOW));
            } else {
                status &= (uint16_t)~SPI_OVERFLOW;
            }
            raw_write_word(cpu, base, status);
            if ((status & SPI_ENABLE) == 0u) {
                spi_clear_buffers(cpu, channel);
            } else {
                spi_refresh_status(cpu, channel);
            }
            return;
        }
        if (offset == 2u) {
            control = raw_word(cpu, (uint16_t)(base + 2u));
            if ((control & SPI_MASTER) == 0u) {
                control &= (uint16_t)~SPI_SAMPLE_END;
                raw_write_word(cpu, (uint16_t)(base + 2u), control);
            }
            if (((control ^ previous) & (SPI_MODE_16 | SPI_MASTER)) != 0u) {
                spi_clear_buffers(cpu, channel);
            } else if ((previous & SPI_DISABLE_CLOCK) != 0u &&
                       (control & SPI_DISABLE_CLOCK) == 0u) {
                cpu->io.spi_generation[channel] =
                    (uint16_t)((cpu->io.spi_generation[channel] + 1u) &
                               SPI_EVENT_GENERATION_MASK);
                spi_schedule_current(cpu, channel);
            }
            return;
        }
        if (offset == 4u) {
            control = raw_word(cpu, (uint16_t)(base + 4u));
            if (((control ^ previous) & SPI_ENHANCED) != 0u) {
                spi_clear_buffers(cpu, channel);
            } else if (((control ^ previous) &
                        (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE | SPI_FRAME_ACTIVE_HIGH |
                         SPI_FRAME_DELAY)) != 0u) {
                cpu->io.spi_frame_active &= (uint8_t)~(1u << channel);
            }
            return;
        }
        if (offset != 8u) {
            return;
        }
        value = raw_word(cpu, (uint16_t)(base + 8u));
        if ((raw_word(cpu, (uint16_t)(base + 2u)) & SPI_MODE_16) == 0u) {
            value &= 0x00ffu;
        }
        capacity = spi_enhanced(cpu, channel) ? 8u : 1u;
        if ((raw_word(cpu, base) & SPI_ENABLE) == 0u ||
            spi_module_disabled(cpu, channel) ||
            cpu->io.spi_tx_fifo[channel].count >= capacity ||
            !word_queue_push(&cpu->io.spi_tx_fifo[channel], value)) {
            spi_restore_buffer(cpu, channel, previous);
            spi_refresh_status(cpu, channel);
            return;
        }
        if (spi_enhanced(cpu, channel) && cpu->io.spi_tx_fifo[channel].count == 8u) {
            spi_raise_mode(cpu, channel, 7u);
        }
        spi_start_next(cpu, channel);
        spi_restore_buffer(cpu, channel, previous);
        spi_refresh_status(cpu, channel);
        return;
    }
    if (address == 0x0760u || address == 0x076au) {
        for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
            if (spi_module_disabled(cpu, channel)) {
                spi_clear_buffers(cpu, channel);
            }
        }
    }
}

static void abort_oscillator_switch(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    if (cpu->oscillator.active) {
        cpu->oscillator.generation++;
        cpu->oscillator.active = false;
        cpu->oscillator.automatic = false;
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
    }
    raw_write_word(cpu, OSCILLATOR_CONTROL,
                   (uint16_t)(control & ~OSCILLATOR_SWITCH_ENABLE));
}

void dspic33_device_abort_oscillator_switch(Dspic33* cpu) {
    if (!cpu->oscillator.active && cpu->oscillator.lock_pending) {
        cpu->oscillator.generation++;
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
    }
    abort_oscillator_switch(cpu);
}

static bool oscillator_pll_mode(uint16_t control) {
    uint16_t source = (uint16_t)((control & OSCILLATOR_REQUEST_MASK) >> 8u);
    return source == 1u || source == 3u;
}

static uint8_t oscillator_current_source(uint16_t control) {
    return (uint8_t)((control & OSCILLATOR_CURRENT_MASK) >> 12u);
}

static uint8_t oscillator_requested_source(uint16_t control) {
    return (uint8_t)((control & OSCILLATOR_REQUEST_MASK) >> 8u);
}

static bool oscillator_source_available(const Dspic33* cpu, uint8_t source) {
    return (source != 2u && source != 3u) || (cpu->configuration[8u] & 0x03u) != 0x03u;
}

static bool oscillator_source_immediately_ready(uint8_t source) {
    return source == 0u || source >= 5u;
}

static bool oscillator_configuration_locked(const Dspic33* cpu, uint16_t control) {
    return (control & OSCILLATOR_CLOCK_LOCK) != 0u &&
           (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_CLOCK_LOCK) != 0u;
}

static bool oscillator_direct_pll_transition(uint16_t control) {
    uint8_t current = oscillator_current_source(control);
    uint8_t requested = oscillator_requested_source(control);
    return (current == 1u && requested == 3u) || (current == 3u && requested == 1u);
}

static bool oscillator_pll_lock_enabled(const Dspic33* cpu) {
    return (cpu->configuration[10u] & OSCILLATOR_CONFIGURATION_PLL_LOCK) != 0u;
}

static bool schedule_oscillator_event(Dspic33* cpu, uint16_t phase, uint64_t delay) {
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_OSCILLATOR, phase,
                          cpu->oscillator.generation, delay)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    return true;
}

static void schedule_oscillator_readiness(Dspic33* cpu, uint16_t control) {
    if (!oscillator_source_available(cpu, oscillator_requested_source(control))) {
        return;
    }
    schedule_oscillator_event(cpu, OSCILLATOR_EVENT_SWITCH,
                              oscillator_pll_mode(control) ? OSCILLATOR_SOURCE_DELAY
                                                           : OSCILLATOR_SWITCH_DELAY);
}

static uint8_t configured_main_pll_source(const Dspic33* cpu, uint16_t control) {
    if (cpu->oscillator.active) {
        if (oscillator_direct_pll_transition(control)) {
            return UINT8_MAX;
        }
        return oscillator_requested_source(control);
    }
    return oscillator_current_source(control);
}

static bool main_pll_relock_required(const Dspic33* cpu, uint16_t address,
                                     uint16_t previous, uint16_t current) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    uint8_t source = configured_main_pll_source(cpu, control);
    uint16_t changed = (uint16_t)(previous ^ current);
    if (source != 1u && source != 3u) {
        return false;
    }
    if (address == MAIN_PLL_FEEDBACK) {
        return (changed & MAIN_PLL_FEEDBACK_DIVISOR) != 0u;
    }
    if (address == MAIN_OSCILLATOR_TUNING) {
        return source == 1u && (changed & MAIN_FRC_TUNING) != 0u;
    }
    if (address != MAIN_CLOCK_DIVISOR) {
        return false;
    }
    if ((changed & MAIN_PLL_PRESCALER) != 0u) {
        return true;
    }
    return source == 1u && (changed & MAIN_FRC_DIVISOR) != 0u;
}

static void restart_main_pll_lock(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    bool source_ready = cpu->oscillator.active && cpu->oscillator.source_ready;
    cpu->oscillator.generation++;
    cpu->oscillator.lock_pending = source_ready || !cpu->oscillator.active;
    cpu->oscillator.source_ready = source_ready;
    raw_write_word(cpu, OSCILLATOR_CONTROL, (uint16_t)(control & ~OSCILLATOR_PLL_LOCK));
    if (cpu->oscillator.lock_pending) {
        schedule_oscillator_event(cpu, OSCILLATOR_EVENT_LOCK, OSCILLATOR_SWITCH_DELAY);
    } else {
        schedule_oscillator_readiness(cpu, control);
    }
}

static void update_main_clock_configuration(Dspic33* cpu, uint16_t address,
                                            uint16_t previous) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    uint16_t current = raw_word(cpu, address);
    if (address != MAIN_CLOCK_DIVISOR && address != MAIN_PLL_FEEDBACK &&
        address != MAIN_OSCILLATOR_TUNING) {
        return;
    }
    if (oscillator_configuration_locked(cpu, control)) {
        raw_write_word(cpu, address, previous);
        return;
    }
    if (address == MAIN_CLOCK_DIVISOR) {
        if ((previous & 0x0800u) != 0u) {
            current = (uint16_t)((current & ~0x7000u) | (previous & 0x7000u));
        }
        if ((current & 0x7000u) == 0u) {
            current &= (uint16_t)~0x0800u;
        }
        raw_write_word(cpu, address, current);
    }
    if (main_pll_relock_required(cpu, address, previous, current)) {
        restart_main_pll_lock(cpu);
    }
}

static void oscillator_configuration_changed(Dspic33* cpu, uint8_t previous) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    uint8_t requested = oscillator_requested_source(control);
    if (cpu->oscillator.active && !cpu->oscillator.automatic &&
        (previous & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) == 0u &&
        (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) != 0u) {
        abort_oscillator_switch(cpu);
        return;
    }
    if (!cpu->oscillator.active || oscillator_direct_pll_transition(control) ||
        (requested != 2u && requested != 3u) ||
        ((previous ^ cpu->configuration[8u]) & 0x03u) == 0u) {
        return;
    }
    cpu->oscillator.generation++;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    schedule_oscillator_readiness(cpu, control);
}

static void oscillator_pll_configuration_changed(Dspic33* cpu, uint8_t previous) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    if (!cpu->oscillator.active || !cpu->oscillator.source_ready ||
        oscillator_direct_pll_transition(control) || !oscillator_pll_mode(control) ||
        ((previous ^ cpu->configuration[10u]) & OSCILLATOR_CONFIGURATION_PLL_LOCK) ==
            0u) {
        return;
    }
    if (!oscillator_pll_lock_enabled(cpu)) {
        control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK &
                              ~OSCILLATOR_SWITCH_ENABLE) |
                             ((control & OSCILLATOR_REQUEST_MASK) << 4u));
        cpu->oscillator.active = false;
        cpu->oscillator.automatic = false;
        cpu->watchdog.ticks = 0u;
        raw_write_word(cpu, OSCILLATOR_CONTROL, control);
    }
}

static void oscillator_startup_configuration_changed(Dspic33* cpu, uint8_t previous) {
    uint8_t source = (uint8_t)(cpu->configuration[6u] & 0x07u);
    if (((previous ^ cpu->configuration[6u]) & 0x07u) == 0u ||
        (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) == 0u) {
        return;
    }
    dspic33_device_abort_oscillator_switch(cpu);
    start_automatic_oscillator_switch(cpu, source);
}

static void start_oscillator_switch(Dspic33* cpu, uint16_t control) {
    cpu->oscillator.generation++;
    cpu->oscillator.active = true;
    cpu->oscillator.automatic = false;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    raw_write_word(cpu, OSCILLATOR_CONTROL,
                   (uint16_t)((control | OSCILLATOR_SWITCH_ENABLE) &
                              ~OSCILLATOR_PLL_LOCK & ~OSCILLATOR_CLOCK_FAIL));
    schedule_oscillator_readiness(cpu, control);
}

static void start_automatic_oscillator_switch(Dspic33* cpu, uint8_t source) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    control = (uint16_t)((control & ~OSCILLATOR_REQUEST_MASK & ~OSCILLATOR_PLL_LOCK &
                          ~OSCILLATOR_CLOCK_FAIL & ~OSCILLATOR_SWITCH_ENABLE) |
                         ((uint16_t)source << 8u));
    raw_write_word(cpu, OSCILLATOR_CONTROL, control);
    if (oscillator_source_immediately_ready(source)) {
        control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK) |
                             ((uint16_t)source << 12u));
        raw_write_word(cpu, OSCILLATOR_CONTROL, control);
        return;
    }
    cpu->oscillator.generation++;
    cpu->oscillator.active = true;
    cpu->oscillator.automatic = true;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    schedule_oscillator_readiness(cpu, control);
}

static void reset_main_oscillator(Dspic33* cpu) {
    uint8_t source = (uint8_t)(cpu->configuration[6u] & 0x07u);
    uint16_t control = (uint16_t)(source << 8u);
    if ((cpu->configuration[6u] & 0x80u) == 0u) {
        control |= (uint16_t)(source << 12u);
        if (source == 1u || source == 3u) {
            control |= OSCILLATOR_PLL_LOCK;
        }
    }
    raw_write_word(cpu, OSCILLATOR_CONTROL, control);
}

void dspic33_device_power_on_reset(Dspic33* cpu) {
    uint8_t source = (uint8_t)(cpu->configuration[6u] & 0x07u);
    if ((cpu->configuration[6u] & 0x80u) != 0u && source != 0u) {
        start_automatic_oscillator_switch(cpu, source);
    }
}

void dspic33_device_reset_restored(Dspic33* cpu) {
    pps_capture_shadow(cpu);
    refresh_gpio_change_notification(cpu);
    refresh_external_interrupts(cpu);
    output_compare_refresh_fault_pps_inputs(cpu);
    dci_refresh_pps_inputs(cpu);
    dspic33_i2c_refresh_pins(cpu);
}

void dspic33_device_brown_out_reset(Dspic33* cpu) {
    size_t destination = 0u;
    size_t source;
    for (source = 0u; source < cpu->events.count; source++) {
        if (cpu->events.items[source].type != DSPIC33_EVENT_OSCILLATOR) {
            cpu->events.items[destination++] = cpu->events.items[source];
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
    memset(&cpu->oscillator, 0, sizeof(cpu->oscillator));
    reset_main_oscillator(cpu);
    dspic33_device_power_on_reset(cpu);
    dspic33_device_reset_restored(cpu);
}

static void complete_oscillator_event(Dspic33* cpu, uint16_t phase,
                                      uint32_t generation) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    if (generation != cpu->oscillator.generation) {
        return;
    }
    if (phase == OSCILLATOR_EVENT_LOCK) {
        if (!cpu->oscillator.lock_pending) {
            return;
        }
        if (cpu->oscillator.active && cpu->oscillator.source_ready) {
            control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK &
                                  ~OSCILLATOR_SWITCH_ENABLE) |
                                 ((control & OSCILLATOR_REQUEST_MASK) << 4u) |
                                 OSCILLATOR_PLL_LOCK);
            cpu->oscillator.active = false;
            cpu->oscillator.automatic = false;
            cpu->watchdog.ticks = 0u;
            raw_write_word(cpu, OSCILLATOR_CONTROL, control);
        } else if (oscillator_current_source(control) == 1u ||
                   oscillator_current_source(control) == 3u) {
            raw_write_word(cpu, OSCILLATOR_CONTROL,
                           (uint16_t)(control | OSCILLATOR_PLL_LOCK));
        }
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
        return;
    }
    if (!cpu->oscillator.active ||
        (!cpu->oscillator.automatic && (control & OSCILLATOR_SWITCH_ENABLE) == 0u)) {
        return;
    }
    if (!oscillator_source_available(cpu, oscillator_requested_source(control))) {
        return;
    }
    if (oscillator_pll_mode(control)) {
        cpu->oscillator.source_ready = true;
        cpu->oscillator.lock_pending = true;
        schedule_oscillator_event(cpu, OSCILLATOR_EVENT_LOCK,
                                  OSCILLATOR_SWITCH_DELAY - OSCILLATOR_SOURCE_DELAY);
        if (oscillator_pll_lock_enabled(cpu)) {
            return;
        }
    }
    control =
        (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_SWITCH_ENABLE) |
                   ((control & OSCILLATOR_REQUEST_MASK) << 4u));
    cpu->oscillator.active = false;
    cpu->oscillator.automatic = false;
    cpu->watchdog.ticks = 0u;
    raw_write_word(cpu, OSCILLATOR_CONTROL, control);
}

static bool oscillator_key_authorized(const Dspic33* cpu, uint8_t lane, uint8_t stage) {
    return cpu->oscillator.key_stage == stage && cpu->oscillator.key_lane == lane &&
           cpu->instructions == cpu->oscillator.key_instruction + 1u &&
           cpu->interrupt_count == cpu->oscillator.key_interrupt_count &&
           cpu->trap_count == cpu->oscillator.key_trap_count;
}

static void oscillator_key_start(Dspic33* cpu, uint8_t lane) {
    cpu->oscillator.key_stage = 1u;
    cpu->oscillator.key_lane = lane;
    cpu->oscillator.key_instruction = cpu->instructions;
    cpu->oscillator.key_interrupt_count = cpu->interrupt_count;
    cpu->oscillator.key_trap_count = cpu->trap_count;
}

static void apply_oscillator_high(Dspic33* cpu, uint16_t previous, uint16_t requested) {
    uint16_t control;
    if (oscillator_configuration_locked(cpu, previous)) {
        raw_write_word(cpu, OSCILLATOR_CONTROL, previous);
        return;
    }
    control = (uint16_t)((previous & ~OSCILLATOR_REQUEST_MASK) |
                         (requested & OSCILLATOR_REQUEST_MASK));
    if (cpu->oscillator.active) {
        abort_oscillator_switch(cpu);
        control &= (uint16_t)~OSCILLATOR_SWITCH_ENABLE;
    }
    raw_write_word(cpu, OSCILLATOR_CONTROL, control);
}

static void apply_oscillator_low(Dspic33* cpu, uint16_t previous, uint16_t requested) {
    uint16_t control = previous;
    uint16_t writable = OSCILLATOR_IO_LOCK;
    bool was_io_locked = (previous & OSCILLATOR_IO_LOCK) != 0u;
    bool requests_io_lock = (requested & OSCILLATOR_IO_LOCK) != 0u;
    if (was_io_locked && !requests_io_lock && (cpu->configuration[8u] & 0x20u) != 0u &&
        cpu->io.pps.one_way_committed) {
        requested |= OSCILLATOR_IO_LOCK;
    }
    if (!oscillator_configuration_locked(cpu, previous)) {
        writable |= OSCILLATOR_LP_ENABLE;
    }
    control = (uint16_t)((control & ~writable) | (requested & writable));
    if (!was_io_locked && (control & OSCILLATOR_IO_LOCK) != 0u) {
        cpu->io.pps.one_way_committed = true;
    }
    control |= (uint16_t)(requested & OSCILLATOR_CLOCK_LOCK);
    if ((requested & OSCILLATOR_CLOCK_FAIL) == 0u) {
        control &= (uint16_t)~OSCILLATOR_CLOCK_FAIL;
    }
    if ((requested & OSCILLATOR_SWITCH_ENABLE) == 0u) {
        raw_write_word(cpu, OSCILLATOR_CONTROL, control);
        if (!cpu->oscillator.automatic) {
            abort_oscillator_switch(cpu);
        }
        return;
    }
    if ((cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) != 0u ||
        oscillator_configuration_locked(cpu, control) ||
        ((control & OSCILLATOR_CURRENT_MASK) >> 4u) ==
            (control & OSCILLATOR_REQUEST_MASK)) {
        raw_write_word(cpu, OSCILLATOR_CONTROL,
                       (uint16_t)(control & ~OSCILLATOR_SWITCH_ENABLE));
        if (!cpu->oscillator.automatic) {
            abort_oscillator_switch(cpu);
        }
        return;
    }
    if (oscillator_direct_pll_transition(control)) {
        cpu->oscillator.generation++;
        cpu->oscillator.active = true;
        raw_write_word(cpu, OSCILLATOR_CONTROL,
                       (uint16_t)(control | OSCILLATOR_SWITCH_ENABLE));
        return;
    }
    if (cpu->oscillator.active) {
        abort_oscillator_switch(cpu);
    }
    start_oscillator_switch(cpu, control);
}

bool dspic33_oscillator_failure_detected(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, OSCILLATOR_CONTROL);
    uint8_t current = oscillator_current_source(control);
    if ((cpu->configuration[8u] & 0xc0u) != 0u ||
        cpu->power_state == DSPIC33_POWER_SLEEP ||
        (current != 2u && current != 3u && current != 4u)) {
        return false;
    }
    dspic33_device_abort_oscillator_switch(cpu);
    control = raw_word(cpu, OSCILLATOR_CONTROL);
    control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_PLL_LOCK) |
                         OSCILLATOR_CLOCK_FAIL);
    raw_write_word(cpu, OSCILLATOR_CONTROL, control);
    dspic33_raise_oscillator_fail_trap(cpu);
    return true;
}

static bool protect_oscillator_write(Dspic33* cpu, uint16_t address,
                                     uint16_t previous) {
    uint16_t requested;
    uint8_t lane;
    uint8_t value;
    uint8_t first;
    uint8_t second;
    if (address != OSCILLATOR_CONTROL && address != OSCILLATOR_CONTROL + 1u) {
        return false;
    }
    requested = raw_word(cpu, OSCILLATOR_CONTROL);
    lane = (uint8_t)(address - OSCILLATOR_CONTROL);
    value = cpu->data[address];
    first = lane == 0u ? 0x46u : 0x78u;
    second = lane == 0u ? 0x57u : 0x9au;
    raw_write_word(cpu, OSCILLATOR_CONTROL, previous);
    if (!cpu->instruction_active || cpu->io.cpu_write_width != 1u) {
        cpu->oscillator.key_stage = 0u;
        return true;
    }
    if (oscillator_key_authorized(cpu, lane, 2u)) {
        cpu->oscillator.key_stage = 0u;
        if (lane == 0u) {
            apply_oscillator_low(cpu, previous, requested);
        } else {
            apply_oscillator_high(cpu, previous, requested);
        }
        return true;
    }
    if (oscillator_key_authorized(cpu, lane, 1u) && value == second) {
        cpu->oscillator.key_stage = 2u;
        cpu->oscillator.key_instruction = cpu->instructions;
        return true;
    }
    if (value == first) {
        oscillator_key_start(cpu, lane);
        return true;
    }
    cpu->oscillator.key_stage = 0u;
    return true;
}

static bool dma_register_write_mask(uint16_t address, uint16_t* writable) {
    static const uint16_t channel_masks[] = {0xf833u, 0x80ffu, 0xffffu, 0x00ffu,
                                             0xffffu, 0x00ffu, 0xffffu, 0x3fffu};
    if (address >= DMA_CHANNEL_BASE &&
        address < DMA_CHANNEL_BASE + DSPIC33_DMA_COUNT * DMA_CHANNEL_STRIDE) {
        *writable = channel_masks[((address - DMA_CHANNEL_BASE) & 0x000fu) / 2u];
        return true;
    }
    if (address >= DMA_PWC && address <= DMA_SADRH) {
        *writable = 0u;
        return true;
    }
    return false;
}

static bool interrupt_control_write(Dspic33* cpu, uint16_t base, uint16_t previous,
                                    uint16_t requested) {
    uint16_t current;
    if (base == 0x08c0u) {
        current = (uint16_t)(requested & 0xfffeu);
        raw_write_word(cpu, base, current);
        if ((previous & 0x0020u) != 0u && (current & 0x0020u) == 0u) {
            raw_write_word(cpu, DMA_PWC, 0u);
            raw_write_word(cpu, DMA_RQC, 0u);
        }
        if ((current & 0x0010u) == 0u) {
            dspic33_set_math_error_source(cpu, false);
        }
        return true;
    }
    if (base == 0x08c2u) {
        current = (uint16_t)((previous & 0x4000u) | (requested & 0xa01fu));
        raw_write_word(cpu, base, current);
        if ((current & 0x2000u) != 0u) {
            raw_write_word(cpu, 0x08c6u, (uint16_t)(raw_word(cpu, 0x08c6u) | 0x0001u));
        }
        dspic33_set_generic_hard_trap_source(
            cpu, (current & 0x2000u) != 0u || (raw_word(cpu, 0x08c6u) & 0x0001u) != 0u);
        if ((current & 0x8000u) != 0u) {
            cpu->gie_disable_deferred = 0u;
            cpu->gie_disable_deferred_next = 0u;
        } else if ((previous & 0x8000u) != 0u) {
            cpu->gie_disable_deferred_next = 1u;
        }
        return true;
    }
    if (base == 0x08c4u) {
        current = (uint16_t)(requested & 0x0070u);
        raw_write_word(cpu, base, current);
        dspic33_set_generic_soft_trap_source(cpu, current != 0u);
        return true;
    }
    if (base == 0x08c6u) {
        current = (uint16_t)(requested & 0x0001u);
        raw_write_word(cpu, base, current);
        dspic33_set_generic_hard_trap_source(
            cpu, current != 0u || (raw_word(cpu, 0x08c2u) & 0x2000u) != 0u);
        return true;
    }
    if (base == 0x08c8u) {
        raw_write_word(cpu, base, previous);
        return true;
    }
    return false;
}

static uint32_t dma_register_address(const Dspic33* cpu, uint16_t base,
                                     uint16_t low_offset) {
    return ((uint32_t)(raw_word(cpu, (uint16_t)(base + low_offset + 2u)) & 0x00ffu)
            << 16u) |
           raw_word(cpu, (uint16_t)(base + low_offset));
}

static void initialize_dma_channel(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dma_channel_base(channel);
    uint16_t bit = dma_channel_bit(channel);
    cpu->io.dma_index[channel] = 0u;
    cpu->io.dma_start_a[channel] = dma_register_address(cpu, base, 4u);
    cpu->io.dma_start_b[channel] = dma_register_address(cpu, base, 8u);
    cpu->io.dma_address[channel] = cpu->io.dma_start_a[channel];
    cpu->io.dma_bank &= (uint16_t)~bit;
    cpu->io.dma_half_raised &= (uint16_t)~bit;
    cpu->io.dma_forced_pending &= (uint16_t)~bit;
    cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
    cpu->io.dma_active &= (uint16_t)~bit;
    cpu->io.dma_enabled |= bit;
    cpu->io.dma_generation[channel]++;
    raw_write_word(cpu, DMA_PPS, (uint16_t)(raw_word(cpu, DMA_PPS) & ~bit));
}

static void update_dma_control(Dspic33* cpu, uint8_t channel, uint16_t previous) {
    uint16_t base = dma_channel_base(channel);
    uint16_t current = raw_word(cpu, base);
    uint16_t bit = dma_channel_bit(channel);
    bool was_enabled = (previous & DMA_CON_CHEN) != 0u;
    bool enabled = (current & DMA_CON_CHEN) != 0u;
    if (enabled && !was_enabled) {
        initialize_dma_channel(cpu, channel);
    } else if (!enabled && was_enabled) {
        cpu->io.dma_enabled &= (uint16_t)~bit;
        cpu->io.dma_forced_pending &= (uint16_t)~bit;
        cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
        cpu->io.dma_active &= (uint16_t)~bit;
        cpu->io.dma_generation[channel]++;
        raw_write_word(
            cpu, (uint16_t)(base + 2u),
            (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
    }
}

static void update_dma_request(Dspic33* cpu, uint8_t channel, uint16_t previous) {
    uint16_t base = dma_channel_base(channel);
    uint16_t address = (uint16_t)(base + 2u);
    uint16_t request = raw_word(cpu, address);
    uint16_t bit = dma_channel_bit(channel);
    if ((raw_word(cpu, base) & DMA_CON_CHEN) == 0u) {
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((previous & DMA_REQ_FORCE) != 0u) {
        raw_write_word(cpu, address, (uint16_t)(request | DMA_REQ_FORCE));
        return;
    }
    if ((request & DMA_REQ_FORCE) == 0u) {
        return;
    }
    if ((cpu->io.dma_active & bit) != 0u) {
        dma_request_collision(cpu, channel);
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((cpu->io.dma_peripheral_pending & bit) != 0u) {
        dma_request_collision(cpu, channel);
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((cpu->io.dma_forced_pending & bit) == 0u &&
        !schedule_dma_channel(cpu, channel, 0u, true, 1u)) {
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
    }
}

static void can_abort_transmissions(Dspic33* cpu, uint8_t channel) {
    uint8_t buffer;
    for (buffer = 0u; buffer < 8u; buffer++) {
        uint16_t control = can_buffer_control(cpu, channel, buffer);
        if ((control & CAN_BUFFER_REQUEST) != 0u) {
            can_set_buffer_control(
                cpu, channel, buffer,
                (uint16_t)((control & ~CAN_BUFFER_REQUEST) | CAN_BUFFER_ABORTED));
            can_raise_event(cpu, channel, CAN_INTERRUPT_TRANSMIT, buffer, 0u);
        }
    }
    raw_write_word(cpu, can_bases[channel],
                   (uint16_t)(raw_word(cpu, can_bases[channel]) & ~CAN_ABORT_ALL));
    cpu->io.can_tx_busy &= (uint8_t)~(uint8_t)(1u << channel);
}

static void can_clear_receive_flags(Dspic33* cpu, uint8_t channel, uint16_t address,
                                    uint16_t previous, uint16_t requested) {
    uint16_t cleared;
    uint16_t result = (uint16_t)(previous & requested);
    uint8_t high = address == can_bases[channel] + 0x22u ? 16u : 0u;
    uint8_t bit;
    raw_write_word(cpu, address, result);
    if (address != can_bases[channel] + 0x20u &&
        address != can_bases[channel] + 0x22u) {
        return;
    }
    cleared = (uint16_t)(previous & ~result);
    for (bit = 0u; bit < 16u; bit++) {
        uint8_t buffer = (uint8_t)(high + bit);
        if ((cleared & (uint16_t)(1u << bit)) != 0u &&
            buffer >= (raw_word(cpu, (uint16_t)(can_bases[channel] + 6u)) & 0x001fu)) {
            uint8_t next = can_next_fifo_buffer(cpu, channel, buffer);
            uint16_t fifo = raw_word(cpu, (uint16_t)(can_bases[channel] + 8u));
            raw_write_word(cpu, (uint16_t)(can_bases[channel] + 8u),
                           (uint16_t)((fifo & 0x3f00u) | next));
        }
    }
}

static void can_update_transmit_control(Dspic33* cpu, uint8_t channel, uint16_t address,
                                        uint16_t previous) {
    uint8_t first = (uint8_t)(((address - can_bases[channel]) - 0x30u));
    uint8_t half;
    for (half = 0u; half < 2u; half++) {
        uint8_t buffer = (uint8_t)(first + half);
        uint8_t shift = (uint8_t)(half * 8u);
        uint16_t before = (uint16_t)((previous >> shift) & 0xffu);
        uint16_t current = can_buffer_control(cpu, channel, buffer);
        if ((current & CAN_BUFFER_REQUEST) != 0u &&
            (before & CAN_BUFFER_REQUEST) == 0u) {
            current &=
                (uint16_t)~(CAN_BUFFER_ABORTED | CAN_BUFFER_LOST | CAN_BUFFER_ERROR);
            can_set_buffer_control(cpu, channel, buffer, current);
            dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START,
                             0u);
        } else if ((current & CAN_BUFFER_REQUEST) == 0u &&
                   (before & CAN_BUFFER_REQUEST) != 0u) {
            can_set_buffer_control(cpu, channel, buffer,
                                   (uint16_t)(current | CAN_BUFFER_ABORTED));
            can_raise_event(cpu, channel, CAN_INTERRUPT_TRANSMIT, buffer, 0u);
        }
    }
}

static void update_can_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = can_bases[channel];
        uint16_t offset = (uint16_t)(address - base);
        bool window;
        uint16_t writable;
        if (offset > 0x7eu || (offset & 1u) != 0u) {
            continue;
        }
        window = (raw_word(cpu, base) & CAN_WINDOW) != 0u;
        if (offset == 0u) {
            uint16_t control = raw_word(cpu, base);
            uint16_t mode = (uint16_t)((control & CAN_MODE_MASK) >> CAN_MODE_SHIFT);
            control = (uint16_t)((control & ~0x00e0u) | (mode << 5u));
            raw_write_word(cpu, base, control);
            if (mode == CAN_MODE_CONFIGURATION &&
                ((previous >> 5u) & 7u) != CAN_MODE_CONFIGURATION) {
                raw_write_word(cpu, (uint16_t)(base + 0x0eu), 0u);
                raw_write_word(
                    cpu, (uint16_t)(base + 0x0au),
                    (uint16_t)(raw_word(cpu, (uint16_t)(base + 0x0au)) & 0x00ffu));
                can_refresh_error_status(cpu, channel);
            }
            if ((control & CAN_ABORT_ALL) != 0u) {
                can_abort_transmissions(cpu, channel);
            }
            return;
        }
        if ((offset == 6u || offset == 0x10u || offset == 0x12u || offset == 0x14u ||
             offset == 0x18u || offset == 0x1au) &&
            can_mode(cpu, channel) != CAN_MODE_CONFIGURATION) {
            raw_write_word(cpu, address, previous);
            return;
        }
        if (offset == 0x0au) {
            raw_write_word(
                cpu, address,
                (uint16_t)((previous & ~0x00efu) | (previous & requested & 0x00efu)));
            can_refresh_error_status(cpu, channel);
            can_update_vector(cpu, channel);
            return;
        }
        if (window && offset >= 0x20u) {
            uint16_t prior = can_filter_word(cpu, channel, offset);
            if (can_mode(cpu, channel) == CAN_MODE_CONFIGURATION &&
                can_register_write_mask(cpu, address, &writable)) {
                cpu->io.can_filter_window[channel][(offset - 0x20u) / 2u] =
                    (uint16_t)((prior & ~writable) | (requested & writable));
            }
            raw_write_word(cpu, address, previous);
            return;
        }
        if (!window && (offset == 0x20u || offset == 0x22u || offset == 0x28u ||
                        offset == 0x2au)) {
            can_clear_receive_flags(cpu, channel, address, previous, requested);
            return;
        }
        if (!window && offset >= 0x30u && offset <= 0x36u) {
            can_update_transmit_control(cpu, channel, address, previous);
            return;
        }
        if (!window && offset == 0x42u &&
            (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) != 0u &&
            cpu->io.can_tx_word[channel] != 0u) {
            cpu->io.can_tx_words[channel][cpu->io.can_tx_word[channel] - 1u] =
                raw_word(cpu, address);
            return;
        }
        if (offset == 6u) {
            uint8_t start = (uint8_t)(raw_word(cpu, address) & 0x001fu);
            cpu->io.can_fifo_write[channel] = start;
            raw_write_word(cpu, (uint16_t)(base + 8u),
                           (uint16_t)(((uint16_t)start << 8u) | start));
        }
        return;
    }
    if (address == 0x0760u) {
        for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
            if ((raw_word(cpu, address) & (uint16_t)(2u << channel)) != 0u) {
                cpu->io.can_rx_busy &= (uint8_t)~(uint8_t)(1u << channel);
                cpu->io.can_tx_busy &= (uint8_t)~(uint8_t)(1u << channel);
            }
        }
    }
}

static void usb_start_host_token(Dspic33* cpu) {
    Dspic33UsbPacket packet;
    uint16_t token = raw_word(cpu, USB_TOK);
    uint8_t direction;
    uint8_t bank;
    uint16_t words[4];
    uint32_t buffer;
    bool increment;
    memset(&packet, 0, sizeof(packet));
    packet.address = (uint8_t)(raw_word(cpu, USB_ADDR) & 0x007fu);
    packet.low_speed = (raw_word(cpu, USB_ADDR) & 0x0080u) != 0u;
    packet.endpoint = (uint8_t)(token & 0x0fu);
    packet.pid = (uint8_t)((token >> 4u) & 0x0fu);
    if ((raw_word(cpu, 0x0766u) & 1u) != 0u ||
        (raw_word(cpu, USB_PWRC) & USB_POWER) == 0u ||
        (raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u ||
        (raw_word(cpu, USB_CON) & USB_TOKEN_BUSY) != 0u ||
        (packet.pid != DSPIC33_USB_PID_OUT && packet.pid != DSPIC33_USB_PID_IN &&
         packet.pid != DSPIC33_USB_PID_SETUP)) {
        return;
    }
    direction = packet.pid == DSPIC33_USB_PID_IN ? 0u : 1u;
    bank = (raw_word(cpu, USB_CON) & USB_PING_PONG_RESET) != 0u
               ? 0u
               : cpu->io.usb_next_bank[0][direction];
    if (!usb_descriptor(cpu, 0u, direction, bank, words)) {
        usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
        return;
    }
    if ((words[0] & USB_DESCRIPTOR_OWNED) == 0u) {
        return;
    }
    if (direction != 0u) {
        packet.size = words[1] & USB_DESCRIPTOR_COUNT_MASK;
        packet.data1 = (words[0] & USB_DESCRIPTOR_DATA1) != 0u;
        buffer = ((uint32_t)words[3] << 16u) | words[2];
        increment = (words[0] & USB_DESCRIPTOR_NO_INCREMENT) == 0u;
        if (!usb_read_memory(cpu, buffer, packet.data, packet.size, increment)) {
            usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
            return;
        }
    }
    if (!usb_queue_push(&cpu->io.usb_tx, &packet)) {
        usb_set_error(cpu, USB_ERROR_DMA);
        return;
    }
    cpu->io.usb_host_pid = packet.pid;
    cpu->io.usb_host_endpoint = packet.endpoint;
    cpu->io.usb_host_pending = true;
    raw_write_word(cpu, USB_CON, (uint16_t)(raw_word(cpu, USB_CON) | USB_TOKEN_BUSY));
}

static void update_usb_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint16_t current = raw_word(cpu, address);
    if (address == USB_OTGIR) {
        raw_write_word(cpu, address, (uint16_t)(previous & ~(requested & 0x00fdu)));
        usb_refresh_interrupt(cpu);
        return;
    }
    if (address == USB_EIR) {
        raw_write_word(cpu, address, (uint16_t)(previous & ~(requested & 0x00ffu)));
        usb_refresh_interrupt(cpu);
        return;
    }
    if (address == USB_IR) {
        uint16_t cleared = requested & 0x00fdu;
        uint16_t remaining =
            (uint16_t)(previous & ~(cleared & ~USB_TRANSACTION_INTERRUPT));
        if ((raw_word(cpu, USB_CON) & USB_HOST_ENABLE) != 0u) {
            if ((previous & USB_ATTACH_INTERRUPT) != 0u && cpu->io.usb_host_attached) {
                remaining |= USB_ATTACH_INTERRUPT;
            }
            if ((previous & USB_DETACH_INTERRUPT) != 0u && !cpu->io.usb_host_attached) {
                remaining |= USB_DETACH_INTERRUPT;
            }
        }
        raw_write_word(cpu, address, remaining);
        if ((cleared & USB_TRANSACTION_INTERRUPT) != 0u) {
            usb_pop_transaction_status(cpu);
        } else {
            usb_refresh_transaction_status(cpu);
        }
        return;
    }
    if (address == USB_PWRC) {
        if ((previous & USB_POWER) != 0u && (current & USB_POWER) == 0u) {
            usb_reset_registers(cpu);
        }
        return;
    }
    if (address == USB_CON) {
        bool previous_host = (previous & USB_HOST_ENABLE) != 0u;
        bool current_host = (current & USB_HOST_ENABLE) != 0u;
        if ((current & USB_PING_PONG_RESET) != 0u) {
            usb_reset_ping_pong(cpu);
        }
        if (previous_host != current_host) {
            raw_write_word(cpu, USB_IR,
                           (uint16_t)(raw_word(cpu, USB_IR) &
                                      ~(USB_ATTACH_INTERRUPT | USB_DETACH_INTERRUPT)));
            cpu->io.usb_host_attached = false;
            usb_refresh_interrupt(cpu);
        }
        if (previous_host && !current_host) {
            cpu->io.usb_host_pending = false;
        }
        if ((current & (USB_HOST_ENABLE | USB_ENABLE)) ==
                (USB_HOST_ENABLE | USB_ENABLE) &&
            (previous & (USB_HOST_ENABLE | USB_ENABLE)) !=
                (USB_HOST_ENABLE | USB_ENABLE)) {
            usb_schedule_bus_event(cpu, DSPIC33_USB_BUS_SOF, UINT16_MAX,
                                   USB_FRAME_CYCLES);
        }
        return;
    }
    if (address == USB_TOK) {
        usb_start_host_token(cpu);
        return;
    }
    if (address == USB_EIE || address == USB_IE || address == USB_OTGIE) {
        usb_refresh_interrupt(cpu);
        return;
    }
    if (address >= USB_EP0 && address < USB_EP0 + DSPIC33_USB_ENDPOINT_COUNT * 2u &&
        (requested & USB_ENDPOINT_STALL) == 0u) {
        raw_write_word(cpu, address,
                       (uint16_t)(raw_word(cpu, address) & ~USB_ENDPOINT_STALL));
    }
}

static void update_nvm_key(Dspic33* cpu, uint16_t requested) {
    uint8_t key = (uint8_t)requested;
    if (key == 0x55u) {
        cpu->nvm.key_stage = 1u;
        cpu->nvm.key_instruction = cpu->instructions;
        cpu->nvm.key_interrupt_count = cpu->interrupt_count;
        cpu->nvm.key_trap_count = cpu->trap_count;
    } else if (key == 0xaau && cpu->nvm.key_stage == 1u &&
               cpu->interrupt_count == cpu->nvm.key_interrupt_count &&
               cpu->trap_count == cpu->nvm.key_trap_count) {
        cpu->nvm.key_stage = 2u;
        cpu->nvm.key_instruction = cpu->instructions;
    } else {
        cpu->nvm.key_stage = 0u;
    }
    raw_write_word(cpu, NVM_KEY, 0u);
}

static uint8_t crc_write_width(const Dspic33* cpu) {
    if (cpu->io.dma_transfer_active) {
        return cpu->io.dma_transfer_width;
    }
    return cpu->io.cpu_write_valid ? cpu->io.cpu_write_width : 1u;
}

static void update_crc_data(Dspic33* cpu, uint16_t address, uint16_t requested) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    uint8_t width = crc_data_width(cpu);
    uint8_t write_width = crc_write_width(cpu);
    bool high_byte = (address & 1u) != 0u;
    if ((raw_word(cpu, CRC_CONTROL) & CRC_ENABLE) == 0u) {
        raw_write_word(cpu, base, 0u);
        return;
    }
    if (base == CRC_DATA_LOW) {
        if (width <= 8u) {
            if (write_width == 1u) {
                crc_push(cpu, requested >> (high_byte ? 8u : 0u));
            }
        } else if (width <= 16u) {
            if (write_width == 2u) {
                crc_push(cpu, requested);
                cpu->io.crc.data_latch = 0u;
            }
        } else if (write_width == 2u) {
            cpu->io.crc.data_latch = (cpu->io.crc.data_latch & 0xffff0000u) | requested;
        } else if (high_byte) {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0xffff00ffu) | (requested & 0xff00u);
        } else {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0xffffff00u) | (requested & 0x00ffu);
        }
    } else if (width > 16u) {
        if (write_width == 2u) {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0x0000ffffu) | ((uint32_t)requested << 16u);
            crc_push(cpu, cpu->io.crc.data_latch);
            cpu->io.crc.data_latch = 0u;
        } else if (high_byte) {
            cpu->io.crc.data_latch = (cpu->io.crc.data_latch & 0x00ffffffu) |
                                     ((uint32_t)(requested & 0xff00u) << 16u);
            if (width > 24u) {
                crc_push(cpu, cpu->io.crc.data_latch);
                cpu->io.crc.data_latch = 0u;
            }
        } else {
            cpu->io.crc.data_latch = (cpu->io.crc.data_latch & 0xff00ffffu) |
                                     ((uint32_t)(requested & 0x00ffu) << 16u);
            if (width <= 24u) {
                crc_push(cpu, cpu->io.crc.data_latch);
                cpu->io.crc.data_latch = 0u;
            }
        }
    }
    raw_write_word(cpu, base, 0u);
}

static void update_crc_control(Dspic33* cpu, uint16_t previous) {
    uint16_t control = raw_word(cpu, CRC_CONTROL);
    bool enabled = (control & CRC_ENABLE) != 0u;
    bool was_go = (previous & CRC_GO) != 0u;
    bool go = (control & CRC_GO) != 0u;
    if (!enabled) {
        crc_reset_runtime(cpu);
        return;
    }
    if (was_go && !go) {
        crc_abort(cpu);
    }
    crc_refresh_status(cpu);
    if (!was_go && go) {
        crc_start_if_ready(cpu);
    }
}

static void update_crc_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    if (base == CRC_PMD_ADDRESS) {
        update_crc_pmd(cpu, previous);
    } else if (base < CRC_CONTROL || base > CRC_SHIFT_HIGH) {
        return;
    } else if (cpu->io.crc.pmd_disabled) {
        raw_write_word(cpu, base, previous);
    } else if (base == CRC_CONTROL) {
        update_crc_control(cpu, previous);
    } else if (base == CRC_DATA_LOW || base == CRC_DATA_HIGH) {
        update_crc_data(cpu, address, requested);
    } else if ((base == CRC_SHIFT_LOW || base == CRC_SHIFT_HIGH) &&
               (raw_word(cpu, CRC_CONTROL) & CRC_GO) != 0u) {
        raw_write_word(cpu, base, previous);
    }
}

static void fail_nvm_write(Dspic33* cpu) {
    uint16_t control = raw_word(cpu, NVM_CONTROL);
    cpu->nvm.key_stage = 0u;
    cpu->nvm.active = false;
    raw_write_word(cpu, NVM_CONTROL,
                   (uint16_t)((control & ~NVM_WRITE) | NVM_WRITE_ERROR));
}

static bool nvm_program_range_valid(uint32_t address, uint32_t size) {
    return dspic33_program_range_implemented(address, size);
}

static bool nvm_target_valid(uint16_t control, uint32_t address) {
    switch (control & 0x000fu) {
    case 0u:
        return address == 0xf80004u || address == 0xf80006u || address == 0xf80008u ||
               address == 0xf8000au || address == 0xf8000cu || address == 0xf8000eu ||
               address == 0xf80010u || address == 0xf80012u;
    case 1u:
        address &= 0x00fffffcu;
        return nvm_program_range_valid(address, 4u);
    case 2u:
        address &= 0x00ffff00u;
        return nvm_program_range_valid(address, DSPIC33_WRITE_LATCH_WORDS * 2u);
    case 3u:
        address &= 0x00fff800u;
        return nvm_program_range_valid(address, 0x800u);
    case 0x0au:
    case 0x0du:
        return true;
    default:
        return false;
    }
}

static void update_nvm_control(Dspic33* cpu, uint16_t requested) {
    uint16_t control = raw_word(cpu, NVM_CONTROL);
    uint32_t target =
        ((uint32_t)raw_word(cpu, NVM_ADDRESS_HIGH) << 16u) | raw_word(cpu, NVM_ADDRESS);
    uint64_t completion_delay = cpu->non_cpu_sfr_read ? 3u : 2u;
    bool write_requested = (requested & NVM_WRITE) != 0u;
    if (cpu->nvm.active) {
        raw_write_word(cpu, NVM_CONTROL,
                       (uint16_t)(control | NVM_WRITE | NVM_WRITE_ERROR));
        return;
    }
    if (!write_requested) {
        return;
    }
    if ((control & NVM_WRITE_ENABLE) == 0u || !nvm_key_authorized(cpu) ||
        cpu->cycles > UINT64_MAX - completion_delay ||
        !nvm_target_valid(control, target)) {
        fail_nvm_write(cpu);
        return;
    }
    cpu->nvm.control = control;
    cpu->nvm.address = target;
    cpu->nvm.auxiliary_origin =
        cpu->instruction_active
            ? cpu->current_instruction_pc >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
                  cpu->current_instruction_pc < DSPIC33_AUXILIARY_PROGRAM_LIMIT
            : cpu->pc >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
                  cpu->pc < DSPIC33_AUXILIARY_PROGRAM_LIMIT;
    memcpy(cpu->nvm.latches, cpu->write_latches, sizeof(cpu->nvm.latches));
    cpu->nvm.key_stage = 0u;
    cpu->nvm.active = true;
    cpu->nvm.completion_cycle = cpu->cycles + completion_delay;
    raw_write_word(cpu, NVM_CONTROL, (uint16_t)(control | NVM_WRITE | NVM_WRITE_ERROR));
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_NVM, 0u, 0u,
                          dspic33_device_instruction_cycles(cpu, completion_delay))) {
        fail_nvm_write(cpu);
    }
}

void dspic33_device_write_byte(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    uint16_t requested = raw_word(cpu, base);
    uint16_t writable;
    uint8_t channel;
    if (protect_oscillator_write(cpu, address, previous)) {
        return;
    }
    if (base >= 0x0680u && base <= 0x06f6u &&
        !pps_register_write_mask(base, &writable)) {
        raw_write_word(cpu, base, previous);
        return;
    }
    if (base >= 0x0680u && base <= 0x06f6u &&
        (raw_word(cpu, 0x0742u) & 0x0040u) != 0u) {
        raw_write_word(cpu, base, previous);
        return;
    }
    if (dspic33_i2c_write_register(cpu, address, previous, requested)) {
        return;
    }
    if (register_write_mask(base, &writable) ||
        input_capture_register_write_mask(base, &writable) ||
        output_compare_register_write_mask(base, &writable) ||
        comparator_register_write_mask(base, &writable) ||
        adc_register_write_mask(base, &writable) ||
        pwm_register_write_mask(base, &writable) ||
        uart_register_write_mask(cpu, base, &writable) ||
        spi_register_write_mask(base, &writable) ||
        can_register_write_mask(cpu, base, &writable) ||
        usb_register_write_mask(cpu, base, previous, &writable) ||
        dma_register_write_mask(base, &writable)) {
        raw_write_word(cpu, base,
                       (uint16_t)((previous & ~writable) | (requested & writable)));
    }
    dspic33_i2c_update_pmd(cpu, base, previous);
    if (base == PMP_PMD_ADDRESS) {
        update_pmp_pmd(cpu, previous);
    }
    update_input_capture_pmd(cpu, base, previous);
    update_output_compare_pmd(cpu, base, previous);
    if (base == 0x0740u && (cpu->configuration[10u] & 0x80u) == 0u &&
        (previous & 0x0020u) == 0u && (raw_word(cpu, base) & 0x0020u) != 0u) {
        cpu->watchdog.ticks = 0u;
    }
    pps_update_shadow(cpu, base);
    update_gpio_latch(cpu, address, requested);
    if (base >= 0x0800u && base < 0x0800u + DSPIC33_IRQ_GROUP_COUNT * 2u) {
        uint16_t group = (uint16_t)((base - 0x0800u) / 2u);
        uint16_t current = raw_word(cpu, base);
        uint16_t cleared = (uint16_t)(previous & ~current);
        cpu->interrupt_deferred[group] &= (uint16_t)~cleared;
        cpu->interrupt_deferred_next[group] =
            (uint16_t)((cpu->interrupt_deferred_next[group] & ~cleared) |
                       (current & ~previous));
    }
    interrupt_control_write(cpu, base, previous, requested);
    if (base == AUXILIARY_CLOCK_CONTROL) {
        if (auxiliary_clock_configuration_locked(cpu)) {
            raw_write_word(cpu, base, previous);
        } else {
            uint16_t control = (uint16_t)((previous & ~AUXILIARY_CLOCK_WRITABLE) |
                                          (requested & AUXILIARY_CLOCK_WRITABLE));
            raw_write_word(cpu, base, control);
            if (auxiliary_pll_reconfiguration(previous, control)) {
                reconfigure_auxiliary_pll(cpu);
            }
        }
    }
    if (base == AUXILIARY_CLOCK_DIVISOR) {
        if (auxiliary_clock_configuration_locked(cpu)) {
            raw_write_word(cpu, base, previous);
        } else {
            uint16_t divisor = requested & AUXILIARY_CLOCK_DIVISOR_WRITABLE;
            raw_write_word(cpu, base, divisor);
            if (((previous ^ divisor) & AUXILIARY_CLOCK_DIVISOR_WRITABLE) != 0u) {
                reconfigure_auxiliary_pll(cpu);
            }
        }
    }
    update_main_clock_configuration(cpu, base, previous);
    if (base == 0x0748u &&
        auxiliary_pll_input(raw_word(cpu, AUXILIARY_CLOCK_CONTROL)) == 1u &&
        ((previous ^ raw_word(cpu, base)) & 0x003fu) != 0u) {
        reconfigure_auxiliary_pll(cpu);
    }
    if (base == REFERENCE_CLOCK_CONTROL && (previous & REFERENCE_CLOCK_ENABLE) != 0u) {
        uint16_t control = raw_word(cpu, base);
        raw_write_word(cpu, base,
                       (uint16_t)((control & ~REFERENCE_CLOCK_DIVISOR) |
                                  (previous & REFERENCE_CLOCK_DIVISOR)));
    }
    update_timer_register(cpu, base);
    update_adc_register(cpu, base, previous, requested);
    update_pwm_register(cpu, base, previous);
    update_spi_register(cpu, base, previous, requested);
    update_can_register(cpu, base, previous, requested);
    update_usb_register(cpu, base, previous, requested);
    update_crc_register(cpu, address, previous, requested);
    update_pmp_register(cpu, address, previous);
    update_input_capture_register(cpu, base, previous);
    update_output_compare_register(cpu, base, previous);
    update_comparator_register(cpu, base, previous, requested);
    update_rtcc_register(cpu, address, previous);
    update_qei_register(cpu, base, previous, requested);
    update_dci_register(cpu, base, previous);
    update_uart_register(cpu, base, previous, requested);
    if (base == NVM_KEY && (cpu->io.cpu_write_width == 2u || address == NVM_KEY)) {
        update_nvm_key(cpu, requested);
    } else if (base == NVM_CONTROL) {
        update_nvm_control(cpu, requested);
    }
    if (base >= DMA_CHANNEL_BASE &&
        base < DMA_CHANNEL_BASE + DSPIC33_DMA_COUNT * DMA_CHANNEL_STRIDE) {
        channel = (uint8_t)((base - DMA_CHANNEL_BASE) / DMA_CHANNEL_STRIDE);
        if ((base & 0x000fu) == 0u) {
            update_dma_control(cpu, channel, previous);
        } else if ((base & 0x000fu) == 2u) {
            update_dma_request(cpu, channel, previous);
        }
    }
    refresh_gpio_change_notification(cpu);
    refresh_external_interrupts(cpu);
    output_compare_refresh_fault_pps_inputs(cpu);
    dci_refresh_pps_inputs(cpu);
    dspic33_i2c_refresh_pins(cpu);
}

static uint16_t gpio_pin_values(const Dspic33* cpu, uint8_t port) {
    uint16_t tris = raw_word(cpu, gpio_tris_addresses[port]);
    uint16_t lat = raw_word(cpu, gpio_latch_addresses[port]);
    uint16_t open_drain = raw_word(cpu, gpio_open_drain_addresses[port]);
    uint16_t driven = cpu->io.gpio_driven[port];
    uint16_t pull_up = raw_word(cpu, gpio_pull_up_addresses[port]);
    uint16_t pull_down = raw_word(cpu, gpio_pull_down_addresses[port]);
    uint16_t inputs = (uint16_t)(tris | gpio_input_only_masks[port]);
    uint16_t analog = gpio_analog_addresses[port] != 0u
                          ? raw_word(cpu, gpio_analog_addresses[port])
                          : 0u;
    uint16_t external =
        (uint16_t)((cpu->io.gpio[port] & driven) | (pull_up & ~driven & ~pull_down));
    if (port == 2u && (cpu->configuration[8u] & 0x04u) == 0u) {
        uint8_t source = oscillator_current_source(raw_word(cpu, OSCILLATOR_CONTROL));
        bool frc = source == 0u || source == 1u || source == 6u || source == 7u;
        bool external_clock =
            (source == 2u || source == 3u) && (cpu->configuration[8u] & 0x03u) == 0u;
        if ((frc || external_clock) &&
            (raw_word(cpu, gpio_open_drain_addresses[0]) & 0x0008u) != 0u) {
            open_drain |= 0x8000u;
        }
    }
    if (port == 6u && (raw_word(cpu, USB_CON) & USB_ENABLE) != 0u) {
        analog |= 0x000cu;
    }
    return (uint16_t)(((external & inputs & ~analog) | (lat & ~inputs & ~open_drain) |
                       (external & ~inputs & open_drain & lat)) &
                      gpio_port_masks[port]);
}

static uint16_t gpio_change_notification_qualified(const Dspic33* cpu, uint8_t port) {
    uint16_t inputs = (uint16_t)(raw_word(cpu, gpio_tris_addresses[port]) |
                                 gpio_input_only_masks[port]);
    uint16_t analog = gpio_analog_addresses[port] != 0u
                          ? raw_word(cpu, gpio_analog_addresses[port])
                          : 0u;
    if (port == 6u && (raw_word(cpu, USB_CON) & USB_ENABLE) != 0u) {
        analog |= 0x000cu;
    }
    return (uint16_t)(raw_word(cpu, gpio_change_notification_addresses[port]) & inputs &
                      ~analog & gpio_port_masks[port]);
}

static uint16_t gpio_change_notification_mismatch(const Dspic33* cpu, uint8_t port) {
    return (uint16_t)((cpu->io.gpio_cn_values[port] ^ cpu->io.gpio_cn_reference[port]) &
                      cpu->io.gpio_cn_qualified[port]);
}

static void refresh_gpio_change_notification(Dspic33* cpu) {
    bool changed = false;
    uint8_t port;
    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        uint16_t previous_mismatch = gpio_change_notification_mismatch(cpu, port);
        uint16_t values = gpio_pin_values(cpu, port);
        uint16_t qualified = gpio_change_notification_qualified(cpu, port);
        uint16_t newly_qualified =
            (uint16_t)(qualified & ~cpu->io.gpio_cn_qualified[port]);
        cpu->io.gpio_cn_reference[port] =
            (uint16_t)((cpu->io.gpio_cn_reference[port] & qualified &
                        ~newly_qualified) |
                       (values & (~qualified | newly_qualified)));
        cpu->io.gpio_cn_values[port] = values;
        cpu->io.gpio_cn_qualified[port] = qualified;
        changed = changed || (gpio_change_notification_mismatch(cpu, port) &
                              ~previous_mismatch) != 0u;
    }
    if (changed) {
        dspic33_raise_interrupt(cpu, 19u);
    }
}

static uint8_t external_interrupt_selection(const Dspic33* cpu, uint8_t channel) {
    static const uint16_t addresses[DSPIC33_EXTERNAL_INTERRUPT_COUNT] = {
        0u, 0x06a0u, 0x06a2u, 0x06a2u, 0x06a4u};
    static const uint8_t shifts[DSPIC33_EXTERNAL_INTERRUPT_COUNT] = {0u, 8u, 0u, 8u,
                                                                     0u};
    if (channel == 0u) {
        return 64u;
    }
    return (uint8_t)((raw_word(cpu, addresses[channel]) >> shifts[channel]) & 0x007fu);
}

static bool external_interrupt_level(const Dspic33* cpu, uint8_t channel,
                                     uint8_t selection, bool* high) {
    const Dspic33PpsPin* mapping;
    uint16_t bit;
    if (channel != 0u && selection == 0u) {
        *high = false;
        return true;
    }
    if (channel != 0u && selection == 1u) {
        *high = (cpu->io.comparator.output_high & 1u) != 0u;
        return true;
    }
    mapping = pps_pin(selection);
    if (mapping == NULL || !pps_physical_input_enabled(cpu, selection)) {
        return false;
    }
    bit = (uint16_t)(1u << mapping->bit);
    *high = (gpio_pin_values(cpu, mapping->port) & bit) != 0u;
    return true;
}

static void refresh_external_interrupts(Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_EXTERNAL_INTERRUPT_COUNT; channel++) {
        uint8_t mask = (uint8_t)(1u << channel);
        uint8_t selection = external_interrupt_selection(cpu, channel);
        bool previous_high = (cpu->io.external_interrupt_levels & mask) != 0u;
        bool previous_qualified = (cpu->io.external_interrupt_qualified & mask) != 0u;
        bool high = false;
        bool qualified = external_interrupt_level(cpu, channel, selection, &high);
        bool reconfigured =
            selection != cpu->io.external_interrupt_selection[channel] ||
            qualified != previous_qualified;
        cpu->io.external_interrupt_selection[channel] = selection;
        if (high) {
            cpu->io.external_interrupt_levels |= mask;
        } else {
            cpu->io.external_interrupt_levels &= (uint8_t)~mask;
        }
        if (qualified) {
            cpu->io.external_interrupt_qualified |= mask;
        } else {
            cpu->io.external_interrupt_qualified &= (uint8_t)~mask;
        }
        if (!reconfigured && qualified && high != previous_high) {
            bool falling = (raw_word(cpu, 0x08c2u) & mask) != 0u;
            if (falling == previous_high) {
                raise_external_interrupt(cpu, channel);
            }
        }
    }
}

static void acknowledge_gpio_change_notification(Dspic33* cpu, uint8_t port,
                                                 uint16_t values) {
    cpu->io.gpio_cn_reference[port] = values;
    cpu->io.gpio_cn_values[port] = values;
    cpu->io.gpio_cn_qualified[port] = gpio_change_notification_qualified(cpu, port);
}

uint8_t dspic33_device_read_byte(Dspic33* cpu, uint16_t address, uint8_t value) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    uint8_t port;
    uint8_t channel;
    uint8_t timer;
    if (address == 0x0741u) {
        return (uint8_t)(value & ~0x08u);
    }
    if (address >= INPUT_CAPTURE_BASE &&
        address <
            INPUT_CAPTURE_BASE + DSPIC33_INPUT_CAPTURE_COUNT * INPUT_CAPTURE_STRIDE) {
        channel = (uint8_t)((address - INPUT_CAPTURE_BASE) / INPUT_CAPTURE_STRIDE);
        if (input_capture_pmd_disabled(cpu, channel)) {
            return 0u;
        }
    }
    if (address >= OUTPUT_COMPARE_BASE &&
        address < OUTPUT_COMPARE_BASE +
                      DSPIC33_OUTPUT_COMPARE_COUNT * OUTPUT_COMPARE_STRIDE) {
        channel = (uint8_t)((address - OUTPUT_COMPARE_BASE) / OUTPUT_COMPARE_STRIDE);
        if (output_compare_pmd_disabled(cpu, channel)) {
            return 0u;
        }
    }
    if (cpu->io.crc.pmd_disabled && base >= CRC_CONTROL && base <= CRC_SHIFT_HIGH) {
        return 0u;
    }
    if (cpu->io.pmp.pmd_disabled && base >= PMP_CONTROL && base <= PMP_STATUS) {
        return 0u;
    }
    pmp_read_register(cpu, address);
    if (dspic33_i2c_read_register(cpu, address, &value)) {
        return value;
    }
    if (qei_read_register(cpu, address, &value)) {
        return value;
    }
    if (dci_read_register(cpu, address, &value)) {
        return value;
    }
    if (base >= RTCC_ALARM_VALUE && base <= RTCC_CONTROL) {
        if (cpu->io.rtcc.pmd_disabled) {
            return 0u;
        }
        if (base == RTCC_ALARM_VALUE) {
            return rtcc_read_window(cpu, address, true);
        }
        if (base == RTCC_VALUE) {
            return rtcc_read_window(cpu, address, false);
        }
    }
    if ((address & 0xfffeu) == 0x072eu) {
        return 0u;
    }
    if ((address & 0xfffeu) == CRC_DATA_LOW || (address & 0xfffeu) == CRC_DATA_HIGH) {
        return 0u;
    }
    if (address == USB_IR && (raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u) {
        return (uint8_t)(value & ~USB_ATTACH_INTERRUPT);
    }
    if (address == USB_IE && (raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u) {
        return (uint8_t)(value & ~USB_ATTACH_INTERRUPT);
    }
    if (!cpu->io.comparator.pmd_disabled && address >= COMPARATOR_BASE + 1u &&
        address < COMPARATOR_BASE + DSPIC33_COMPARATOR_COUNT * COMPARATOR_STRIDE &&
        ((address - COMPARATOR_BASE) % COMPARATOR_STRIDE) == 1u) {
        uint8_t comparator = (uint8_t)((address - COMPARATOR_BASE) / COMPARATOR_STRIDE);
        uint8_t bit = (uint8_t)(1u << comparator);
        if ((value & 1u) != 0u) {
            cpu->io.comparator.last_read_cout |= bit;
        } else {
            cpu->io.comparator.last_read_cout &= (uint8_t)~bit;
        }
    }
    if (address >= INPUT_CAPTURE_BASE &&
        address <
            INPUT_CAPTURE_BASE + DSPIC33_INPUT_CAPTURE_COUNT * INPUT_CAPTURE_STRIDE &&
        ((address - INPUT_CAPTURE_BASE) % INPUT_CAPTURE_STRIDE) == 5u) {
        channel = (uint8_t)((address - INPUT_CAPTURE_BASE) / INPUT_CAPTURE_STRIDE);
        input_capture_read_complete(cpu, channel);
    }
    if (address == 0x08c3u) {
        return cpu->disicnt != 0u ? (uint8_t)(value | 0x40u)
                                  : (uint8_t)(value & ~0x40u);
    }
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = can_bases[channel];
        uint16_t word_address = (uint16_t)(address & 0xfffeu);
        uint16_t offset = (uint16_t)(word_address - base);
        if (offset <= 0x7eu && (raw_word(cpu, base) & CAN_WINDOW) != 0u &&
            offset >= 0x20u) {
            uint16_t word = can_filter_word(cpu, channel, offset);
            return (uint8_t)(word >> ((address & 1u) * 8u));
        }
        if (offset == 0x40u && (raw_word(cpu, base) & CAN_WINDOW) == 0u &&
            (cpu->io.can_rx_busy & (uint8_t)(1u << channel)) != 0u &&
            cpu->io.can_rx_word[channel] != 0u) {
            uint16_t word =
                cpu->io.can_rx_words[channel][cpu->io.can_rx_word[channel] - 1u];
            return (uint8_t)(word >> ((address & 1u) * 8u));
        }
    }
    if ((address & 1u) == 0u) {
        for (timer = 1u; timer < DSPIC33_TIMER_COUNT; timer += 2u) {
            if (address == timer_registers[timer] && timer_pair_enabled(cpu, timer)) {
                raw_write_word(cpu, timer_holding_registers[timer / 2u],
                               raw_word(cpu, timer_registers[timer + 1u]));
                break;
            }
        }
    }
    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        if ((address & 0xfffeu) == gpio_port_addresses[port]) {
            uint16_t pins = gpio_pin_values(cpu, port);
            acknowledge_gpio_change_notification(cpu, port, pins);
            return (uint8_t)(pins >> ((address & 1u) * 8u));
        }
    }
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t base = uart_bases[channel];
        bool high_byte = (address & 1u) != 0u;
        bool nine_bit =
            (raw_word(cpu, base) & UART_MODE_DATA_MASK) == UART_MODE_DATA_MASK;
        if ((address & 0xfffeu) == base + 6u && high_byte == nine_bit) {
            uart_read_complete(cpu, channel);
        }
    }
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = spi_bases[channel];
        if ((address & 0xfffeu) == base + 8u && (address & 1u) != 0u) {
            uint16_t discarded;
            if (word_queue_pop(&cpu->io.spi_rx_fifo[channel], &discarded)) {
                if (word_queue_front(&cpu->io.spi_rx_fifo[channel], &discarded)) {
                    raw_write_word(cpu, (uint16_t)(base + 8u), discarded);
                }
                spi_refresh_status(cpu, channel);
                if (spi_enhanced(cpu, channel) &&
                    cpu->io.spi_rx_fifo[channel].count == 0u) {
                    spi_raise_mode(cpu, channel, 0u);
                }
            } else {
                raw_write_word(cpu, base,
                               (uint16_t)(raw_word(cpu, base) & ~SPI_RX_FULL));
            }
        }
    }
    return value;
}

bool dspic33_uart_receive(Dspic33* cpu, uint8_t channel, uint8_t value,
                          uint64_t delay) {
    Dspic33UartFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.value = value;
    return dspic33_uart_receive_frame(cpu, channel, &frame, delay);
}

bool dspic33_uart_receive_frame(Dspic33* cpu, uint8_t channel,
                                const Dspic33UartFrame* frame, uint64_t delay) {
    uint32_t event_value;
    if (channel >= DSPIC33_UART_COUNT || frame == NULL || frame->value > 0x01ffu) {
        return false;
    }
    event_value =
        frame->value | ((uint32_t)frame->baud_period << UART_EVENT_BAUD_SHIFT);
    if (frame->parity_error) {
        event_value |= UART_EVENT_PARITY_ERROR;
    }
    if (frame->framing_error) {
        event_value |= UART_EVENT_FRAMING_ERROR;
    }
    return dspic33_schedule(cpu, DSPIC33_EVENT_UART, channel, event_value, delay);
}

bool dspic33_uart_set_cts(Dspic33* cpu, uint8_t channel, bool clear, uint64_t delay) {
    return channel < DSPIC33_UART_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_UART, channel,
                            UART_EVENT_CTS | (clear ? 1u : 0u), delay);
}

bool dspic33_uart_transmit(Dspic33* cpu, uint8_t channel, Dspic33UartFrame* frame) {
    return channel < DSPIC33_UART_COUNT && frame != NULL &&
           uart_queue_pop(&cpu->io.uart_tx[channel], frame);
}

bool dspic33_spi_receive(Dspic33* cpu, uint8_t channel, uint16_t value,
                         uint64_t delay) {
    return channel < DSPIC33_SPI_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, SPI_EVENT_EXTERNAL | value,
                            delay);
}

bool dspic33_spi_select(Dspic33* cpu, uint8_t channel, bool selected, uint64_t delay) {
    return channel < DSPIC33_SPI_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_SPI_SELECT, channel,
                            selected ? SPI_SELECT_ACTIVE : 0u, delay);
}

bool dspic33_spi_frame_output(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t control;
    uint8_t bit;
    if (channel >= DSPIC33_SPI_COUNT || high == NULL ||
        spi_module_disabled(cpu, channel) ||
        (raw_word(cpu, spi_bases[channel]) & SPI_ENABLE) == 0u) {
        return false;
    }
    control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u));
    if ((control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) != SPI_FRAME_ENABLE) {
        return false;
    }
    bit = (uint8_t)(1u << channel);
    *high = (control & SPI_FRAME_ACTIVE_HIGH) != 0u
                ? (cpu->io.spi_frame_active & bit) != 0u
                : (cpu->io.spi_frame_active & bit) == 0u;
    return true;
}

bool dspic33_spi_frame_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    static const uint8_t functions[DSPIC33_SPI_COUNT] = {7u, 10u, 33u, 36u};
    uint8_t function = 0u;
    uint8_t channel;
    size_t index;
    if (high == NULL) {
        return false;
    }
    for (index = 0u; index < sizeof(pps_outputs) / sizeof(pps_outputs[0]); index++) {
        if (pps_outputs[index].pin == pin) {
            function = (uint8_t)((raw_word(cpu, pps_outputs[index].address) >>
                                  pps_outputs[index].shift) &
                                 0x003fu);
            break;
        }
    }
    if (index == sizeof(pps_outputs) / sizeof(pps_outputs[0])) {
        return false;
    }
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        if (function == functions[channel]) {
            return dspic33_spi_frame_output(cpu, channel, high);
        }
    }
    return false;
}

bool dspic33_dma_request(Dspic33* cpu, uint8_t request, uint16_t indirect_address,
                         uint64_t delay) {
    uint8_t channel;
    bool succeeded = true;
    for (channel = 0u; channel < DSPIC33_DMA_COUNT; channel++) {
        uint16_t base = dma_channel_base(channel);
        uint16_t bit = dma_channel_bit(channel);
        if ((raw_word(cpu, base) & DMA_CON_CHEN) == 0u ||
            (raw_word(cpu, (uint16_t)(base + 2u)) & DMA_REQ_SOURCE_MASK) != request ||
            (raw_word(cpu, DMA_PWC) & bit) != 0u) {
            continue;
        }
        if ((cpu->io.dma_peripheral_pending & bit) != 0u) {
            continue;
        }
        if ((cpu->io.dma_forced_pending & bit) != 0u) {
            dma_request_collision(cpu, channel);
        }
        if (!schedule_dma_channel(cpu, channel, indirect_address, false, delay)) {
            succeeded = false;
        }
    }
    return succeeded;
}

bool dspic33_pmp_transmit(Dspic33* cpu, Dspic33PmpTransfer* transfer) {
    return transfer != NULL && pmp_output_pop(&cpu->io.pmp.output, transfer);
}

bool dspic33_pmp_respond(Dspic33* cpu, uint16_t value, uint64_t delay) {
    Dspic33PmpResponse response;
    if (delay > UINT64_MAX - cpu->device_cycles) {
        return false;
    }
    response.cycle = cpu->device_cycles + delay;
    response.value = value;
    return pmp_response_push(&cpu->io.pmp.input, &response);
}

bool dspic33_pmp_slave_read(Dspic33* cpu, uint8_t address, uint64_t delay) {
    return address < 4u && dspic33_schedule(cpu, DSPIC33_EVENT_PMP,
                                            PMP_EVENT_SLAVE_READ, address, delay);
}

bool dspic33_pmp_slave_write(Dspic33* cpu, uint8_t address, uint8_t value,
                             uint64_t delay) {
    return address < 4u &&
           dspic33_schedule(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_SLAVE_WRITE,
                            ((uint32_t)address << 8u) | value, delay);
}

bool dspic33_input_capture_input(Dspic33* cpu, uint8_t channel, bool high,
                                 uint64_t delay) {
    return channel < DSPIC33_INPUT_CAPTURE_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_INPUT_CAPTURE, channel,
                            INPUT_CAPTURE_EVENT_INPUT |
                                (high ? INPUT_CAPTURE_EVENT_HIGH : 0u),
                            delay);
}

bool dspic33_input_capture_pin(Dspic33* cpu, uint8_t pin, bool high, uint64_t delay) {
    return pps_pin(pin) != NULL &&
           dspic33_schedule(
               cpu, DSPIC33_EVENT_INPUT_CAPTURE, pin,
               INPUT_CAPTURE_EVENT_PIN | (high ? INPUT_CAPTURE_EVENT_HIGH : 0u), delay);
}

bool dspic33_output_compare_output(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t control2;
    uint16_t bit;
    if (channel >= DSPIC33_OUTPUT_COMPARE_COUNT || high == NULL ||
        !output_compare_supported(cpu, channel) ||
        output_compare_pmd_disabled(cpu, channel) ||
        (output_compare_cascade_requested(cpu, channel) &&
         (output_compare_pmd_disabled(cpu, output_compare_pair_low(channel)) ||
          output_compare_pmd_disabled(cpu, output_compare_pair_high(channel)))) ||
        (output_compare_cascade_requested(cpu, channel) &&
         channel == output_compare_pair_low(channel))) {
        return false;
    }
    control2 = raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
    bit = (uint16_t)(1u << channel);
    if ((cpu->io.output_compare.fault_held & bit) != 0u) {
        *high = (control2 & OUTPUT_COMPARE_FAULT_OUTPUT) != 0u;
    } else {
        *high = output_compare_high(cpu, channel) !=
                ((control2 & OUTPUT_COMPARE_INVERT) != 0u);
    }
    return true;
}

bool dspic33_output_compare_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    uint8_t channel;
    uint16_t control2;
    uint16_t bit;
    if (high == NULL || !output_compare_pin_channel(cpu, pin, &channel)) {
        return false;
    }
    control2 = raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
    bit = (uint16_t)(1u << channel);
    if ((control2 & OUTPUT_COMPARE_TRISTATE) != 0u ||
        ((cpu->io.output_compare.fault_held & bit) != 0u &&
         (control2 & OUTPUT_COMPARE_FAULT_TRISTATE) != 0u)) {
        return false;
    }
    return dspic33_output_compare_output(cpu, channel, high);
}

bool dspic33_output_compare_fault(Dspic33* cpu, uint8_t source, bool high,
                                  uint64_t delay) {
    return source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_OUTPUT_COMPARE_FAULT, source,
                            high ? OUTPUT_COMPARE_FAULT_EVENT_HIGH : 0u, delay);
}

bool dspic33_output_compare_fault_pin(Dspic33* cpu, uint8_t pin, bool high,
                                      uint64_t delay) {
    return pps_pin(pin) != NULL &&
           dspic33_schedule(cpu, DSPIC33_EVENT_OUTPUT_COMPARE_FAULT, pin,
                            OUTPUT_COMPARE_FAULT_EVENT_PIN |
                                (high ? OUTPUT_COMPARE_FAULT_EVENT_HIGH : 0u),
                            delay);
}

bool dspic33_comparator_input(Dspic33* cpu, uint8_t comparator,
                              Dspic33ComparatorInput input, uint16_t level,
                              uint64_t delay) {
    uint16_t source;
    if (comparator >= DSPIC33_COMPARATOR_COUNT ||
        input >= DSPIC33_COMPARATOR_INPUT_COUNT) {
        return false;
    }
    source = (uint16_t)(comparator * DSPIC33_COMPARATOR_INPUT_COUNT + input);
    return dspic33_schedule(cpu, DSPIC33_EVENT_COMPARATOR, source, level, delay);
}

bool dspic33_comparator_reference(Dspic33* cpu, Dspic33ComparatorReference reference,
                                  uint16_t level, uint64_t delay) {
    return reference < DSPIC33_COMPARATOR_REFERENCE_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_COMPARATOR,
                            (uint16_t)(COMPARATOR_EVENT_REFERENCE_FIRST + reference),
                            level, delay);
}

bool dspic33_comparator_output(const Dspic33* cpu, uint8_t comparator, bool* high) {
    if (comparator >= DSPIC33_COMPARATOR_COUNT || high == NULL ||
        cpu->io.comparator.pmd_disabled ||
        !comparator_configuration_supported(cpu, comparator)) {
        return false;
    }
    *high = (cpu->io.comparator.output_high & (uint8_t)(1u << comparator)) != 0u;
    return true;
}

bool dspic33_comparator_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    uint8_t comparator;
    if (high == NULL || !comparator_pin_channel(cpu, pin, &comparator) ||
        (raw_word(cpu, comparator_base(comparator)) & COMPARATOR_OUTPUT_ENABLE) == 0u) {
        return false;
    }
    return dspic33_comparator_output(cpu, comparator, high);
}

bool dspic33_rtcc_clock(Dspic33* cpu, uint32_t edges, uint64_t delay) {
    return edges != 0u && dspic33_schedule(cpu, DSPIC33_EVENT_RTCC, 0u, edges, delay);
}

bool dspic33_rtcc_output(const Dspic33* cpu, bool* high) {
    uint16_t control = raw_word(cpu, RTCC_CONTROL);
    if (high == NULL || cpu->io.rtcc.pmd_disabled ||
        (control & RTCC_OUTPUT_ENABLE) == 0u) {
        return false;
    }
    if ((raw_word(cpu, RTCC_PAD_CONTROL) & RTCC_SECONDS_OUTPUT) != 0u) {
        *high = (control & RTCC_HALF_SECOND) != 0u;
    } else {
        *high = cpu->io.rtcc.alarm_output;
    }
    return true;
}

bool dspic33_qei_input(Dspic33* cpu, uint8_t channel, Dspic33QeiInput input, bool high,
                       uint64_t delay) {
    return channel < DSPIC33_QEI_COUNT && input <= DSPIC33_QEI_HOME &&
           dspic33_schedule(cpu, DSPIC33_EVENT_QEI,
                            (uint16_t)(channel * 4u + (uint8_t)input), high ? 1u : 0u,
                            delay);
}

bool dspic33_qei_compare_output(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t io_control;
    uint8_t mode;
    int32_t position;
    int32_t greater_equal;
    int32_t less_equal;
    if (channel >= DSPIC33_QEI_COUNT || high == NULL ||
        cpu->io.qei.pmd_disabled[channel]) {
        return false;
    }
    io_control = raw_word(cpu, (uint16_t)(qei_bases[channel] + 2u));
    mode = (uint8_t)((io_control & QEI_IO_OUTPUT_MASK) >> QEI_IO_OUTPUT_SHIFT);
    position = (int32_t)qei_read_counter(cpu, channel, QEI_POSITION_LOW);
    greater_equal = (int32_t)qei_read_counter(cpu, channel, QEI_GREATER_EQUAL_LOW);
    less_equal = (int32_t)qei_read_counter(cpu, channel, QEI_LESS_EQUAL_LOW);
    *high = (mode == 1u && position >= greater_equal) ||
            (mode == 2u && position <= less_equal) ||
            (mode == 3u && (position >= greater_equal || position <= less_equal));
    return true;
}

void dspic33_dci_input(Dspic33* cpu, uint16_t value) { cpu->io.dci.input = value; }

bool dspic33_dci_clock(Dspic33* cpu, uint16_t value, bool frame_sync, uint64_t delay) {
    return dspic33_schedule(cpu, DSPIC33_EVENT_DCI,
                            frame_sync ? DCI_EVENT_EXTERNAL_FRAME : DCI_EVENT_EXTERNAL,
                            value, delay);
}

bool dspic33_dci_transmit(Dspic33* cpu, Dspic33DciTransfer* transfer) {
    return transfer != NULL && dci_output_pop(&cpu->io.dci.output, transfer);
}

bool dspic33_dci_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    uint8_t function = 0u;
    uint16_t control;
    size_t index;
    if (high == NULL) {
        return false;
    }
    for (index = 0u; index < sizeof(pps_outputs) / sizeof(pps_outputs[0]); index++) {
        if (pps_outputs[index].pin == pin) {
            function = (uint8_t)((raw_word(cpu, pps_outputs[index].address) >>
                                  pps_outputs[index].shift) &
                                 0x003fu);
            break;
        }
    }
    control = raw_word(cpu, DCI_CONTROL1);
    if (index == sizeof(pps_outputs) / sizeof(pps_outputs[0]) ||
        cpu->io.dci.pmd_disabled) {
        return false;
    }
    if (function == DCI_PPS_CLOCK_OUTPUT &&
        (control & DCI_CONTROL_EXTERNAL_CLOCK) == 0u &&
        raw_word(cpu, DCI_CONTROL3) != 0u) {
        return dci_internal_clock_high(cpu, high);
    }
    if (!dci_configuration_supported(cpu) ||
        ((control & DCI_CONTROL_ENABLE) == 0u && !cpu->io.dci.disable_pending)) {
        return false;
    }
    if (function == DCI_PPS_DATA_OUTPUT) {
        return dci_data_output(cpu, high);
    }
    if (function == DCI_PPS_FRAME_OUTPUT &&
        (control & DCI_CONTROL_EXTERNAL_FRAME) == 0u) {
        return dci_frame_output(cpu, high);
    }
    return false;
}

bool dspic33_timer_pulse(Dspic33* cpu, uint8_t timer, uint32_t pulses, uint64_t delay) {
    return timer < DSPIC33_TIMER_COUNT && pulses != 0u &&
           dspic33_schedule(cpu, DSPIC33_EVENT_TIMER, timer, pulses, delay);
}

bool dspic33_timer_gate(Dspic33* cpu, uint8_t timer, bool high, uint64_t delay) {
    return timer < DSPIC33_TIMER_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_TIMER_GATE, timer, high ? 1u : 0u,
                            delay);
}

bool dspic33_adc_trigger(Dspic33* cpu, uint8_t module, uint8_t source, uint64_t delay) {
    uint32_t value;
    if (module >= DSPIC33_ADC_COUNT || source == 0u || source == 6u || source == 7u ||
        source >= 15u) {
        return false;
    }
    value = source | ((uint32_t)UINT16_MAX << ADC_EVENT_GENERATION_SHIFT);
    return dspic33_schedule(cpu, DSPIC33_EVENT_ADC, module, value, delay);
}

bool dspic33_pwm_fault(Dspic33* cpu, uint8_t source, bool high, uint64_t delay) {
    return source < DSPIC33_PWM_INPUT_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_PWM_FAULT, source,
                            high ? PWM_INPUT_HIGH : 0u, delay);
}

bool dspic33_pwm_current_limit(Dspic33* cpu, uint8_t source, bool high,
                               uint64_t delay) {
    return source < DSPIC33_PWM_INPUT_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_PWM_CURRENT_LIMIT, source,
                            high ? PWM_INPUT_HIGH : 0u, delay);
}

bool dspic33_pwm_dead_time(Dspic33* cpu, uint8_t generator, bool high, uint64_t delay) {
    return generator < DSPIC33_PWM_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_PWM_DEAD_TIME, generator,
                            high ? PWM_INPUT_HIGH : 0u, delay);
}

bool dspic33_pwm_sync(Dspic33* cpu, uint8_t input, bool high, uint64_t delay) {
    return input < 2u && dspic33_schedule(cpu, DSPIC33_EVENT_PWM_SYNC, input,
                                          high ? PWM_INPUT_HIGH : 0u, delay);
}

bool dspic33_pwm_sync_output(const Dspic33* cpu, uint8_t time_base) {
    uint16_t control;
    bool active;
    if (time_base >= 2u) {
        return false;
    }
    control =
        raw_word(cpu, (uint16_t)(PWM_GLOBAL_BASE + (time_base == 0u ? 0u : 0x0eu)));
    if ((control & 0x0100u) == 0u) {
        return false;
    }
    active = cpu->device_cycles < cpu->io.pwm_sync_until[time_base];
    return (control & 0x0200u) != 0u ? !active : active;
}

bool dspic33_pwm_output(const Dspic33* cpu, uint8_t generator, bool high) {
    uint8_t output = (uint8_t)(generator * 2u + (high ? 0u : 1u));
    return generator < DSPIC33_PWM_COUNT && cpu->io.pwm[output] != 0u;
}

bool dspic33_can_receive(Dspic33* cpu, uint8_t channel, const Dspic33CanFrame* frame,
                         uint64_t delay) {
    return channel < DSPIC33_CAN_COUNT && frame->length <= 8u &&
           (!frame->extended ? frame->identifier <= 0x7ffu
                             : frame->identifier <= 0x1fffffffu) &&
           can_queue_push(&cpu->io.can_rx[channel], frame) &&
           dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_START,
                            delay);
}

bool dspic33_can_error(Dspic33* cpu, uint8_t channel, bool transmit, uint8_t count,
                       uint64_t delay) {
    uint32_t value;
    if (channel >= DSPIC33_CAN_COUNT || count == 0u) {
        return false;
    }
    value = CAN_EVENT_ERROR | ((uint32_t)count << CAN_EVENT_ERROR_COUNT_SHIFT);
    if (transmit) {
        value |= CAN_EVENT_TRANSMIT_ERROR;
    }
    return dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, value, delay);
}

bool dspic33_can_transmit(Dspic33* cpu, uint8_t channel, Dspic33CanFrame* frame) {
    return channel < DSPIC33_CAN_COUNT &&
           can_queue_pop(&cpu->io.can_tx[channel], frame);
}

static bool usb_schedule_pending(Dspic33* cpu, const Dspic33UsbPending* pending,
                                 uint64_t delay) {
    uint16_t slot;
    for (slot = 0u; slot < DSPIC33_USB_PENDING_COUNT; slot++) {
        if (!cpu->io.usb_pending[slot].active) {
            cpu->io.usb_pending[slot] = *pending;
            cpu->io.usb_pending[slot].active = true;
            if (dspic33_schedule(cpu, DSPIC33_EVENT_USB, slot, 0u, delay)) {
                return true;
            }
            cpu->io.usb_pending[slot].active = false;
            return false;
        }
    }
    return false;
}

static bool usb_schedule_token(Dspic33* cpu, uint8_t address, uint8_t endpoint,
                               uint8_t pid, const uint8_t* data, uint16_t size,
                               bool data1, Dspic33UsbHandshake handshake,
                               uint64_t delay) {
    Dspic33UsbPending pending;
    if (endpoint >= DSPIC33_USB_ENDPOINT_COUNT || size > DSPIC33_USB_PACKET_SIZE ||
        (size != 0u && data == NULL)) {
        return false;
    }
    memset(&pending, 0, sizeof(pending));
    pending.packet.address = address;
    pending.packet.endpoint = endpoint;
    pending.packet.pid = pid;
    pending.packet.size = size;
    pending.packet.data1 = data1;
    pending.packet.handshake = handshake;
    if (size != 0u) {
        memcpy(pending.packet.data, data, size);
    }
    return usb_schedule_pending(cpu, &pending, delay);
}

bool dspic33_usb_receive_toggle(Dspic33* cpu, uint8_t endpoint, const uint8_t* data,
                                uint16_t size, bool data1, uint64_t delay) {
    return usb_schedule_token(cpu, (uint8_t)(raw_word(cpu, USB_ADDR) & 0x007fu),
                              endpoint, DSPIC33_USB_PID_OUT, data, size, data1,
                              DSPIC33_USB_HANDSHAKE_NONE, delay);
}

bool dspic33_usb_receive(Dspic33* cpu, uint8_t endpoint, const uint8_t* data,
                         uint16_t size, uint64_t delay) {
    return dspic33_usb_receive_toggle(cpu, endpoint, data, size, false, delay);
}

bool dspic33_usb_setup(Dspic33* cpu, uint8_t endpoint, const uint8_t* data,
                       uint16_t size, uint64_t delay) {
    return size == 8u &&
           usb_schedule_token(cpu, (uint8_t)(raw_word(cpu, USB_ADDR) & 0x007fu),
                              endpoint, DSPIC33_USB_PID_SETUP, data, size, false,
                              DSPIC33_USB_HANDSHAKE_NONE, delay);
}

bool dspic33_usb_request(Dspic33* cpu, uint8_t endpoint, uint64_t delay) {
    return usb_schedule_token(cpu, (uint8_t)(raw_word(cpu, USB_ADDR) & 0x007fu),
                              endpoint, DSPIC33_USB_PID_IN, NULL, 0u, false,
                              DSPIC33_USB_HANDSHAKE_NONE, delay);
}

bool dspic33_usb_token(Dspic33* cpu, uint8_t address, uint8_t endpoint,
                       Dspic33UsbPid pid, const uint8_t* data, uint16_t size,
                       bool data1, uint64_t delay) {
    return address <= 0x7fu &&
           (pid == DSPIC33_USB_PID_OUT || pid == DSPIC33_USB_PID_IN ||
            pid == DSPIC33_USB_PID_SETUP) &&
           (pid != DSPIC33_USB_PID_SETUP || size == 8u) &&
           (pid != DSPIC33_USB_PID_IN || size == 0u) &&
           usb_schedule_token(cpu, address, endpoint, (uint8_t)pid, data, size, data1,
                              DSPIC33_USB_HANDSHAKE_NONE, delay);
}

bool dspic33_usb_host_response(Dspic33* cpu, Dspic33UsbHandshake handshake,
                               const uint8_t* data, uint16_t size, bool data1,
                               uint64_t delay) {
    return cpu->io.usb_host_pending &&
           usb_schedule_token(cpu, (uint8_t)(raw_word(cpu, USB_ADDR) & 0x007fu),
                              cpu->io.usb_host_endpoint, cpu->io.usb_host_pid, data,
                              size, data1, handshake, delay);
}

bool dspic33_usb_bus(Dspic33* cpu, Dspic33UsbBusEvent event, uint16_t value,
                     uint64_t delay) {
    return usb_schedule_bus_event(cpu, event, value, delay);
}

static bool usb_schedule_bus_event(Dspic33* cpu, Dspic33UsbBusEvent event,
                                   uint16_t value, uint64_t delay) {
    Dspic33UsbPending pending;
    if (event > DSPIC33_USB_BUS_OTG_STATE) {
        return false;
    }
    memset(&pending, 0, sizeof(pending));
    pending.bus_event = true;
    pending.event = event;
    pending.value = value;
    return usb_schedule_pending(cpu, &pending, delay);
}

bool dspic33_usb_transmit(Dspic33* cpu, Dspic33UsbPacket* packet) {
    return usb_queue_pop(&cpu->io.usb_tx, packet);
}

void dspic33_adc_input(Dspic33* cpu, uint8_t channel, uint16_t value) {
    if (channel < DSPIC33_ADC_CHANNEL_COUNT) {
        cpu->io.adc[channel] = (uint16_t)(value & 0x0fffu);
    }
}

bool dspic33_gpio_drive(Dspic33* cpu, uint8_t port, uint16_t value, uint16_t mask) {
    uint16_t selected;
    if (port >= DSPIC33_GPIO_PORT_COUNT) {
        return false;
    }
    selected = (uint16_t)(mask & gpio_port_masks[port]);
    cpu->io.gpio[port] =
        (uint16_t)((cpu->io.gpio[port] & ~selected) | (value & selected));
    cpu->io.gpio_driven[port] |= selected;
    refresh_gpio_change_notification(cpu);
    refresh_external_interrupts(cpu);
    output_compare_refresh_fault_pps_inputs(cpu);
    dci_refresh_pps_inputs(cpu);
    dspic33_i2c_refresh_pins(cpu);
    return true;
}

bool dspic33_gpio_release(Dspic33* cpu, uint8_t port, uint16_t mask) {
    if (port >= DSPIC33_GPIO_PORT_COUNT) {
        return false;
    }
    cpu->io.gpio_driven[port] &= (uint16_t)~(mask & gpio_port_masks[port]);
    refresh_gpio_change_notification(cpu);
    refresh_external_interrupts(cpu);
    output_compare_refresh_fault_pps_inputs(cpu);
    dci_refresh_pps_inputs(cpu);
    dspic33_i2c_refresh_pins(cpu);
    return true;
}

bool dspic33_gpio_pin(const Dspic33* cpu, uint8_t port, uint8_t bit, bool* high) {
    uint16_t mask;
    if (port >= DSPIC33_GPIO_PORT_COUNT || bit >= 16u || high == NULL) {
        return false;
    }
    mask = (uint16_t)(1u << bit);
    if ((gpio_port_masks[port] & mask) == 0u) {
        return false;
    }
    *high = (gpio_pin_values(cpu, port) & mask) != 0u;
    return true;
}

bool dspic33_device_gpio_input_high(const Dspic33* cpu, uint8_t port, uint8_t bit,
                                    bool* high) {
    uint16_t driven;
    uint16_t mask;
    uint16_t pull_down;
    uint16_t pull_up;
    if (port >= DSPIC33_GPIO_PORT_COUNT || bit >= 16u || high == NULL) {
        return false;
    }
    mask = (uint16_t)(1u << bit);
    if ((gpio_port_masks[port] & mask) == 0u) {
        return false;
    }
    driven = cpu->io.gpio_driven[port];
    pull_up = raw_word(cpu, gpio_pull_up_addresses[port]);
    pull_down = raw_word(cpu, gpio_pull_down_addresses[port]);
    *high = (((cpu->io.gpio[port] & driven) | (pull_up & ~driven & ~pull_down)) &
             mask) != 0u;
    return true;
}

void dspic33_gpio_input(Dspic33* cpu, uint8_t port, uint16_t value) {
    if (port < DSPIC33_GPIO_PORT_COUNT) {
        dspic33_gpio_drive(cpu, port, value, gpio_port_masks[port]);
    }
}

void dspic33_device_reset(Dspic33* cpu) {
    uint16_t gpio[DSPIC33_GPIO_PORT_COUNT];
    uint16_t gpio_driven[DSPIC33_GPIO_PORT_COUNT];
    size_t index;
    memcpy(gpio, cpu->io.gpio, sizeof(gpio));
    memcpy(gpio_driven, cpu->io.gpio_driven, sizeof(gpio_driven));
    memset(&cpu->io, 0, sizeof(cpu->io));
    cpu->io.dci.bcg_paused = true;
    cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_AVDD] = 3300u;
    cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE] = 3300u;
    memcpy(cpu->io.gpio, gpio, sizeof(gpio));
    memcpy(cpu->io.gpio_driven, gpio_driven, sizeof(gpio_driven));
    memcpy(cpu->io.qei.filtered_inputs, cpu->qei_inputs, sizeof(cpu->qei_inputs));
    memcpy(cpu->io.qei.logical_inputs, cpu->qei_inputs, sizeof(cpu->qei_inputs));
    cpu->io.uart_cts = (uint8_t)((1u << DSPIC33_UART_COUNT) - 1u);
    cpu->io.input_capture.sync_output_high = 0x00ffu;
    cpu->io.output_compare.fault_inputs = 0u;
    memset(cpu->interrupt_deferred, 0, sizeof(cpu->interrupt_deferred));
    memset(cpu->interrupt_deferred_next, 0, sizeof(cpu->interrupt_deferred_next));
    cpu->gie_disable_deferred = 0u;
    cpu->gie_disable_deferred_next = 0u;
    for (index = 0u; index < sizeof(reset_values) / sizeof(reset_values[0]); index++) {
        raw_write_word(cpu, reset_values[index].address, reset_values[index].value);
    }
    dspic33_i2c_reset(cpu);
    usb_reset_registers(cpu);
    raw_write_word(cpu, USB_PWRC, 0u);
    reset_main_oscillator(cpu);
    raw_write_word(cpu, 0x08c2u, 0x8000u);
    raw_write_word(cpu, 0x08c8u, 0u);
    raw_write_word(cpu, DMA_LCA, 0x000fu);
    dspic33_device_reset_restored(cpu);
}
