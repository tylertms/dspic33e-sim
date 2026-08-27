#include "device/dspic33ep_mu/internal.h"

void dspic33_device_internal_complete_oscillator_event(Dspic33* cpu, uint16_t phase,
                                                       uint32_t generation);
void dspic33_device_internal_oscillator_configuration_changed(Dspic33* cpu, uint8_t previous);
void dspic33_device_internal_oscillator_startup_configuration_changed(Dspic33* cpu,
                                                                      uint8_t previous);
void dspic33_device_internal_oscillator_pll_configuration_changed(Dspic33* cpu, uint8_t previous);
void dspic33_device_internal_start_automatic_oscillator_switch(Dspic33* cpu, uint8_t source);
void dspic33_device_internal_reset_main_oscillator(Dspic33* cpu);
void dspic33_device_internal_refresh_gpio_change_notification(Dspic33* cpu);
void dspic33_device_internal_refresh_external_interrupts(Dspic33* cpu);
void dspic33_device_internal_refresh_timer_inputs(Dspic33* cpu);
void dspic33_device_internal_refresh_input_capture_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_refresh_qei_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_run_timer_pmd(Dspic33* cpu, uint16_t timer, uint32_t value);
void dspic33_device_internal_run_adc_pmd(Dspic33* cpu, uint16_t module, uint32_t value);
void dspic33_device_internal_adc_update_power_state(Dspic33* cpu);
void dspic33_device_internal_adc_begin_sampling(Dspic33* cpu, uint8_t module);
void dspic33_device_internal_run_pwm_pmd(Dspic33* cpu, uint16_t source, uint32_t value);
void dspic33_device_internal_refresh_pwm_inputs(Dspic33* cpu);
void dspic33_device_internal_refresh_pwm_pins(Dspic33* cpu);
void dspic33_device_internal_usb_update_power_state(Dspic33* cpu);
bool dspic33_device_internal_interrupt_enabled(const Dspic33* cpu, uint16_t irq);
void dspic33_device_internal_output_compare_pulse_source(Dspic33* cpu, uint8_t source);
void dspic33_device_internal_output_compare_update_power_state(Dspic33* cpu);
void dspic33_device_internal_output_compare_raise(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_output_compare_refresh_fault_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_dci_discard_internal_events(Dspic33* cpu);
void dspic33_device_internal_dci_update_power_state(Dspic33* cpu);
void dspic33_device_internal_dma_update_power_state(Dspic33* cpu);
void dspic33_device_internal_dci_refresh_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_spi_refresh_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_uart_refresh_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_uart_update_power_state(Dspic33* cpu);
void dspic33_device_internal_refresh_can_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_can_invalid_event(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_spi_update_power_state(Dspic33* cpu);
void dspic33_device_internal_comparator_update_filter_power(Dspic33* cpu);
void dspic33_device_internal_comparator_evaluate_all(Dspic33* cpu);
uint64_t dspic33_device_internal_spi_transfer_cycles(const Dspic33* cpu, uint8_t channel);
uint8_t dspic33_device_internal_dci_pps_selection(const Dspic33* cpu, uint16_t address,
                                                  uint8_t shift);
bool dspic33_device_internal_dci_pps_input_high(const Dspic33* cpu, uint8_t selection);
uint16_t dspic33_device_internal_gpio_pin_values(const Dspic33* cpu, uint8_t port);
bool dspic33_device_internal_pwm_pin_value(const Dspic33* cpu, uint8_t port, uint8_t pin,
                                           bool* high);
void dspic33_device_internal_refresh_physical_pin_inputs(Dspic33* cpu);
void dspic33_device_internal_apply_physical_pin_level(Dspic33* cpu, uint8_t pin, bool high);
void dspic33_device_internal_uart_receive_complete(Dspic33* cpu, uint8_t channel,
                                                   const Dspic33UartFrame* incoming);

const uint16_t dspic33_device_timer_registers[DSPIC33_TIMER_COUNT] = {
    0x0100u, 0x0106u, 0x010au, 0x0114u, 0x0118u, 0x0122u, 0x0126u, 0x0130u, 0x0134u};
const uint16_t dspic33_device_timer_periods[DSPIC33_TIMER_COUNT] = {
    0x0102u, 0x010cu, 0x010eu, 0x011au, 0x011cu, 0x0128u, 0x012au, 0x0136u, 0x0138u};
const uint16_t dspic33_device_timer_controls[DSPIC33_TIMER_COUNT] = {
    0x0104u, 0x0110u, 0x0112u, 0x011eu, 0x0120u, 0x012cu, 0x012eu, 0x013au, 0x013cu};
const uint16_t dspic33_device_timer_holding_registers[4] = {0x0108u, 0x0116u, 0x0124u, 0x0132u};
const uint8_t dspic33_device_timer_irqs[DSPIC33_TIMER_COUNT] = {3u,  7u,  8u,  27u, 28u,
                                                                47u, 48u, 51u, 52u};
const uint8_t dspic33_device_dma_irqs[DSPIC33_DMA_COUNT] = {
    4u, 14u, 24u, 36u, 46u, 61u, 68u, 69u, 118u, 119u, 120u, 121u, 130u, 131u, 132u};
const uint8_t dspic33_device_uart_rx_irqs[DSPIC33_UART_COUNT] = {11u, 30u, 82u, 88u};
const uint8_t dspic33_device_uart_tx_irqs[DSPIC33_UART_COUNT] = {12u, 31u, 83u, 89u};
const uint8_t dspic33_device_uart_error_irqs[DSPIC33_UART_COUNT] = {65u, 66u, 81u, 87u};
const uint8_t dspic33_device_spi_error_irqs[DSPIC33_SPI_COUNT] = {9u, 32u, 90u, 122u};
const uint8_t dspic33_device_spi_irqs[DSPIC33_SPI_COUNT] = {10u, 33u, 91u, 123u};
const uint8_t dspic33_device_spi_dma_requests[DSPIC33_SPI_COUNT] = {10u, 33u, 91u, 123u};
const uint16_t dspic33_device_can_bases[DSPIC33_CAN_COUNT] = {0x0400u, 0x0500u};
const uint8_t dspic33_device_can_rx_irqs[DSPIC33_CAN_COUNT] = {34u, 55u};
const uint8_t dspic33_device_can_event_irqs[DSPIC33_CAN_COUNT] = {35u, 56u};
const uint8_t dspic33_device_can_tx_irqs[DSPIC33_CAN_COUNT] = {70u, 71u};
const uint8_t dspic33_device_can_rx_requests[DSPIC33_CAN_COUNT] = {34u, 55u};
const uint8_t dspic33_device_can_tx_requests[DSPIC33_CAN_COUNT] = {70u, 71u};
const uint16_t dspic33_device_uart_bases[DSPIC33_UART_COUNT] = {0x0220u, 0x0230u, 0x0250u, 0x02b0u};
const uint16_t dspic33_device_uart_pps_registers[DSPIC33_UART_COUNT] = {0x06c4u, 0x06c6u, 0x06d6u,
                                                                        0x06d8u};
const uint8_t dspic33_device_uart_tx_functions[DSPIC33_UART_COUNT] = {1u, 3u, 27u, 29u};
const uint8_t dspic33_device_uart_rts_functions[DSPIC33_UART_COUNT] = {2u, 4u, 28u, 30u};
const uint16_t dspic33_device_spi_bases[DSPIC33_SPI_COUNT] = {0x0240u, 0x0260u, 0x02a0u, 0x02c0u};
const uint16_t dspic33_device_adc_buffers[DSPIC33_ADC_COUNT] = {0x0300u, 0x0340u};
const uint16_t dspic33_device_adc_controls[DSPIC33_ADC_COUNT] = {0x0320u, 0x0360u};
const uint8_t dspic33_device_adc_irqs[DSPIC33_ADC_COUNT] = {13u, 21u};
const uint8_t dspic33_device_input_capture_irqs[DSPIC33_INPUT_CAPTURE_COUNT] = {
    1u, 5u, 37u, 38u, 39u, 40u, 22u, 23u, 93u, 125u, 127u, 129u, 135u, 137u, 139u, 141u};
const uint16_t dspic33_device_input_capture_pps_registers[DSPIC33_INPUT_CAPTURE_COUNT / 2u] = {
    0x06aeu, 0x06b0u, 0x06b2u, 0x06b4u, 0x06e2u, 0x06e4u, 0x06e6u, 0x06e8u};
const uint8_t dspic33_device_output_compare_irqs[DSPIC33_OUTPUT_COMPARE_COUNT] = {
    2u, 6u, 25u, 26u, 41u, 42u, 43u, 44u, 92u, 124u, 126u, 128u, 134u, 136u, 138u, 140u};
const uint8_t dspic33_device_external_interrupt_irqs[DSPIC33_EXTERNAL_INTERRUPT_COUNT] = {
    0u, 20u, 29u, 53u, 54u};
const uint8_t dspic33_device_pwm_irqs[DSPIC33_PWM_MAX_COUNT] = {94u, 95u, 96u, 97u, 98u, 99u, 100u};

uint8_t dspic33_device_internal_pwm_generator_count(const Dspic33* cpu) {
    const Dspic33epMuProfile* profile = dspic33_device_profile(cpu);
    return profile == NULL ? 0u : profile->pwm_generator_count;
}
const uint16_t dspic33_device_qei_bases[DSPIC33_QEI_COUNT] = {0x01c0u, 0x05c0u};
const uint8_t dspic33_device_qei_irqs[DSPIC33_QEI_COUNT] = {58u, 75u};
const uint16_t dspic33_device_qei_pps_registers[DSPIC33_QEI_COUNT][2] = {{0x06bcu, 0x06beu},
                                                                         {0x06c0u, 0x06c2u}};
const uint16_t dspic33_device_gpio_port_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e02u, 0x0e12u, 0x0e22u, 0x0e32u, 0x0e42u, 0x0e52u, 0x0e62u, 0x0e72u, 0x0e82u, 0x0e92u};
const uint16_t dspic33_device_gpio_tris_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e00u, 0x0e10u, 0x0e20u, 0x0e30u, 0x0e40u, 0x0e50u, 0x0e60u, 0x0e70u, 0x0e80u, 0x0e90u};
const uint16_t dspic33_device_gpio_latch_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e04u, 0x0e14u, 0x0e24u, 0x0e34u, 0x0e44u, 0x0e54u, 0x0e64u, 0x0e74u, 0x0e84u, 0x0e94u};
const uint16_t dspic33_device_gpio_open_drain_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e06u, 0x0e16u, 0x0e26u, 0x0e36u, 0x0e46u, 0x0e56u, 0x0e66u, 0x0e76u, 0x0e86u, 0x0e96u};
const uint16_t dspic33_device_gpio_change_notification_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e08u, 0x0e18u, 0x0e28u, 0x0e38u, 0x0e48u, 0x0e58u, 0x0e68u, 0x0e78u, 0x0e88u, 0x0e98u};
const uint16_t dspic33_device_gpio_analog_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e0eu, 0x0e1eu, 0x0e2eu, 0x0e3eu, 0x0e4eu, 0u, 0x0e6eu, 0u, 0u, 0u};
const uint16_t dspic33_device_gpio_analog_masks[DSPIC33_GPIO_PORT_COUNT] = {
    0x06c0u, 0xffffu, 0x601eu, 0x00c0u, 0x03ffu, 0u, 0x03c0u, 0u, 0u, 0u};
const uint16_t dspic33_device_gpio_pull_up_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e0au, 0x0e1au, 0x0e2au, 0x0e3au, 0x0e4au, 0x0e5au, 0x0e6au, 0x0e7au, 0x0e8au, 0x0e9au};
const uint16_t dspic33_device_gpio_pull_down_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e0cu, 0x0e1cu, 0x0e2cu, 0x0e3cu, 0x0e4cu, 0x0e5cu, 0x0e6cu, 0x0e7cu, 0x0e8cu, 0x0e9cu};
const uint16_t dspic33_device_gpio_port_masks[DSPIC33_GPIO_PORT_COUNT] = {
    0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x313fu, 0xf3cfu, 0xffffu, 0xffffu, 0xf803u};
const uint16_t dspic33_device_gpio_input_only_masks[DSPIC33_GPIO_PORT_COUNT] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0x000cu, 0u, 0u, 0u};

uint16_t dspic33_device_internal_gpio_port_mask(const Dspic33* cpu, uint8_t port) {
    return cpu == NULL ? 0u : dspic33ep_mu_gpio_port_mask(cpu->device, port);
}

const Dspic33PpsOutput dspic33_device_pps_outputs[] = {
    {0x0680u, 64u, 0u},  {0x0680u, 65u, 8u},  {0x0682u, 66u, 0u},  {0x0682u, 67u, 8u},
    {0x0684u, 68u, 0u},  {0x0684u, 69u, 8u},  {0x0686u, 70u, 0u},  {0x0686u, 71u, 8u},
    {0x0688u, 79u, 0u},  {0x0688u, 80u, 8u},  {0x068au, 82u, 0u},  {0x068au, 84u, 8u},
    {0x068cu, 85u, 0u},  {0x068cu, 87u, 8u},  {0x068eu, 96u, 0u},  {0x068eu, 97u, 8u},
    {0x0690u, 98u, 0u},  {0x0690u, 99u, 8u},  {0x0692u, 100u, 0u}, {0x0692u, 101u, 8u},
    {0x0696u, 104u, 0u}, {0x0696u, 108u, 8u}, {0x0698u, 109u, 0u}, {0x0698u, 112u, 8u},
    {0x069au, 113u, 0u}, {0x069au, 118u, 8u}, {0x069cu, 120u, 0u}, {0x069cu, 125u, 8u},
    {0x069eu, 126u, 0u}, {0x069eu, 127u, 8u}};

const Dspic33PpsPin dspic33_device_pps_pins[] = {
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

bool dspic33_device_internal_usb_schedule_bus_event(Dspic33* cpu, Dspic33UsbBusEvent event,
                                                    uint16_t value, uint64_t delay, bool external);

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
    {0x0762u, 0xffffu}, {0x0764u, 0xf7abu}, {0x0766u, 0x0029u}, {0x0768u, 0xffffu},
    {0x076au, 0x3f03u}, {0x076cu, 0x00f0u}, {0x0800u, 0xffffu}, {0x0802u, 0xffffu},
    {0x0804u, 0xffffu}, {0x0806u, 0x7fffu}, {0x0808u, 0x0afeu}, {0x080au, 0xffceu},
    {0x080cu, 0xc3efu}, {0x080eu, 0xffc0u}, {0x0810u, 0x7fdfu}, {0x0820u, 0xffffu},
    {0x0822u, 0xffffu}, {0x0824u, 0xffffu}, {0x0826u, 0x7fffu}, {0x0828u, 0x0afeu},
    {0x082au, 0xffceu}, {0x082cu, 0xffefu}, {0x082eu, 0xffc0u}, {0x0830u, 0x7fdfu},
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
    {0x0e70u, 0xffffu}, {0x0e74u, 0xffffu}, {0x0e76u, 0xffffu}, {0x0e78u, 0xffffu},
    {0x0e7au, 0xffffu}, {0x0e7cu, 0xffffu}, {0x0e80u, 0xffffu}, {0x0e84u, 0xffffu},
    {0x0e86u, 0xffffu}, {0x0e88u, 0xffffu}, {0x0e8au, 0xffffu}, {0x0e8cu, 0xffffu},
    {0x0e90u, 0xf803u}, {0x0e94u, 0xf803u}, {0x0e96u, 0xf803u}, {0x0e98u, 0xf803u},
    {0x0e9au, 0xf803u}, {0x0e9cu, 0xf803u}, {0x0efeu, 0x0003u}, {0x0f82u, 0x00ffu},
    {0x0f8cu, 0x00ffu}, {0x0fa4u, 0x001fu}};

const Dspic33ResetValue dspic33_device_reset_values[] = {
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
    {0x0e6eu, 0x03c0u}, {0x0e70u, 0xffffu}, {0x0e80u, 0xffffu}, {0x0e90u, 0xf803u}};

uint16_t dspic33_device_internal_raw_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[(uint16_t)(address + 1u)] << 8u));
}

void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[(uint16_t)(address + 1u)] = (uint8_t)(value >> 8u);
}

bool dspic33_device_internal_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                 uint16_t* writable) {
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
    if (address == 0x076au &&
        dspic33_device_internal_pwm_generator_count(cpu) == DSPIC33_PWM_MAX_COUNT) {
        *writable |= 0x4000u;
    }
    if (address >= 0x0e00u && address < 0x0ea0u) {
        uint8_t port = (uint8_t)((address - 0x0e00u) / 0x10u);
        *writable &= dspic33_device_internal_gpio_port_mask(cpu, port);
    }
    return true;
}

bool dspic33_device_internal_pps_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                     uint16_t* writable) {
    return address >= 0x0680u && address <= 0x06f6u &&
           dspic33_device_internal_register_write_mask(cpu, address, writable);
}

void dspic33_device_internal_pps_capture_shadow(Dspic33* cpu) {
    uint8_t index;
    for (index = 0u; index < DSPIC33_PPS_REGISTER_COUNT; index++) {
        uint16_t address = (uint16_t)(0x0680u + index * 2u);
        uint16_t writable;
        cpu->io.pps.shadow[index] =
            dspic33_device_internal_pps_register_write_mask(cpu, address, &writable)
                ? (uint16_t)(dspic33_device_internal_raw_word(cpu, address) & writable)
                : 0u;
    }
}

void dspic33_device_internal_pps_update_shadow(Dspic33* cpu, uint16_t address) {
    uint16_t writable;
    if (dspic33_device_internal_pps_register_write_mask(cpu, address, &writable)) {
        cpu->io.pps.shadow[(address - 0x0680u) / 2u] =
            (uint16_t)(dspic33_device_internal_raw_word(cpu, address) & writable);
    }
}

bool dspic33_device_internal_pps_shadow_matches(const Dspic33* cpu) {
    uint8_t index;
    for (index = 0u; index < DSPIC33_PPS_REGISTER_COUNT; index++) {
        uint16_t address = (uint16_t)(0x0680u + index * 2u);
        uint16_t writable;
        if (dspic33_device_internal_pps_register_write_mask(cpu, address, &writable) &&
            (dspic33_device_internal_raw_word(cpu, address) & writable) !=
                cpu->io.pps.shadow[index]) {
            return false;
        }
    }
    return true;
}

bool dspic33_device_internal_input_capture_register_write_mask(uint16_t address,
                                                               uint16_t* writable) {
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

bool dspic33_device_internal_output_compare_register_write_mask(uint16_t address,
                                                                uint16_t* writable) {
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

bool dspic33_device_internal_comparator_register_write_mask(uint16_t address, uint16_t* writable) {
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

void dspic33_device_internal_update_gpio_latch(Dspic33* cpu, uint16_t address, uint16_t requested) {
    uint16_t port_address = (uint16_t)(address & 0xfffeu);
    uint8_t port;

    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        uint16_t latch_address;
        uint16_t latch;
        uint16_t writable;
        bool word_write;

        if (port_address != dspic33_device_gpio_port_addresses[port]) {
            continue;
        }
        latch_address = dspic33_device_gpio_latch_addresses[port];
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
        if (dspic33_device_internal_register_write_mask(cpu, latch_address, &writable)) {
            latch = (uint16_t)((dspic33_device_internal_raw_word(cpu, latch_address) & ~writable) |
                               (latch & writable));
        }
        dspic33_device_internal_raw_write_word(cpu, latch_address, latch);
        return;
    }
}

bool dspic33_device_internal_adc_register_write_mask(uint16_t address, uint16_t* writable) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        uint16_t control = dspic33_device_adc_controls[module];
        uint16_t buffer = dspic33_device_adc_buffers[module];
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

bool dspic33_device_internal_uart_module_disabled(const Dspic33* cpu, uint8_t channel) {
    return channel >= DSPIC33_UART_COUNT ||
           dspic33_device_internal_platform_pmd_disabled(
               cpu, (uint8_t)(PLATFORM_PMD_UART_BASE + channel));
}

bool dspic33_device_internal_uart_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                      uint16_t* writable) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t offset = (uint16_t)(address - dspic33_device_uart_bases[channel]);
        if (offset > 8u || (offset & 1u) != 0u) {
            continue;
        }
        if (dspic33_device_internal_uart_module_disabled(cpu, channel)) {
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

bool dspic33_device_internal_spi_register_write_mask(uint16_t address, uint16_t* writable) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t offset = (uint16_t)(address - dspic33_device_spi_bases[channel]);
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

bool dspic33_device_internal_can_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                     uint16_t* writable) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = dspic33_device_can_bases[channel];
        uint16_t offset = (uint16_t)(address - base);
        bool window = (dspic33_device_internal_raw_word(cpu, base) & CAN_WINDOW) != 0u;
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

bool dspic33_device_internal_usb_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                     uint16_t previous, uint16_t* writable) {
    bool host = ((address == USB_CON ? previous : dspic33_device_internal_raw_word(cpu, USB_CON)) &
                 USB_HOST_ENABLE) != 0u;
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

bool dspic33_device_internal_pwm_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                     uint16_t* writable) {
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
        address < PWM_GENERATOR_BASE +
                      dspic33_device_internal_pwm_generator_count(cpu) * PWM_GENERATOR_STRIDE) {
        *writable = generator_masks[((address - PWM_GENERATOR_BASE) & 0x001fu) / 2u];
        return true;
    }
    return false;
}

bool dspic33_device_internal_byte_queue_push(Dspic33ByteQueue* queue, uint8_t byte_value) {
    uint16_t queue_index;
    if (queue->count == sizeof(queue->bytes)) {
        return false;
    }

    queue_index = (uint16_t)((queue->head + queue->count) % sizeof(queue->bytes));
    queue->bytes[queue_index] = byte_value;
    queue->count++;
    return true;
}

bool dspic33_device_internal_byte_queue_pop(Dspic33ByteQueue* queue, uint8_t* output_byte) {
    if (queue->count == 0u) {
        return false;
    }

    *output_byte = queue->bytes[queue->head];
    queue->head = (uint16_t)((queue->head + 1u) % sizeof(queue->bytes));
    queue->count--;
    return true;
}

bool dspic33_device_internal_uart_fifo_push(Dspic33UartFifo* fifo,
                                            const Dspic33UartFrame* input_frame) {
    uint8_t queue_index;
    if (fifo->count == DSPIC33_UART_FIFO_SIZE) {
        return false;
    }

    queue_index = (uint8_t)((fifo->head + fifo->count) % DSPIC33_UART_FIFO_SIZE);
    fifo->frames[queue_index] = *input_frame;
    fifo->count++;
    return true;
}

bool dspic33_device_internal_uart_fifo_front(const Dspic33UartFifo* fifo,
                                             Dspic33UartFrame* output_frame) {
    if (fifo->count == 0u) {
        return false;
    }

    *output_frame = fifo->frames[fifo->head];
    return true;
}

bool dspic33_device_internal_uart_fifo_pop(Dspic33UartFifo* fifo, Dspic33UartFrame* output_frame) {
    if (!dspic33_device_internal_uart_fifo_front(fifo, output_frame)) {
        return false;
    }

    fifo->head = (uint8_t)((fifo->head + 1u) % DSPIC33_UART_FIFO_SIZE);
    fifo->count--;
    return true;
}

bool dspic33_device_internal_uart_queue_push(Dspic33UartQueue* queue,
                                             const Dspic33UartFrame* input_frame) {
    uint16_t queue_index;
    if (queue->count == DSPIC33_UART_QUEUE_SIZE) {
        return false;
    }

    queue_index = (uint16_t)((queue->head + queue->count) % DSPIC33_UART_QUEUE_SIZE);
    queue->frames[queue_index] = *input_frame;
    queue->count++;
    return true;
}

bool dspic33_device_internal_uart_queue_pop(Dspic33UartQueue* queue,
                                            Dspic33UartFrame* output_frame) {
    if (queue->count == 0u) {
        return false;
    }

    *output_frame = queue->frames[queue->head];
    queue->head = (uint16_t)((queue->head + 1u) % DSPIC33_UART_QUEUE_SIZE);
    queue->count--;
    return true;
}

bool dspic33_device_internal_word_queue_push(Dspic33WordQueue* queue, uint16_t word_value) {
    uint8_t queue_index;
    if (queue->count == sizeof(queue->words) / sizeof(queue->words[0])) {
        return false;
    }

    queue_index =
        (uint8_t)((queue->head + queue->count) % (sizeof(queue->words) / sizeof(queue->words[0])));
    queue->words[queue_index] = word_value;
    queue->count++;
    return true;
}

bool dspic33_device_internal_word_queue_pop(Dspic33WordQueue* queue, uint16_t* output_word) {
    if (queue->count == 0u) {
        return false;
    }

    *output_word = queue->words[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % (sizeof(queue->words) / sizeof(queue->words[0])));
    queue->count--;
    return true;
}

bool dspic33_device_internal_word_queue_push_front(Dspic33WordQueue* queue, uint16_t word_value) {
    uint8_t queue_capacity = (uint8_t)(sizeof(queue->words) / sizeof(queue->words[0]));
    if (queue->count == queue_capacity) {
        return false;
    }

    queue->head = (uint8_t)((queue->head + queue_capacity - 1u) % queue_capacity);
    queue->words[queue->head] = word_value;
    queue->count++;
    return true;
}

bool dspic33_device_internal_word_queue_front(const Dspic33WordQueue* queue,
                                              uint16_t* output_word) {
    if (queue->count == 0u) {
        return false;
    }

    *output_word = queue->words[queue->head];
    return true;
}
