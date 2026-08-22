#include "device/dspic33ep_mu/internal.h"

bool dspic33_device_internal_usb_queue_push(Dspic33UsbQueue* queue,
                                            const Dspic33UsbPacket* packet) {
    uint8_t index;
    if (queue->count == DSPIC33_USB_PACKET_QUEUE_SIZE) {
        return false;
    }
    index = (uint8_t)((queue->head + queue->count) % DSPIC33_USB_PACKET_QUEUE_SIZE);
    queue->packets[index] = *packet;
    queue->count++;
    return true;
}

bool dspic33_device_internal_usb_queue_pop(Dspic33UsbQueue* queue, Dspic33UsbPacket* packet) {
    if (queue->count == 0u) {
        return false;
    }
    *packet = queue->packets[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % DSPIC33_USB_PACKET_QUEUE_SIZE);
    queue->count--;
    return true;
}

static uint32_t usb_bdt_base(const Dspic33* cpu) {
    return ((uint32_t)(dspic33_device_internal_raw_word(cpu, USB_BDTP3) & 0x00ffu) << 24u) |
           ((uint32_t)(dspic33_device_internal_raw_word(cpu, USB_BDTP2) & 0x00ffu) << 16u) |
           ((uint32_t)(dspic33_device_internal_raw_word(cpu, USB_BDTP1) & 0x00feu) << 8u);
}

static uint32_t usb_descriptor_address(const Dspic33* cpu, uint8_t endpoint, uint8_t direction,
                                       uint8_t bank) {
    uint32_t index = (uint32_t)endpoint * 4u + (uint32_t)direction * 2u + bank;
    return usb_bdt_base(cpu) + index * 8u;
}

static bool usb_memory_word(const Dspic33* cpu, uint32_t address, uint16_t* value) {
    if (!dspic33_data_range_valid(address, 2u)) {
        return false;
    }
    *value = (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
    return true;
}

static bool usb_write_memory_word(Dspic33* cpu, uint32_t address, uint16_t value) {
    if (!dspic33_data_range_valid(address, 2u)) {
        return false;
    }
    cpu->data[address] = (uint8_t)value;
    cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    return true;
}

void dspic33_device_internal_usb_refresh_activity_pending(Dspic33* cpu) {
    uint16_t power = dspic33_device_internal_raw_word(cpu, USB_PWRC);
    bool pending = dspic33_device_internal_raw_word(cpu, USB_IR) != 0u ||
                   dspic33_device_internal_raw_word(cpu, USB_EIR) != 0u ||
                   dspic33_device_internal_raw_word(cpu, USB_OTGIR) != 0u;
    if ((power & USB_SLEEP_GUARD) != 0u && pending) {
        power |= USB_ACTIVITY_PENDING;
    } else {
        power &= (uint16_t)~USB_ACTIVITY_PENDING;
    }
    dspic33_device_internal_raw_write_word(cpu, USB_PWRC, power);
}

bool dspic33_device_internal_usb_descriptor(const Dspic33* cpu, uint8_t endpoint, uint8_t direction,
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

static bool usb_write_descriptor(Dspic33* cpu, uint8_t endpoint, uint8_t direction, uint8_t bank,
                                 const uint16_t words[4]) {
    uint32_t address = usb_descriptor_address(cpu, endpoint, direction, bank);
    uint8_t index;
    for (index = 0u; index < 4u; index++) {
        if (!usb_write_memory_word(cpu, address + index * 2u, words[index])) {
            return false;
        }
    }
    return true;
}

void dspic33_device_internal_usb_refresh_interrupt(Dspic33* cpu) {
    uint16_t errors = (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_EIR) &
                                 dspic33_device_internal_raw_word(cpu, USB_EIE));
    uint16_t interrupts = dspic33_device_internal_raw_word(cpu, USB_IR);
    if (errors != 0u) {
        interrupts |= USB_ERROR_INTERRUPT;
    } else {
        interrupts &= (uint16_t)~USB_ERROR_INTERRUPT;
    }
    dspic33_device_internal_raw_write_word(cpu, USB_IR, interrupts);
    dspic33_device_internal_usb_refresh_activity_pending(cpu);
    if ((interrupts & dspic33_device_internal_raw_word(cpu, USB_IE)) != 0u ||
        (dspic33_device_internal_raw_word(cpu, USB_OTGIR) &
         dspic33_device_internal_raw_word(cpu, USB_OTGIE)) != 0u) {
        dspic33_raise_interrupt(cpu, USB_IRQ);
    }
}

void dspic33_device_internal_usb_set_error(Dspic33* cpu, uint8_t error) {
    dspic33_device_internal_raw_write_word(
        cpu, USB_EIR,
        (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_EIR) | (error & 0x00ffu)));
    dspic33_device_internal_usb_refresh_interrupt(cpu);
}

void dspic33_device_internal_usb_refresh_transaction_status(Dspic33* cpu) {
    uint16_t interrupts = dspic33_device_internal_raw_word(cpu, USB_IR);
    if (cpu->io.usb_status_count == 0u) {
        dspic33_device_internal_raw_write_word(cpu, USB_STAT, 0u);
        interrupts &= (uint16_t)~USB_TRANSACTION_INTERRUPT;
    } else {
        dspic33_device_internal_raw_write_word(cpu, USB_STAT,
                                               cpu->io.usb_status[cpu->io.usb_status_head]);
        interrupts |= USB_TRANSACTION_INTERRUPT;
    }
    dspic33_device_internal_raw_write_word(cpu, USB_IR, interrupts);
    dspic33_device_internal_usb_refresh_interrupt(cpu);
}

static void usb_push_transaction_status(Dspic33* cpu, uint8_t status) {
    uint8_t index;
    if (cpu->io.usb_status_count == sizeof(cpu->io.usb_status)) {
        dspic33_device_internal_usb_set_error(cpu, USB_ERROR_DMA);
        return;
    }
    index = (uint8_t)((cpu->io.usb_status_head + cpu->io.usb_status_count) %
                      sizeof(cpu->io.usb_status));
    cpu->io.usb_status[index] = status;
    cpu->io.usb_status_count++;
    dspic33_device_internal_usb_refresh_transaction_status(cpu);
}

void dspic33_device_internal_usb_pop_transaction_status(Dspic33* cpu) {
    if (cpu->io.usb_status_count != 0u) {
        cpu->io.usb_status_head =
            (uint8_t)((cpu->io.usb_status_head + 1u) % sizeof(cpu->io.usb_status));
        cpu->io.usb_status_count--;
    }
    dspic33_device_internal_usb_refresh_transaction_status(cpu);
}

void dspic33_device_internal_usb_reset_ping_pong(Dspic33* cpu) {
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
    dspic33_device_internal_usb_reset_ping_pong(cpu);
}

void dspic33_device_internal_usb_reset_registers(Dspic33* cpu) {
    uint16_t address;
    dspic33_device_internal_raw_write_word(cpu, USB_OTGIR, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_OTGIE, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_OTGSTAT, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_OTGCON, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_IR, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_IE, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_EIR, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_EIE, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_STAT, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_CON, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_ADDR, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_BDTP1, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_FRML, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_FRMH, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_TOK, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_SOF, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_BDTP2, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_BDTP3, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_CNFG1, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_CNFG2, 0u);
    for (address = USB_EP0; address < USB_EP0 + DSPIC33_USB_ENDPOINT_COUNT * 2u; address += 2u) {
        dspic33_device_internal_raw_write_word(cpu, address, 0u);
    }
    dspic33_device_internal_raw_write_word(cpu, USB_PWMRRS, 0u);
    dspic33_device_internal_raw_write_word(cpu, USB_PWMCON, 0u);
    usb_reset_runtime(cpu);
}

static void usb_set_frame(Dspic33* cpu, uint16_t frame) {
    frame &= 0x07ffu;
    dspic33_device_internal_raw_write_word(cpu, USB_FRML, frame & 0x00ffu);
    dspic33_device_internal_raw_write_word(cpu, USB_FRMH, frame >> 8u);
}

static uint16_t usb_frame(const Dspic33* cpu) {
    return (uint16_t)(((dspic33_device_internal_raw_word(cpu, USB_FRMH) & 7u) << 8u) |
                      (dspic33_device_internal_raw_word(cpu, USB_FRML) & 0xffu));
}

static void usb_response(Dspic33* cpu, const Dspic33UsbPacket* token, Dspic33UsbHandshake handshake,
                         const uint8_t* data, uint16_t size, bool data1) {
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
    dspic33_device_internal_usb_queue_push(&cpu->io.usb_tx, &response);
}

bool dspic33_device_internal_usb_read_memory(const Dspic33* cpu, uint32_t address, uint8_t* data,
                                             uint16_t size, bool increment) {
    uint16_t index;
    for (index = 0u; index < size; index++) {
        uint32_t current = address + (increment ? index : 0u);
        if (!dspic33_data_range_valid(current, 1u)) {
            return false;
        }
        data[index] = cpu->data[current];
    }
    return true;
}

static bool usb_write_memory(Dspic33* cpu, uint32_t address, const uint8_t* data, uint16_t size,
                             bool increment) {
    uint16_t index;
    for (index = 0u; index < size; index++) {
        uint32_t current = address + (increment ? index : 0u);
        if (!dspic33_data_range_valid(current, 1u)) {
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
            if (dspic33_device_internal_usb_descriptor(cpu, endpoint, direction, bank, words)) {
                words[0] &= (uint16_t)~USB_DESCRIPTOR_STALL;
                usb_write_descriptor(cpu, endpoint, direction, bank, words);
            }
        }
    }
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(USB_EP0 + endpoint * 2u),
        (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(USB_EP0 + endpoint * 2u)) &
                   ~USB_ENDPOINT_STALL));
}

static bool usb_device_active(const Dspic33* cpu) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, USB_CON);
    return !cpu->io.usb_pmd_disabled &&
           (dspic33_device_internal_raw_word(cpu, USB_PWRC) & USB_POWER) != 0u &&
           (dspic33_device_internal_raw_word(cpu, USB_PWRC) & USB_SUSPEND) == 0u &&
           (control & (USB_ENABLE | USB_HOST_ENABLE | USB_PACKET_DISABLE)) == USB_ENABLE;
}

static bool usb_device_ready(const Dspic33* cpu, uint8_t endpoint, uint8_t pid) {
    uint16_t endpoint_control;
    if (endpoint >= DSPIC33_USB_ENDPOINT_COUNT || !usb_device_active(cpu)) {
        return false;
    }
    endpoint_control = dspic33_device_internal_raw_word(cpu, (uint16_t)(USB_EP0 + endpoint * 2u));
    if (pid == DSPIC33_USB_PID_IN) {
        return (endpoint_control & USB_ENDPOINT_TX_ENABLE) != 0u;
    }
    if ((endpoint_control & USB_ENDPOINT_RX_ENABLE) == 0u) {
        return false;
    }
    return pid != DSPIC33_USB_PID_SETUP || (endpoint_control & USB_ENDPOINT_CONTROL_DISABLED) == 0u;
}

static bool usb_endpoint_handshake(const Dspic33* cpu, uint8_t endpoint) {
    return (dspic33_device_internal_raw_word(cpu, (uint16_t)(USB_EP0 + endpoint * 2u)) &
            USB_ENDPOINT_HANDSHAKE) != 0u;
}

static void usb_complete_descriptor(Dspic33* cpu, uint8_t endpoint, uint8_t direction, uint8_t bank,
                                    uint16_t words[4], uint8_t pid, bool data1, uint16_t count,
                                    bool keep) {
    if (!keep) {
        words[0] = (uint16_t)((data1 ? USB_DESCRIPTOR_DATA1 : 0u) | ((uint16_t)pid << 2u));
    }
    words[1] = count & USB_DESCRIPTOR_COUNT_MASK;
    usb_write_descriptor(cpu, endpoint, direction, bank, words);
    if (!keep) {
        cpu->io.usb_next_bank[endpoint][direction] ^= 1u;
    }
    usb_push_transaction_status(cpu,
                                (uint8_t)((endpoint << 4u) | (direction << 3u) | (bank << 2u)));
}

static void usb_run_device_token(Dspic33* cpu, const Dspic33UsbPacket* token) {
    uint8_t response_data[DSPIC33_USB_PACKET_SIZE];
    uint8_t direction = token->pid == DSPIC33_USB_PID_IN ? 1u : 0u;
    uint8_t bank = (dspic33_device_internal_raw_word(cpu, USB_CON) & USB_PING_PONG_RESET) != 0u
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
    if (token->address != (dspic33_device_internal_raw_word(cpu, USB_ADDR) & 0x007fu)) {
        usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_TIMEOUT, NULL, 0u, false);
        return;
    }
    if (!usb_device_ready(cpu, token->endpoint, token->pid)) {
        usb_response(cpu, token,
                     usb_endpoint_handshake(cpu, token->endpoint) ? DSPIC33_USB_HANDSHAKE_NAK
                                                                  : DSPIC33_USB_HANDSHAKE_TIMEOUT,
                     NULL, 0u, false);
        return;
    }
    if (token->pid == DSPIC33_USB_PID_SETUP) {
        usb_clear_endpoint_stalls(cpu, token->endpoint);
        bank = 0u;
        cpu->io.usb_next_bank[token->endpoint][0] = 0u;
        dspic33_device_internal_raw_write_word(
            cpu, USB_CON,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_CON) | USB_PACKET_DISABLE));
    }
    if (!dspic33_device_internal_usb_descriptor(cpu, token->endpoint, direction, bank, words)) {
        dspic33_device_internal_usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
        usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_ERROR, NULL, 0u, false);
        return;
    }
    if ((words[0] & USB_DESCRIPTOR_OWNED) == 0u) {
        usb_response(cpu, token,
                     usb_endpoint_handshake(cpu, token->endpoint) ? DSPIC33_USB_HANDSHAKE_NAK
                                                                  : DSPIC33_USB_HANDSHAKE_TIMEOUT,
                     NULL, 0u, false);
        return;
    }
    if ((words[0] & USB_DESCRIPTOR_STALL) != 0u && usb_endpoint_handshake(cpu, token->endpoint)) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(USB_EP0 + token->endpoint * 2u),
            (uint16_t)(dspic33_device_internal_raw_word(
                           cpu, (uint16_t)(USB_EP0 + token->endpoint * 2u)) |
                       USB_ENDPOINT_STALL));
        dspic33_device_internal_raw_write_word(
            cpu, USB_IR,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_IR) | USB_STALL_INTERRUPT));
        dspic33_device_internal_usb_refresh_interrupt(cpu);
        usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_STALL, NULL, 0u, false);
        return;
    }
    expected_data1 = (words[0] & USB_DESCRIPTOR_DATA1) != 0u;
    if (token->pid != DSPIC33_USB_PID_SETUP && (words[0] & USB_DESCRIPTOR_DTS_ENABLE) != 0u &&
        token->pid != DSPIC33_USB_PID_IN && token->data1 != expected_data1) {
        usb_response(cpu, token,
                     usb_endpoint_handshake(cpu, token->endpoint) ? DSPIC33_USB_HANDSHAKE_ACK
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
        if (!dspic33_device_internal_usb_read_memory(cpu, buffer, response_data, count,
                                                     increment)) {
            dspic33_device_internal_usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
            usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_ERROR, NULL, 0u, false);
            return;
        }
        usb_response(cpu, token,
                     usb_endpoint_handshake(cpu, token->endpoint) ? DSPIC33_USB_HANDSHAKE_ACK
                                                                  : DSPIC33_USB_HANDSHAKE_NONE,
                     response_data, count, expected_data1);
    } else {
        count = token->size < descriptor_count ? token->size : descriptor_count;
        if (!usb_write_memory(cpu, buffer, token->data, count, increment)) {
            dspic33_device_internal_usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
            usb_response(cpu, token, DSPIC33_USB_HANDSHAKE_ERROR, NULL, 0u, false);
            return;
        }
        if (token->size > descriptor_count) {
            dspic33_device_internal_usb_set_error(cpu, USB_ERROR_DMA);
        }
        usb_response(cpu, token,
                     usb_endpoint_handshake(cpu, token->endpoint) ? DSPIC33_USB_HANDSHAKE_ACK
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
    if (!cpu->io.usb_host_pending ||
        (dspic33_device_internal_raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u) {
        return;
    }
    direction = cpu->io.usb_host_pid == DSPIC33_USB_PID_IN ? 0u : 1u;
    bank = (dspic33_device_internal_raw_word(cpu, USB_CON) & USB_PING_PONG_RESET) != 0u
               ? 0u
               : cpu->io.usb_next_bank[0][direction];
    if (!dspic33_device_internal_usb_descriptor(cpu, 0u, direction, bank, words)) {
        dspic33_device_internal_usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
        response = NULL;
    } else if ((words[0] & USB_DESCRIPTOR_OWNED) == 0u) {
        dspic33_device_internal_usb_set_error(cpu, USB_ERROR_DMA);
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
        if (direction == 0u && !usb_write_memory(cpu, buffer, response->data, count, increment)) {
            dspic33_device_internal_usb_set_error(cpu, USB_ERROR_BUS_ACCESS);
            complete = false;
        }
    } else if (response != NULL && response->handshake == DSPIC33_USB_HANDSHAKE_NAK) {
        pid = 0x0au;
        complete = true;
    } else if (response != NULL && response->handshake == DSPIC33_USB_HANDSHAKE_STALL) {
        dspic33_device_internal_raw_write_word(
            cpu, USB_IR,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_IR) | USB_STALL_INTERRUPT));
        pid = 0x0eu;
        complete = true;
    } else if (response != NULL && response->handshake == DSPIC33_USB_HANDSHAKE_TIMEOUT) {
        dspic33_device_internal_usb_set_error(cpu, USB_ERROR_BTO);
        complete = true;
    } else if (response != NULL && response->handshake == DSPIC33_USB_HANDSHAKE_ERROR) {
        dspic33_device_internal_usb_set_error(cpu, response->error != 0u ? response->error
                                                                         : USB_ERROR_PID);
        complete = true;
    }
    if (complete) {
        usb_complete_descriptor(cpu, 0u, direction, bank, words, pid, response->data1, count, keep);
    }
    cpu->io.usb_last_handshake =
        response != NULL ? response->handshake : DSPIC33_USB_HANDSHAKE_ERROR;
    cpu->io.usb_host_pending = false;
    dspic33_device_internal_raw_write_word(
        cpu, USB_CON, (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_CON) & ~USB_TOKEN_BUSY));
    dspic33_device_internal_usb_refresh_interrupt(cpu);
}

static void usb_run_bus_event(Dspic33* cpu, Dspic33UsbBusEvent event, uint16_t value) {
    uint16_t status;
    switch (event) {
    case DSPIC33_USB_BUS_RESET:
        cpu->io.usb_bus_idle = false;
        dspic33_device_internal_raw_write_word(cpu, USB_ADDR, 0u);
        dspic33_device_internal_raw_write_word(
            cpu, USB_CON,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_CON) & ~USB_PACKET_DISABLE));
        cpu->io.usb_status_head = 0u;
        cpu->io.usb_status_count = 0u;
        dspic33_device_internal_usb_reset_ping_pong(cpu);
        if ((dspic33_device_internal_raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u) {
            dspic33_device_internal_raw_write_word(
                cpu, USB_IR,
                (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_IR) | USB_RESET_INTERRUPT));
        }
        dspic33_device_internal_usb_refresh_transaction_status(cpu);
        break;
    case DSPIC33_USB_BUS_SOF:
        cpu->io.usb_bus_idle = false;
        usb_set_frame(cpu, value == UINT16_MAX ? (uint16_t)(usb_frame(cpu) + 1u) : value);
        dspic33_device_internal_raw_write_word(
            cpu, USB_IR,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_IR) | USB_SOF_INTERRUPT));
        dspic33_device_internal_usb_refresh_interrupt(cpu);
        if ((dspic33_device_internal_raw_word(cpu, USB_PWRC) & USB_POWER) != 0u &&
            (dspic33_device_internal_raw_word(cpu, USB_CON) & (USB_HOST_ENABLE | USB_ENABLE)) ==
                (USB_HOST_ENABLE | USB_ENABLE)) {
            dspic33_device_internal_usb_schedule_bus_event(cpu, DSPIC33_USB_BUS_SOF, UINT16_MAX,
                                                           USB_FRAME_CYCLES, false);
        }
        break;
    case DSPIC33_USB_BUS_IDLE:
        if (!cpu->io.usb_bus_idle) {
            cpu->io.usb_bus_idle = true;
            dspic33_device_internal_raw_write_word(
                cpu, USB_IR,
                (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_IR) | USB_IDLE_INTERRUPT));
            dspic33_device_internal_usb_refresh_interrupt(cpu);
        }
        break;
    case DSPIC33_USB_BUS_RESUME:
        cpu->io.usb_bus_idle = false;
        dspic33_device_internal_raw_write_word(
            cpu, USB_PWRC,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_PWRC) & ~USB_SUSPEND));
        dspic33_device_internal_raw_write_word(
            cpu, USB_IR,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_IR) | USB_RESUME_INTERRUPT));
        dspic33_device_internal_raw_write_word(
            cpu, USB_OTGIR, (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_OTGIR) | 0x0010u));
        dspic33_device_internal_usb_refresh_interrupt(cpu);
        dspic33_device_internal_usb_update_power_state(cpu);
        break;
    case DSPIC33_USB_BUS_ATTACH:
        dspic33_device_internal_raw_write_word(
            cpu, USB_OTGSTAT,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_OTGSTAT) |
                       USB_OTG_VOLTAGE_STATUS));
        if ((dspic33_device_internal_raw_word(cpu, USB_CON) & USB_HOST_ENABLE) != 0u) {
            cpu->io.usb_host_attached = true;
            dspic33_device_internal_raw_write_word(
                cpu, USB_IR,
                (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_IR) | USB_ATTACH_INTERRUPT));
        }
        dspic33_device_internal_usb_refresh_interrupt(cpu);
        break;
    case DSPIC33_USB_BUS_DETACH:
        cpu->io.usb_host_attached = false;
        dspic33_device_internal_raw_write_word(
            cpu, USB_OTGSTAT,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_OTGSTAT) &
                       ~USB_OTG_VOLTAGE_STATUS));
        dspic33_device_internal_raw_write_word(
            cpu, USB_OTGIR, (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_OTGIR) | 0x0020u));
        if ((dspic33_device_internal_raw_word(cpu, USB_CON) & USB_HOST_ENABLE) != 0u) {
            dspic33_device_internal_raw_write_word(
                cpu, USB_IR,
                (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_IR) | USB_DETACH_INTERRUPT));
        }
        dspic33_device_internal_usb_refresh_interrupt(cpu);
        break;
    case DSPIC33_USB_BUS_ERROR:
        cpu->io.usb_bus_idle = false;
        dspic33_device_internal_usb_set_error(cpu, (uint8_t)value);
        break;
    case DSPIC33_USB_BUS_OTG_STATE:
        status = dspic33_device_internal_raw_word(cpu, USB_OTGSTAT);
        dspic33_device_internal_raw_write_word(cpu, USB_OTGSTAT, value & 0x00adu);
        if (status != (value & 0x00adu)) {
            dspic33_device_internal_raw_write_word(
                cpu, USB_OTGIR,
                (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_OTGIR) |
                           ((status ^ value) & 0x00adu)));
        }
        dspic33_device_internal_usb_refresh_interrupt(cpu);
        break;
    }
}

static bool usb_clock_available(const Dspic33* cpu) {
    return !cpu->io.usb_pmd_disabled &&
           (dspic33_device_internal_raw_word(cpu, USB_PWRC) & USB_SUSPEND) == 0u &&
           cpu->power_state != DSPIC33_POWER_SLEEP &&
           (cpu->power_state != DSPIC33_POWER_IDLE ||
            (dspic33_device_internal_raw_word(cpu, USB_CNFG1) & 0x0010u) == 0u);
}

static bool usb_sof_event(const Dspic33* cpu, const Dspic33Event* event) {
    const Dspic33UsbPending* pending;
    if (event->type != DSPIC33_EVENT_USB || event->source >= DSPIC33_USB_PENDING_COUNT) {
        return false;
    }
    pending = &cpu->io.usb_pending[event->source];
    return pending->active && pending->bus_event && pending->event == DSPIC33_USB_BUS_SOF;
}

static void usb_pause_sof_events(Dspic33* cpu) {
    size_t index;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (!usb_sof_event(cpu, event) || event->paused) {
            continue;
        }
        event->paused_remaining =
            event->cycle > cpu->device_cycles ? event->cycle - cpu->device_cycles : 0u;
        event->paused = true;
    }
    dspic33_reorder_events(cpu);
}

static void usb_resume_sof_events(Dspic33* cpu) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (!usb_sof_event(cpu, event) || !event->paused) {
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

void dspic33_device_internal_usb_update_power_state(Dspic33* cpu) {
    if (usb_clock_available(cpu)) {
        usb_resume_sof_events(cpu);
    } else {
        usb_pause_sof_events(cpu);
    }
}

void dspic33_device_internal_run_usb_pmd(Dspic33* cpu, uint32_t value) {
    uint16_t generation = (uint16_t)(value >> USB_PMD_EVENT_GENERATION_SHIFT);
    if (generation != cpu->io.usb_pmd_generation) {
        return;
    }
    cpu->io.usb_pmd_disabled = (value & USB_PMD_EVENT_DISABLED) != 0u;
    dspic33_device_internal_usb_update_power_state(cpu);
}

void dspic33_device_internal_run_usb(Dspic33* cpu, uint16_t slot) {
    Dspic33UsbPending* pending;
    bool power_stopped;
    if (slot >= DSPIC33_USB_PENDING_COUNT) {
        return;
    }
    pending = &cpu->io.usb_pending[slot];
    if (!pending->active) {
        return;
    }
    power_stopped = cpu->power_state == DSPIC33_POWER_SLEEP ||
                    (cpu->power_state == DSPIC33_POWER_IDLE &&
                     (dspic33_device_internal_raw_word(cpu, USB_CNFG1) & 0x0010u) != 0u);
    if (cpu->io.usb_pmd_disabled || power_stopped) {
        if (!cpu->io.usb_pmd_disabled && pending->bus_event &&
            pending->event == DSPIC33_USB_BUS_RESUME) {
            dspic33_device_internal_raw_write_word(
                cpu, USB_OTGIR,
                (uint16_t)(dspic33_device_internal_raw_word(cpu, USB_OTGIR) | 0x0010u));
            dspic33_device_internal_usb_refresh_interrupt(cpu);
        }
        pending->active = false;
        return;
    }
    if ((dspic33_device_internal_raw_word(cpu, USB_PWRC) & USB_SUSPEND) != 0u &&
        ((pending->bus_event && pending->event != DSPIC33_USB_BUS_RESUME) ||
         (!pending->bus_event &&
          (dspic33_device_internal_raw_word(cpu, USB_CON) & USB_HOST_ENABLE) != 0u))) {
        pending->active = false;
        return;
    }
    if (pending->bus_event) {
        usb_run_bus_event(cpu, pending->event, pending->value);
    } else if ((dspic33_device_internal_raw_word(cpu, USB_CON) & USB_HOST_ENABLE) != 0u &&
               pending->packet.handshake != DSPIC33_USB_HANDSHAKE_NONE) {
        usb_run_host_response(cpu, &pending->packet);
    } else {
        usb_run_device_token(cpu, &pending->packet);
    }
    pending->active = false;
}
