#ifndef DSPIC33EP_MU_SIM_DEVICE_TABLES_H
#define DSPIC33EP_MU_SIM_DEVICE_TABLES_H

#include "device/dspic33ep_mu/device.h"

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

extern const Dspic33PpsOutput dspic33_device_pps_outputs[30];
extern const Dspic33PpsPin dspic33_device_pps_pins[78];
extern const Dspic33ResetValue dspic33_device_reset_values[97];
extern const uint16_t dspic33_device_adc_buffers[DSPIC33_ADC_COUNT];
extern const uint16_t dspic33_device_adc_controls[DSPIC33_ADC_COUNT];
extern const uint16_t dspic33_device_can_bases[DSPIC33_CAN_COUNT];
extern const uint16_t dspic33_device_gpio_analog_addresses[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_gpio_analog_masks[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_gpio_change_notification_addresses[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_gpio_input_only_masks[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_gpio_latch_addresses[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_gpio_open_drain_addresses[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_gpio_port_addresses[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_gpio_port_masks[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_gpio_pull_down_addresses[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_gpio_pull_up_addresses[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_gpio_tris_addresses[DSPIC33_GPIO_PORT_COUNT];
extern const uint16_t dspic33_device_input_capture_pps_registers[DSPIC33_INPUT_CAPTURE_COUNT / 2u];
extern const uint16_t dspic33_device_qei_bases[DSPIC33_QEI_COUNT];
extern const uint16_t dspic33_device_qei_pps_registers[DSPIC33_QEI_COUNT][2];
extern const uint16_t dspic33_device_spi_bases[DSPIC33_SPI_COUNT];
extern const uint16_t dspic33_device_timer_controls[DSPIC33_TIMER_COUNT];
extern const uint16_t dspic33_device_timer_holding_registers[4];
extern const uint16_t dspic33_device_timer_periods[DSPIC33_TIMER_COUNT];
extern const uint16_t dspic33_device_timer_registers[DSPIC33_TIMER_COUNT];
extern const uint16_t dspic33_device_uart_bases[DSPIC33_UART_COUNT];
extern const uint16_t dspic33_device_uart_pps_registers[DSPIC33_UART_COUNT];
extern const uint8_t dspic33_device_adc_irqs[DSPIC33_ADC_COUNT];
extern const uint8_t dspic33_device_can_event_irqs[DSPIC33_CAN_COUNT];
extern const uint8_t dspic33_device_can_rx_irqs[DSPIC33_CAN_COUNT];
extern const uint8_t dspic33_device_can_rx_requests[DSPIC33_CAN_COUNT];
extern const uint8_t dspic33_device_can_tx_irqs[DSPIC33_CAN_COUNT];
extern const uint8_t dspic33_device_can_tx_requests[DSPIC33_CAN_COUNT];
extern const uint8_t dspic33_device_dma_irqs[DSPIC33_DMA_COUNT];
extern const uint8_t dspic33_device_external_interrupt_irqs[DSPIC33_EXTERNAL_INTERRUPT_COUNT];
extern const uint8_t dspic33_device_input_capture_irqs[DSPIC33_INPUT_CAPTURE_COUNT];
extern const uint8_t dspic33_device_output_compare_irqs[DSPIC33_OUTPUT_COMPARE_COUNT];
extern const uint8_t dspic33_device_pwm_irqs[DSPIC33_PWM_MAX_COUNT];
extern const uint8_t dspic33_device_qei_irqs[DSPIC33_QEI_COUNT];
extern const uint8_t dspic33_device_spi_dma_requests[DSPIC33_SPI_COUNT];
extern const uint8_t dspic33_device_spi_error_irqs[DSPIC33_SPI_COUNT];
extern const uint8_t dspic33_device_spi_irqs[DSPIC33_SPI_COUNT];
extern const uint8_t dspic33_device_timer_irqs[DSPIC33_TIMER_COUNT];
extern const uint8_t dspic33_device_uart_error_irqs[DSPIC33_UART_COUNT];
extern const uint8_t dspic33_device_uart_rts_functions[DSPIC33_UART_COUNT];
extern const uint8_t dspic33_device_uart_rx_irqs[DSPIC33_UART_COUNT];
extern const uint8_t dspic33_device_uart_tx_functions[DSPIC33_UART_COUNT];
extern const uint8_t dspic33_device_uart_tx_irqs[DSPIC33_UART_COUNT];

#endif
