#ifndef DSPIC33EP_MU_SIM_DEVICE_INTERNAL_H
#define DSPIC33EP_MU_SIM_DEVICE_INTERNAL_H

#include <stdlib.h>
#include <string.h>

#include "device/dspic33ep_mu/communication/i2c/api.h"
#include "device/dspic33ep_mu/data.h"
#include "device/dspic33ep_mu/device.h"
#include "device/dspic33ep_mu/registers.h"
#include "device/dspic33ep_mu/tables.h"

typedef enum { CAN_SERIAL_INCOMPLETE, CAN_SERIAL_VALID, CAN_SERIAL_INVALID } Dspic33CanSerialResult;

bool dspic33_device_internal_adc_module_address(uint16_t address, uint8_t module);
bool dspic33_device_internal_adc_pmd_disabled(const Dspic33* cpu, uint8_t module);
bool dspic33_device_internal_adc_register_write_mask(uint16_t address, uint16_t* writable);
bool dspic33_device_internal_auxiliary_clock_configuration_locked(const Dspic33* cpu);
bool dspic33_device_internal_auxiliary_pll_reconfiguration(uint16_t previous, uint16_t control);
bool dspic33_device_internal_byte_queue_pop(Dspic33ByteQueue* queue, uint8_t* output_byte);
bool dspic33_device_internal_byte_queue_push(Dspic33ByteQueue* queue, uint8_t byte_value);
bool dspic33_device_internal_can_capture_enabled(const Dspic33* cpu);
bool dspic33_device_internal_can_power_enabled(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_can_queue_pop(Dspic33CanQueue* queue, Dspic33CanFrame* output_frame);
bool dspic33_device_internal_can_queue_push(Dspic33CanQueue* queue,
                                            const Dspic33CanFrame* input_frame);
bool dspic33_device_internal_can_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                     uint16_t* writable);
bool dspic33_device_internal_can_schedule_mode_transition(Dspic33* cpu, uint8_t channel,
                                                          uint8_t mode);
bool dspic33_device_internal_comparator_configuration_supported(const Dspic33* cpu,
                                                                uint8_t comparator);
bool dspic33_device_internal_comparator_pin_channel(const Dspic33* cpu, uint8_t pin,
                                                    uint8_t* comparator);
bool dspic33_device_internal_comparator_register_write_mask(uint16_t address, uint16_t* writable);
bool dspic33_device_internal_dci_configuration_supported(const Dspic33* cpu);
bool dspic33_device_internal_dci_data_output(const Dspic33* cpu, bool* high);
bool dspic33_device_internal_dci_frame_output(const Dspic33* cpu, bool* high);
bool dspic33_device_internal_dci_internal_clock_high(const Dspic33* cpu, bool* high);
bool dspic33_device_internal_dci_output_pop(Dspic33DciQueue* queue, Dspic33DciTransfer* transfer);
bool dspic33_device_internal_dci_pps_input_high(const Dspic33* cpu, uint8_t selection);
bool dspic33_device_internal_dci_read_register(Dspic33* cpu, uint16_t address, uint8_t* value);
bool dspic33_device_internal_dma_register_write_mask(uint16_t address, uint16_t* writable);
bool dspic33_device_internal_input_capture_pair_configured(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_input_capture_pmd_disabled(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_input_capture_register_write_mask(uint16_t address,
                                                               uint16_t* writable);
bool dspic33_device_internal_input_capture_source_awaited(const Dspic33* cpu, uint8_t source);
bool dspic33_device_internal_interrupt_control_write(Dspic33* cpu, uint16_t register_address,
                                                     uint16_t previous_word,
                                                     uint16_t requested_word);
bool dspic33_device_internal_interrupt_enabled(const Dspic33* cpu, uint16_t irq);
bool dspic33_device_internal_nvm_key_authorized(const Dspic33* cpu);
bool dspic33_device_internal_oscillator_pin_clock_output(const Dspic33* cpu);
bool dspic33_device_internal_oscillator_pin_owned(const Dspic33* cpu);
bool dspic33_device_internal_output_compare_cascade_requested(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_output_compare_high(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_output_compare_pin_channel(const Dspic33* cpu, uint8_t pin,
                                                        uint8_t* channel);
bool dspic33_device_internal_output_compare_pmd_disabled(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_output_compare_register_write_mask(uint16_t address,
                                                                uint16_t* writable);
bool dspic33_device_internal_output_compare_source_awaited(const Dspic33* cpu, uint8_t source);
bool dspic33_device_internal_output_compare_supported(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_pmp_output_pop(Dspic33PmpQueue* queue, Dspic33PmpTransfer* transfer);
bool dspic33_device_internal_pmp_response_push(Dspic33PmpResponseQueue* queue,
                                               const Dspic33PmpResponse* response);
bool dspic33_device_internal_pps_physical_input_enabled(const Dspic33* cpu, uint8_t pin);
bool dspic33_device_internal_pps_physical_input_high(const Dspic33* cpu, uint8_t pin, bool* high);
bool dspic33_device_internal_pps_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                     uint16_t* writable);
bool dspic33_device_internal_pps_shadow_matches(const Dspic33* cpu);
bool dspic33_device_internal_protect_oscillator_write(Dspic33* cpu, uint16_t address,
                                                      uint16_t previous_control);
bool dspic33_device_internal_pwm_address_inaccessible(const Dspic33* cpu, uint16_t address);
bool dspic33_device_internal_pwm_fault_active(const Dspic33* cpu, uint8_t generator);
bool dspic33_device_internal_pwm_generator_pmd_disabled(const Dspic33* cpu, uint8_t generator);
bool dspic33_device_internal_pwm_global_pmd_disabled(const Dspic33* cpu);
bool dspic33_device_internal_pwm_pin_value(const Dspic33* cpu, uint8_t port, uint8_t pin,
                                           bool* high);
bool dspic33_device_internal_pwm_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                     uint16_t* writable);
bool dspic33_device_internal_qei_compare_output_value(const Dspic33* cpu, uint8_t channel,
                                                      bool* is_high);
bool dspic33_device_internal_qei_pps_output_value(const Dspic33* cpu, uint8_t port,
                                                  uint8_t port_bit, bool* is_high);
bool dspic33_device_internal_qei_read_register(Dspic33* cpu, uint16_t address, uint8_t* read_value);
bool dspic33_device_internal_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                 uint16_t* writable);
bool dspic33_device_internal_schedule_dma_channel(Dspic33* cpu, uint8_t channel,
                                                  uint16_t indirect_address, bool forced,
                                                  uint64_t delay);
bool dspic33_device_internal_spi_enhanced(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_spi_master_frame_slave(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_spi_master(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_spi_module_disabled(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_spi_power_enabled(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_spi_read_complete(const Dspic33* cpu, uint16_t address);
bool dspic33_device_internal_spi_register_write_mask(uint16_t address, uint16_t* writable);
bool dspic33_device_internal_spi_selected(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_spi_slave_frame_master(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_timer_is_paired_high(const Dspic33* cpu, uint8_t timer_index);
bool dspic33_device_internal_timer_is_type_b(uint8_t timer);
bool dspic33_device_internal_timer_pair_enabled(const Dspic33* cpu, uint8_t timer_index);
bool dspic33_device_internal_timer_pmd_disabled(const Dspic33* cpu, uint8_t timer_index);
bool dspic33_device_internal_timer_power_enabled(const Dspic33* cpu, uint8_t timer_index,
                                                 bool is_external_clock);
bool dspic33_device_internal_uart_fifo_front(const Dspic33UartFifo* fifo,
                                             Dspic33UartFrame* output_frame);
bool dspic33_device_internal_uart_fifo_pop(Dspic33UartFifo* fifo, Dspic33UartFrame* output_frame);
bool dspic33_device_internal_uart_fifo_push(Dspic33UartFifo* fifo,
                                            const Dspic33UartFrame* input_frame);
bool dspic33_device_internal_uart_module_disabled(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_uart_pps_output_value(const Dspic33* cpu, uint8_t port, uint8_t pin,
                                                   bool* high);
bool dspic33_device_internal_uart_queue_pop(Dspic33UartQueue* queue,
                                            Dspic33UartFrame* output_frame);
bool dspic33_device_internal_uart_queue_push(Dspic33UartQueue* queue,
                                             const Dspic33UartFrame* input_frame);
bool dspic33_device_internal_uart_receiver_operating(const Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_irda_edge(Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_uart_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                      uint16_t* writable);
bool dspic33_device_internal_uart_rx_logical_level(const Dspic33* cpu, uint8_t channel, bool* high);
bool dspic33_device_internal_usb_descriptor(const Dspic33* cpu, uint8_t endpoint, uint8_t direction,
                                            uint8_t bank, uint16_t words[4]);
bool dspic33_device_internal_usb_queue_pop(Dspic33UsbQueue* queue, Dspic33UsbPacket* packet);
bool dspic33_device_internal_usb_queue_push(Dspic33UsbQueue* queue, const Dspic33UsbPacket* packet);
bool dspic33_device_internal_usb_read_memory(const Dspic33* cpu, uint32_t address, uint8_t* data,
                                             uint16_t size, bool increment);
bool dspic33_device_internal_usb_register_address(uint16_t address);
bool dspic33_device_internal_usb_register_write_mask(const Dspic33* cpu, uint16_t address,
                                                     uint16_t previous, uint16_t* writable);
bool dspic33_device_internal_usb_schedule_bus_event(Dspic33* cpu, Dspic33UsbBusEvent event,
                                                    uint16_t value, uint64_t delay, bool external);
bool dspic33_device_internal_word_queue_front(const Dspic33WordQueue* queue, uint16_t* output_word);
bool dspic33_device_internal_word_queue_pop(Dspic33WordQueue* queue, uint16_t* output_word);
bool dspic33_device_internal_word_queue_push_front(Dspic33WordQueue* queue, uint16_t word_value);
bool dspic33_device_internal_word_queue_push(Dspic33WordQueue* queue, uint16_t word_value);
const Dspic33PpsPin* dspic33_device_internal_pps_pin(uint8_t pin);
bool dspic33_device_internal_pps_pin_bonded(const Dspic33* cpu, uint8_t pin);
Dspic33CanFrame dspic33_device_internal_can_decode_frame(const uint16_t words[8]);
Dspic33Event dspic33_device_internal_event_pop(Dspic33EventQueue* queue);
uint16_t dspic33_device_internal_adc_register(const Dspic33* cpu, uint8_t module, uint16_t offset);
uint16_t dspic33_device_internal_can_buffer_control(const Dspic33* cpu, uint8_t channel,
                                                    uint8_t buffer);
uint16_t dspic33_device_internal_can_filter_word(const Dspic33* cpu, uint8_t channel,
                                                 uint16_t offset);
uint16_t dspic33_device_internal_can_frame_bits(const Dspic33CanFrame* frame, bool bits[160]);
uint16_t dspic33_device_internal_comparator_base(uint8_t comparator);
uint16_t dspic33_device_internal_dma_channel_base(uint8_t channel_index);
uint16_t dspic33_device_internal_dma_channel_bit(uint8_t channel_index);
uint16_t dspic33_device_internal_gpio_pin_values(const Dspic33* cpu, uint8_t port);
uint16_t dspic33_device_internal_gpio_port_mask(const Dspic33* cpu, uint8_t port);
uint16_t dspic33_device_internal_output_compare_base(uint8_t channel);
uint16_t dspic33_device_internal_pwm_generator_base(uint8_t generator);
uint16_t dspic33_device_internal_pwm_register(const Dspic33* cpu, uint8_t generator,
                                              uint16_t offset);
uint16_t dspic33_device_internal_raw_word(const Dspic33* cpu, uint16_t address);
uint32_t dspic33_device_internal_timer_prescale(uint16_t control);
uint64_t dspic33_device_internal_can_bit_cycles(const Dspic33* cpu, uint8_t channel);
uint64_t dspic33_device_internal_output_compare_clock_boundary_ticks(const Dspic33* cpu,
                                                                     uint8_t timer);
uint64_t dspic33_device_internal_qei_boundary_cycles(const Dspic33* cpu, uint64_t maximum_cycles);
uint64_t dspic33_device_internal_spi_transfer_cycles(const Dspic33* cpu, uint8_t channel);
uint64_t dspic33_device_internal_timer_ticks_until_period(const Dspic33* cpu, uint8_t timer_index);
uint8_t dspic33_device_internal_auxiliary_pll_input(uint16_t control);
uint8_t dspic33_device_internal_can_mode(const Dspic33* cpu, uint8_t channel);
uint8_t dspic33_device_internal_can_next_fifo_buffer(const Dspic33* cpu, uint8_t channel,
                                                     uint8_t buffer);
uint8_t dspic33_device_internal_crc_data_width(const Dspic33* cpu);
uint8_t dspic33_device_internal_dci_pps_selection(const Dspic33* cpu, uint16_t address,
                                                  uint8_t shift);
uint8_t dspic33_device_internal_input_capture_pps_pin(const Dspic33* cpu, uint8_t channel);
uint8_t dspic33_device_internal_oscillator_current_source(uint16_t oscillator_control);
uint8_t dspic33_device_internal_output_compare_pair_high(uint8_t channel);
uint8_t dspic33_device_internal_output_compare_pair_low(uint8_t channel);
uint8_t dspic33_device_internal_pps_output_function(const Dspic33* cpu, uint8_t pin);
uint8_t dspic33_device_internal_pwm_generator_count(const Dspic33* cpu);
uint8_t dspic33_device_internal_rtcc_read_window(Dspic33* cpu, uint16_t address, bool alarm);
uint8_t dspic33_device_internal_uart_transmit_interrupt_mode(const Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_adc_abort(Dspic33* cpu, uint8_t module);
void dspic33_device_internal_adc_begin_sampling(Dspic33* cpu, uint8_t module);
void dspic33_device_internal_adc_start_conversion(Dspic33* cpu, uint8_t module);
void dspic33_device_internal_adc_update_power_state(Dspic33* cpu);
void dspic33_device_internal_advance_input_capture(Dspic33* cpu, uint64_t cycles);
void dspic33_device_internal_advance_output_compare(Dspic33* cpu, uint64_t cycles);
void dspic33_device_internal_advance_pwm(Dspic33* cpu, uint64_t cycles);
void dspic33_device_internal_advance_qei(Dspic33* cpu, uint64_t cycles);
void dspic33_device_internal_apply_physical_pin_level(Dspic33* cpu, uint8_t pin, bool high);
void dspic33_device_internal_can_invalid_event(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_can_queue_discard_last(Dspic33CanQueue* queue);
void dspic33_device_internal_can_raise_event(Dspic33* cpu, uint8_t channel, uint16_t flag,
                                             uint8_t buffer, uint8_t filter);
void dspic33_device_internal_can_refresh_error_status(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_can_remove_transmit_events(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_can_set_buffer_control(Dspic33* cpu, uint8_t channel, uint8_t buffer,
                                                    uint16_t value);
void dspic33_device_internal_can_update_vector(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_clock_timer(Dspic33* cpu, uint8_t timer_index, uint64_t clock_count,
                                         uint16_t* synchronization_sources,
                                         bool flush_synchronization_sources);
void dspic33_device_internal_comparator_evaluate_all(Dspic33* cpu);
void dspic33_device_internal_comparator_filter_clock(Dspic33* cpu, uint8_t source, uint64_t clocks);
void dspic33_device_internal_comparator_update_filter_power(Dspic33* cpu);
void dspic33_device_internal_complete_oscillator_event(Dspic33* cpu, uint16_t phase,
                                                       uint32_t generation);
void dspic33_device_internal_crc_abort(Dspic33* cpu);
void dspic33_device_internal_crc_push(Dspic33* cpu, uint32_t value);
void dspic33_device_internal_crc_refresh_status(Dspic33* cpu);
void dspic33_device_internal_crc_reset_runtime(Dspic33* cpu);
void dspic33_device_internal_crc_start_if_ready(Dspic33* cpu);
void dspic33_device_internal_dci_discard_internal_events(Dspic33* cpu);
void dspic33_device_internal_dci_refresh_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_dci_update_power_state(Dspic33* cpu);
void dspic33_device_internal_dma_advance_generation(Dspic33* cpu, uint8_t channel_index);
void dspic33_device_internal_dma_request_collision(Dspic33* cpu, uint8_t channel_index);
void dspic33_device_internal_dma_update_power_state(Dspic33* cpu);
void dspic33_device_internal_input_capture_advance_clock(Dspic33* cpu, uint16_t timer_source,
                                                         uint64_t cycles);
void dspic33_device_internal_input_capture_level(Dspic33* cpu, uint8_t channel, bool high);
void dspic33_device_internal_input_capture_pulse_source(Dspic33* cpu, uint8_t source);
void dspic33_device_internal_input_capture_read_complete(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_oscillator_configuration_changed(Dspic33* cpu, uint8_t previous);
void dspic33_device_internal_oscillator_pll_configuration_changed(Dspic33* cpu, uint8_t previous);
void dspic33_device_internal_oscillator_startup_configuration_changed(Dspic33* cpu,
                                                                      uint8_t previous);
void dspic33_device_internal_output_compare_advance_clock(Dspic33* cpu, uint8_t timer,
                                                          uint64_t ticks);
void dspic33_device_internal_output_compare_fault_input(Dspic33* cpu, uint8_t source, bool high);
void dspic33_device_internal_output_compare_pulse_source(Dspic33* cpu, uint8_t source);
void dspic33_device_internal_output_compare_raise(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_output_compare_refresh_fault_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_output_compare_update_power_state(Dspic33* cpu);
void dspic33_device_internal_pmp_clear_busy(Dspic33* cpu, uint16_t generation);
void dspic33_device_internal_pmp_read_register(Dspic33* cpu, uint16_t address);
void dspic33_device_internal_pmp_slave_read_event(Dspic33* cpu, uint8_t address);
void dspic33_device_internal_pmp_slave_write_event(Dspic33* cpu, uint8_t address, uint8_t value);
void dspic33_device_internal_pps_capture_shadow(Dspic33* cpu);
void dspic33_device_internal_pps_update_shadow(Dspic33* cpu, uint16_t address);
void dspic33_device_internal_pulse_timer_synchronization_sources(Dspic33* cpu,
                                                                 uint16_t* pending_sources);
void dspic33_device_internal_pulse_timer(Dspic33* cpu, uint8_t timer, uint32_t pulses);
void dspic33_device_internal_pwm_dead_time_event(Dspic33* cpu, uint8_t generator, bool high);
void dspic33_device_internal_pwm_input_event(Dspic33* cpu, uint8_t source, bool high,
                                             bool current_limit);
void dspic33_device_internal_pwm_latch_generator(Dspic33* cpu, uint8_t generator);
void dspic33_device_internal_pwm_refresh_status(Dspic33* cpu, uint8_t generator);
void dspic33_device_internal_pwm_start(Dspic33* cpu);
void dspic33_device_internal_pwm_sync_event(Dspic33* cpu, uint8_t input, bool high);
void dspic33_device_internal_pwm_update_output(Dspic33* cpu, uint8_t generator);
void dspic33_device_internal_qei_set_physical_inputs(Dspic33* cpu, uint8_t channel,
                                                     uint8_t input_values);
void dspic33_device_internal_raise_external_interrupt(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_raise_scheduled_interrupt(Dspic33* cpu, uint16_t irq);
void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_device_internal_reconfigure_auxiliary_pll(Dspic33* cpu);
void dspic33_device_internal_refresh_can_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_refresh_external_interrupts(Dspic33* cpu);
void dspic33_device_internal_refresh_gpio_change_notification(Dspic33* cpu);
void dspic33_device_internal_refresh_input_capture_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_refresh_physical_pin_inputs(Dspic33* cpu);
void dspic33_device_internal_refresh_pwm_inputs(Dspic33* cpu);
void dspic33_device_internal_refresh_pwm_pins(Dspic33* cpu);
void dspic33_device_internal_refresh_qei_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_refresh_timer_inputs(Dspic33* cpu);
void dspic33_device_internal_reset_main_oscillator(Dspic33* cpu);
void dspic33_device_internal_run_adc_pmd(Dspic33* cpu, uint16_t module, uint32_t value);
void dspic33_device_internal_run_adc(Dspic33* cpu, uint8_t module, uint32_t event_value);
void dspic33_device_internal_run_can(Dspic33* cpu, uint8_t channel, uint32_t value);
void dspic33_device_internal_run_comparator(Dspic33* cpu, uint16_t source, uint32_t value);
void dspic33_device_internal_run_crc_pmd(Dspic33* cpu, uint32_t value);
void dspic33_device_internal_run_crc(Dspic33* cpu, uint16_t generation);
void dspic33_device_internal_run_dci(Dspic33* cpu, uint16_t source, uint32_t value);
void dspic33_device_internal_run_dma(Dspic33* cpu, uint16_t source, uint32_t event_value);
void dspic33_device_internal_run_input_capture(Dspic33* cpu, uint16_t source, uint32_t value);
void dspic33_device_internal_run_output_compare(Dspic33* cpu, uint16_t source, uint32_t value);
void dspic33_device_internal_run_pmp_pmd(Dspic33* cpu, uint32_t value);
void dspic33_device_internal_run_pmp(Dspic33* cpu, uint16_t generation);
void dspic33_device_internal_run_pwm_pmd(Dspic33* cpu, uint16_t source, uint32_t value);
void dspic33_device_internal_run_qei(Dspic33* cpu, uint16_t source, uint32_t value);
void dspic33_device_internal_run_rtcc(Dspic33* cpu, uint16_t source, uint32_t value);
void dspic33_device_internal_run_spi_select(Dspic33* cpu, uint8_t channel, bool selected);
void dspic33_device_internal_run_spi(Dspic33* cpu, uint8_t channel, uint32_t event_value);
void dspic33_device_internal_run_timer_pmd(Dspic33* cpu, uint16_t timer, uint32_t value);
void dspic33_device_internal_run_uart(Dspic33* cpu, uint8_t channel, uint32_t event_value);
void dspic33_device_internal_run_usb_pmd(Dspic33* cpu, uint32_t value);
void dspic33_device_internal_run_usb(Dspic33* cpu, uint16_t slot);
void dspic33_device_internal_set_timer_gate(Dspic33* cpu, uint8_t timer_index, bool is_gate_high);
void dspic33_device_internal_spi_clear_buffers(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_spi_complete_transfer(Dspic33* cpu, uint8_t channel, uint16_t value);
void dspic33_device_internal_spi_raise_mode(Dspic33* cpu, uint8_t channel, uint8_t mode);
void dspic33_device_internal_spi_refresh_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_spi_refresh_status(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_spi_remove_internal_events(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_spi_schedule_current(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_spi_schedule_frame_input_sample(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_spi_start_next(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_spi_update_power_state(Dspic33* cpu);
void dspic33_device_internal_spi_update_slave_selection(Dspic33* cpu, uint8_t channel,
                                                        bool previous, bool selected);
void dspic33_device_internal_start_automatic_oscillator_switch(Dspic33* cpu, uint8_t source);
void dspic33_device_internal_uart_auto_baud_edge(Dspic33* cpu, uint8_t channel, bool previous_high,
                                                 bool high);
void dspic33_device_internal_uart_begin_physical_receive(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_cancel_physical_receive(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_clear_receive(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_clear_transmit(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_disable_module(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_raise_transmit(Dspic33* cpu, uint8_t channel, bool dma);
void dspic33_device_internal_uart_read_complete(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_receive_complete(Dspic33* cpu, uint8_t channel,
                                                   const Dspic33UartFrame* incoming);
void dspic33_device_internal_uart_refresh_pps_inputs(Dspic33* cpu);
void dspic33_device_internal_uart_refresh_status(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_reset_auto_baud(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_reset_runtime(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_schedule_transmit(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_start_transmit(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_uart_update_power_state(Dspic33* cpu);
void dspic33_device_internal_update_adc_pmd(Dspic33* cpu, uint16_t address, uint16_t previous);
void dspic33_device_internal_update_adc_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                 uint16_t requested);
void dspic33_device_internal_update_can_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                 uint16_t requested);
void dspic33_device_internal_update_comparator_register(Dspic33* cpu, uint16_t address,
                                                        uint16_t previous, uint16_t requested);
void dspic33_device_internal_update_crc_pmd(Dspic33* cpu, uint16_t previous);
void dspic33_device_internal_update_crc_register(Dspic33* cpu, uint16_t access_address,
                                                 uint16_t previous_word, uint16_t requested_word);
void dspic33_device_internal_update_dci_register(Dspic33* cpu, uint16_t address, uint16_t previous);
void dspic33_device_internal_update_dma_control(Dspic33* cpu, uint8_t channel_index,
                                                uint16_t previous_control);
void dspic33_device_internal_update_dma_request(Dspic33* cpu, uint8_t channel_index,
                                                uint16_t previous_request);
void dspic33_device_internal_update_gpio_latch(Dspic33* cpu, uint16_t address, uint16_t requested);
void dspic33_device_internal_update_input_capture_pmd(Dspic33* cpu, uint16_t address,
                                                      uint16_t previous);
void dspic33_device_internal_update_input_capture_register(Dspic33* cpu, uint16_t address,
                                                           uint16_t previous);
void dspic33_device_internal_update_main_clock_configuration(Dspic33* cpu, uint16_t address,
                                                             uint16_t previous_value);
void dspic33_device_internal_update_nvm_control(Dspic33* cpu, uint16_t requested_control);
void dspic33_device_internal_update_nvm_key(Dspic33* cpu, uint16_t key_word);
void dspic33_device_internal_update_output_compare_pmd(Dspic33* cpu, uint16_t address,
                                                       uint16_t previous);
void dspic33_device_internal_update_output_compare_register(Dspic33* cpu, uint16_t address,
                                                            uint16_t previous);
void dspic33_device_internal_update_pmp_pmd(Dspic33* cpu, uint16_t previous);
void dspic33_device_internal_update_pmp_register(Dspic33* cpu, uint16_t address, uint16_t previous);
void dspic33_device_internal_update_pwm_pmd(Dspic33* cpu, uint16_t address, uint16_t previous);
void dspic33_device_internal_update_pwm_register(Dspic33* cpu, uint16_t address, uint16_t previous);
void dspic33_device_internal_update_qei_register(Dspic33* cpu, uint16_t address,
                                                 uint16_t previous_value, uint16_t requested_value);
void dspic33_device_internal_update_rtcc_register(Dspic33* cpu, uint16_t address,
                                                  uint16_t previous);
void dspic33_device_internal_update_spi_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                 uint16_t requested);
void dspic33_device_internal_update_timer_pmd(Dspic33* cpu, uint16_t address, uint16_t previous);
void dspic33_device_internal_update_timer_register(Dspic33* cpu, uint16_t address,
                                                   uint16_t previous);
void dspic33_device_internal_update_uart_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                  uint16_t requested);
void dspic33_device_internal_update_usb_pmd(Dspic33* cpu, uint16_t address, uint16_t previous);
void dspic33_device_internal_update_usb_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                 uint16_t requested);
void dspic33_device_internal_usb_pop_transaction_status(Dspic33* cpu);
void dspic33_device_internal_usb_refresh_activity_pending(Dspic33* cpu);
void dspic33_device_internal_usb_refresh_interrupt(Dspic33* cpu);
void dspic33_device_internal_usb_refresh_transaction_status(Dspic33* cpu);
void dspic33_device_internal_usb_reset_ping_pong(Dspic33* cpu);
void dspic33_device_internal_usb_reset_registers(Dspic33* cpu);
void dspic33_device_internal_usb_set_error(Dspic33* cpu, uint8_t error);
void dspic33_device_internal_usb_update_power_state(Dspic33* cpu);

bool dspic33_device_internal_can_buffer_flag(const Dspic33* cpu, uint8_t channel, uint8_t buffer,
                                             bool overflow);
bool dspic33_device_internal_can_dma_ready(const Dspic33* cpu, uint8_t request, uint16_t pad,
                                           bool transmit);
bool dspic33_device_internal_can_schedule_intermission(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_can_start_overload(Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_can_schedule_receive_sample(Dspic33* cpu, uint8_t channel,
                                                         uint64_t delay);
bool dspic33_device_internal_can_schedule_transmit_sample(Dspic33* cpu, uint8_t channel,
                                                          uint64_t delay);
bool dspic33_device_internal_can_select_receive_buffer(Dspic33* cpu, uint8_t channel,
                                                       const Dspic33CanFrame* frame,
                                                       uint8_t* buffer, uint8_t* matched_filter);
bool dspic33_device_internal_can_serial_receive_enabled(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_can_triple_sample(const Dspic33* cpu, uint8_t channel);
Dspic33CanSerialResult dspic33_device_internal_can_decode_serial(const Dspic33* cpu,
                                                                 uint8_t channel,
                                                                 Dspic33CanFrame* frame,
                                                                 uint16_t* tail_start);
uint64_t dspic33_device_internal_can_first_sample_delay(const Dspic33* cpu, uint8_t channel);
uint64_t dspic33_device_internal_can_frame_cycles(const Dspic33* cpu, uint8_t channel,
                                                  const Dspic33CanFrame* frame);
uint64_t dspic33_device_internal_can_sample_cycles(const Dspic33* cpu, uint8_t channel);
uint64_t dspic33_device_internal_can_time_quantum(const Dspic33* cpu, uint8_t channel);
uint8_t dspic33_device_internal_can_advance_fifo_write(Dspic33* cpu, uint8_t channel,
                                                       uint8_t buffer);
uint8_t dspic33_device_internal_can_buffer_count(const Dspic33* cpu, uint8_t channel);
uint8_t dspic33_device_internal_can_fifo_end(const Dspic33* cpu, uint8_t channel);
uint8_t dspic33_device_internal_can_filter_buffer(const Dspic33* cpu, uint8_t channel,
                                                  uint8_t filter);
void dspic33_device_internal_can_capture_received_frame(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_can_encode_frame(const Dspic33CanFrame* frame, uint8_t filter,
                                              uint16_t words[8]);
void dspic33_device_internal_can_error_event(Dspic33* cpu, uint8_t channel, uint32_t value);
void dspic33_device_internal_can_intermission_finish(Dspic33* cpu, uint8_t channel, uint32_t value);
void dspic33_device_internal_can_monitor_transmit_sample(Dspic33* cpu, uint8_t channel,
                                                         bool bus_high);
void dspic33_device_internal_can_overload_finish(Dspic33* cpu, uint8_t channel, uint32_t value);
void dspic33_device_internal_can_receive_error(Dspic33* cpu, uint8_t channel,
                                               const Dspic33CanFrame* frame);
void dspic33_device_internal_can_receive_success(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_can_set_buffer_flag(Dspic33* cpu, uint8_t channel, uint8_t buffer,
                                                 bool overflow);

bool dspic33_device_internal_output_compare_boundary(Dspic33* cpu, uint8_t channel, uint16_t mode);
bool dspic33_device_internal_output_compare_cascade_controls_supported(uint8_t channel,
                                                                       uint16_t control1,
                                                                       uint16_t control2);
bool dspic33_device_internal_output_compare_cascade_owner(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_output_compare_cascade_supported(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_output_compare_configuration_supported(uint8_t channel,
                                                                    uint16_t control1,
                                                                    uint16_t control2);
bool dspic33_device_internal_output_compare_fp_clocked(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_output_compare_operating(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_output_compare_primary_match(Dspic33* cpu, uint8_t channel,
                                                          uint16_t mode);
bool dspic33_device_internal_output_compare_schedule_next(Dspic33* cpu, uint8_t channel,
                                                          uint64_t initial_delay);
bool dspic33_device_internal_output_compare_secondary_match(Dspic33* cpu, uint8_t channel,
                                                            uint16_t mode);
bool dspic33_device_internal_output_compare_timer_owner(const Dspic33* cpu, uint8_t channel);
uint32_t dspic33_device_internal_output_compare_cascade_timer(const Dspic33* cpu, uint8_t channel);
uint64_t dspic33_device_internal_output_compare_next_timer_event(const Dspic33* cpu,
                                                                 uint8_t channel, uint32_t* kind);
uint8_t dspic33_device_internal_output_compare_output_channel(const Dspic33* cpu, uint8_t channel);
uint8_t dspic33_device_internal_output_compare_timer_source(const Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_output_compare_adopt_input_capture_timer(Dspic33* cpu, uint8_t channel,
                                                                      uint8_t source);
void dspic33_device_internal_output_compare_pulse_sync_source(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_output_compare_refresh_fault(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_output_compare_set_high(Dspic33* cpu, uint8_t channel, bool high);
void dspic33_device_internal_output_compare_start_cascade(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_output_compare_start(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_output_compare_stop_cascade(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_output_compare_stop(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_output_compare_write_cascade_timer(Dspic33* cpu, uint8_t channel,
                                                                uint32_t timer);

#endif
