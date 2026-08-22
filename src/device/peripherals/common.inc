
static void complete_oscillator_event(Dspic33* cpu, uint16_t phase, uint32_t generation);
static void oscillator_configuration_changed(Dspic33* cpu, uint8_t previous);
static void oscillator_startup_configuration_changed(Dspic33* cpu, uint8_t previous);
static void oscillator_pll_configuration_changed(Dspic33* cpu, uint8_t previous);
static void start_automatic_oscillator_switch(Dspic33* cpu, uint8_t source);
static void reset_main_oscillator(Dspic33* cpu);
static void refresh_gpio_change_notification(Dspic33* cpu);
static void refresh_external_interrupts(Dspic33* cpu);
static void refresh_timer_inputs(Dspic33* cpu);
static void refresh_input_capture_pps_inputs(Dspic33* cpu);
static void refresh_qei_pps_inputs(Dspic33* cpu);
static void run_timer_pmd(Dspic33* cpu, uint16_t timer, uint32_t value);
static void run_adc_pmd(Dspic33* cpu, uint16_t module, uint32_t value);
static void adc_update_power_state(Dspic33* cpu);
static void adc_begin_sampling(Dspic33* cpu, uint8_t module);
static void run_pwm_pmd(Dspic33* cpu, uint16_t source, uint32_t value);
static void refresh_pwm_inputs(Dspic33* cpu);
static void refresh_pwm_pins(Dspic33* cpu);
static void usb_update_power_state(Dspic33* cpu);
static bool interrupt_enabled(const Dspic33* cpu, uint16_t irq);
static void output_compare_pulse_source(Dspic33* cpu, uint8_t source);
static void output_compare_update_power_state(Dspic33* cpu);
static void output_compare_raise(Dspic33* cpu, uint8_t channel);
static void output_compare_refresh_fault_pps_inputs(Dspic33* cpu);
static void dci_discard_internal_events(Dspic33* cpu);
static void dci_update_power_state(Dspic33* cpu);
static void dma_update_power_state(Dspic33* cpu);
static void dci_refresh_pps_inputs(Dspic33* cpu);
static void spi_refresh_pps_inputs(Dspic33* cpu);
static void uart_refresh_pps_inputs(Dspic33* cpu);
static void uart_update_power_state(Dspic33* cpu);
static void refresh_can_pps_inputs(Dspic33* cpu);
static void can_invalid_event(Dspic33* cpu, uint8_t channel);
static void spi_update_power_state(Dspic33* cpu);
static void comparator_update_filter_power(Dspic33* cpu);
static void comparator_evaluate_all(Dspic33* cpu);
static uint64_t spi_transfer_cycles(const Dspic33* cpu, uint8_t channel);
static uint8_t dci_pps_selection(const Dspic33* cpu, uint16_t address, uint8_t shift);
static bool dci_pps_input_high(const Dspic33* cpu, uint8_t selection);
static uint16_t gpio_pin_values(const Dspic33* cpu, uint8_t port);
static bool pwm_pin_value(const Dspic33* cpu, uint8_t port, uint8_t pin, bool* high);
static void refresh_physical_pin_inputs(Dspic33* cpu);
static void apply_physical_pin_level(Dspic33* cpu, uint8_t pin, bool high);
static void uart_receive_complete(Dspic33* cpu, uint8_t channel, const Dspic33UartFrame* incoming);

static const uint16_t timer_registers[DSPIC33_TIMER_COUNT] = {
    0x0100u, 0x0106u, 0x010au, 0x0114u, 0x0118u, 0x0122u, 0x0126u, 0x0130u, 0x0134u};
static const uint16_t timer_periods[DSPIC33_TIMER_COUNT] = {
    0x0102u, 0x010cu, 0x010eu, 0x011au, 0x011cu, 0x0128u, 0x012au, 0x0136u, 0x0138u};
static const uint16_t timer_controls[DSPIC33_TIMER_COUNT] = {
    0x0104u, 0x0110u, 0x0112u, 0x011eu, 0x0120u, 0x012cu, 0x012eu, 0x013au, 0x013cu};
static const uint16_t timer_holding_registers[4] = {0x0108u, 0x0116u, 0x0124u, 0x0132u};
static const uint8_t timer_irqs[DSPIC33_TIMER_COUNT] = {3u, 7u, 8u, 27u, 28u, 47u, 48u, 51u, 52u};
static const uint8_t dma_irqs[DSPIC33_DMA_COUNT] = {4u,   14u,  24u,  36u,  46u,  61u,  68u, 69u,
                                                    118u, 119u, 120u, 121u, 130u, 131u, 132u};
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
static const uint16_t uart_bases[DSPIC33_UART_COUNT] = {0x0220u, 0x0230u, 0x0250u, 0x02b0u};
static const uint16_t uart_pps_registers[DSPIC33_UART_COUNT] = {0x06c4u, 0x06c6u, 0x06d6u, 0x06d8u};
static const uint8_t uart_tx_functions[DSPIC33_UART_COUNT] = {1u, 3u, 27u, 29u};
static const uint8_t uart_rts_functions[DSPIC33_UART_COUNT] = {2u, 4u, 28u, 30u};
static const uint16_t spi_bases[DSPIC33_SPI_COUNT] = {0x0240u, 0x0260u, 0x02a0u, 0x02c0u};
static const uint16_t adc_buffers[DSPIC33_ADC_COUNT] = {0x0300u, 0x0340u};
static const uint16_t adc_controls[DSPIC33_ADC_COUNT] = {0x0320u, 0x0360u};
static const uint8_t adc_irqs[DSPIC33_ADC_COUNT] = {13u, 21u};
static const uint8_t input_capture_irqs[DSPIC33_INPUT_CAPTURE_COUNT] = {
    1u, 5u, 37u, 38u, 39u, 40u, 22u, 23u, 93u, 125u, 127u, 129u, 135u, 137u, 139u, 141u};
static const uint16_t input_capture_pps_registers[DSPIC33_INPUT_CAPTURE_COUNT / 2u] = {
    0x06aeu, 0x06b0u, 0x06b2u, 0x06b4u, 0x06e2u, 0x06e4u, 0x06e6u, 0x06e8u};
static const uint8_t output_compare_irqs[DSPIC33_OUTPUT_COMPARE_COUNT] = {
    2u, 6u, 25u, 26u, 41u, 42u, 43u, 44u, 92u, 124u, 126u, 128u, 134u, 136u, 138u, 140u};
static const uint8_t external_interrupt_irqs[DSPIC33_EXTERNAL_INTERRUPT_COUNT] = {0u, 20u, 29u, 53u,
                                                                                  54u};
static const uint8_t pwm_irqs[DSPIC33_PWM_MAX_COUNT] = {94u, 95u, 96u, 97u, 98u, 99u, 100u};

static uint8_t pwm_generator_count(const Dspic33* cpu) {
    const Dspic33epMuProfile* profile = dspic33_device_profile(cpu);
    return profile == NULL ? 0u : profile->pwm_generator_count;
}
static const uint16_t qei_bases[DSPIC33_QEI_COUNT] = {0x01c0u, 0x05c0u};
static const uint8_t qei_irqs[DSPIC33_QEI_COUNT] = {58u, 75u};
static const uint16_t qei_pps_registers[DSPIC33_QEI_COUNT][2] = {{0x06bcu, 0x06beu},
                                                                 {0x06c0u, 0x06c2u}};
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
static const uint16_t gpio_analog_masks[DSPIC33_GPIO_PORT_COUNT] = {
    0x06c0u, 0xffffu, 0x601eu, 0x00c0u, 0x03ffu, 0u, 0x03c0u};
static const uint16_t gpio_pull_up_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e0au, 0x0e1au, 0x0e2au, 0x0e3au, 0x0e4au, 0x0e5au, 0x0e6au};
static const uint16_t gpio_pull_down_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e0cu, 0x0e1cu, 0x0e2cu, 0x0e3cu, 0x0e4cu, 0x0e5cu, 0x0e6cu};
static const uint16_t gpio_port_masks[DSPIC33_GPIO_PORT_COUNT] = {
    0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x317fu, 0xf3cfu};
static const uint16_t gpio_input_only_masks[DSPIC33_GPIO_PORT_COUNT] = {0u, 0u, 0u,     0u,
                                                                        0u, 0u, 0x000cu};

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
    TIMER_EVENT_PMD_DISABLED = 0x00000001u,
    TIMER_EVENT_PMD_GENERATION_SHIFT = 1u,
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
    COMPARATOR_EVENT_INPUT_COUNT = DSPIC33_COMPARATOR_COUNT * DSPIC33_COMPARATOR_INPUT_COUNT,
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
    UART_MODE_STOP_IDLE = 0x2000u,
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
    UART_EVENT_PIN = 0xc0000000u,
    UART_EVENT_PIN_TRANSMIT = 0x20000000u,
    UART_EVENT_PIN_PHASE_SHIFT = 16u,
    UART_EVENT_PIN_PHASE_MASK = 0x0fff0000u,
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
    SPI_DISABLE_OUTPUT = 0x0800u,
    SPI_MODE_16 = 0x0400u,
    SPI_SAMPLE_END = 0x0200u,
    SPI_CLOCK_EDGE = 0x0100u,
    SPI_SLAVE_SELECT = 0x0080u,
    SPI_CLOCK_POLARITY = 0x0040u,
    SPI_MASTER = 0x0020u,
    SPI_FRAME_ENABLE = 0x8000u,
    SPI_FRAME_SLAVE = 0x4000u,
    SPI_FRAME_ACTIVE_HIGH = 0x2000u,
    SPI_FRAME_DELAY = 0x0002u,
    SPI_ENHANCED = 0x0001u,
    SPI_EVENT_EXTERNAL = 0x00010000u,
    SPI_EVENT_INTERNAL = 0x00020000u,
    SPI_EVENT_FRAME = 0x00030000u,
    SPI_EVENT_SAMPLE = 0x00040000u,
    SPI_EVENT_FRAME_INPUT = 0x00050000u,
    SPI_EVENT_FRAME_START = 0x00060000u,
    SPI_EVENT_FRAME_CLEAR = 0x00070000u,
    SPI_EVENT_KIND_MASK = 0x00070000u,
    SPI_EVENT_FRAME_DATA = 0x00000001u,
    SPI_EVENT_GENERATION_SHIFT = 19u,
    SPI_EVENT_GENERATION_MASK = 0x1fffu,
    SPI_SELECT_ACTIVE = 0x00000001u,
    CAN_WINDOW = 0x0001u,
    CAN_CAPTURE = 0x0008u,
    CAN_WAKE_FILTER = 0x4000u,
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
    CAN_EVENT_TRANSMIT_BUS_FINISH = 8u,
    CAN_EVENT_KIND_MASK = 0x000000ffu,
    CAN_EVENT_CAPTURE_RELEASE = 9u,
    CAN_EVENT_INVALID = 10u,
    CAN_EVENT_RECEIVE_PIN = 11u,
    CAN_EVENT_RECEIVE_SAMPLE = 12u,
    CAN_EVENT_ACK_START = 13u,
    CAN_EVENT_ACK_FINISH = 14u,
    CAN_EVENT_TRANSMIT_RETRY = 15u,
    CAN_EVENT_TRANSMIT_ERROR_START = 16u,
    CAN_EVENT_TRANSMIT_SAMPLE = 17u,
    CAN_EVENT_RECEIVE_ERROR_START = 18u,
    CAN_EVENT_RECEIVE_ERROR_FINISH = 19u,
    CAN_EVENT_MODE_TRANSITION = 20u,
    CAN_EVENT_RECEIVE_SUCCESS = 21u,
    CAN_EVENT_RECEIVE_SAMPLE_FIRST = 22u,
    CAN_EVENT_RECEIVE_SAMPLE_SECOND = 23u,
    CAN_EVENT_TRANSMIT_SAMPLE_FIRST = 24u,
    CAN_EVENT_TRANSMIT_SAMPLE_SECOND = 25u,
    CAN_EVENT_INTERMISSION_FINISH = 26u,
    CAN_EVENT_OVERLOAD_FINISH = 27u,
    CAN_EVENT_TRANSMIT_ERROR = 0x00000100u,
    CAN_EVENT_PIN_HIGH = 0x00000200u,
    CAN_EVENT_MODE_SHIFT = 8u,
    CAN_EVENT_MODE_GENERATION_SHIFT = 11u,
    CAN_EVENT_ERROR_COUNT_SHIFT = 16u,
    CAN_EVENT_GENERATION_SHIFT = 16u,
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
    USB_PMD_ADDRESS = 0x0766u,
    USB_PMD = 0x0001u,
    USB_PMD_EVENT_DISABLED = 0x00000001u,
    USB_PMD_EVENT_GENERATION_SHIFT = 1u,
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
    ADC_PMD_EVENT_DISABLED = 0x00000001u,
    ADC_PMD_EVENT_GENERATION_SHIFT = 1u,
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
    PWM_SYNCHRONIZED_IO = PWM_OVERRIDE_HIGH | PWM_OVERRIDE_LOW | PWM_OVERRIDE_DATA | PWM_SWAP,
    PWM_FAULT_MODE_MASK = 0x0003u,
    PWM_FAULT_CYCLE = 0x0001u,
    PWM_FAULT_DISABLED = 0x0003u,
    PWM_CURRENT_LIMIT_MODE = 0x0100u,
    PWM_INPUT_HIGH = 0x00000001u,
    PWM_INPUT_KIND = 0x00000002u,
    PWM_PMD_EVENT_DISABLED = 0x00000001u,
    PWM_PMD_EVENT_GENERATION_SHIFT = 1u,
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

static bool usb_schedule_bus_event(Dspic33* cpu, Dspic33UsbBusEvent event, uint16_t value,
                                   uint64_t delay, bool external);

static const Dspic33RegisterMask register_masks[] = {
    {0x0046u, 0xcfffu}, {0x0048u, 0xfffeu}, {0x004au, 0xfffeu}, {0x004cu, 0xfffeu},
    {0x004eu, 0xfffeu}, {0x0050u, 0xffffu}, {0x0104u, 0xa076u}, {0x0110u, 0xa07au},
    {0x0112u, 0xa072u}, {0x011eu, 0xa07au}, {0x0120u, 0xa072u}, {0x012cu, 0xa07au},
    {0x012eu, 0xa072u}, {0x013au, 0xa07au}, {0x013cu, 0xa072u}, {0x01c0u, 0xbf7fu},
    {0x01c2u, 0xfff0u}, {0x01c4u, 0x3fffu}, {0x0280u, 0xafe3u}, {0x0282u, 0x0defu},
    {0x0284u, 0x0fffu}, {0x0286u, 0x0000u}, {0x0288u, 0xffffu}, {0x028cu, 0xffffu},
    {0x0290u, 0x0000u}, {0x0292u, 0x0000u}, {0x0294u, 0x0000u}, {0x0296u, 0x0000u},
    {0x0298u, 0xffffu}, {0x029au, 0xffffu}, {0x029cu, 0xffffu}, {0x029eu, 0xffffu},
    {0x0402u, 0x001fu}, {0x0502u, 0x001fu}, {0x05c0u, 0xbf7fu}, {0x05c2u, 0xfff0u},
    {0x05c4u, 0x3fffu}, {0x0600u, 0xbfffu}, {0x0602u, 0x7fffu}, {0x060eu, 0x4040u},
    {0x0620u, 0xffffu}, {0x0622u, 0xffffu}, {0x0624u, 0xffffu}, {0x0626u, 0xa7ffu},
    {0x0640u, 0xa038u}, {0x0642u, 0x1f1fu}, {0x0644u, 0xfffeu}, {0x0646u, 0xffffu},
    {0x0648u, 0xffffu}, {0x064au, 0xffffu}, {0x064cu, 0xffffu}, {0x064eu, 0xffffu},
    {0x0680u, 0x3f3fu}, {0x0682u, 0x3f3fu}, {0x0684u, 0x3f3fu}, {0x0686u, 0x3f3fu},
    {0x0688u, 0x3f3fu}, {0x068au, 0x3f3fu}, {0x068cu, 0x3f3fu}, {0x068eu, 0x3f3fu},
    {0x0690u, 0x3f3fu}, {0x0692u, 0x3f3fu}, {0x0696u, 0x3f3fu}, {0x0698u, 0x3f3fu},
    {0x069au, 0x3f3fu}, {0x069cu, 0x3f3fu}, {0x069eu, 0x3f3fu}, {0x06a0u, 0x7f00u},
    {0x06a2u, 0x7f7fu}, {0x06a4u, 0x7f7fu}, {0x06a6u, 0x7f7fu}, {0x06a8u, 0x7f7fu},
    {0x06aau, 0x7f7fu}, {0x06acu, 0x7f7fu}, {0x06aeu, 0x7f7fu}, {0x06b0u, 0x7f7fu},
    {0x06b2u, 0x7f7fu}, {0x06b4u, 0x7f7fu}, {0x06b6u, 0x7f7fu}, {0x06b8u, 0x7f7fu},
    {0x06bau, 0x7f7fu}, {0x06bcu, 0x7f7fu}, {0x06beu, 0x7f7fu}, {0x06c0u, 0x7f7fu},
    {0x06c2u, 0x7f7fu}, {0x06c4u, 0x7f7fu}, {0x06c6u, 0x7f7fu}, {0x06c8u, 0x7f7fu},
    {0x06cau, 0x007fu}, {0x06ceu, 0x007fu}, {0x06d0u, 0x7f7fu}, {0x06d2u, 0x007fu},
    {0x06d4u, 0x7f7fu}, {0x06d6u, 0x7f7fu}, {0x06d8u, 0x7f7fu}, {0x06dau, 0x7f7fu},
    {0x06dcu, 0x007fu}, {0x06deu, 0x7f7fu}, {0x06e0u, 0x007fu}, {0x06e2u, 0x7f7fu},
    {0x06e4u, 0x7f7fu}, {0x06e6u, 0x7f7fu}, {0x06e8u, 0x7f7fu}, {0x06eau, 0x7f7fu},
    {0x06ecu, 0x7f7fu}, {0x06eeu, 0x7f7fu}, {0x06f0u, 0x7f7fu}, {0x06f2u, 0x007fu},
    {0x06f4u, 0x7f7fu}, {0x06f6u, 0x007fu}, {0x0728u, 0x700fu}, {0x072cu, 0x00ffu},
    {0x072eu, 0x00ffu}, {0x0740u, 0xcbffu}, {0x0744u, 0xffdfu}, {0x0746u, 0x01ffu},
    {0x0748u, 0x003fu}, {0x074eu, 0xbf00u}, {0x075au, 0x0007u}, {0x0760u, 0xffffu},
    {0x0762u, 0xffffu}, {0x0764u, 0xf7abu}, {0x0766u, 0x0021u}, {0x0768u, 0xffffu},
    {0x076au, 0x3f03u}, {0x076cu, 0x00f0u}, {0x0800u, 0xffffu}, {0x0802u, 0xffffu},
    {0x0804u, 0xffffu}, {0x0806u, 0x7fffu}, {0x0808u, 0x0afeu}, {0x080au, 0xdfeeu},
    {0x080cu, 0xc3efu}, {0x080eu, 0xffc0u}, {0x0810u, 0x7fdfu}, {0x0820u, 0xffffu},
    {0x0822u, 0xffffu}, {0x0824u, 0xffffu}, {0x0826u, 0x7fffu}, {0x0828u, 0x0afeu},
    {0x082au, 0xdfeeu}, {0x082cu, 0xffefu}, {0x082eu, 0xffc0u}, {0x0830u, 0x7fdfu},
    {0x0840u, 0x7777u}, {0x0842u, 0x7777u}, {0x0844u, 0x7777u}, {0x0846u, 0x7777u},
    {0x0848u, 0x7777u}, {0x084au, 0x7777u}, {0x084cu, 0x7777u}, {0x084eu, 0x7777u},
    {0x0850u, 0x7777u}, {0x0852u, 0x7777u}, {0x0854u, 0x7777u}, {0x0856u, 0x7777u},
    {0x0858u, 0x7777u}, {0x085au, 0x7777u}, {0x085cu, 0x7777u}, {0x085eu, 0x0777u},
    {0x0860u, 0x7770u}, {0x0862u, 0x7777u}, {0x0864u, 0x7070u}, {0x0868u, 0x7770u},
    {0x086au, 0x7700u}, {0x086cu, 0x7777u}, {0x086eu, 0x7777u}, {0x0870u, 0x7777u},
    {0x087au, 0x7700u}, {0x087cu, 0x7777u}, {0x087eu, 0x7777u}, {0x0880u, 0x7777u},
    {0x0882u, 0x7707u}, {0x0884u, 0x7777u}, {0x0886u, 0x0777u}, {0x0e00u, 0xc6ffu},
    {0x0e04u, 0xc6ffu}, {0x0e06u, 0xc03fu}, {0x0e08u, 0xc6ffu}, {0x0e0au, 0xc6ffu},
    {0x0e0cu, 0xc6ffu}, {0x0e0eu, 0x06c0u}, {0x0e10u, 0xffffu}, {0x0e14u, 0xffffu},
    {0x0e18u, 0xffffu}, {0x0e1au, 0xffffu}, {0x0e1cu, 0xffffu}, {0x0e1eu, 0xffffu},
    {0x0e20u, 0xf01eu}, {0x0e24u, 0xf01eu}, {0x0e28u, 0xf01eu}, {0x0e2au, 0xf01eu},
    {0x0e2cu, 0xf01eu}, {0x0e2eu, 0x601eu}, {0x0e30u, 0xffffu}, {0x0e34u, 0xffffu},
    {0x0e36u, 0xff3fu}, {0x0e38u, 0xffffu}, {0x0e3au, 0xffffu}, {0x0e3cu, 0xffffu},
    {0x0e3eu, 0x00c0u}, {0x0e40u, 0x03ffu}, {0x0e44u, 0x03ffu}, {0x0e48u, 0x03ffu},
    {0x0e4au, 0x03ffu}, {0x0e4cu, 0x03ffu}, {0x0e4eu, 0x03ffu}, {0x0e50u, 0x317fu},
    {0x0e54u, 0x317fu}, {0x0e56u, 0x317fu}, {0x0e58u, 0x317fu}, {0x0e5au, 0x317fu},
    {0x0e5cu, 0x317fu}, {0x0e60u, 0xf3c3u}, {0x0e64u, 0xf3c3u}, {0x0e66u, 0xf003u},
    {0x0e68u, 0xf3cfu}, {0x0e6au, 0xf3c3u}, {0x0e6cu, 0xf3c3u}, {0x0e6eu, 0x03c0u},
    {0x0efeu, 0x0003u}, {0x0f82u, 0x00ffu}, {0x0f8cu, 0x00ffu}, {0x0fa4u, 0x001fu}};

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
    {0x0e40u, 0x03ffu}, {0x0e4eu, 0x03ffu}, {0x0e50u, 0x317fu}, {0x0e60u, 0xf3c3u},
    {0x0e6eu, 0x03c0u}};

static uint16_t raw_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[(uint16_t)(address + 1u)] << 8u));
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
    return address >= 0x0680u && address <= 0x06f6u && register_write_mask(address, writable);
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
        address >= INPUT_CAPTURE_BASE + DSPIC33_INPUT_CAPTURE_COUNT * INPUT_CAPTURE_STRIDE) {
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
        address >= OUTPUT_COMPARE_BASE + DSPIC33_OUTPUT_COMPARE_COUNT * OUTPUT_COMPARE_STRIDE) {
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
            latch = (uint16_t)((raw_word(cpu, latch_address) & ~writable) | (latch & writable));
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
        if ((module == 0u && address == 0x0332u) || (module == 1u && address == 0x0372u)) {
            *writable = 0x0107u;
            return true;
        }
    }
    return false;
}

static bool uart_module_disabled(const Dspic33* cpu, uint8_t channel) {
    static const uint16_t pmd_addresses[DSPIC33_UART_COUNT] = {0x0760u, 0x0760u, 0x0764u, 0x0766u};
    static const uint16_t pmd_masks[DSPIC33_UART_COUNT] = {0x0020u, 0x0040u, 0x0008u, 0x0020u};
    return channel >= DSPIC33_UART_COUNT ||
           (raw_word(cpu, pmd_addresses[channel]) & pmd_masks[channel]) != 0u;
}

static bool uart_register_write_mask(const Dspic33* cpu, uint16_t address, uint16_t* writable) {
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

static bool can_register_write_mask(const Dspic33* cpu, uint16_t address, uint16_t* writable) {
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
            } else if (offset == 0x28u || offset == 0x2au || offset == 0x3cu || offset == 0x3eu) {
                *writable = 0u;
            } else {
                *writable = 0xffffu;
            }
        } else if (!window &&
                   (offset == 0x20u || offset == 0x22u || offset == 0x28u || offset == 0x2au)) {
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

static bool usb_register_write_mask(const Dspic33* cpu, uint16_t address, uint16_t previous,
                                    uint16_t* writable) {
    bool host = ((address == USB_CON ? previous : raw_word(cpu, USB_CON)) & USB_HOST_ENABLE) != 0u;
    if (address == USB_OTGIR || address == USB_OTGSTAT || address == USB_IR || address == USB_EIR ||
        address == USB_STAT || address == USB_FRML || address == USB_FRMH) {
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
    } else if (address >= USB_EP0 && address < USB_EP0 + DSPIC33_USB_ENDPOINT_COUNT * 2u) {
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

static bool pwm_register_write_mask(const Dspic33* cpu, uint16_t address, uint16_t* writable) {
    static const uint16_t global_masks[16] = {0xafffu, 0x0007u, 0xffffu, 0xffffu, 0x0000u, 0xffffu,
                                              0x0000u, 0x0fffu, 0x0007u, 0xffffu, 0xffffu, 0x0000u,
                                              0x0000u, 0x83ffu, 0x0000u, 0x0000u};
    static const uint16_t generator_masks[16] = {
        0x1fefu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0x3fffu, 0x3fffu, 0xffffu,
        0xffffu, 0xffffu, 0xf03fu, 0x0000u, 0x0000u, 0xfc3fu, 0x0fffu, 0x0f3fu};
    if (address >= PWM_GLOBAL_BASE && address < PWM_GENERATOR_BASE) {
        *writable = global_masks[(address - PWM_GLOBAL_BASE) / 2u];
        return true;
    }
    if (address >= PWM_GENERATOR_BASE &&
        address < PWM_GENERATOR_BASE + pwm_generator_count(cpu) * PWM_GENERATOR_STRIDE) {
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

static bool byte_queue_pop(Dspic33ByteQueue* queue, uint8_t* value) {
    if (queue->count == 0u) {
        return false;
    }
    *value = queue->bytes[queue->head];
    queue->head = (uint16_t)((queue->head + 1u) % sizeof(queue->bytes));
    queue->count--;
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
    index =
        (uint8_t)((queue->head + queue->count) % (sizeof(queue->words) / sizeof(queue->words[0])));
    queue->words[index] = value;
    queue->count++;
    return true;
}

static bool word_queue_pop(Dspic33WordQueue* queue, uint16_t* value) {
    if (queue->count == 0u) {
        return false;
    }
    *value = queue->words[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % (sizeof(queue->words) / sizeof(queue->words[0])));
    queue->count--;
    return true;
}

static bool word_queue_push_front(Dspic33WordQueue* queue, uint16_t value) {
    uint8_t capacity = (uint8_t)(sizeof(queue->words) / sizeof(queue->words[0]));
    if (queue->count == capacity) {
        return false;
    }
    queue->head = (uint8_t)((queue->head + capacity - 1u) % capacity);
    queue->words[queue->head] = value;
    queue->count++;
    return true;
}

static bool word_queue_front(const Dspic33WordQueue* queue, uint16_t* value) {
    if (queue->count == 0u) {
        return false;
    }
    *value = queue->words[queue->head];
    return true;
}
