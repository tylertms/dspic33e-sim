#include "device/dspic33ep_mu/communication/usb/internal.h"

static void host_cases(TestState* state, Dspic33* cpu) {
    Dspic33UsbPacket packet;
    uint8_t input[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    uint8_t output[3] = {0xa1u, 0xb2u, 0xc3u};
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PWRC, 1u);
    dspic33_write_word(cpu, BDTP1, 0x0060u);
    dspic33_write_word(cpu, CON, 0x0008u);
    dspic33_write_word(cpu, ADDR, 0x0085u);
    dspic33_usb_test_write_descriptor(cpu, 0u, 0u, 0u, 0x0088u, sizeof(input), BUFFER);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state,
           dspic33_usb_transmit(cpu, &packet) && packet.pid == DSPIC33_USB_PID_IN &&
               packet.address == 5u && packet.low_speed && packet.endpoint == 0u &&
               packet.size == 0u && (dspic33_read_word(cpu, CON) & 0x0020u) != 0u,
           "USB host IN token");
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_ACK, input, sizeof(input), false,
                                     1u) &&
               dspic33_device_advance(cpu, 1u),
           "USB host IN response schedule");
    expect(state,
           memcmp(cpu->data + BUFFER, input, sizeof(input)) == 0 &&
               (dspic33_read_word(cpu, CON) & 0x0020u) == 0u && dspic33_read_word(cpu, STAT) == 0u,
           "USB host IN completion");
    dspic33_usb_test_clear_transaction(cpu);
    memcpy(cpu->data + BUFFER + 0x100u, output, sizeof(output));
    dspic33_usb_test_write_descriptor(cpu, 0u, 1u, 0u, 0x0088u, sizeof(output), BUFFER + 0x100u);
    dspic33_write_word(cpu, TOK, 0x0013u);
    expect(state,
           dspic33_usb_transmit(cpu, &packet) && packet.pid == DSPIC33_USB_PID_OUT &&
               packet.endpoint == 3u &&
               dspic33_usb_test_packet_data(&packet, output, sizeof(output)),
           "USB host OUT token payload");
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_ACK, NULL, 0u, false, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host OUT response schedule");
    expect(state,
           dspic33_usb_test_memory_word(cpu, dspic33_usb_test_descriptor_address(0u, 1u, 0u)) ==
                   0x0008u &&
               dspic33_read_word(cpu, STAT) == 8u,
           "USB host OUT completion");
    dspic33_usb_test_clear_transaction(cpu);
    dspic33_usb_test_write_descriptor(cpu, 0u, 0u, 1u, 0x0088u, 4u, BUFFER);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_TIMEOUT, NULL, 0u, false, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host timeout schedule");
    expect(state,
           (dspic33_read_word(cpu, EIR) & 0x0010u) != 0u &&
               cpu->io.usb_last_handshake == DSPIC33_USB_HANDSHAKE_TIMEOUT,
           "USB host timeout error");
    dspic33_usb_test_clear_transaction(cpu);
    dspic33_usb_test_write_descriptor(cpu, 0u, 0u, 0u, 0x0088u, 4u, BUFFER);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_NAK, NULL, 0u, false, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host NAK schedule");
    expect(state,
           dspic33_usb_test_memory_word(cpu, dspic33_usb_test_descriptor_address(0u, 0u, 0u)) ==
               0x0028u,
           "USB host NAK status");
    dspic33_usb_test_clear_transaction(cpu);
    dspic33_usb_test_write_descriptor(cpu, 0u, 0u, 1u, 0x0088u, 4u, BUFFER);
    dspic33_write_word(cpu, TOK, 0x0090u);
    expect(state,
           dspic33_usb_host_response(cpu, DSPIC33_USB_HANDSHAKE_STALL, NULL, 0u, false, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "USB host STALL schedule");
    expect(state,
           dspic33_usb_test_memory_word(cpu, dspic33_usb_test_descriptor_address(0u, 0u, 1u)) ==
               0x0038u,
           "USB host STALL status");
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PWRC, 1u);
    dspic33_write_word(cpu, CON, 9u);
    expect(state, dspic33_device_advance(cpu, 59999u) && dspic33_read_word(cpu, FRML) == 0u,
           "USB host SOF period boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, FRML) == 1u &&
               (dspic33_read_word(cpu, IR) & 4u) != 0u,
           "USB host first SOF");
    dspic33_write_word(cpu, IR, 4u);
    expect(state, dspic33_device_advance(cpu, 60000u) && dspic33_read_word(cpu, FRML) == 2u,
           "USB host recurring SOF");
}

static void pmd_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33UsbPacket packet;
    uint64_t device_cycles;
    bool initialized;

    dspic33_usb_test_configure_device(cpu);
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
    expect(state, dspic33_usb_test_memory_word(cpu, ADDR) == 6u, "USB PMD ignores register writes");
    dspic33_write_word(cpu, USB_PMD_ADDRESS, 0u);
    expect(state, dspic33_read_word(cpu, ADDR) == 0u, "USB PMD enable waits one instruction cycle");
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.usb_pmd_disabled &&
               dspic33_read_word(cpu, ADDR) == 6u,
           "USB PMD enable restores preserved registers");

    dspic33_usb_test_configure_device(cpu);
    dspic33_write_word(cpu, CLKDIV, 0x3800u);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state, dspic33_device_advance(cpu, 7u) && !cpu->io.usb_pmd_disabled,
           "DOZE scales USB PMD instruction boundary");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.usb_pmd_disabled,
           "USB PMD completes at divided instruction boundary");

    dspic33_usb_test_configure_device(cpu);
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

    dspic33_usb_test_configure_device(cpu);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.usb_pmd_disabled && cpu->events.count == 0u,
           "new USB PMD request invalidates stale transition");

    dspic33_usb_test_configure_device(cpu);
    cpu->io.usb_pmd_generation = 0x7fffu;
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.usb_pmd_disabled &&
               cpu->io.usb_pmd_generation == 0x8000u && cpu->events.count == 0u,
           "USB PMD transition crosses high generation bit");

    dspic33_usb_test_configure_device(cpu);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               dspic33_read_word(cpu, USB_PMD_ADDRESS) == 0u && !cpu->io.usb_pmd_disabled &&
               cpu->events.count == 0u,
           "USB PMD scheduling failure rolls back request");

    dspic33_usb_test_configure_device(cpu);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state, dspic33_device_advance(cpu, 1u), "establish USB PMD disable for missed event");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESET, 0u, 0u) && dspic33_device_advance(cpu, 0u) &&
               (dspic33_usb_test_memory_word(cpu, IR) & 1u) == 0u && cpu->events.count == 0u,
           "USB PMD drops external bus events");
    dspic33_write_word(cpu, USB_PMD_ADDRESS, 0u);
    dspic33_device_advance(cpu, 1u);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESET, 0u, 0u) && dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, IR) & 1u) != 0u,
           "re-enabled USB accepts new bus events");

    dspic33_usb_test_configure_host(cpu);
    dspic33_write_word(cpu, CON, 9u);
    expect(state, cpu->events.count == 1u, "USB host schedules SOF clock");
    dspic33_device_advance(cpu, 10u);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.usb_pmd_disabled && cpu->events.count == 1u &&
               cpu->events.items[0].paused,
           "USB PMD stops internal SOF clock");
    expect(state,
           dspic33_device_advance(cpu, USB_FRAME_CYCLES) &&
               dspic33_usb_test_memory_word(cpu, FRML) == 0u,
           "USB frame counter freezes while PMD disabled");
    dspic33_write_word(cpu, USB_PMD_ADDRESS, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.usb_pmd_disabled && cpu->events.count == 1u,
           "USB PMD enable restarts host SOF clock");
    expect(state,
           dspic33_device_advance(cpu, USB_FRAME_CYCLES) && dspic33_read_word(cpu, FRML) == 1u,
           "USB host SOF resumes after PMD enable");

    dspic33_usb_test_configure_device(cpu);
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
    expect(state, dspic33_load_program_word(cpu, 0u, OPCODE_RESET), "load warm reset for USB PMD");
    cpu->pc = 0u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->io.usb_pmd_disabled &&
               dspic33_read_word(cpu, USB_PMD_ADDRESS) == 0u,
           "warm reset clears USB PMD lifecycle");
    dspic33_reset(cpu, 0u);
    expect(state, !cpu->io.usb_pmd_disabled && cpu->io.usb_pmd_generation == 0u,
           "power-on reset clears USB PMD lifecycle");

    dspic33_usb_test_configure_device(cpu);
    dspic33_usb_test_write_descriptor(cpu, 1u, 1u, 0u, 0x0088u, 0u, BUFFER);
    dspic33_write_word(cpu, USB_PMD_ADDRESS, USB_PMD);
    dspic33_device_advance(cpu, 1u);
    expect(state,
           dspic33_usb_request(cpu, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
               !dspic33_usb_transmit(cpu, &packet),
           "disabled USB produces no device response");
}

static void power_cases(TestState* state, Dspic33* cpu) {
    dspic33_usb_test_configure_host(cpu);
    dspic33_write_word(cpu, CON, 9u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, USB_FRAME_CYCLES) && dspic33_read_word(cpu, FRML) == 1u,
           "USB continues in Idle when USBSIDL is clear");

    dspic33_usb_test_configure_host(cpu);
    dspic33_write_word(cpu, CON, 9u);
    dspic33_write_word(cpu, CNFG1, 0x0010u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           cpu->events.count == 1u && cpu->events.items[0].paused &&
               dspic33_device_advance(cpu, USB_FRAME_CYCLES) &&
               dspic33_usb_test_memory_word(cpu, FRML) == 0u,
           "USBSIDL gates the USB clock in Idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, USB_FRAME_CYCLES) && dspic33_read_word(cpu, FRML) == 1u,
           "USB resumes retained SOF phase after Idle");

    dspic33_usb_test_configure_device(cpu);
    dspic33_write_word(cpu, OTGIE, 0x0010u);
    dspic33_write_word(cpu, CNFG1, 0x0010u);
    dspic33_usb_test_enable_usb_interrupt(cpu);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESUME, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_usb_test_memory_word(cpu, OTGIR) & 0x0010u) != 0u &&
               dspic33_usb_test_interrupt_flag(cpu, USB_IRQ),
           "USB activity interrupt operates while stopped in Idle");
    expect(state,
           dspic33_device_wake(cpu) && cpu->last_interrupt == USB_IRQ &&
               cpu->pc == USB_VECTOR_ADDRESS,
           "USB activity wakes stopped Idle mode");

    dspic33_usb_test_configure_device(cpu);
    dspic33_write_word(cpu, OTGIE, 0x0010u);
    dspic33_write_word(cpu, PWRC, 0x0013u);
    dspic33_usb_test_enable_usb_interrupt(cpu);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESUME, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_usb_test_memory_word(cpu, OTGIR) & 0x0010u) != 0u &&
               (dspic33_usb_test_memory_word(cpu, PWRC) & 0x0002u) != 0u,
           "suspended USB detects activity in Sleep");
    expect(state,
           dspic33_device_wake(cpu) && cpu->last_interrupt == USB_IRQ &&
               cpu->pc == USB_VECTOR_ADDRESS,
           "USB activity wakes Sleep mode");

    dspic33_usb_test_configure_host(cpu);
    dspic33_write_word(cpu, CON, 9u);
    dspic33_device_advance(cpu, 10u);
    dspic33_write_word(cpu, PWRC, 3u);
    expect(state,
           cpu->events.count == 1u && cpu->events.items[0].paused &&
               dspic33_device_advance(cpu, USB_FRAME_CYCLES) &&
               dspic33_usb_test_memory_word(cpu, FRML) == 0u,
           "USB Suspend gates the host frame clock");
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_RESUME, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) && (dspic33_read_word(cpu, PWRC) & 2u) == 0u,
           "USB resume leaves Suspend state");
    expect(state,
           dspic33_device_advance(cpu, USB_FRAME_CYCLES) && dspic33_read_word(cpu, FRML) == 1u,
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
    dspic33_usb_test_configure_device(cpu);
    dspic33_usb_test_write_descriptor(cpu, 2u, 0u, 0u, 0x0088u, 3u, BUFFER);
    expect(state, dspic33_usb_receive(cpu, 2u, data, 3u, 2u), "USB copy pending schedule");
    expect(state, dspic33_copy(&copy, cpu), "copy pending USB state");
    expect(state, dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u),
           "USB copy advance");
    expect(state,
           dspic33_usb_transmit(cpu, &first) && dspic33_usb_transmit(&copy, &second) &&
               first.handshake == second.handshake &&
               memcmp(cpu->data + BUFFER, copy.data + BUFFER, 3u) == 0,
           "USB copied state matches");
    dspic33_usb_test_configure_device(cpu);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_IDLE, 0u, 0u) && dspic33_device_advance(cpu, 0u),
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
           cpu->io.usb_tx.count == 0u && cpu->io.usb_status_count == 0u && !cpu->io.usb_bus_idle &&
               dspic33_read_word(cpu, PWRC) == 0u && dspic33_read_word(cpu, CON) == 0u,
           "USB reset clears runtime");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    if (!dspic33_initialize(&cpu)) {
        fprintf(stderr, "[usb-error] cannot initialize emulator\n");
        return 2;
    }
    dspic33_usb_test_register_cases(&state, &cpu);
    dspic33_usb_test_out_payload_domain(&state, &cpu);
    dspic33_usb_test_in_payload_domain(&state, &cpu);
    dspic33_usb_test_descriptor_behavior_cases(&state, &cpu);
    dspic33_usb_test_status_fifo_cases(&state, &cpu);
    dspic33_usb_test_boundary_and_order_cases(&state, &cpu);
    dspic33_usb_test_bus_access_error_cases(&state, &cpu);
    dspic33_usb_test_interrupt_and_bus_cases(&state, &cpu);
    dspic33_usb_test_idle_rearm_cases(&state, &cpu);
    dspic33_usb_test_sleep_guard_cases(&state, &cpu);
    dspic33_usb_test_host_interrupt_cases(&state, &cpu);
    host_cases(&state, &cpu);
    pmd_cases(&state, &cpu);
    power_cases(&state, &cpu);
    copy_and_reset_cases(&state, &cpu);
    dspic33_release(&cpu);
    return test_finish(&state);
}
