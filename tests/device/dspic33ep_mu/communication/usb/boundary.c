#include "device/dspic33ep_mu/communication/usb/internal.h"

bool dspic33_device_internal_usb_descriptor(const Dspic33* cpu, uint8_t endpoint, uint8_t direction,
                                            uint8_t bank, uint16_t words[4]);
bool dspic33_device_internal_usb_queue_push(Dspic33UsbQueue* queue, const Dspic33UsbPacket* packet);
bool dspic33_device_internal_usb_read_memory(const Dspic33* cpu, uint32_t address, uint8_t* data,
                                             uint16_t size, bool increment);
void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_device_internal_run_usb(Dspic33* cpu, uint16_t slot);
void dspic33_device_internal_usb_reset_registers(Dspic33* cpu);
void dspic33_device_internal_usb_update_power_state(Dspic33* cpu);

static void queue_and_memory_cases(TestState* state, Dspic33* cpu) {
    Dspic33UsbQueue queue = {0};
    Dspic33UsbPacket packet = {0};
    uint16_t words[4];
    uint8_t byte;

    queue.count = DSPIC33_USB_PACKET_QUEUE_SIZE;
    expect(state, !dspic33_device_internal_usb_queue_push(&queue, &packet),
           "full USB response queue rejects packets");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, BDTP2, 0x0010u);
    expect(state, !dspic33_device_internal_usb_descriptor(cpu, 0u, 0u, 0u, words),
           "USB descriptor rejects an out-of-range table");
    expect(state, !dspic33_device_internal_usb_read_memory(cpu, DSPIC33_DATA_SIZE, &byte, 1u, true),
           "USB memory read rejects an out-of-range address");

    dspic33_usb_test_configure_host(cpu);
    dspic33_device_internal_raw_write_word(cpu, BDTP2, 0x0010u);
    dspic33_write_word(cpu, TOK, (uint16_t)(DSPIC33_USB_PID_IN << 4u));
    expect(state, (dspic33_read_word(cpu, EIR) & 0x0040u) != 0u,
           "USB host token rejects an out-of-range descriptor table");

    dspic33_usb_test_configure_host(cpu);
    dspic33_usb_test_write_descriptor(cpu, 0u, 0u, 0u, 0x0088u, 0u, BUFFER);
    cpu->io.usb_tx.count = DSPIC33_USB_PACKET_QUEUE_SIZE;
    dspic33_write_word(cpu, TOK, (uint16_t)(DSPIC33_USB_PID_IN << 4u));
    expect(state, (dspic33_read_word(cpu, EIR) & 0x0020u) != 0u,
           "USB host token reports a full transmit queue");
}

static void set_bdt_base(Dspic33* cpu, uint32_t address) {
    dspic33_write_word(cpu, BDTP1, (uint16_t)((address >> 8u) & 0x00feu));
    dspic33_write_word(cpu, BDTP2, (uint16_t)(address >> 16u));
    dspic33_write_word(cpu, BDTP3, (uint16_t)(address >> 24u));
}

static void profile_memory_cases(TestState* state) {
    for (Dspic33epMuDevice device = DSPIC33EP_MU_DEVICE_256MU806;
         device < DSPIC33EP_MU_DEVICE_COUNT; device++) {
        Dspic33* cpu = dspic33_create_for_device(device);
        Dspic33UsbPacket response;
        uint16_t words[4];
        uint8_t data = 0x5au;
        expect(state, cpu != NULL, "create profile USB processor");
        if (cpu == NULL) {
            continue;
        }
        uint32_t data_limit = dspic33_device_profile(cpu)->data_limit;
        uint8_t read_data;

        cpu->data[0x1000u] = data;
        cpu->data[data_limit - 1u] = data;
        expect(state,
               !dspic33_device_internal_usb_read_memory(cpu, 0x0fffu, &read_data, 1u, true) &&
                   dspic33_device_internal_usb_read_memory(cpu, 0x1000u, &read_data, 1u, true) &&
                   read_data == data,
               "USB payload accepts device RAM lower boundary");
        expect(
            state,
            dspic33_device_internal_usb_read_memory(cpu, data_limit - 1u, &read_data, 1u, true) &&
                read_data == data &&
                !dspic33_device_internal_usb_read_memory(cpu, data_limit, &read_data, 1u, true),
            "USB payload honors device RAM upper boundary");

        dspic33_usb_test_configure_device(cpu);
        set_bdt_base(cpu, 0u);
        expect(state, !dspic33_device_internal_usb_descriptor(cpu, 0u, 0u, 0u, words),
               "USB rejects descriptor below device RAM");
        expect(state,
               dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_usb_transmit(cpu, &response) &&
                   response.handshake == DSPIC33_USB_HANDSHAKE_ERROR &&
                   (dspic33_read_word(cpu, EIR) & 0x0040u) != 0u,
               "USB descriptor below device RAM raises bus access error");

        dspic33_usb_test_configure_device(cpu);
        set_bdt_base(cpu, data_limit - 0x200u);
        expect(state, dspic33_device_internal_usb_descriptor(cpu, 15u, 1u, 1u, words),
               "USB accepts final implemented descriptor");
        set_bdt_base(cpu, data_limit);
        expect(state, !dspic33_device_internal_usb_descriptor(cpu, 0u, 0u, 0u, words),
               "USB rejects descriptor beyond device RAM");
        expect(state,
               dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_usb_transmit(cpu, &response) &&
                   response.handshake == DSPIC33_USB_HANDSHAKE_ERROR &&
                   (dspic33_read_word(cpu, EIR) & 0x0040u) != 0u,
               "USB descriptor beyond device RAM raises bus access error");

        dspic33_usb_test_configure_device(cpu);
        dspic33_usb_test_write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 1u, 0x0fffu);
        expect(state,
               dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_usb_transmit(cpu, &response) &&
                   response.handshake == DSPIC33_USB_HANDSHAKE_ERROR &&
                   (dspic33_read_word(cpu, EIR) & 0x0040u) != 0u,
               "USB IN below device RAM raises bus access error");

        dspic33_usb_test_configure_device(cpu);
        dspic33_usb_test_write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 1u, data_limit);
        expect(state,
               dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_usb_transmit(cpu, &response) &&
                   response.handshake == DSPIC33_USB_HANDSHAKE_ERROR &&
                   (dspic33_read_word(cpu, EIR) & 0x0040u) != 0u,
               "USB IN beyond device RAM raises bus access error");

        dspic33_usb_test_configure_device(cpu);
        dspic33_usb_test_write_descriptor(cpu, 1u, 0u, 0u, 0x0088u, 1u, 0x0fffu);
        expect(state,
               dspic33_usb_receive(cpu, 1u, &data, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_usb_transmit(cpu, &response) &&
                   response.handshake == DSPIC33_USB_HANDSHAKE_ERROR &&
                   (dspic33_read_word(cpu, EIR) & 0x0040u) != 0u,
               "USB OUT below device RAM raises bus access error");

        dspic33_usb_test_configure_device(cpu);
        dspic33_usb_test_write_descriptor(cpu, 1u, 0u, 0u, 0x0088u, 1u, data_limit);
        expect(state,
               dspic33_usb_receive(cpu, 1u, &data, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_usb_transmit(cpu, &response) &&
                   response.handshake == DSPIC33_USB_HANDSHAKE_ERROR &&
                   (dspic33_read_word(cpu, EIR) & 0x0040u) != 0u,
               "USB OUT beyond device RAM raises bus access error");
        dspic33_destroy(cpu);
    }
}

static void reset_event_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 1u),
           "schedule non-USB reset survivor");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_USB, 0u, 0u, 1u),
           "schedule USB reset cancellation");
    dspic33_device_internal_usb_reset_registers(cpu);
    expect(state, cpu->events.count == 1u && cpu->events.items[0].type == DSPIC33_EVENT_INTERRUPT,
           "USB reset preserves unrelated events");
}

static void full_status_cases(TestState* state, Dspic33* cpu) {
    Dspic33UsbPacket response;
    dspic33_usb_test_configure_device(cpu);
    cpu->io.usb_status_count = sizeof(cpu->io.usb_status);
    dspic33_usb_test_write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 0u, BUFFER);
    expect(state,
           dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_usb_transmit(cpu, &response),
           "USB transaction completes with a full status queue");
    expect(state, (dspic33_read_word(cpu, EIR) & 0x0020u) != 0u,
           "full USB status queue raises a DMA error");
}

static void run_slot(Dspic33* cpu, Dspic33UsbHandshake handshake) {
    cpu->io.usb_pending[0].active = true;
    cpu->io.usb_pending[0].packet.handshake = handshake;
    dspic33_device_internal_run_usb(cpu, 0u);
}

static void host_response_guard_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, CON, 0x0008u);
    run_slot(cpu, DSPIC33_USB_HANDSHAKE_ACK);
    expect(state, !cpu->io.usb_pending[0].active, "USB ignores a host response without a token");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, CON, 0x0008u);
    dspic33_device_internal_raw_write_word(cpu, BDTP2, 0x0010u);
    cpu->io.usb_host_pending = true;
    cpu->io.usb_host_pid = DSPIC33_USB_PID_IN;
    run_slot(cpu, DSPIC33_USB_HANDSHAKE_ACK);
    expect(state, (dspic33_read_word(cpu, EIR) & 0x0040u) != 0u,
           "USB host response rejects an out-of-range descriptor");

    dspic33_usb_test_configure_host(cpu);
    cpu->io.usb_host_pending = true;
    cpu->io.usb_host_pid = DSPIC33_USB_PID_IN;
    run_slot(cpu, DSPIC33_USB_HANDSHAKE_ACK);
    expect(state, (dspic33_read_word(cpu, EIR) & 0x0020u) != 0u,
           "USB host response rejects an unowned descriptor");

    dspic33_usb_test_configure_host(cpu);
    dspic33_usb_test_write_descriptor(cpu, 0u, 0u, 0u, 0x0088u, 4u, BUFFER);
    cpu->io.usb_host_pending = true;
    cpu->io.usb_host_pid = DSPIC33_USB_PID_IN;
    cpu->io.usb_pending[0].packet.size = 1u;
    cpu->io.usb_pending[0].packet.data[0] = 0x5au;
    run_slot(cpu, DSPIC33_USB_HANDSHAKE_ACK);
    expect(state, cpu->data[BUFFER] == 0x5au, "USB host accepts a short IN response");

    dspic33_usb_test_configure_host(cpu);
    dspic33_usb_test_write_descriptor(cpu, 0u, 0u, 0u, 0x0088u, 0u, BUFFER);
    cpu->io.usb_host_pending = true;
    cpu->io.usb_host_pid = DSPIC33_USB_PID_IN;
    cpu->io.usb_pending[0].packet.error = 0x0004u;
    run_slot(cpu, DSPIC33_USB_HANDSHAKE_ERROR);
    expect(state, (dspic33_read_word(cpu, EIR) & 0x0004u) != 0u,
           "USB host propagates a response error code");
}

static void device_response_guard_cases(TestState* state, Dspic33* cpu) {
    Dspic33UsbPacket response;

    dspic33_usb_test_configure_device(cpu);
    cpu->io.usb_pending[0].active = true;
    cpu->io.usb_pending[0].packet.endpoint = DSPIC33_USB_ENDPOINT_COUNT;
    cpu->io.usb_pending[0].packet.pid = DSPIC33_USB_PID_IN;
    dspic33_device_internal_run_usb(cpu, 0u);
    expect(state,
           dspic33_usb_transmit(cpu, &response) &&
               response.handshake == DSPIC33_USB_HANDSHAKE_TIMEOUT,
           "USB device rejects an invalid endpoint");

    dspic33_usb_test_configure_device(cpu);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(EP0 + 2u), 0u);
    cpu->io.usb_pending[0].active = true;
    cpu->io.usb_pending[0].packet.endpoint = 1u;
    cpu->io.usb_pending[0].packet.pid = DSPIC33_USB_PID_OUT;
    dspic33_device_internal_run_usb(cpu, 0u);
    expect(state,
           dspic33_usb_transmit(cpu, &response) &&
               response.handshake == DSPIC33_USB_HANDSHAKE_TIMEOUT,
           "USB device rejects a disabled receive endpoint");

    dspic33_usb_test_configure_device(cpu);
    expect(state,
           dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_usb_transmit(cpu, &response) &&
               response.handshake == DSPIC33_USB_HANDSHAKE_NAK,
           "USB device rejects an unowned descriptor");
}

static void scheduler_boundary_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 1u),
           "schedule USB pause filtering case");
    cpu->io.usb_pmd_disabled = true;
    dspic33_device_internal_usb_update_power_state(cpu);
    expect(state, !cpu->events.items[0].paused, "USB pause ignores unrelated events");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_USB, 0u, 0u, 1u),
           "schedule USB resume overflow case");
    cpu->io.usb_pending[0].active = true;
    cpu->io.usb_pending[0].bus_event = true;
    cpu->io.usb_pending[0].event = DSPIC33_USB_BUS_SOF;
    cpu->events.items[0].paused = true;
    cpu->events.items[0].paused_remaining = UINT64_MAX;
    cpu->device_cycles = 1u;
    dspic33_device_internal_usb_update_power_state(cpu);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "USB rejects a resumed event cycle overflow");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_run_usb(cpu, DSPIC33_USB_PENDING_COUNT);
    dspic33_device_internal_run_usb(cpu, 0u);
    expect(state, cpu->stop_reason == DSPIC33_RUNNING,
           "USB ignores invalid and inactive pending slots");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, CON, 0x0008u);
    dspic33_device_internal_raw_write_word(cpu, PWRC, 0x0002u);
    cpu->io.usb_pending[0].active = true;
    cpu->io.usb_pending[0].packet.handshake = DSPIC33_USB_HANDSHAKE_ACK;
    dspic33_device_internal_run_usb(cpu, 0u);
    expect(state, !cpu->io.usb_pending[0].active, "USB suspend drops host responses");
}

void dspic33_usb_test_runtime_boundary_cases(TestState* state, Dspic33* cpu) {
    queue_and_memory_cases(state, cpu);
    profile_memory_cases(state);
    reset_event_cases(state, cpu);
    full_status_cases(state, cpu);
    host_response_guard_cases(state, cpu);
    device_response_guard_cases(state, cpu);
    scheduler_boundary_cases(state, cpu);
}
