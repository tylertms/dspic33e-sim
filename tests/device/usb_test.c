#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device.h"
#include "dspic33.h"
#include "test.h"

enum {
    OTGIR = 0x0488u,
    OTGIE = 0x048au,
    OTGSTAT = 0x048cu,
    OTGCON = 0x048eu,
    PWRC = 0x0490u,
    IR = 0x04c0u,
    IE = 0x04c2u,
    EIR = 0x04c4u,
    EIE = 0x04c6u,
    STAT = 0x04c8u,
    CON = 0x04cau,
    ADDR = 0x04ccu,
    BDTP1 = 0x04ceu,
    FRML = 0x04d0u,
    FRMH = 0x04d2u,
    TOK = 0x04d4u,
    SOF = 0x04d6u,
    BDTP2 = 0x04d8u,
    BDTP3 = 0x04dau,
    CNFG1 = 0x04dcu,
    CNFG2 = 0x04deu,
    EP0 = 0x04e0u,
    PWMRRS = 0x0580u,
    PWMCON = 0x0582u,
    BDT = 0x6000u,
    BUFFER = 0x8000u,
    USB_IRQ = 86u,
    USB_INTERRUPT_PRIORITY = 3u,
    USB_VECTOR_ADDRESS = 0x1200u,
    USB_PMD_ADDRESS = 0x0766u,
    USB_PMD = 0x0001u,
    USB_FRAME_CYCLES = 60000u,
    CLKDIV = 0x0744u,
    OPCODE_MOV_W0_INDIRECT_W1 = 0x780880u,
    OPCODE_RESET = 0xfe0000u
};

static bool interrupt_flag(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

static void clear_interrupt(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    uint16_t mask = (uint16_t)(1u << (irq % 16u));
    dspic33_write_word(cpu, address,
                       (uint16_t)(dspic33_read_word(cpu, address) & ~mask));
}

static void enable_usb_interrupt(Dspic33* cpu) {
    uint16_t enable_address = (uint16_t)(0x0820u + (USB_IRQ / 16u) * 2u);
    uint16_t enable_mask = (uint16_t)(1u << (USB_IRQ % 16u));
    uint16_t priority_address = (uint16_t)(0x0840u + (USB_IRQ / 4u) * 2u);
    uint16_t priority_shift = (uint16_t)((USB_IRQ % 4u) * 4u);
    uint16_t priority_mask = (uint16_t)(7u << priority_shift);
    dspic33_write_word(
        cpu, enable_address,
        (uint16_t)(dspic33_read_word(cpu, enable_address) | enable_mask));
    dspic33_write_word(
        cpu, priority_address,
        (uint16_t)((dspic33_read_word(cpu, priority_address) & ~priority_mask) |
                   (USB_INTERRUPT_PRIORITY << priority_shift)));
    cpu->program[(0x0014u + USB_IRQ * 2u) / 2u] = USB_VECTOR_ADDRESS;
}

static bool service_usb_interrupt(Dspic33* cpu) {
    return dspic33_device_interrupt_pending(cpu) &&
           dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == USB_IRQ &&
           cpu->pc == USB_VECTOR_ADDRESS &&
           dspic33_read_word(cpu, 0x08c8u) ==
               (uint16_t)((USB_INTERRUPT_PRIORITY << 8u) | (USB_IRQ + 8u));
}

static uint32_t descriptor_address(uint8_t endpoint, uint8_t direction, uint8_t bank) {
    return BDT + ((uint32_t)endpoint * 4u + (uint32_t)direction * 2u + bank) * 8u;
}

static uint16_t memory_word(const Dspic33* cpu, uint32_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

static void write_memory_word(Dspic33* cpu, uint32_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[address + 1u] = (uint8_t)(value >> 8u);
}

static void write_descriptor(Dspic33* cpu, uint8_t endpoint, uint8_t direction,
                             uint8_t bank, uint16_t status, uint16_t count,
                             uint32_t buffer) {
    uint32_t address = descriptor_address(endpoint, direction, bank);
    write_memory_word(cpu, address, status);
    write_memory_word(cpu, address + 2u, count);
    write_memory_word(cpu, address + 4u, (uint16_t)buffer);
    write_memory_word(cpu, address + 6u, (uint16_t)(buffer >> 16u));
}

static void configure_device(Dspic33* cpu) {
    uint8_t endpoint;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PWRC, 1u);
    dspic33_write_word(cpu, BDTP1, 0x0060u);
    for (endpoint = 0u; endpoint < DSPIC33_USB_ENDPOINT_COUNT; endpoint++) {
        dspic33_write_word(cpu, (uint16_t)(EP0 + endpoint * 2u),
                           endpoint == 0u ? 0x000du : 0x001du);
    }
    dspic33_write_word(cpu, CON, 1u);
}

static void configure_host(Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PWRC, 1u);
    dspic33_write_word(cpu, BDTP1, 0x0060u);
    dspic33_write_word(cpu, CON, 0x0008u);
}

static void clear_transaction(Dspic33* cpu) { dspic33_write_word(cpu, IR, 0x0008u); }

static bool packet_data(const Dspic33UsbPacket* packet, const uint8_t* data,
                        uint16_t size) {
    return packet->size == size && memcmp(packet->data, data, size) == 0;
}

static void register_cases(TestState* state, Dspic33* cpu) {
    static const struct {
        uint16_t address;
        uint16_t mask;
    } masks[] = {{OTGIE, 0x00fdu},  {OTGCON, 0x00ffu}, {PWRC, 0x0013u},
                 {IE, 0x00bfu},     {EIE, 0x00ffu},    {ADDR, 0x00ffu},
                 {BDTP1, 0x00feu},  {SOF, 0x00ffu},    {BDTP2, 0x00ffu},
                 {BDTP3, 0x00ffu},  {CNFG1, 0x00d0u},  {CNFG2, 0x003fu},
                 {PWMRRS, 0xffffu}, {PWMCON, 0x8300u}};
    static const uint16_t read_only[] = {OTGIR, OTGSTAT, IR, EIR, STAT, FRML, FRMH};
    size_t index;
    uint8_t endpoint;
    for (index = 0u; index < sizeof(masks) / sizeof(masks[0]); index++) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, masks[index].address, 0xffffu);
        expect(state, dspic33_read_word(cpu, masks[index].address) == masks[index].mask,
               "USB register writable mask");
    }
    for (index = 0u; index < sizeof(read_only) / sizeof(read_only[0]); index++) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, read_only[index], 0xffffu);
        expect(state, dspic33_read_word(cpu, read_only[index]) == 0u,
               "USB read-only register");
    }
    for (endpoint = 0u; endpoint < DSPIC33_USB_ENDPOINT_COUNT; endpoint++) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(EP0 + endpoint * 2u), 0xffffu);
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(EP0 + endpoint * 2u)) ==
                   (endpoint == 0u ? 0x00dfu : 0x001fu),
               "USB endpoint writable mask");
    }
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, CON, 0xffffu);
    expect(state, dspic33_read_word(cpu, CON) == 0x002fu,
           "USB device to host control mask");
    dspic33_write_word(cpu, IE, 0xffffu);
    expect(state, dspic33_read_word(cpu, IE) == 0x00ffu,
           "USB host interrupt enable mask");
    dspic33_write_word(cpu, IE, 0x0040u);
    expect(state, dspic33_read_word(cpu, IE) == 0x0040u,
           "USB host attach interrupt enable visible");
    dspic33_write_word(cpu, CON, 0x0001u);
    expect(state, dspic33_read_word(cpu, IE) == 0u,
           "USB device mode hides host attach interrupt enable");
    dspic33_write_word(cpu, IE, 0u);
    expect(state, dspic33_read_word(cpu, IE) == 0u,
           "USB device mode ignores host attach interrupt enable write");
    dspic33_write_word(cpu, CON, 0x0008u);
    expect(state, dspic33_read_word(cpu, IE) == 0x0040u,
           "USB host mode restores hidden attach interrupt enable");
    configure_device(cpu);
    dspic33_write_word(cpu, ADDR, 0x005au);
    dspic33_write_word(cpu, CNFG1, 0x00d0u);
    dspic33_write_word(cpu, PWRC, 0u);
    expect(state,
           dspic33_read_word(cpu, ADDR) == 0u && dspic33_read_word(cpu, CNFG1) == 0u &&
               dspic33_read_word(cpu, PWRC) == 0u,
           "USB power clear resets registers");
    write_memory_word(cpu, BDT, 0xa55au);
    expect(state, memory_word(cpu, BDT) == 0xa55au,
           "USB power clear preserves BDT memory");
}

static void out_payload_domain(TestState* state, Dspic33* cpu) {
    uint8_t data[65];
    uint8_t endpoint;
    uint16_t size;
    for (endpoint = 0u; endpoint < DSPIC33_USB_ENDPOINT_COUNT; endpoint++) {
        configure_device(cpu);
        for (size = 0u; size <= sizeof(data); size++) {
            Dspic33UsbPacket response;
            uint8_t bank = (uint8_t)(size & 1u);
            bool data1 = (size & 1u) != 0u;
            uint16_t index;
            uint32_t buffer = BUFFER + endpoint * 0x100u;
            for (index = 0u; index < size; index++) {
                data[index] = (uint8_t)(endpoint * 17u + size + index * 3u);
            }
            memset(cpu->data + buffer, 0, sizeof(data));
            write_descriptor(cpu, endpoint, 0u, bank,
                             (uint16_t)(0x0088u | (data1 ? 0x0040u : 0u)), size,
                             buffer);
            expect(state,
                   dspic33_usb_receive_toggle(cpu, endpoint, data, size, data1, 1u) &&
                       dspic33_device_advance(cpu, 1u),
                   "USB OUT schedule");
            expect(state,
                   dspic33_usb_transmit(cpu, &response) &&
                       response.handshake == DSPIC33_USB_HANDSHAKE_ACK,
                   "USB OUT ACK");
            expect(state, memcmp(cpu->data + buffer, data, size) == 0,
                   "USB OUT payload");
            expect(state,
                   memory_word(cpu, descriptor_address(endpoint, 0u, bank)) ==
                           (uint16_t)(0x0004u | (data1 ? 0x0040u : 0u)) &&
                       memory_word(cpu, descriptor_address(endpoint, 0u, bank) + 2u) ==
                           size,
                   "USB OUT descriptor status");
            expect(state,
                   dspic33_read_word(cpu, STAT) ==
                           (uint16_t)((endpoint << 4u) | (bank << 2u)) &&
                       (dspic33_read_word(cpu, IR) & 0x0008u) != 0u,
                   "USB OUT transaction status");
            clear_transaction(cpu);
            expect(state, (dspic33_read_word(cpu, IR) & 0x0008u) == 0u,
                   "USB OUT status pop");
        }
    }
}

static void in_payload_domain(TestState* state, Dspic33* cpu) {
    uint8_t data[65];
    uint8_t endpoint;
    uint16_t size;
    for (endpoint = 0u; endpoint < DSPIC33_USB_ENDPOINT_COUNT; endpoint++) {
        configure_device(cpu);
        for (size = 0u; size <= sizeof(data); size++) {
            Dspic33UsbPacket response;
            uint8_t bank = (uint8_t)(size & 1u);
            bool data1 = (size & 1u) != 0u;
            uint16_t index;
            uint32_t buffer = BUFFER + endpoint * 0x100u;
            for (index = 0u; index < size; index++) {
                data[index] = (uint8_t)(endpoint * 29u + size + index * 5u);
            }
            memcpy(cpu->data + buffer, data, size);
            write_descriptor(cpu, endpoint, 1u, bank,
                             (uint16_t)(0x0088u | (data1 ? 0x0040u : 0u)), size,
                             buffer);
            expect(state,
                   dspic33_usb_request(cpu, endpoint, 1u) &&
                       dspic33_device_advance(cpu, 1u),
                   "USB IN schedule");
            expect(state,
                   dspic33_usb_transmit(cpu, &response) &&
                       response.handshake == DSPIC33_USB_HANDSHAKE_ACK,
                   "USB IN ACK");
            expect(state, packet_data(&response, data, size) && response.data1 == data1,
                   "USB IN payload");
            expect(state,
                   memory_word(cpu, descriptor_address(endpoint, 1u, bank)) ==
                       (uint16_t)(0x0024u | (data1 ? 0x0040u : 0u)),
                   "USB IN descriptor status");
            expect(state,
                   dspic33_read_word(cpu, STAT) ==
                       (uint16_t)((endpoint << 4u) | 0x0008u | (bank << 2u)),
                   "USB IN transaction status");
            clear_transaction(cpu);
        }
    }
}

static void descriptor_behavior_cases(TestState* state, Dspic33* cpu) {
    Dspic33UsbPacket response;
    uint8_t data[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    configure_device(cpu);
    expect(state, !dspic33_usb_setup(cpu, 0u, data, 7u, 0u),
           "USB SETUP requires eight bytes");
    expect(state,
           !dspic33_usb_token(cpu, 0u, 0u, DSPIC33_USB_PID_IN, data, 1u, false, 0u),
           "USB IN token has no payload");
    write_descriptor(cpu, 1u, 0u, 0u, 0x00c8u, 8u, BUFFER);
    expect(state,
           dspic33_usb_receive_toggle(cpu, 1u, data, sizeof(data), false, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB DTS mismatch schedule");
    expect(state,
           dspic33_usb_transmit(cpu, &response) &&
               response.handshake == DSPIC33_USB_HANDSHAKE_ACK &&
               memory_word(cpu, descriptor_address(1u, 0u, 0u)) == 0x00c8u,
           "USB DTS mismatch ignored");
    write_descriptor(cpu, 1u, 0u, 0u, 0x00a8u, 8u, BUFFER);
    expect(state,
           dspic33_usb_receive(cpu, 1u, data, sizeof(data), 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB KEEP schedule");
    expect(state,
           memory_word(cpu, descriptor_address(1u, 0u, 0u)) == 0x00a8u &&
               cpu->io.usb_next_bank[1][0] == 0u,
           "USB KEEP ownership and bank");
    dspic33_usb_transmit(cpu, &response);
    clear_transaction(cpu);
    expect(state,
           dspic33_usb_receive(cpu, 1u, data, sizeof(data), 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               dspic33_usb_transmit(cpu, &response) &&
               response.handshake == DSPIC33_USB_HANDSHAKE_ACK,
           "USB KEEP repeated transfer");
    clear_transaction(cpu);
    write_descriptor(cpu, 2u, 0u, 0u, 0x0098u, 8u, BUFFER + 0x100u);
    memset(cpu->data + BUFFER + 0x100u, 0, 8u);
    expect(state,
           dspic33_usb_receive(cpu, 2u, data, sizeof(data), 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB no-increment OUT schedule");
    expect(state,
           cpu->data[BUFFER + 0x100u] == data[7] && cpu->data[BUFFER + 0x101u] == 0u,
           "USB no-increment OUT behavior");
    dspic33_usb_transmit(cpu, &response);
    clear_transaction(cpu);
    write_descriptor(cpu, 3u, 0u, 0u, 0x0088u, 4u, BUFFER + 0x200u);
    expect(state,
           dspic33_usb_receive(cpu, 3u, data, sizeof(data), 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB overflow schedule");
    expect(state,
           memory_word(cpu, descriptor_address(3u, 0u, 0u) + 2u) == 4u &&
               (dspic33_read_word(cpu, EIR) & 0x0020u) != 0u,
           "USB overflow truncation and error");
    dspic33_usb_transmit(cpu, &response);
    clear_transaction(cpu);
    write_descriptor(cpu, 4u, 0u, 0u, 0x008cu, 8u, BUFFER + 0x300u);
    expect(state,
           dspic33_usb_receive(cpu, 4u, data, sizeof(data), 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB stall schedule");
    expect(state,
           dspic33_usb_transmit(cpu, &response) &&
               response.handshake == DSPIC33_USB_HANDSHAKE_STALL &&
               (dspic33_read_word(cpu, EP0 + 8u) & 2u) != 0u &&
               (dspic33_read_word(cpu, IR) & 0x0080u) != 0u,
           "USB stall response");
    dspic33_write_word(cpu, EP0 + 8u, 0x000du);
    write_descriptor(cpu, 4u, 0u, 0u, 0x008cu, 8u, BUFFER + 0x300u);
    expect(state,
           dspic33_usb_setup(cpu, 4u, data, 8u, 0u) && dspic33_device_advance(cpu, 0u),
           "USB SETUP schedule");
    expect(state,
           dspic33_usb_transmit(cpu, &response) &&
               response.handshake == DSPIC33_USB_HANDSHAKE_ACK &&
               (dspic33_read_word(cpu, CON) & 0x0020u) != 0u &&
               (dspic33_read_word(cpu, EP0 + 8u) & 2u) == 0u,
           "USB SETUP clears stall and disables packets");
    configure_device(cpu);
    dspic33_write_word(cpu, EP0 + 10u, 0x001cu);
    memcpy(cpu->data + BUFFER, data, sizeof(data));
    write_descriptor(cpu, 5u, 1u, 0u, 0x0088u, sizeof(data), BUFFER);
    expect(state,
           dspic33_usb_request(cpu, 5u, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_usb_transmit(cpu, &response),
           "USB isochronous IN schedule");
    expect(state,
           response.handshake == DSPIC33_USB_HANDSHAKE_NONE &&
               packet_data(&response, data, sizeof(data)),
           "USB isochronous transfer without handshake");
}

static void status_fifo_cases(TestState* state, Dspic33* cpu) {
    Dspic33UsbPacket response;
    uint8_t endpoint;
    configure_device(cpu);
    for (endpoint = 0u; endpoint < 4u; endpoint++) {
        write_descriptor(cpu, endpoint, 1u, 0u, 0x0088u, 0u, BUFFER);
        expect(state, dspic33_usb_request(cpu, endpoint, endpoint),
               "USB status FIFO schedule");
    }
    expect(state, dspic33_device_advance(cpu, 4u), "USB status FIFO advance");
    expect(state, cpu->io.usb_status_count == 4u && dspic33_read_word(cpu, STAT) == 8u,
           "USB status FIFO fills");
    for (endpoint = 0u; endpoint < 4u; endpoint++) {
        expect(state, dspic33_read_word(cpu, STAT) == (uint16_t)((endpoint << 4u) | 8u),
               "USB status FIFO order");
        clear_transaction(cpu);
        dspic33_usb_transmit(cpu, &response);
    }
    expect(state,
           cpu->io.usb_status_count == 0u && (dspic33_read_word(cpu, IR) & 8u) == 0u,
           "USB status FIFO empty");
}

static void boundary_and_order_cases(TestState* state, Dspic33* cpu) {
    uint8_t data[DSPIC33_USB_PACKET_SIZE];
    Dspic33UsbPacket packet;
    uint16_t index;
    uint32_t buffer = DSPIC33_DATA_SIZE - DSPIC33_USB_PACKET_SIZE;
    for (index = 0u; index < sizeof(data); index++) {
        data[index] = (uint8_t)(index * 37u + 11u);
    }
    configure_device(cpu);
    write_descriptor(cpu, 15u, 0u, 0u, 0x0088u, DSPIC33_USB_PACKET_SIZE, buffer);
    expect(state,
           dspic33_usb_receive(cpu, 15u, data, DSPIC33_USB_PACKET_SIZE, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB maximum OUT schedule");
    expect(state,
           memcmp(cpu->data + buffer, data, sizeof(data)) == 0 &&
               memory_word(cpu, descriptor_address(15u, 0u, 0u) + 2u) ==
                   DSPIC33_USB_PACKET_SIZE,
           "USB maximum OUT payload");
    expect(state,
           dspic33_usb_transmit(cpu, &packet) &&
               packet.handshake == DSPIC33_USB_HANDSHAKE_ACK,
           "USB maximum OUT ACK");
    clear_transaction(cpu);
    memcpy(cpu->data + buffer, data, sizeof(data));
    write_descriptor(cpu, 15u, 1u, 0u, 0x0088u, DSPIC33_USB_PACKET_SIZE, buffer);
    expect(state, dspic33_usb_request(cpu, 15u, 0u) && dspic33_device_advance(cpu, 0u),
           "USB maximum IN schedule");
    expect(state,
           dspic33_usb_transmit(cpu, &packet) &&
               packet_data(&packet, data, DSPIC33_USB_PACKET_SIZE),
           "USB maximum IN payload");
    clear_transaction(cpu);
    configure_device(cpu);
    write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 0u, BUFFER);
    write_descriptor(cpu, 2u, 1u, 0u, 0x0088u, 0u, BUFFER);
    expect(state, dspic33_usb_request(cpu, 1u, 3u) && dspic33_usb_request(cpu, 2u, 1u),
           "USB out-of-order schedule");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_usb_transmit(cpu, &packet) &&
               packet.endpoint == 2u,
           "USB earlier event first");
    clear_transaction(cpu);
    expect(state,
           dspic33_device_advance(cpu, 2u) && dspic33_usb_transmit(cpu, &packet) &&
               packet.endpoint == 1u,
           "USB later event second");
    clear_transaction(cpu);
    configure_device(cpu);
    write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 0u, BUFFER);
    expect(state, dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u),
           "USB bank advance schedule");
    dspic33_usb_transmit(cpu, &packet);
    clear_transaction(cpu);
    expect(state, cpu->io.usb_next_bank[1][1] == 1u, "USB bank advances to odd");
    dspic33_write_word(cpu, CON, 3u);
    expect(state, cpu->io.usb_next_bank[1][1] == 0u, "USB ping-pong reset forces even");
    dspic33_write_word(cpu, CON, 1u);
    configure_device(cpu);
    write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 0u, BUFFER);
    dspic33_write_word(cpu, ADDR, 5u);
    expect(state,
           dspic33_usb_token(cpu, 4u, 1u, DSPIC33_USB_PID_IN, NULL, 0u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) && dspic33_usb_transmit(cpu, &packet) &&
               packet.handshake == DSPIC33_USB_HANDSHAKE_TIMEOUT &&
               memory_word(cpu, descriptor_address(1u, 1u, 0u)) == 0x0088u,
           "USB device address mismatch");
    expect(state,
           dspic33_usb_token(cpu, 5u, 1u, DSPIC33_USB_PID_IN, NULL, 0u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) && dspic33_usb_transmit(cpu, &packet) &&
               packet.handshake == DSPIC33_USB_HANDSHAKE_ACK,
           "USB device address match");
    clear_transaction(cpu);
    configure_device(cpu);
    write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 0u, BUFFER);
    dspic33_write_word(cpu, PWRC, 3u);
    expect(state,
           dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_usb_transmit(cpu, &packet) &&
               packet.handshake == DSPIC33_USB_HANDSHAKE_TIMEOUT,
           "USB suspend timeout");
    configure_device(cpu);
    write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 0u, BUFFER);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_usb_request(cpu, 1u, 0u) &&
               dspic33_device_advance(cpu, 0u) && !dspic33_usb_transmit(cpu, &packet),
           "USB peripheral disable drops bus token");
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < DSPIC33_USB_PENDING_COUNT; index++) {
        expect(state, dspic33_usb_request(cpu, 0u, index),
               "USB pending slot allocation");
    }
    expect(state, !dspic33_usb_request(cpu, 0u, 0u), "USB pending slot capacity");
}

static void bus_access_error_cases(TestState* state, Dspic33* cpu) {
    Dspic33UsbPacket packet;
    uint8_t data = 0x5au;

    configure_device(cpu);
    dspic33_write_word(cpu, IE, 0x0002u);
    dspic33_write_word(cpu, EIE, 0x0040u);
    dspic33_write_word(cpu, BDTP1, 0u);
    dspic33_write_word(cpu, BDTP2, 0u);
    dspic33_write_word(cpu, BDTP3, 1u);
    expect(state,
           dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_usb_transmit(cpu, &packet) &&
               packet.handshake == DSPIC33_USB_HANDSHAKE_ERROR,
           "USB unimplemented BDT access");
    expect(state, (dspic33_read_word(cpu, EIR) & 0x0060u) == 0x0040u,
           "USB bus access flag without DMA error");
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0002u) != 0u && interrupt_flag(cpu, USB_IRQ),
           "USB bus access aggregate interrupt");
    dspic33_write_word(cpu, EIR, 0x0020u);
    expect(state,
           (dspic33_read_word(cpu, EIR) & 0x0060u) == 0x0040u &&
               (dspic33_read_word(cpu, IR) & 0x0002u) != 0u &&
               interrupt_flag(cpu, USB_IRQ),
           "USB bus access write zero preservation");
    dspic33_write_word(cpu, EIR, 0x0040u);
    expect(state,
           (dspic33_read_word(cpu, EIR) & 0x0060u) == 0u &&
               (dspic33_read_word(cpu, IR) & 0x0002u) == 0u &&
               interrupt_flag(cpu, USB_IRQ),
           "USB bus access write one clear");
    clear_interrupt(cpu, USB_IRQ);
    expect(state, !interrupt_flag(cpu, USB_IRQ),
           "USB bus access software interrupt clear");

    configure_device(cpu);
    write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 1u, DSPIC33_DATA_SIZE);
    expect(state, dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u),
           "USB out-of-range IN buffer schedule");
    expect(state,
           dspic33_usb_transmit(cpu, &packet) &&
               packet.handshake == DSPIC33_USB_HANDSHAKE_ERROR &&
               (dspic33_read_word(cpu, EIR) & 0x0060u) == 0x0040u,
           "USB out-of-range IN buffer access");

    configure_device(cpu);
    write_descriptor(cpu, 1u, 0u, 0u, 0x0088u, 1u, DSPIC33_DATA_SIZE);
    expect(state,
           dspic33_usb_receive(cpu, 1u, &data, 1u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB out-of-range OUT buffer schedule");
    expect(state,
           dspic33_usb_transmit(cpu, &packet) &&
               packet.handshake == DSPIC33_USB_HANDSHAKE_ERROR &&
               (dspic33_read_word(cpu, EIR) & 0x0060u) == 0x0040u,
           "USB out-of-range OUT buffer access");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PWRC, 1u);
    dspic33_write_word(cpu, BDTP1, 0x0060u);
    dspic33_write_word(cpu, CON, 0x0008u);
    write_descriptor(cpu, 0u, 1u, 0u, 0x0088u, 1u, DSPIC33_DATA_SIZE);
    dspic33_write_word(cpu, TOK, 0x0010u);
    expect(state, !dspic33_usb_transmit(cpu, &packet),
           "USB host out-of-range OUT buffer blocks token");
    expect(state, (dspic33_read_word(cpu, EIR) & 0x0060u) == 0x0040u,
           "USB host out-of-range OUT buffer access");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PWRC, 1u);
    dspic33_write_word(cpu, BDTP1, 0x0060u);
    dspic33_write_word(cpu, CON, 0x0008u);
    write_descriptor(cpu, 0u, 0u, 0u, 0x0088u, 1u, DSPIC33_DATA_SIZE);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state, dspic33_usb_transmit(cpu, &packet),
           "USB host out-of-range IN buffer token");
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_ACK, &data, 1u, false,
                                     0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host out-of-range IN buffer response");
    expect(state, (dspic33_read_word(cpu, EIR) & 0x0060u) == 0x0040u,
           "USB host out-of-range IN buffer access");
}

static void interrupt_and_bus_cases(TestState* state, Dspic33* cpu) {
    configure_device(cpu);
    dspic33_write_word(cpu, IE, 0x0037u);
    dspic33_write_word(cpu, EIE, 0x00ffu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESET, 0u, 1u) &&
               dspic33_usb_bus(cpu, DSPIC33_USB_BUS_SOF, 0x057au, 2u) &&
               dspic33_usb_bus(cpu, DSPIC33_USB_BUS_IDLE, 0u, 3u) &&
               dspic33_device_advance(cpu, 3u),
           "USB bus event schedule");
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0015u) == 0x0015u &&
               dspic33_read_word(cpu, FRML) == 0x007au &&
               dspic33_read_word(cpu, FRMH) == 5u,
           "USB reset SOF idle flags");
    expect(state, interrupt_flag(cpu, USB_IRQ), "USB enabled event IRQ");
    dspic33_write_word(cpu, PWRC, 3u);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESUME, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB resume schedule");
    expect(state,
           (dspic33_read_word(cpu, PWRC) & 2u) == 0u &&
               (dspic33_read_word(cpu, IR) & 0x0020u) != 0u &&
               (dspic33_read_word(cpu, OTGIR) & 0x0010u) != 0u,
           "USB resume state");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_ERROR, 0x0025u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB error schedule");
    expect(state,
           (dspic33_read_word(cpu, EIR) & 0x0025u) == 0x0025u &&
               (dspic33_read_word(cpu, IR) & 2u) != 0u,
           "USB error aggregation");
    dspic33_write_word(cpu, EIR, 0x0025u);
    expect(state,
           (dspic33_read_word(cpu, EIR) & 0x0025u) == 0u &&
               (dspic33_read_word(cpu, IR) & 2u) == 0u,
           "USB error W1C");
    dspic33_write_word(cpu, OTGIE, 0x00fdu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_OTG_STATE, 0x00adu, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB OTG state schedule");
    expect(state,
           dspic33_read_word(cpu, OTGSTAT) == 0x00adu &&
               (dspic33_read_word(cpu, OTGIR) & 0x00adu) != 0u,
           "USB OTG state interrupts");
    dspic33_write_word(cpu, OTGIR, 0x00fdu);
    expect(state, dspic33_read_word(cpu, OTGIR) == 0u, "USB OTG W1C");
}

static void idle_rearm_cases(TestState* state, Dspic33* cpu) {
    static const Dspic33UsbBusEvent resume_events[] = {
        DSPIC33_USB_BUS_RESUME,
        DSPIC33_USB_BUS_RESET,
        DSPIC33_USB_BUS_SOF,
        DSPIC33_USB_BUS_ERROR,
    };
    static const uint16_t resume_values[] = {0u, 0u, 1u, 1u};
    size_t index;
    configure_device(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_IDLE, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, IR) & 0x0010u) != 0u && cpu->io.usb_bus_idle,
           "B1 first USB idle indication sets UIDLE");
    dspic33_write_word(cpu, IR, 0x0010u);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_IDLE, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, IR) & 0x0010u) == 0u && cpu->io.usb_bus_idle,
           "B1 persistent USB idle does not reassert UIDLE");
    for (index = 0u; index < sizeof(resume_events) / sizeof(resume_events[0]);
         index++) {
        expect(state,
               dspic33_usb_bus(cpu, resume_events[index], resume_values[index], 0u) &&
                   dspic33_device_advance(cpu, 0u) && !cpu->io.usb_bus_idle,
               "B1 USB bus activity rearms UIDLE detection");
        dspic33_write_word(cpu, IR, 0xffffu);
        dspic33_write_word(cpu, EIR, 0xffffu);
        expect(state,
               dspic33_usb_bus(cpu, DSPIC33_USB_BUS_IDLE, 0u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   (dspic33_read_word(cpu, IR) & 0x0010u) != 0u && cpu->io.usb_bus_idle,
               "B1 USB idle reasserts after bus activity");
        dspic33_write_word(cpu, IR, 0x0010u);
    }
}

static void sleep_guard_cases(TestState* state, Dspic33* cpu) {
    configure_device(cpu);
    dspic33_write_word(cpu, PWRC, 0x0091u);
    expect(state, dspic33_read_word(cpu, PWRC) == 0x0011u,
           "USB activity pending is hardware controlled");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_SOF, 1u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, PWRC) & 0x0080u) != 0u,
           "USB notification sets guarded activity pending");
    dspic33_write_word(cpu, PWRC, 0x0011u);
    expect(state, (dspic33_read_word(cpu, PWRC) & 0x0080u) != 0u,
           "software cannot clear guarded activity pending");
    dspic33_write_word(cpu, IR, 0x0004u);
    expect(state, (dspic33_read_word(cpu, PWRC) & 0x0080u) == 0u,
           "clearing USB notification clears activity pending");

    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_ERROR, 1u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, PWRC) & 0x0080u) != 0u,
           "USB error notification sets activity pending");
    dspic33_write_word(cpu, EIR, 1u);
    expect(state, (dspic33_read_word(cpu, PWRC) & 0x0080u) == 0u,
           "clearing USB error notification clears activity pending");

    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_OTG_STATE, 1u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, PWRC) & 0x0080u) != 0u,
           "USB OTG notification sets activity pending");
    dspic33_write_word(cpu, PWRC, 1u);
    expect(state,
           (dspic33_read_word(cpu, PWRC) & 0x0080u) == 0u &&
               dspic33_read_word(cpu, OTGIR) != 0u,
           "clearing sleep guard clears activity pending");
    dspic33_write_word(cpu, PWRC, 0x0011u);
    expect(state, (dspic33_read_word(cpu, PWRC) & 0x0080u) != 0u,
           "setting sleep guard detects an existing notification");
    dspic33_write_word(cpu, OTGIR, 0x00fdu);
    expect(state, (dspic33_read_word(cpu, PWRC) & 0x0080u) == 0u,
           "clearing OTG notification clears activity pending");

    configure_device(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_IDLE, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, PWRC) & 0x0080u) == 0u,
           "USB notification does not set activity pending without guard");
    dspic33_write_word(cpu, PWRC, 0x0011u);
    expect(state, (dspic33_read_word(cpu, PWRC) & 0x0080u) != 0u,
           "sleep guard observes a prior USB notification");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, PWRC) == 0u,
           "USB reset clears sleep guard activity state");
}

static void host_interrupt_cases(TestState* state, Dspic33* cpu) {
    Dspic33UsbPacket packet;
    uint8_t data[8] = {0u};

    configure_host(cpu);
    dspic33_write_word(cpu, IE, 0x0040u);
    enable_usb_interrupt(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_ATTACH, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host attach interrupt schedule");
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0040u) != 0u && interrupt_flag(cpu, USB_IRQ),
           "USB host attach interrupt state");
    expect(state, service_usb_interrupt(cpu), "USB host attach interrupt vector");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_OTG_STATE, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host attach voltage-state perturbation");
    dspic33_write_word(cpu, IR, 0x0040u);
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0040u) != 0u && interrupt_flag(cpu, USB_IRQ),
           "USB host attach persistent W1C");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_DETACH, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host attach cause release");
    dspic33_write_word(cpu, IR, 0x0040u);
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0040u) == 0u && interrupt_flag(cpu, USB_IRQ),
           "USB host attach W1C after detach");
    clear_interrupt(cpu, USB_IRQ);
    expect(state, !interrupt_flag(cpu, USB_IRQ), "USB host attach IFS clear");

    configure_host(cpu);
    dspic33_write_word(cpu, IE, 0x0001u);
    enable_usb_interrupt(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_ATTACH, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host attach schedule");
    expect(state,
           (dspic33_read_word(cpu, OTGSTAT) & 0x0009u) == 0x0009u &&
               (dspic33_read_word(cpu, IR) & 0x0040u) != 0u,
           "USB host attach state");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_DETACH, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host detach schedule");
    expect(state,
           (dspic33_read_word(cpu, OTGSTAT) & 0x0009u) == 0u &&
               (dspic33_read_word(cpu, OTGIR) & 0x0020u) != 0u &&
               (dspic33_read_word(cpu, IR) & 0x0001u) != 0u &&
               interrupt_flag(cpu, USB_IRQ),
           "USB host detach interrupt state");
    expect(state, service_usb_interrupt(cpu), "USB host detach interrupt vector");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_OTG_STATE, 0x0009u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host detach voltage-state perturbation");
    dspic33_write_word(cpu, IR, 0x0001u);
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0001u) != 0u && interrupt_flag(cpu, USB_IRQ),
           "USB host detach persistent W1C");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_ATTACH, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host detach cause release");
    dspic33_write_word(cpu, IR, 0x0001u);
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0001u) == 0u && interrupt_flag(cpu, USB_IRQ),
           "USB host detach W1C after attach");
    clear_interrupt(cpu, USB_IRQ);
    expect(state, !interrupt_flag(cpu, USB_IRQ), "USB host detach IFS clear");

    configure_host(cpu);
    dspic33_write_word(cpu, IE, 0x0001u);
    enable_usb_interrupt(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESET, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host reset schedule");
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0001u) == 0u &&
               !interrupt_flag(cpu, USB_IRQ) && !dspic33_device_interrupt_pending(cpu),
           "USB host reset does not assert detach interrupt");

    configure_host(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_ATTACH, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, IR) & 0x0040u) != 0u,
           "USB host attach before device transition");
    dspic33_write_word(cpu, CON, 0x0001u);
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0041u) == 0u && !cpu->io.usb_host_attached,
           "USB device transition clears host interrupt state");

    configure_device(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESET, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, IR) & 0x0001u) != 0u,
           "USB device reset before host transition");
    dspic33_write_word(cpu, CON, 0x0008u);
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0041u) == 0u && !cpu->io.usb_host_attached,
           "USB host transition clears device interrupt state");

    configure_device(cpu);
    write_memory_word(cpu, IR, 0x0040u);
    expect(state, (dspic33_read_word(cpu, IR) & 0x0040u) == 0u,
           "USB device attach bit reads unimplemented");

    configure_host(cpu);
    dspic33_write_word(cpu, IE, 0x0080u);
    enable_usb_interrupt(cpu);
    write_descriptor(cpu, 0u, 0u, 0u, 0x0088u, 1u, BUFFER);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state, dspic33_usb_transmit(cpu, &packet), "USB host STALL token");
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_STALL, NULL, 0u, false,
                                     0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host STALL response schedule");
    expect(state,
           memory_word(cpu, descriptor_address(0u, 0u, 0u)) == 0x0038u &&
               (dspic33_read_word(cpu, IR) & 0x0080u) != 0u &&
               interrupt_flag(cpu, USB_IRQ),
           "USB host STALL interrupt state");
    expect(state, service_usb_interrupt(cpu), "USB host STALL interrupt vector");
    dspic33_write_word(cpu, IR, 0x0080u);
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0080u) == 0u && interrupt_flag(cpu, USB_IRQ),
           "USB host STALL W1C and sticky IFS");
    clear_interrupt(cpu, USB_IRQ);
    expect(state, !interrupt_flag(cpu, USB_IRQ), "USB host STALL IFS clear");

    configure_host(cpu);
    dspic33_write_word(cpu, IE, 0x0080u);
    enable_usb_interrupt(cpu);
    write_descriptor(cpu, 0u, 0u, 0u, 0x0088u, 1u, BUFFER);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state, dspic33_usb_transmit(cpu, &packet), "USB host NAK token");
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_NAK, NULL, 0u, false,
                                     0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host NAK response schedule");
    expect(state,
           memory_word(cpu, descriptor_address(0u, 0u, 0u)) == 0x0028u &&
               (dspic33_read_word(cpu, IR) & 0x0080u) == 0u &&
               !interrupt_flag(cpu, USB_IRQ),
           "USB host NAK does not assert STALL interrupt");

    configure_device(cpu);
    dspic33_write_word(cpu, IE, 0x0001u);
    enable_usb_interrupt(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESET, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB device reset schedule");
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0001u) != 0u && interrupt_flag(cpu, USB_IRQ),
           "USB device reset interrupt state");
    expect(state, service_usb_interrupt(cpu), "USB device reset interrupt vector");
    dspic33_write_word(cpu, IR, 0x0001u);
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0001u) == 0u && interrupt_flag(cpu, USB_IRQ),
           "USB device reset W1C and sticky IFS");
    clear_interrupt(cpu, USB_IRQ);
    expect(state, !interrupt_flag(cpu, USB_IRQ), "USB device reset IFS clear");

    configure_device(cpu);
    dspic33_write_word(cpu, IE, 0x0001u);
    enable_usb_interrupt(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_DETACH, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB device detach schedule");
    expect(state,
           (dspic33_read_word(cpu, OTGIR) & 0x0020u) != 0u &&
               (dspic33_read_word(cpu, IR) & 0x0001u) == 0u &&
               !interrupt_flag(cpu, USB_IRQ) && !dspic33_device_interrupt_pending(cpu),
           "USB device detach does not assert reset interrupt");

    configure_device(cpu);
    dspic33_write_word(cpu, IE, 0x0080u);
    enable_usb_interrupt(cpu);
    write_descriptor(cpu, 4u, 0u, 0u, 0x008cu, sizeof(data), BUFFER);
    expect(state,
           dspic33_usb_receive(cpu, 4u, data, sizeof(data), 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB device STALL schedule");
    expect(state,
           dspic33_usb_transmit(cpu, &packet) &&
               packet.handshake == DSPIC33_USB_HANDSHAKE_STALL &&
               (dspic33_read_word(cpu, IR) & 0x0080u) != 0u &&
               interrupt_flag(cpu, USB_IRQ),
           "USB device STALL interrupt state");
    expect(state, service_usb_interrupt(cpu), "USB device STALL interrupt vector");
    dspic33_write_word(cpu, IR, 0x0080u);
    expect(state,
           (dspic33_read_word(cpu, IR) & 0x0080u) == 0u && interrupt_flag(cpu, USB_IRQ),
           "USB device STALL W1C and sticky IFS");
    clear_interrupt(cpu, USB_IRQ);
    expect(state, !interrupt_flag(cpu, USB_IRQ), "USB device STALL IFS clear");
}

static void host_cases(TestState* state, Dspic33* cpu) {
    Dspic33UsbPacket packet;
    uint8_t input[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    uint8_t output[3] = {0xa1u, 0xb2u, 0xc3u};
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PWRC, 1u);
    dspic33_write_word(cpu, BDTP1, 0x0060u);
    dspic33_write_word(cpu, CON, 0x0008u);
    dspic33_write_word(cpu, ADDR, 0x0085u);
    write_descriptor(cpu, 0u, 0u, 0u, 0x0088u, sizeof(input), BUFFER);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state,
           dspic33_usb_transmit(cpu, &packet) && packet.pid == DSPIC33_USB_PID_IN &&
               packet.address == 5u && packet.low_speed && packet.endpoint == 0u &&
               packet.size == 0u && (dspic33_read_word(cpu, CON) & 0x0020u) != 0u,
           "USB host IN token");
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_ACK, input,
                                     sizeof(input), false, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "USB host IN response schedule");
    expect(state,
           memcmp(cpu->data + BUFFER, input, sizeof(input)) == 0 &&
               (dspic33_read_word(cpu, CON) & 0x0020u) == 0u &&
               dspic33_read_word(cpu, STAT) == 0u,
           "USB host IN completion");
    clear_transaction(cpu);
    memcpy(cpu->data + BUFFER + 0x100u, output, sizeof(output));
    write_descriptor(cpu, 0u, 1u, 0u, 0x0088u, sizeof(output), BUFFER + 0x100u);
    dspic33_write_word(cpu, TOK, 0x0013u);
    expect(state,
           dspic33_usb_transmit(cpu, &packet) && packet.pid == DSPIC33_USB_PID_OUT &&
               packet.endpoint == 3u && packet_data(&packet, output, sizeof(output)),
           "USB host OUT token payload");
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_ACK, NULL, 0u, false,
                                     0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host OUT response schedule");
    expect(state,
           memory_word(cpu, descriptor_address(0u, 1u, 0u)) == 0x0008u &&
               dspic33_read_word(cpu, STAT) == 8u,
           "USB host OUT completion");
    clear_transaction(cpu);
    write_descriptor(cpu, 0u, 0u, 1u, 0x0088u, 4u, BUFFER);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_TIMEOUT, NULL, 0u,
                                     false, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host timeout schedule");
    expect(state,
           (dspic33_read_word(cpu, EIR) & 0x0010u) != 0u &&
               cpu->io.usb_last_handshake == DSPIC33_USB_HANDSHAKE_TIMEOUT,
           "USB host timeout error");
    clear_transaction(cpu);
    write_descriptor(cpu, 0u, 0u, 0u, 0x0088u, 4u, BUFFER);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_NAK, NULL, 0u, false,
                                     0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host NAK schedule");
    expect(state, memory_word(cpu, descriptor_address(0u, 0u, 0u)) == 0x0028u,
           "USB host NAK status");
    clear_transaction(cpu);
    write_descriptor(cpu, 0u, 0u, 1u, 0x0088u, 4u, BUFFER);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_STALL, NULL, 0u, false,
                                     0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host STALL schedule");
    expect(state, memory_word(cpu, descriptor_address(0u, 0u, 1u)) == 0x0038u,
           "USB host STALL status");
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PWRC, 1u);
    dspic33_write_word(cpu, CON, 9u);
    expect(state,
           dspic33_device_advance(cpu, 59999u) && dspic33_read_word(cpu, FRML) == 0u,
           "USB host SOF period boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, FRML) == 1u &&
               (dspic33_read_word(cpu, IR) & 4u) != 0u,
           "USB host first SOF");
    dspic33_write_word(cpu, IR, 4u);
    expect(state,
           dspic33_device_advance(cpu, 60000u) && dspic33_read_word(cpu, FRML) == 2u,
           "USB host recurring SOF");
}

static void pmd_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33UsbPacket packet;
    uint64_t device_cycles;
    bool initialized;

    configure_device(cpu);
    dspic33_write_word(cpu, ADDR, 5u);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    dspic33_write_word(cpu, ADDR, 6u);
    expect(state, !cpu->io.usb_pmd_disabled && dspic33_read_word(cpu, ADDR) == 6u,
           "USB PMD waits one instruction cycle");
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.usb_pmd_disabled &&
               dspic33_read_word(cpu, ADDR) == 0u,
           "USB PMD disables register access");
    dspic33_write_word(cpu, ADDR, 7u);
    expect(state, memory_word(cpu, ADDR) == 6u, "USB PMD ignores register writes");
    dspic33_write_word(cpu, USB_PMD_ADDRESS, 0u);
    expect(state, dspic33_read_word(cpu, ADDR) == 0u,
           "USB PMD enable waits one instruction cycle");
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.usb_pmd_disabled &&
               dspic33_read_word(cpu, ADDR) == 6u,
           "USB PMD enable restores preserved registers");

    configure_device(cpu);
    dspic33_write_word(cpu, CLKDIV, 0x3800u);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state, dspic33_device_advance(cpu, 7u) && !cpu->io.usb_pmd_disabled,
           "DOZE scales USB PMD instruction boundary");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.usb_pmd_disabled,
           "USB PMD completes at divided instruction boundary");

    configure_device(cpu);
    dspic33_write_word(cpu, CLKDIV, 0x3800u);
    expect(state, dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W0_INDIRECT_W1),
           "load stepped USB PMD write");
    dspic33_set_working_register(cpu, 0u, USB_PMD);
    dspic33_set_working_register(cpu, 1u, USB_PMD_ADDRESS);
    cpu->pc = 0u;
    device_cycles = cpu->device_cycles;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->io.usb_pmd_disabled &&
               cpu->device_cycles - device_cycles == 8u && cpu->events.count == 0u,
           "stepped USB PMD write completes after one divided instruction");

    configure_device(cpu);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.usb_pmd_disabled &&
               cpu->events.count == 0u,
           "new USB PMD request invalidates stale transition");

    configure_device(cpu);
    cpu->io.usb_pmd_generation = 0x7fffu;
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.usb_pmd_disabled &&
               cpu->io.usb_pmd_generation == 0x8000u && cpu->events.count == 0u,
           "USB PMD transition crosses high generation bit");

    configure_device(cpu);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               dspic33_read_word(cpu, USB_PMD_ADDRESS) == 0u &&
               !cpu->io.usb_pmd_disabled && cpu->events.count == 0u,
           "USB PMD scheduling failure rolls back request");

    configure_device(cpu);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state, dspic33_device_advance(cpu, 1u),
           "establish USB PMD disable for missed event");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESET, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) && (memory_word(cpu, IR) & 1u) == 0u &&
               cpu->events.count == 0u,
           "USB PMD drops external bus events");
    dspic33_write_word(cpu, USB_PMD_ADDRESS, 0u);
    dspic33_device_advance(cpu, 1u);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESET, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, IR) & 1u) != 0u,
           "re-enabled USB accepts new bus events");

    configure_host(cpu);
    dspic33_write_word(cpu, CON, 9u);
    expect(state, cpu->events.count == 1u, "USB host schedules SOF clock");
    dspic33_device_advance(cpu, 10u);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.usb_pmd_disabled &&
               cpu->events.count == 1u && cpu->events.items[0].paused,
           "USB PMD stops internal SOF clock");
    expect(state,
           dspic33_device_advance(cpu, USB_FRAME_CYCLES) &&
               memory_word(cpu, FRML) == 0u,
           "USB frame counter freezes while PMD disabled");
    dspic33_write_word(cpu, USB_PMD_ADDRESS, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.usb_pmd_disabled &&
               cpu->events.count == 1u,
           "USB PMD enable restarts host SOF clock");
    expect(state,
           dspic33_device_advance(cpu, USB_FRAME_CYCLES) &&
               dspic33_read_word(cpu, FRML) == 1u,
           "USB host SOF resumes after PMD enable");

    configure_device(cpu);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state, dspic33_device_advance(cpu, 1u), "establish USB PMD state for copy");
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize USB PMD copy");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy USB PMD state");
        expect(state,
               copy.io.usb_pmd_disabled == cpu->io.usb_pmd_disabled &&
                   copy.io.usb_pmd_generation == cpu->io.usb_pmd_generation,
               "USB PMD lifecycle survives copy");
        dspic33_release(&copy);
    }
    expect(state, dspic33_load_program_word(cpu, 0u, OPCODE_RESET),
           "load warm reset for USB PMD");
    cpu->pc = 0u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->io.usb_pmd_disabled &&
               dspic33_read_word(cpu, USB_PMD_ADDRESS) == 0u,
           "warm reset clears USB PMD lifecycle");
    dspic33_reset(cpu, 0u);
    expect(state, !cpu->io.usb_pmd_disabled && cpu->io.usb_pmd_generation == 0u,
           "power-on reset clears USB PMD lifecycle");

    configure_device(cpu);
    write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 0u, BUFFER);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    dspic33_device_advance(cpu, 1u);
    expect(state,
           dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
               !dspic33_usb_transmit(cpu, &packet),
           "disabled USB produces no device response");
}

static void power_cases(TestState* state, Dspic33* cpu) {
    configure_host(cpu);
    dspic33_write_word(cpu, CON, 9u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, USB_FRAME_CYCLES) &&
               dspic33_read_word(cpu, FRML) == 1u,
           "USB continues in Idle when USBSIDL is clear");

    configure_host(cpu);
    dspic33_write_word(cpu, CON, 9u);
    dspic33_write_word(cpu, CNFG1, 0x0010u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           cpu->events.count == 1u && cpu->events.items[0].paused &&
               dspic33_device_advance(cpu, USB_FRAME_CYCLES) &&
               memory_word(cpu, FRML) == 0u,
           "USBSIDL gates the USB clock in Idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, USB_FRAME_CYCLES) &&
               dspic33_read_word(cpu, FRML) == 1u,
           "USB resumes retained SOF phase after Idle");

    configure_device(cpu);
    dspic33_write_word(cpu, OTGIE, 0x0010u);
    dspic33_write_word(cpu, CNFG1, 0x0010u);
    enable_usb_interrupt(cpu);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESUME, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (memory_word(cpu, OTGIR) & 0x0010u) != 0u &&
               interrupt_flag(cpu, USB_IRQ),
           "USB activity interrupt operates while stopped in Idle");
    expect(state,
           dspic33_device_wake(cpu) && cpu->last_interrupt == USB_IRQ &&
               cpu->pc == USB_VECTOR_ADDRESS,
           "USB activity wakes stopped Idle mode");

    configure_device(cpu);
    dspic33_write_word(cpu, OTGIE, 0x0010u);
    dspic33_write_word(cpu, PWRC, 0x0013u);
    enable_usb_interrupt(cpu);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESUME, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (memory_word(cpu, OTGIR) & 0x0010u) != 0u &&
               (memory_word(cpu, PWRC) & 0x0002u) != 0u,
           "suspended USB detects activity in Sleep");
    expect(state,
           dspic33_device_wake(cpu) && cpu->last_interrupt == USB_IRQ &&
               cpu->pc == USB_VECTOR_ADDRESS,
           "USB activity wakes Sleep mode");

    configure_host(cpu);
    dspic33_write_word(cpu, CON, 9u);
    dspic33_device_advance(cpu, 10u);
    dspic33_write_word(cpu, PWRC, 3u);
    expect(state,
           cpu->events.count == 1u && cpu->events.items[0].paused &&
               dspic33_device_advance(cpu, USB_FRAME_CYCLES) &&
               memory_word(cpu, FRML) == 0u,
           "USB Suspend gates the host frame clock");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESUME, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, PWRC) & 2u) == 0u,
           "USB resume leaves Suspend state");
    expect(state,
           dspic33_device_advance(cpu, USB_FRAME_CYCLES) &&
               dspic33_read_word(cpu, FRML) == 1u,
           "USB frame clock resumes after Suspend");
}

static void copy_and_reset_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33UsbPacket first;
    Dspic33UsbPacket second;
    uint8_t data[3] = {7u, 8u, 9u};
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize USB copy");
    if (!initialized) {
        return;
    }
    configure_device(cpu);
    write_descriptor(cpu, 2u, 0u, 0u, 0x0088u, 3u, BUFFER);
    expect(state, dspic33_usb_receive(cpu, 2u, data, 3u, 2u),
           "USB copy pending schedule");
    expect(state, dspic33_copy(&copy, cpu), "copy pending USB state");
    expect(state, dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u),
           "USB copy advance");
    expect(state,
           dspic33_usb_transmit(cpu, &first) && dspic33_usb_transmit(&copy, &second) &&
               first.handshake == second.handshake &&
               memcmp(cpu->data + BUFFER, copy.data + BUFFER, 3u) == 0,
           "USB copied state matches");
    configure_device(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_IDLE, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB copy idle state schedule");
    dspic33_write_word(cpu, IR, 0x0010u);
    expect(state, dspic33_copy(&copy, cpu), "copy B1 USB idle state");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_IDLE, 0u, 0u) &&
               dspic33_usb_bus(&copy, DSPIC33_USB_BUS_IDLE, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) && dspic33_device_advance(&copy, 0u) &&
               (dspic33_read_word(cpu, IR) & 0x0010u) == 0u &&
               (dspic33_read_word(&copy, IR) & 0x0010u) == 0u,
           "copied B1 USB idle state suppresses repeated UIDLE");
    dspic33_release(&copy);
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.usb_tx.count == 0u && cpu->io.usb_status_count == 0u &&
               !cpu->io.usb_bus_idle && dspic33_read_word(cpu, PWRC) == 0u &&
               dspic33_read_word(cpu, CON) == 0u,
           "USB reset clears runtime");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    if (!dspic33_initialize(&cpu)) {
        fprintf(stderr, "[usb-error] cannot initialize emulator\n");
        return 2;
    }
    register_cases(&state, &cpu);
    out_payload_domain(&state, &cpu);
    in_payload_domain(&state, &cpu);
    descriptor_behavior_cases(&state, &cpu);
    status_fifo_cases(&state, &cpu);
    boundary_and_order_cases(&state, &cpu);
    bus_access_error_cases(&state, &cpu);
    interrupt_and_bus_cases(&state, &cpu);
    idle_rearm_cases(&state, &cpu);
    sleep_guard_cases(&state, &cpu);
    host_interrupt_cases(&state, &cpu);
    host_cases(&state, &cpu);
    pmd_cases(&state, &cpu);
    power_cases(&state, &cpu);
    copy_and_reset_cases(&state, &cpu);
    dspic33_release(&cpu);
    return test_finish(&state);
}
