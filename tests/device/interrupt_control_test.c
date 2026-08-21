#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"
#include "test.h"

enum {
    INTCON1 = 0x08c0u,
    INTCON2 = 0x08c2u,
    INTCON3 = 0x08c4u,
    INTCON4 = 0x08c6u,
    INTTREG = 0x08c8u,
    RPINR0 = 0x06a0u,
    RPINR1 = 0x06a2u,
    RPINR2 = 0x06a4u,
    TRISD = 0x0e30u,
    ANSELD = 0x0e3eu,
    DMA0_CONTROL = 0x0b00u,
    DMA0_START_LOW = 0x0b04u,
    DMA0_START_HIGH = 0x0b06u,
    DMA0_PAD = 0x0b0cu,
    DMA0_COUNT = 0x0b0eu,
    DMA_RECEIVE_PAD = 0x0290u,
    DMA_TEST_PAD = 0x0906u,
    MAIN_CLOCK_DIVISOR = 0x0744u,
    TIMER2_COUNTER = 0x0106u,
    TIMER2_PERIOD = 0x010cu,
    TIMER2_CONTROL = 0x0110u,
    COMPARATOR1_CONTROL = 0x0a84u,
    OUTPUT_COMPARE16_CONTROL1 = 0x0996u,
    OUTPUT_COMPARE16_CONTROL2 = 0x0998u,
    OUTPUT_COMPARE16_SECONDARY = 0x099au,
    OUTPUT_COMPARE16_PRIMARY = 0x099cu,
    OUTPUT_COMPARE_TRIGGER = 0x0080u,
    OUTPUT_COMPARE_TRIGGER_STATUS = 0x0040u,
    OUTPUT_COMPARE_FP = 0x1c00u,
    OUTPUT_COMPARE_EDGE_PWM = 6u,
    OPCODE_MOV_W0_INTCON2 = 0x884610u,
    OPCODE_MOV_W0_INTCON3 = 0x884620u,
    OPCODE_MOV_W4_W3 = 0x780194u,
    OPCODE_MOV_DOUBLE_W4_W2 = 0xbe0114u,
    OPCODE_TBLRDL_W4_W3 = 0xba0194u,
    OPCODE_BSET_DATA_0 = 0xa81200u,
    OPCODE_NOP = 0u,
    OPCODE_RESET = 0xfe0000u,
    OPCODE_RETFIE = 0x064000u
};

static const uint8_t external_interrupt_irqs[DSPIC33_EXTERNAL_INTERRUPT_COUNT] = {
    0u, 20u, 29u, 53u, 54u};
static const uint16_t external_interrupt_pins[DSPIC33_EXTERNAL_INTERRUPT_COUNT] = {
    0x0001u, 0x0002u, 0x0004u, 0x0008u, 0x0010u};

static Dspic33PendingSoftTrap* pending_trap(Dspic33* cpu, uint16_t trap) {
    size_t index;
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active &&
            cpu->pending_soft_traps[index].trap == trap) {
            return &cpu->pending_soft_traps[index];
        }
    }
    return NULL;
}

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

static void clear_external_interrupts(Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_EXTERNAL_INTERRUPT_COUNT; channel++) {
        clear_interrupt(cpu, external_interrupt_irqs[channel]);
    }
}

static uint8_t external_interrupt_flags(Dspic33* cpu) {
    uint8_t channel;
    uint8_t flags = 0u;
    for (channel = 0u; channel < DSPIC33_EXTERNAL_INTERRUPT_COUNT; channel++) {
        if (interrupt_flag(cpu, external_interrupt_irqs[channel])) {
            flags |= (uint8_t)(1u << channel);
        }
    }
    return flags;
}

static void prepare_external_interrupts(Dspic33* cpu, uint16_t levels,
                                        uint16_t polarity) {
    dspic33_gpio_drive(cpu, 3u, levels, 0x003fu);
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, ANSELD, 0u);
    dspic33_write_word(cpu, TRISD, UINT16_MAX);
    dspic33_write_word(cpu, RPINR0, 65u << 8u);
    dspic33_write_word(cpu, RPINR1, (uint16_t)(66u | (67u << 8u)));
    dspic33_write_word(cpu, RPINR2, 68u);
    dspic33_write_word(cpu, INTCON2, (uint16_t)(0x8000u | polarity));
    clear_external_interrupts(cpu);
}

static void enable_interrupt(Dspic33* cpu, uint8_t irq, uint8_t priority,
                             uint32_t vector) {
    uint16_t enable = (uint16_t)(0x0820u + (irq / 16u) * 2u);
    uint16_t priority_address = (uint16_t)(0x0840u + (irq / 4u) * 2u);
    uint16_t enable_mask = (uint16_t)(1u << (irq % 16u));
    uint16_t shift = (uint16_t)((irq % 4u) * 4u);
    dspic33_write_word(cpu, enable,
                       (uint16_t)(dspic33_read_word(cpu, enable) | enable_mask));
    dspic33_write_word(cpu, priority_address,
                       (uint16_t)((dspic33_read_word(cpu, priority_address) &
                                   ~(uint16_t)(7u << shift)) |
                                  (uint16_t)(priority << shift)));
    dspic33_load_program_word(cpu, (uint32_t)(0x0014u + irq * 2u), vector);
    dspic33_load_program_word(cpu, vector, 0u);
}

static void access_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, INTCON1) == 0u, "INTCON1 POR state");
    expect(state, dspic33_read_word(cpu, INTCON2) == 0x8000u, "INTCON2 POR state");
    expect(state, dspic33_read_word(cpu, INTCON3) == 0u, "INTCON3 POR state");
    expect(state, dspic33_read_word(cpu, INTCON4) == 0u, "INTCON4 POR state");
    expect(state, dspic33_read_word(cpu, INTTREG) == 0u, "INTTREG POR state");

    dspic33_write_word(cpu, INTCON1, 0xffffu);
    expect(state, dspic33_read_word(cpu, INTCON1) == 0xfffeu,
           "INTCON1 implements every status bit except bit zero");
    expect(state, pending_trap(cpu, 4u) == NULL,
           "software MATHERR status does not create a trap source");
    dspic33_write_word(cpu, INTCON1, 0u);
    expect(state, dspic33_read_word(cpu, INTCON1) == 0u,
           "software clears writable INTCON1 status");

    dspic33_device_latch_math_error(cpu, 0x0040u);
    expect(state,
           dspic33_read_word(cpu, INTCON1) == 0x0050u && pending_trap(cpu, 4u) != NULL,
           "hardware math source latches status and schedules trap");
    dspic33_write_word(cpu, INTCON1, 0x0040u);
    expect(state,
           dspic33_read_word(cpu, INTCON1) == 0x0040u && pending_trap(cpu, 4u) == NULL,
           "software MATHERR clear cancels hardware trap source");

    cpu->disicnt = 0x1234u;
    dspic33_write_word(cpu, INTCON2, 0xffffu);
    expect(state, dspic33_read_word(cpu, INTCON2) == 0xe01fu,
           "INTCON2 preserves DISI and rejects reserved fields");
    dspic33_write_word(cpu, INTCON2, 0xc000u);
    expect(state, dspic33_read_word(cpu, INTCON2) == 0xc000u,
           "INTCON2 accepts GIE without software-changing DISI");
    dspic33_write_word(cpu, INTCON4, 0u);
    dspic33_write_word(cpu, INTCON3, 0xffffu);
    expect(state, dspic33_read_word(cpu, INTCON3) == 0x0070u,
           "INTCON3 rejects reserved fields");
    dspic33_write_word(cpu, INTCON3, 0u);
    expect(state, dspic33_read_word(cpu, INTCON3) == 0u,
           "INTCON3 software status clear is retained");
    dspic33_write_word(cpu, INTCON4, 0xffffu);
    expect(state, dspic33_read_word(cpu, INTCON4) == 0x0001u,
           "INTCON4 rejects reserved fields");
    expect(state, pending_trap(cpu, 2u) != NULL,
           "software SGHT creates a generic hard trap source");
    dspic33_write_word(cpu, INTCON4, 0u);
    expect(state, dspic33_read_word(cpu, INTCON4) == 0u,
           "INTCON4 software status clear is retained");
    expect(state, pending_trap(cpu, 2u) == NULL,
           "software SGHT clear cancels the hard trap source");

    dspic33_device_latch_interrupt(cpu, 6u, 9u);
    expect(state, dspic33_read_word(cpu, INTTREG) == 0x0906u,
           "INTTREG exposes dynamic vector and priority");
    dspic33_write_word(cpu, INTTREG, 0xffffu);
    expect(state, dspic33_read_word(cpu, INTTREG) == 0x0906u,
           "INTTREG word write is ignored");
    dspic33_write_byte(cpu, INTTREG, 0u);
    dspic33_write_byte(cpu, INTTREG + 1u, 0u);
    expect(state, dspic33_read_word(cpu, INTTREG) == 0x0906u,
           "INTTREG byte writes are ignored");
}

static void generic_hard_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W0_INTCON2);
    dspic33_load_program_word(cpu, 0x0008u, 0x000300u);
    dspic33_load_program_word(cpu, 0x0300u, OPCODE_RETFIE);
    dspic33_set_working_register(cpu, 0u, 0xa000u);
    dspic33_set_working_register(cpu, 15u, 0x2000u);
    cpu->sr = 0x0105u;
    cpu->stop_on_trap = true;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "SWTRAP instruction dispatches generic hard trap");
    expect(state,
           dspic33_read_word(cpu, INTCON2) == 0xa000u &&
               dspic33_read_word(cpu, INTCON4) == 0x0001u,
           "SWTRAP sets persistent hard-trap sources");
    expect(state,
           cpu->last_trap == 2u && cpu->last_trap_return == 2u && cpu->pc == 0x0300u &&
               cpu->trap_count == 1u,
           "generic hard trap records source and vector");
    expect(state,
           dspic33_read_word(cpu, INTTREG) == 0x0d02u &&
               (cpu->sr & 0x00e0u) == 0x00a0u && (cpu->corcon & 0x0008u) != 0u,
           "generic hard trap enters priority thirteen");
    expect(state,
           dspic33_read_word(cpu, 0x2000u) == 2u &&
               dspic33_read_word(cpu, 0x2002u) == 0x0500u && cpu->w[15] == 0x2004u,
           "generic hard trap stacks completed instruction state");

    dspic33_write_word(cpu, INTCON2, 0u);
    expect(state,
           dspic33_read_word(cpu, INTCON2) == 0u &&
               dspic33_read_word(cpu, INTCON4) == 1u && pending_trap(cpu, 2u) != NULL,
           "clearing SWTRAP leaves SGHT hard source active");
    cpu->stop_reason = DSPIC33_RUNNING;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->trap_count == 2u,
           "persistent SGHT reenters after RETFIE");

    dspic33_write_word(cpu, INTCON4, 0u);
    expect(state, pending_trap(cpu, 2u) == NULL,
           "clearing final hard source cancels reentry");
    cpu->stop_reason = DSPIC33_RUNNING;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
               cpu->interrupt_depth == 0u,
           "RETFIE returns after hard sources clear");
}

static void generic_soft_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W0_INTCON3);
    dspic33_load_program_word(cpu, 0x0010u, 0x000320u);
    dspic33_load_program_word(cpu, 0x0320u, OPCODE_RETFIE);
    dspic33_set_working_register(cpu, 0u, 0x0070u);
    dspic33_set_working_register(cpu, 15u, 0x3000u);
    cpu->sr = 0x0105u;
    cpu->stop_on_trap = true;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "INTCON3 instruction dispatches generic soft trap");
    expect(state, dspic33_read_word(cpu, INTCON3) == 0x0070u,
           "generic soft sources retain all requested status bits");
    expect(state,
           cpu->last_trap == 6u && cpu->last_trap_return == 2u && cpu->pc == 0x0320u &&
               cpu->trap_count == 1u,
           "generic soft trap coalesces sources at vector sixteen");
    expect(state,
           dspic33_read_word(cpu, INTTREG) == 0x0906u &&
               (cpu->sr & 0x00e0u) == 0x0020u && (cpu->corcon & 0x0008u) != 0u,
           "generic soft trap enters priority nine");
    expect(state,
           dspic33_read_word(cpu, 0x3000u) == 2u &&
               dspic33_read_word(cpu, 0x3002u) == 0x0500u && cpu->w[15] == 0x3004u,
           "generic soft trap stacks completed instruction state");

    dspic33_write_word(cpu, INTCON3, 0u);
    expect(state, pending_trap(cpu, 6u) == NULL,
           "clearing generic soft sources cancels reentry");
    cpu->stop_reason = DSPIC33_RUNNING;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
               cpu->interrupt_depth == 0u,
           "RETFIE returns after soft sources clear");
}

static void external_interrupt_edge_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    prepare_external_interrupts(cpu, 0u, 0u);
    for (channel = 0u; channel < DSPIC33_EXTERNAL_INTERRUPT_COUNT; channel++) {
        uint8_t irq = external_interrupt_irqs[channel];
        uint16_t pin = external_interrupt_pins[channel];
        dspic33_gpio_drive(cpu, 3u, pin, pin);
        expect(state, external_interrupt_flags(cpu) == (uint8_t)(1u << channel),
               "positive INT edge raises only its matching flag");
        clear_interrupt(cpu, irq);
        dspic33_gpio_drive(cpu, 3u, pin, pin);
        expect(state, !interrupt_flag(cpu, irq),
               "stable high INT input does not retrigger");
        dspic33_gpio_drive(cpu, 3u, 0u, pin);
        expect(state, !interrupt_flag(cpu, irq),
               "falling edge does not match positive polarity");
    }

    prepare_external_interrupts(cpu, 0x001fu, 0x001fu);
    for (channel = 0u; channel < DSPIC33_EXTERNAL_INTERRUPT_COUNT; channel++) {
        uint8_t irq = external_interrupt_irqs[channel];
        uint16_t pin = external_interrupt_pins[channel];
        dspic33_gpio_drive(cpu, 3u, 0u, pin);
        expect(state, external_interrupt_flags(cpu) == (uint8_t)(1u << channel),
               "negative INT edge raises only its matching flag");
        clear_interrupt(cpu, irq);
        dspic33_gpio_drive(cpu, 3u, 0u, pin);
        expect(state, !interrupt_flag(cpu, irq),
               "stable low INT input does not retrigger");
        dspic33_gpio_drive(cpu, 3u, pin, pin);
        expect(state, !interrupt_flag(cpu, irq),
               "rising edge does not match negative polarity");
    }

    prepare_external_interrupts(cpu, 0u, 0u);
    dspic33_write_byte(cpu, INTCON2, 0x1fu);
    expect(state,
           !interrupt_flag(cpu, 0u) && !interrupt_flag(cpu, 20u) &&
               !interrupt_flag(cpu, 29u) && !interrupt_flag(cpu, 53u) &&
               !interrupt_flag(cpu, 54u),
           "polarity reconfiguration does not synthesize an edge");
}

static void external_interrupt_selection_cases(TestState* state, Dspic33* cpu) {
    prepare_external_interrupts(cpu, 0u, 0u);
    dspic33_gpio_drive(cpu, 3u, 0x0020u, 0x0020u);
    dspic33_write_byte(cpu, RPINR0 + 1u, 69u);
    expect(state,
           !interrupt_flag(cpu, 20u) &&
               cpu->io.external_interrupt_selection[1] == 69u &&
               (cpu->io.external_interrupt_levels & 0x02u) != 0u,
           "INT1 remap baselines the selected high pin");
    dspic33_write_word(cpu, INTCON2, 0x8002u);
    dspic33_gpio_drive(cpu, 3u, 0u, 0x0020u);
    expect(state, interrupt_flag(cpu, 20u), "remapped INT1 detects the selected edge");

    clear_interrupt(cpu, 20u);
    dspic33_write_word(cpu, RPINR0, 48u << 8u);
    dspic33_gpio_drive(cpu, 3u, 0x0020u, 0x0020u);
    expect(state,
           !interrupt_flag(cpu, 20u) &&
               (cpu->io.external_interrupt_qualified & 0x02u) == 0u,
           "unimplemented PPS selection cannot drive INT1");
    dspic33_write_word(cpu, RPINR0, 0u);
    expect(state,
           !interrupt_flag(cpu, 20u) &&
               (cpu->io.external_interrupt_qualified & 0x02u) != 0u &&
               (cpu->io.external_interrupt_levels & 0x02u) == 0u,
           "PPS selection zero ties INT1 to VSS without an edge");

    prepare_external_interrupts(cpu, 0u, 0u);
    dspic33_write_word(cpu, RPINR0, 1u << 8u);
    dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u);
    dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 100u, 0u);
    dspic33_device_advance(cpu, 0u);
    dspic33_write_word(cpu, COMPARATOR1_CONTROL, 0x8000u);
    clear_interrupt(cpu, 20u);
    dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, interrupt_flag(cpu, 20u), "CMP1 rising output drives selected INT1");
    clear_interrupt(cpu, 20u);
    dspic33_write_word(cpu, INTCON2, 0x8002u);
    dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, interrupt_flag(cpu, 20u), "CMP1 falling output drives selected INT1");

    prepare_external_interrupts(cpu, 0u, 0u);
    dspic33_write_word(cpu, TRISD, (uint16_t)(UINT16_MAX & ~0x0002u));
    dspic33_gpio_drive(cpu, 3u, 0x0002u, 0x0002u);
    expect(state, !interrupt_flag(cpu, 20u), "output mode suppresses INT1 input edges");
    dspic33_write_word(cpu, TRISD, UINT16_MAX);
    expect(state, !interrupt_flag(cpu, 20u),
           "input qualification baselines INT1 level");
    dspic33_gpio_drive(cpu, 3u, 0u, 0x0002u);
    dspic33_gpio_drive(cpu, 3u, 0x0002u, 0x0002u);
    expect(state, interrupt_flag(cpu, 20u),
           "qualified digital input restores INT1 edges");

    clear_interrupt(cpu, 20u);
    dspic33_write_word(cpu, ANSELD, 0x0002u);
    dspic33_gpio_drive(cpu, 3u, 0u, 0x0002u);
    expect(state, !interrupt_flag(cpu, 20u), "analog mode suppresses INT1 input edges");
    dspic33_write_word(cpu, ANSELD, 0u);
    expect(state, !interrupt_flag(cpu, 20u),
           "digital qualification rebaselines INT1 level");
    dspic33_gpio_drive(cpu, 3u, 0x0002u, 0x0002u);
    expect(state, interrupt_flag(cpu, 20u), "digital mode restores INT1 input edges");
}

static void configure_output_compare_trigger(Dspic33* cpu, uint8_t source) {
    dspic33_write_word(cpu, OUTPUT_COMPARE16_CONTROL1, 0u);
    dspic33_write_word(cpu, OUTPUT_COMPARE16_CONTROL2, 0u);
    dspic33_write_word(cpu, OUTPUT_COMPARE16_SECONDARY, 4u);
    dspic33_write_word(cpu, OUTPUT_COMPARE16_PRIMARY, 2u);
    dspic33_write_word(cpu, OUTPUT_COMPARE16_CONTROL1,
                       OUTPUT_COMPARE_FP | OUTPUT_COMPARE_EDGE_PWM);
    dspic33_write_word(cpu, OUTPUT_COMPARE16_CONTROL2,
                       (uint16_t)(OUTPUT_COMPARE_TRIGGER | source));
}

static void configure_dma_request(Dspic33* cpu, uint8_t request, uint16_t source) {
    dspic33_write_word(cpu, source, 0x5aa5u);
    dspic33_write_word(cpu, 0x0b00u, 0u);
    dspic33_write_word(cpu, 0x0b02u, request);
    dspic33_write_word(cpu, 0x0b04u, source);
    dspic33_write_word(cpu, 0x0b06u, 0u);
    dspic33_write_word(cpu, 0x0b0cu, DMA_TEST_PAD);
    dspic33_write_word(cpu, 0x0b0eu, 0u);
    dspic33_write_word(cpu, 0x0b00u, 0xa001u);
}

static void external_interrupt_interaction_cases(TestState* state, Dspic33* cpu) {
    prepare_external_interrupts(cpu, 0u, 0u);
    configure_dma_request(cpu, 0u, 0x2200u);
    dspic33_gpio_drive(cpu, 3u, 0x0001u, 0x0001u);
    expect(state,
           interrupt_flag(cpu, 0u) && (cpu->io.dma_peripheral_pending & 1u) != 0u,
           "INT0 edge raises its interrupt and DMA request");
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_read_word(cpu, DMA_TEST_PAD) == 0x5aa5u,
           "INT0 DMA request performs the configured transfer");

    prepare_external_interrupts(cpu, 0u, 0u);
    configure_dma_request(cpu, 20u, 0x2202u);
    dspic33_gpio_drive(cpu, 3u, 0x0002u, 0x0002u);
    expect(state,
           interrupt_flag(cpu, 20u) && cpu->io.dma_peripheral_pending == 0u &&
               dspic33_read_word(cpu, DMA_TEST_PAD) == 0u,
           "INT1 edge does not produce an unsupported DMA request");

    prepare_external_interrupts(cpu, 0u, 0u);
    dspic33_adc_input(cpu, 0u, 400u);
    dspic33_write_word(cpu, 0x0320u, 0u);
    dspic33_write_word(cpu, 0x0322u, 0u);
    dspic33_write_word(cpu, 0x0328u, 0u);
    dspic33_write_word(cpu, 0x0320u, 0x8010u);
    dspic33_write_word(cpu, 0x0320u, 0x8012u);
    dspic33_gpio_drive(cpu, 3u, 0x0001u, 0x0001u);
    expect(state,
           dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x0320u) & 0x0002u) == 0u,
           "INT0 edge starts the selected ADC conversion");
    expect(state,
           dspic33_device_advance(cpu, 12u) &&
               dspic33_read_word(cpu, 0x0300u) == 100u &&
               (dspic33_read_word(cpu, 0x0320u) & 0x0001u) == 0u,
           "INT0 conversion completes with the B1 DONE behavior");

    prepare_external_interrupts(cpu, 0u, 0u);
    configure_output_compare_trigger(cpu, 29u);
    dspic33_gpio_drive(cpu, 3u, 0x0002u, 0x0002u);
    expect(state,
           interrupt_flag(cpu, 20u) &&
               (dspic33_read_word(cpu, OUTPUT_COMPARE16_CONTROL2) &
                OUTPUT_COMPARE_TRIGGER_STATUS) != 0u,
           "INT1 pin edge triggers its selected Output Compare source");

    prepare_external_interrupts(cpu, 0u, 0u);
    configure_output_compare_trigger(cpu, 30u);
    dspic33_gpio_drive(cpu, 3u, 0x0004u, 0x0004u);
    expect(state,
           interrupt_flag(cpu, 29u) &&
               (dspic33_read_word(cpu, OUTPUT_COMPARE16_CONTROL2) &
                OUTPUT_COMPARE_TRIGGER_STATUS) != 0u,
           "INT2 pin edge triggers its selected Output Compare source");
}

static void external_interrupt_wake_lifecycle_cases(TestState* state, Dspic33* source,
                                                    Dspic33* copy) {
    prepare_external_interrupts(source, 0u, 0u);
    enable_interrupt(source, 20u, 0u, 0x04e0u);
    source->power_state = DSPIC33_POWER_SLEEP;
    dspic33_gpio_drive(source, 3u, 0x0002u, 0x0002u);
    expect(state,
           interrupt_flag(source, 20u) && !dspic33_device_wake(source) &&
               source->power_state == DSPIC33_POWER_SLEEP,
           "priority-zero external interrupt cannot wake from Sleep");

    prepare_external_interrupts(source, 0u, 0u);
    enable_interrupt(source, 53u, 3u, 0x0500u);
    source->w[15] = 0x1800u;
    source->sr = 0x0040u;
    source->power_state = DSPIC33_POWER_SLEEP;
    dspic33_gpio_drive(source, 3u, 0x0008u, 0x0008u);
    expect(state,
           dspic33_device_wake(source) && source->last_interrupt == 53u &&
               source->pc == 0x0500u && source->w[15] == 0x1804u,
           "higher-priority INT3 edge wakes through its documented vector");
    expect(state, dspic33_read_word(source, INTTREG) == 0x033du,
           "INT3 wake latches its vector number and priority");

    prepare_external_interrupts(source, 0u, 0u);
    enable_interrupt(source, 54u, 2u, 0x0520u);
    source->w[15] = 0x1900u;
    source->sr = 0x0040u;
    source->power_state = DSPIC33_POWER_IDLE;
    dspic33_gpio_drive(source, 3u, 0x0010u, 0x0010u);
    expect(state,
           dspic33_device_wake(source) && source->interrupt_count == 0u &&
               source->w[15] == 0x1900u && interrupt_flag(source, 54u),
           "equal-priority INT4 edge wakes from Idle without a frame");
    source->sr = 0u;
    expect(state,
           dspic33_device_service_interrupt(source) && source->last_interrupt == 54u &&
               source->pc == 0x0520u,
           "retained INT4 flag vectors after lowering IPL");

    prepare_external_interrupts(source, 0u, 0u);
    expect(state, dspic33_copy(copy, source), "copy preserves external INT state");
    dspic33_gpio_drive(source, 3u, 0x0002u, 0x0002u);
    expect(state,
           interrupt_flag(source, 20u) && !interrupt_flag(copy, 20u) &&
               source->io.external_interrupt_levels !=
                   copy->io.external_interrupt_levels,
           "copied external INT edge state diverges independently");

    clear_external_interrupts(source);
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, RPINR0) == 0u &&
               source->io.external_interrupt_selection[1] == 0u &&
               !interrupt_flag(source, 20u),
           "warm reset clears INT remap and rebaselines retained pins");
    dspic33_write_word(source, ANSELD, 0u);
    dspic33_write_word(source, TRISD, UINT16_MAX);
    dspic33_write_word(source, RPINR0, 65u << 8u);
    expect(state, !interrupt_flag(source, 20u),
           "warm-reset remap baselines the retained high pin");
    dspic33_gpio_drive(source, 3u, 0u, 0x0002u);
    dspic33_gpio_drive(source, 3u, 0x0002u, 0x0002u);
    expect(state, interrupt_flag(source, 20u),
           "warm-reset external INT state detects later edges");

    dspic33_reset(source, 0u);
    expect(state,
           source->io.external_interrupt_selection[0] == 64u &&
               source->io.external_interrupt_selection[1] == 0u &&
               source->io.external_interrupt_qualified == 0x1fu &&
               source->io.external_interrupt_levels == 0u,
           "POR restores fixed INT0 and VSS-remapped INT1 through INT4 state");
}

static void interrupt_entry_latency_cases(TestState* state, Dspic33* cpu) {
    uint64_t cycles;
    uint64_t device_cycles;

    dspic33_reset(cpu, 0x0200u);
    dspic33_load_program_word(cpu, 0x0200u, 0u);
    enable_interrupt(cpu, 0u, 1u, 0x0300u);
    cpu->w[15] = 0x2000u;
    dspic33_write_word(cpu, TIMER2_COUNTER, 0u);
    dspic33_write_word(cpu, TIMER2_PERIOD, UINT16_MAX);
    dspic33_write_word(cpu, TIMER2_CONTROL, 0x8000u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->pc == 0x0300u &&
               cpu->cycles == 9u && cpu->device_cycles == 9u &&
               dspic33_read_word(cpu, TIMER2_COUNTER) == 9u &&
               cpu->interrupt_depth == 1u && cpu->w[15] == 0x2004u,
           "interrupt entry consumes nine cycles before the first handler instruction");
    clear_interrupt(cpu, 0u);
    expect(
        state,
        dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u &&
            cpu->cycles == 10u && cpu->device_cycles == 10u &&
            dspic33_read_word(cpu, TIMER2_COUNTER) == 10u,
        "peripheral interrupt process reaches the handler in ten instruction cycles");

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 1u, 0x0300u);
    enable_interrupt(cpu, 1u, 2u, 0x0320u);
    cpu->w[15] = 0x2100u;
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 4u),
           "schedule interrupt request during entry latency");
    dspic33_raise_interrupt(cpu, 0u);
    expect(
        state,
        dspic33_device_service_interrupt(cpu) && cpu->pc == 0x0300u &&
            cpu->cycles == 9u && cpu->device_cycles == 9u && interrupt_flag(cpu, 1u) &&
            cpu->interrupt_depth == 1u,
        "entry latency processes a higher-priority request without premature service");
    clear_interrupt(cpu, 0u);
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->pc == 0x0320u &&
               cpu->cycles == 18u && cpu->device_cycles == 18u &&
               cpu->interrupt_depth == 2u,
           "pending higher-priority request receives its own entry latency");

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 1u, 0x0300u);
    cpu->w[15] = 0x2200u;
    dspic33_write_word(cpu, MAIN_CLOCK_DIVISOR, 0x1800u);
    dspic33_write_word(cpu, TIMER2_COUNTER, 0u);
    dspic33_write_word(cpu, TIMER2_PERIOD, UINT16_MAX);
    dspic33_write_word(cpu, TIMER2_CONTROL, 0x8000u);
    dspic33_raise_interrupt(cpu, 0u);
    cycles = cpu->cycles;
    device_cycles = cpu->device_cycles;
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->cycles - cycles == 9u &&
               cpu->device_cycles - device_cycles == 18u &&
               dspic33_read_word(cpu, TIMER2_COUNTER) == 18u &&
               (dspic33_read_word(cpu, MAIN_CLOCK_DIVISOR) & 0x0800u) != 0u,
           "entry latency retains the divided CPU ratio when ROI is clear");

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 1u, 0x0300u);
    cpu->w[15] = 0x2300u;
    dspic33_write_word(cpu, MAIN_CLOCK_DIVISOR, 0x9800u);
    dspic33_write_word(cpu, TIMER2_COUNTER, 0u);
    dspic33_write_word(cpu, TIMER2_PERIOD, UINT16_MAX);
    dspic33_write_word(cpu, TIMER2_CONTROL, 0x8000u);
    dspic33_raise_interrupt(cpu, 0u);
    cycles = cpu->cycles;
    device_cycles = cpu->device_cycles;
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->cycles - cycles == 9u &&
               cpu->device_cycles - device_cycles == 9u &&
               dspic33_read_word(cpu, TIMER2_COUNTER) == 9u &&
               (dspic33_read_word(cpu, MAIN_CLOCK_DIVISOR) & 0x0800u) == 0u,
           "ROI restores one-to-one timing before interrupt entry");

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 7u, 0x0300u);
    cpu->w[15] = 0x2400u;
    cpu->disicnt = 15u;
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->disicnt == 6u &&
               cpu->cycles == 9u,
           "priority-seven entry consumes DISI instruction cycles");

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 1u, 0x0300u);
    cpu->w[15] = 0x2500u;
    dspic33_raise_interrupt(cpu, 0u);
    cpu->cycles = UINT64_MAX - 8u;
    expect(state,
           dspic33_device_service_interrupt(cpu) &&
               cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               cpu->cycles == UINT64_MAX - 8u && cpu->pc == 0x0200u &&
               cpu->w[15] == 0x2500u && cpu->interrupt_count == 0u,
           "entry-cycle overflow fails before stack or vector mutation");
}

static bool schedule_interrupt_entry_dma(Dspic33* cpu, uint16_t address, uint16_t value,
                                         uint64_t delay) {
    uint32_t event_value;
    cpu->data[DMA_RECEIVE_PAD] = (uint8_t)value;
    cpu->data[DMA_RECEIVE_PAD + 1u] = (uint8_t)(value >> 8u);
    dspic33_write_word(cpu, DMA0_CONTROL, 0u);
    dspic33_write_word(cpu, DMA0_START_LOW, address);
    dspic33_write_word(cpu, DMA0_START_HIGH, 0u);
    dspic33_write_word(cpu, DMA0_PAD, DMA_RECEIVE_PAD);
    dspic33_write_word(cpu, DMA0_COUNT, 0u);
    dspic33_write_word(cpu, DMA0_CONTROL, 0x8001u);
    event_value = (uint32_t)cpu->io.dma_generation[0] << 17u;
    return dspic33_schedule(cpu, DSPIC33_EVENT_DMA, 0u, event_value, delay);
}

static bool service_interrupt_zero(Dspic33* cpu) {
    dspic33_raise_interrupt(cpu, 0u);
    return dspic33_device_service_interrupt(cpu);
}

static void interrupt_entry_frame_timing_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 1u, 0x0300u);
    cpu->w[15] = 0x2700u;
    expect(state,
           schedule_interrupt_entry_dma(cpu, 0x2700u, 0xa55au, 6u) &&
               service_interrupt_zero(cpu) &&
               dspic33_read_word(cpu, 0x2700u) == 0x0200u,
           "low return PC push follows the first six entry cycles");

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 1u, 0x0300u);
    cpu->w[15] = 0x2700u;
    expect(state,
           schedule_interrupt_entry_dma(cpu, 0x2700u, 0xa55au, 7u) &&
               service_interrupt_zero(cpu) &&
               dspic33_read_word(cpu, 0x2700u) == 0xa55au,
           "device event after the low return PC push observes the staged frame");

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 1u, 0x0300u);
    cpu->w[15] = 0x2700u;
    expect(state,
           schedule_interrupt_entry_dma(cpu, 0x2702u, 0xa55au, 9u) &&
               service_interrupt_zero(cpu) && dspic33_read_word(cpu, 0x2702u) == 0u,
           "high PC and SRL push completes after nine entry cycles");
}

static void interrupt_entry_overlap_cases(TestState* state, Dspic33* source,
                                          Dspic33* copy) {
    static const struct {
        uint32_t opcode;
        uint8_t instruction_cycles;
        uint8_t entry_overlap;
        bool psv;
        const char* name;
    } cases[] = {
        {OPCODE_NOP, 1u, 1u, false, "fixed latency overlaps the final NOP cycle"},
        {OPCODE_TBLRDL_W4_W3, 5u, 1u, false,
         "fixed latency retains the documented TBLRD stall"},
        {OPCODE_MOV_W4_W3, 5u, 4u, true,
         "fixed latency overlaps the ordinary PSV stall"},
        {OPCODE_MOV_DOUBLE_W4_W2, 5u, 2u, true,
         "fixed latency applies the MOV.D PSV second-fetch exception"},
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        uint64_t cycles;
        dspic33_reset(source, 0x0200u);
        dspic33_load_program_word(source, 0x0200u, cases[index].opcode);
        enable_interrupt(source, 0u, 1u, 0x0300u);
        source->w[15] = 0x2600u;
        source->disicnt = 1u;
        source->tblpag = 0u;
        source->dsrpag = cases[index].psv ? 0x0200u : 1u;
        dspic33_set_working_register(source, 4u, cases[index].psv ? 0x8000u : 0x0200u);
        dspic33_raise_interrupt(source, 0u);
        cycles = source->cycles;
        expect(state,
               dspic33_step(source) == DSPIC33_RUNNING &&
                   source->cycles - cycles == cases[index].instruction_cycles &&
                   source->interrupt_entry_overlap == cases[index].entry_overlap &&
                   source->pc == 0x0202u,
               cases[index].name);
        cycles = source->cycles;
        expect(state,
               dspic33_step(source) == DSPIC33_RUNNING && source->pc == 0x0302u &&
                   source->cycles - cycles == 10u - cases[index].entry_overlap &&
                   source->interrupt_entry_overlap == 0u,
               "recorded overlap shortens only the following interrupt entry");
    }

    dspic33_reset(source, 0x0200u);
    dspic33_load_program_word(source, 0x0200u, OPCODE_MOV_W4_W3);
    enable_interrupt(source, 0u, 1u, 0x0300u);
    source->disicnt = 1u;
    source->dsrpag = 0x0200u;
    dspic33_set_working_register(source, 4u, 0x8000u);
    dspic33_raise_interrupt(source, 0u);
    dspic33_step(source);
    expect(state,
           source->interrupt_entry_overlap == 4u && dspic33_copy(copy, source) &&
               copy->interrupt_entry_overlap == 4u,
           "copy preserves a pending fixed-latency overlap");
    source->interrupt_entry_overlap = 0u;
    expect(state, copy->interrupt_entry_overlap == 4u,
           "copied fixed-latency overlap is independent");
    dspic33_reset(source, 0u);
    expect(state, source->interrupt_entry_overlap == 0u,
           "POR clears fixed-latency overlap state");
}

static void priority_control_cases(TestState* state, Dspic33* cpu) {
    uint64_t cycles;

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 3u, 0x0300u);
    enable_interrupt(cpu, 1u, 3u, 0x0320u);
    dspic33_load_program_word(cpu, 0x0300u, OPCODE_RETFIE);
    cpu->w[15] = 0x2800u;
    cpu->sr = 0x0105u;
    dspic33_raise_interrupt(cpu, 1u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == 0u &&
               cpu->pc == 0x0300u && interrupt_flag(cpu, 1u),
           "equal-priority requests select the lowest vector number");
    clear_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0200u &&
               cpu->sr == 0x0105u && cpu->w[15] == 0x2800u,
           "RETFIE restores priority and status before the pending peer interrupt");
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == 1u &&
               cpu->pc == 0x0320u,
           "pending equal-priority peer enters after RETFIE");

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 2u, 0x0300u);
    enable_interrupt(cpu, 1u, 7u, 0x0320u);
    dspic33_load_program_word(cpu, 0x0300u, OPCODE_RETFIE);
    dspic33_write_word(cpu, INTCON1, 0x8000u);
    cpu->w[15] = 0x2900u;
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_device_service_interrupt(cpu) && (cpu->sr & 0x00e0u) == 0x00e0u &&
               cpu->last_interrupt == 0u,
           "NSTDIS forces an entered interrupt to CPU priority seven");
    clear_interrupt(cpu, 0u);
    dspic33_raise_interrupt(cpu, 1u);
    expect(state, !dspic33_device_service_interrupt(cpu) && interrupt_flag(cpu, 1u),
           "NSTDIS masks every nested user interrupt");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && (cpu->sr & 0x00e0u) == 0u &&
               dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == 1u,
           "RETFIE restores priority and permits a deferred interrupt after NSTDIS");

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 0u, 6u, 0x0300u);
    dspic33_raise_interrupt(cpu, 0u);
    dspic33_write_word(cpu, INTCON2, 0u);
    expect(state, !dspic33_device_service_interrupt(cpu) && interrupt_flag(cpu, 0u),
           "GIE masks enabled user interrupts without clearing their flags");
    dspic33_write_word(cpu, INTCON2, 0x8000u);
    cpu->disicnt = 1u;
    expect(state, !dspic33_device_service_interrupt(cpu),
           "DISI masks priorities below seven");
    dspic33_write_word(cpu, 0x0840u, 7u);
    expect(state, dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == 0u,
           "DISI permits priority-seven interrupts");

    dspic33_reset(cpu, 0x0200u);
    dspic33_load_program_word(cpu, 0x0200u, OPCODE_NOP);
    dspic33_load_program_word(cpu, 0x0202u, OPCODE_NOP);
    enable_interrupt(cpu, 0u, 1u, 0x0300u);
    cpu->w[15] = 0x2a00u;
    cpu->disicnt = 1u;
    cpu->corcon |= 0x8000u;
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0202u &&
               cpu->interrupt_entry_overlap == 1u,
           "variable-latency mode records but does not consume fixed overlap");
    cycles = cpu->cycles;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u &&
               cpu->cycles - cycles == 9u && cpu->interrupt_entry_overlap == 0u,
           "variable-latency mode includes the completed instruction in entry timing");

    dspic33_reset(cpu, 0x0200u);
    enable_interrupt(cpu, 142u, 4u, 0x0340u);
    cpu->w[15] = 0x2b00u;
    dspic33_raise_interrupt(cpu, 142u);
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == 142u &&
               cpu->pc == 0x0340u && dspic33_read_word(cpu, INTTREG) == 0x0496u,
           "highest implemented IRQ maps to vector one hundred fifty");
}

static void lifecycle_cases(TestState* state, Dspic33* source, Dspic33* copy) {
    dspic33_reset(source, 0u);
    dspic33_reset(copy, 0u);
    dspic33_write_word(source, INTCON1, 0x7800u);
    dspic33_device_latch_interrupt(source, 2u, 13u);
    expect(state, dspic33_copy(copy, source), "copy preserves interrupt controls");
    expect(state,
           dspic33_read_word(copy, INTCON1) == 0x7800u &&
               dspic33_read_word(copy, INTTREG) == 0x0d02u,
           "copy preserves interrupt status and dynamic latch");
    dspic33_write_word(source, INTCON1, 0u);
    expect(state, dspic33_read_word(copy, INTCON1) == 0x7800u,
           "copied interrupt status is independent");
    dspic33_reset(source, 0u);
    expect(state,
           dspic33_read_word(source, INTCON1) == 0u &&
               dspic33_read_word(source, INTCON2) == 0x8000u &&
               dspic33_read_word(source, INTCON3) == 0u &&
               dspic33_read_word(source, INTCON4) == 0u &&
               dspic33_read_word(source, INTTREG) == 0u,
           "POR restores interrupt-control state");
}

static void variable_latency_erratum_cases(TestState* state, Dspic33* source,
                                           Dspic33* copy) {
    dspic33_reset(source, 0x0200u);
    source->corcon |= 0x8000u;
    source->w[15] = 0x2c00u;
    dspic33_load_program_word(source, 0x0200u, OPCODE_BSET_DATA_0);
    enable_interrupt(source, 0u, 1u, 0x6000u);
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_byte(source, 0x1200u) == 1u,
           "VAR mode records a data-variable write outside an ISR");
    source->program[0x6000u / 2u] = OPCODE_BSET_DATA_0;
    expect(state, dspic33_copy(copy, source),
           "copy preserves VAR write-domain history");
    dspic33_raise_interrupt(source, 0u);
    dspic33_raise_interrupt(copy, 0u);
    expect(state,
           dspic33_step(source) == DSPIC33_SILICON_RESULT_UNDEFINED &&
               dspic33_step(copy) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "B1 shared VAR write across mainline and ISR remains silicon-undefined");

    dspic33_reset(source, 0x0200u);
    source->w[15] = 0x2c00u;
    dspic33_load_program_word(source, 0x0200u, OPCODE_BSET_DATA_0);
    enable_interrupt(source, 0u, 1u, 0x6000u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "fixed-latency mainline data write remains defined");
    source->program[0x6000u / 2u] = OPCODE_BSET_DATA_0;
    dspic33_raise_interrupt(source, 0u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "fixed-latency mode does not enter the VAR erratum boundary");

    dspic33_reset(source, 0x0200u);
    source->w[15] = 0x2c00u;
    dspic33_load_program_word(source, 0x0200u, OPCODE_BSET_DATA_0);
    enable_interrupt(source, 0u, 1u, 0x6000u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "pre-VAR mainline write remains defined");
    source->corcon |= 0x8000u;
    source->program[0x6000u / 2u] = OPCODE_BSET_DATA_0;
    dspic33_raise_interrupt(source, 0u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "pre-VAR history does not contaminate variable-latency tracking");
}

int main(void) {
    Dspic33 source;
    Dspic33 copy;
    TestState state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    access_cases(&state, &source);
    generic_hard_cases(&state, &source);
    generic_soft_cases(&state, &source);
    external_interrupt_edge_cases(&state, &source);
    external_interrupt_selection_cases(&state, &source);
    external_interrupt_interaction_cases(&state, &source);
    external_interrupt_wake_lifecycle_cases(&state, &source, &copy);
    interrupt_entry_latency_cases(&state, &source);
    interrupt_entry_overlap_cases(&state, &source, &copy);
    interrupt_entry_frame_timing_cases(&state, &source);
    priority_control_cases(&state, &source);
    variable_latency_erratum_cases(&state, &source, &copy);
    lifecycle_cases(&state, &source, &copy);
    dspic33_release(&copy);
    dspic33_release(&source);
    return test_finish(&state);
}
