#ifndef DSPIC33_USB_TEST_INTERNAL_H
#define DSPIC33_USB_TEST_INTERNAL_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device/dspic33ep_mu/device.h"
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

bool dspic33_usb_test_interrupt_flag(Dspic33* cpu, uint8_t irq);
bool dspic33_usb_test_packet_data(const Dspic33UsbPacket* packet, const uint8_t* data,
                                  uint16_t size);
uint16_t dspic33_usb_test_memory_word(const Dspic33* cpu, uint32_t address);
uint32_t dspic33_usb_test_descriptor_address(uint8_t endpoint, uint8_t direction, uint8_t bank);
void dspic33_usb_test_boundary_and_order_cases(TestState* state, Dspic33* cpu);
void dspic33_usb_test_runtime_boundary_cases(TestState* state, Dspic33* cpu);
void dspic33_usb_test_bus_access_error_cases(TestState* state, Dspic33* cpu);
void dspic33_usb_test_clear_transaction(Dspic33* cpu);
void dspic33_usb_test_configure_device(Dspic33* cpu);
void dspic33_usb_test_configure_host(Dspic33* cpu);
void dspic33_usb_test_descriptor_behavior_cases(TestState* state, Dspic33* cpu);
void dspic33_usb_test_enable_usb_interrupt(Dspic33* cpu);
void dspic33_usb_test_host_interrupt_cases(TestState* state, Dspic33* cpu);
void dspic33_usb_test_idle_rearm_cases(TestState* state, Dspic33* cpu);
void dspic33_usb_test_in_payload_domain(TestState* state, Dspic33* cpu);
void dspic33_usb_test_interrupt_and_bus_cases(TestState* state, Dspic33* cpu);
void dspic33_usb_test_out_payload_domain(TestState* state, Dspic33* cpu);
void dspic33_usb_test_register_cases(TestState* state, Dspic33* cpu);
void dspic33_usb_test_sleep_guard_cases(TestState* state, Dspic33* cpu);
void dspic33_usb_test_status_fifo_cases(TestState* state, Dspic33* cpu);
void dspic33_usb_test_write_descriptor(Dspic33* cpu, uint8_t endpoint, uint8_t direction,
                                       uint8_t bank, uint16_t status, uint16_t count,
                                       uint32_t buffer);

#endif
