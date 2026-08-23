#include "device/dspic33ep_mu/internal.h"

void dspic33_device_internal_uart_read_complete(Dspic33* cpu, uint8_t channel) {
    uint8_t channel_mask = (uint8_t)(1u << channel);
    Dspic33UartFrame discarded_frame;

    if (!dspic33_device_internal_uart_fifo_pop(&cpu->io.uart_rx_fifo[channel], &discarded_frame)) {
        return;
    }
    if ((cpu->io.uart_rx_hold_valid & channel_mask) != 0u &&
        dspic33_device_internal_uart_fifo_push(&cpu->io.uart_rx_fifo[channel],
                                               &cpu->io.uart_rx_hold[channel])) {
        memset(&cpu->io.uart_rx_hold[channel], 0, sizeof(cpu->io.uart_rx_hold[channel]));
        cpu->io.uart_rx_hold_valid &= (uint8_t)~channel_mask;
    }
    dspic33_device_internal_uart_refresh_status(cpu, channel);
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}

static void spi_restore_buffer(Dspic33* cpu, uint8_t channel, uint16_t fallback_word) {
    uint16_t restored_word;

    if (!dspic33_device_internal_word_queue_front(&cpu->io.spi_rx_fifo[channel], &restored_word)) {
        restored_word = fallback_word;
    }

    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 8u),
                                           restored_word);
}

bool dspic33_device_internal_spi_read_complete(const Dspic33* cpu, uint16_t address) {
    if (cpu->io.dma_transfer_active) {
        return cpu->io.dma_transfer_width == 1u || (address & 1u) != 0u;
    }
    if (cpu->io.cpu_read_valid) {
        return cpu->io.cpu_read_width == 1u || address == cpu->io.cpu_read_address + 1u;
    }
    return (address & 1u) != 0u;
}

void dspic33_device_internal_update_spi_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                 uint16_t requested) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = dspic33_device_spi_bases[channel];
        uint16_t offset;
        uint16_t control;
        uint16_t value;
        uint8_t capacity;
        if (address < base || address > base + 8u) {
            continue;
        }
        offset = (uint16_t)(address - base);
        if (offset == 0u) {
            uint16_t status = dspic33_device_internal_raw_word(cpu, base);
            if ((requested & SPI_OVERFLOW) != 0u) {
                status = (uint16_t)((status & ~SPI_OVERFLOW) | (previous & SPI_OVERFLOW));
            } else {
                status &= (uint16_t)~SPI_OVERFLOW;
            }
            dspic33_device_internal_raw_write_word(cpu, base, status);
            if ((previous & SPI_ENABLE) == 0u && (status & SPI_ENABLE) != 0u) {
                cpu->io.spi_clock_start_cycle[channel] = cpu->device_cycles;
            }
            if ((status & SPI_ENABLE) == 0u) {
                dspic33_device_internal_spi_clear_buffers(cpu, channel);
            } else {
                dspic33_device_internal_spi_refresh_status(cpu, channel);
            }
            return;
        }
        if (offset == 2u) {
            control = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
            if ((control & SPI_MASTER) == 0u) {
                control &= (uint16_t)~SPI_SAMPLE_END;
                dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 2u), control);
            }
            if (((control ^ previous) & (SPI_MODE_16 | SPI_MASTER)) != 0u) {
                dspic33_device_internal_spi_clear_buffers(cpu, channel);
            } else if ((previous & SPI_DISABLE_CLOCK) != 0u &&
                       (control & SPI_DISABLE_CLOCK) == 0u) {
                cpu->io.spi_generation[channel] =
                    (uint16_t)((cpu->io.spi_generation[channel] + 1u) & SPI_EVENT_GENERATION_MASK);
                dspic33_device_internal_spi_schedule_current(cpu, channel);
            }
            return;
        }
        if (offset == 4u) {
            control = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 4u));
            if (((control ^ previous) & SPI_ENHANCED) != 0u) {
                dspic33_device_internal_spi_clear_buffers(cpu, channel);
            } else if (((control ^ previous) & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE |
                                                SPI_FRAME_ACTIVE_HIGH | SPI_FRAME_DELAY)) != 0u) {
                uint8_t bit = (uint8_t)(1u << channel);
                cpu->io.spi_frame_active &= (uint8_t)~(1u << channel);
                cpu->io.spi_frame_output_pending &= (uint8_t)~bit;
                cpu->io.spi_frame_output_clear_pending &= (uint8_t)~bit;
                if ((cpu->io.spi_busy & bit) == 0u) {
                    cpu->io.spi_generation[channel] =
                        (uint16_t)((cpu->io.spi_generation[channel] + 1u) &
                                   SPI_EVENT_GENERATION_MASK);
                    dspic33_device_internal_spi_remove_internal_events(cpu, channel);
                }
                if ((previous & SPI_FRAME_ENABLE) == 0u && (control & SPI_FRAME_ENABLE) != 0u) {
                    cpu->io.spi_clock_start_cycle[channel] = cpu->device_cycles;
                }
                dspic33_device_internal_spi_start_next(cpu, channel);
            }
            return;
        }
        if (offset != 8u) {
            return;
        }
        value = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 8u));
        if ((dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) & SPI_MODE_16) == 0u) {
            value &= 0x00ffu;
        }
        capacity = dspic33_device_internal_spi_enhanced(cpu, channel) ? 8u : 1u;
        if ((dspic33_device_internal_raw_word(cpu, base) & SPI_ENABLE) == 0u ||
            dspic33_device_internal_spi_module_disabled(cpu, channel) ||
            cpu->io.spi_tx_fifo[channel].count >= capacity ||
            !dspic33_device_internal_word_queue_push(&cpu->io.spi_tx_fifo[channel], value)) {
            spi_restore_buffer(cpu, channel, previous);
            dspic33_device_internal_spi_refresh_status(cpu, channel);
            return;
        }
        if (dspic33_device_internal_spi_enhanced(cpu, channel) &&
            cpu->io.spi_tx_fifo[channel].count == 8u) {
            dspic33_device_internal_spi_raise_mode(cpu, channel, 7u);
        }
        dspic33_device_internal_spi_start_next(cpu, channel);
        spi_restore_buffer(cpu, channel, previous);
        dspic33_device_internal_spi_refresh_status(cpu, channel);
        return;
    }
    if (address == 0x0760u || address == 0x076au) {
        for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
            if (dspic33_device_internal_spi_module_disabled(cpu, channel)) {
                dspic33_device_internal_spi_clear_buffers(cpu, channel);
            }
        }
    }
}
