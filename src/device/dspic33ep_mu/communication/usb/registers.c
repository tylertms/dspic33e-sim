#include "device/dspic33ep_mu/internal.h"

static void usb_start_host_token(Dspic33* cpu) {
    Dspic33UsbPacket packet;
    uint16_t token = dspic33_device_internal_raw_word(cpu, USB_TOK);
    uint8_t direction;
    uint8_t bank;
    uint16_t words[4];
    uint32_t buffer;
    bool increment;
    memset(&packet, 0, sizeof(packet));
    packet.address = (uint8_t)(dspic33_device_internal_raw_word(cpu, USB_ADDR) & 0x007fu);
    packet.low_speed = (dspic33_device_internal_raw_word(cpu, USB_ADDR) & 0x0080u) != 0u;
    packet.endpoint = (uint8_t)(token & 0x0fu);
    packet.pid = (uint8_t)((token >> 4u) & 0x0fu);
    if (cpu->io.usb_pmd_disabled ||
        (dspic33_device_internal_raw_word(cpu, USB_PWRC) & USB_POWER) == 0u ||
        (dspic33_device_internal_raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u ||
        (dspic33_device_internal_raw_word(cpu, USB_CON) & USB_TOKEN_BUSY) != 0u ||
        (packet.pid != DSPIC33_USB_PID_OUT && packet.pid != DSPIC33_USB_PID_IN &&
         packet.pid != DSPIC33_USB_PID_SETUP)) {
        return;
    }
    direction = packet.pid == DSPIC33_USB_PID_IN ? 0u : 1u;
    bank = (dspic33_device_internal_raw_word(cpu, USB_CON) & USB_PING_PONG_RESET) != 0u
               ? 0u
               : cpu->io.usb_next_bank[0][direction];
    if (!dspic33_device_internal_usb_descriptor(cpu, 0u, direction, bank, words)) {
        dspic33_device_internal_usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
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
        if (!dspic33_device_internal_usb_read_memory(cpu, buffer, packet.data, packet.size,
                                                     increment)) {
            dspic33_device_internal_usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
            return;
        }
    }
    if (!dspic33_device_internal_usb_queue_push(&cpu->io.usb_tx, &packet)) {
        dspic33_device_internal_usb_set_error(cpu, USB_ERROR_DMA);
        return;
    }
    cpu->io.usb_host_pid = packet.pid;
    cpu->io.usb_host_endpoint = packet.endpoint;
    cpu->io.usb_host_pending = true;
    dspic33_device_internal_raw_write_word(
        cpu, USB_CON, (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_CON) | USB_TOKEN_BUSY));
}

void dspic33_device_internal_update_usb_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                 uint16_t requested) {
    uint16_t current = dspic33_device_internal_raw_word(cpu, address);
    if (address == USB_OTGIR) {
        dspic33_device_internal_raw_write_word(cpu, address,
                                               (uint16_t)(previous & ~(requested & 0x00fdu)));
        dspic33_device_internal_usb_refresh_interrupt(cpu);
        return;
    }
    if (address == USB_EIR) {
        dspic33_device_internal_raw_write_word(cpu, address,
                                               (uint16_t)(previous & ~(requested & 0x00ffu)));
        dspic33_device_internal_usb_refresh_interrupt(cpu);
        return;
    }
    if (address == USB_IR) {
        uint16_t cleared = requested & 0x00fdu;
        uint16_t remaining = (uint16_t)(previous & ~(cleared & ~USB_TRANSACTION_INTERRUPT));
        if ((dspic33_device_internal_raw_word(cpu, USB_CON) & USB_HOST_ENABLE) != 0u) {
            if ((previous & USB_ATTACH_INTERRUPT) != 0u && cpu->io.usb_host_attached) {
                remaining |= USB_ATTACH_INTERRUPT;
            }
            if ((previous & USB_DETACH_INTERRUPT) != 0u && !cpu->io.usb_host_attached) {
                remaining |= USB_DETACH_INTERRUPT;
            }
        }
        dspic33_device_internal_raw_write_word(cpu, address, remaining);
        if ((cleared & USB_TRANSACTION_INTERRUPT) != 0u) {
            dspic33_device_internal_usb_pop_transaction_status(cpu);
        } else {
            dspic33_device_internal_usb_refresh_transaction_status(cpu);
        }
        return;
    }
    if (address == USB_PWRC) {
        if ((previous & USB_POWER) != 0u && (current & USB_POWER) == 0u) {
            dspic33_device_internal_usb_reset_registers(cpu);
        } else {
            dspic33_device_internal_usb_refresh_activity_pending(cpu);
            dspic33_device_internal_usb_update_power_state(cpu);
        }
        return;
    }
    if (address == USB_CON) {
        bool previous_host = (previous & USB_HOST_ENABLE) != 0u;
        bool current_host = (current & USB_HOST_ENABLE) != 0u;
        if ((current & USB_PING_PONG_RESET) != 0u) {
            dspic33_device_internal_usb_reset_ping_pong(cpu);
        }
        if (previous_host != current_host) {
            dspic33_device_internal_raw_write_word(
                cpu, USB_IR,
                (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_IR) &
                           ~(USB_ATTACH_INTERRUPT | USB_DETACH_INTERRUPT)));
            cpu->io.usb_host_attached = false;
            dspic33_device_internal_usb_refresh_interrupt(cpu);
        }
        if (previous_host && !current_host) {
            cpu->io.usb_host_pending = false;
        }
        if ((current & (USB_HOST_ENABLE | USB_ENABLE)) == (USB_HOST_ENABLE | USB_ENABLE) &&
            (previous & (USB_HOST_ENABLE | USB_ENABLE)) != (USB_HOST_ENABLE | USB_ENABLE)) {
            dspic33_device_internal_usb_schedule_bus_event(cpu, DSPIC33_USB_BUS_SOF, UINT16_MAX,
                                                           USB_FRAME_CYCLES, false);
        }
        return;
    }
    if (address == USB_TOK) {
        usb_start_host_token(cpu);
        return;
    }
    if (address == USB_CNFG1) {
        dspic33_device_internal_usb_update_power_state(cpu);
        return;
    }
    if (address == USB_EIE || address == USB_IE || address == USB_OTGIE) {
        dspic33_device_internal_usb_refresh_interrupt(cpu);
        return;
    }
    if (address >= USB_EP0 && address < USB_EP0 + DSPIC33_USB_ENDPOINT_COUNT * 2u &&
        (requested & USB_ENDPOINT_STALL) == 0u) {
        dspic33_device_internal_raw_write_word(
            cpu, address,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, address) & ~USB_ENDPOINT_STALL));
    }
}
bool dspic33_device_internal_usb_register_address(uint16_t address) {
    return (address >= USB_OTGIR && address <= USB_PWRC) ||
           (address >= USB_IR && address < USB_EP0 + DSPIC33_USB_ENDPOINT_COUNT * 2u) ||
           address == USB_PWMRRS || address == USB_PWMCON;
}

void dspic33_device_internal_update_usb_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    bool disabled;
    if (address != USB_PMD_ADDRESS ||
        ((previous ^ dspic33_device_internal_raw_word(cpu, address)) & USB_PMD) == 0u) {
        return;
    }
    disabled = (dspic33_device_internal_raw_word(cpu, address) & USB_PMD) != 0u;
    cpu->io.usb_pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_USB_PMD, 0u,
                          ((uint32_t)cpu->io.usb_pmd_generation << USB_PMD_EVENT_GENERATION_SHIFT) |
                              (disabled ? USB_PMD_EVENT_DISABLED : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        dspic33_device_internal_raw_write_word(cpu, address, previous);
        cpu->io.usb_pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}
