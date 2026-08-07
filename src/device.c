#include "device.h"

#include <stdlib.h>
#include <string.h>

static const uint16_t timer_registers[DSPIC33_TIMER_COUNT] = {
    0x0100u, 0x0106u, 0x010au, 0x0114u, 0x0118u, 0x0122u, 0x0126u, 0x0130u, 0x0134u};
static const uint16_t timer_periods[DSPIC33_TIMER_COUNT] = {
    0x0102u, 0x010cu, 0x010eu, 0x011au, 0x011cu, 0x0128u, 0x012au, 0x0136u, 0x0138u};
static const uint16_t timer_controls[DSPIC33_TIMER_COUNT] = {
    0x0104u, 0x0110u, 0x0112u, 0x011eu, 0x0120u, 0x012cu, 0x012eu, 0x013au, 0x013cu};
static const uint8_t timer_irqs[DSPIC33_TIMER_COUNT] = {3u,  7u,  8u,  27u, 28u,
                                                        47u, 48u, 51u, 52u};
static const uint8_t dma_irqs[DSPIC33_DMA_COUNT] = {
    4u, 14u, 24u, 36u, 46u, 61u, 68u, 69u, 118u, 119u, 120u, 121u, 130u, 131u, 132u};
static const uint8_t uart_rx_irqs[DSPIC33_UART_COUNT] = {11u, 30u, 82u, 88u};
static const uint8_t uart_tx_irqs[DSPIC33_UART_COUNT] = {12u, 31u, 83u, 89u};
static const uint8_t spi_irqs[DSPIC33_SPI_COUNT] = {10u, 33u, 91u, 123u};
static const uint16_t uart_bases[DSPIC33_UART_COUNT] = {0x0220u, 0x0230u, 0x0250u,
                                                        0x02b0u};
static const uint16_t spi_bases[DSPIC33_SPI_COUNT] = {0x0240u, 0x0260u, 0x02a0u,
                                                      0x02c0u};

typedef struct {
    uint16_t address;
    uint16_t value;
} Dspic33ResetValue;

static const Dspic33ResetValue reset_values[] = {
    {0x004au, 0x0001u}, {0x004eu, 0x0001u}, {0x0102u, 0xffffu}, {0x010cu, 0xffffu},
    {0x010eu, 0xffffu}, {0x011au, 0xffffu}, {0x011cu, 0xffffu}, {0x0128u, 0xffffu},
    {0x012au, 0xffffu}, {0x0136u, 0xffffu}, {0x0138u, 0xffffu}, {0x0142u, 0x000du},
    {0x014au, 0x000du}, {0x0152u, 0x000du}, {0x015au, 0x000du}, {0x0162u, 0x000du},
    {0x016au, 0x000du}, {0x0172u, 0x000du}, {0x017au, 0x000du}, {0x0182u, 0x000du},
    {0x018au, 0x000du}, {0x0192u, 0x000du}, {0x019au, 0x000du}, {0x01a2u, 0x000du},
    {0x01aau, 0x000du}, {0x01b2u, 0x000du}, {0x01bau, 0x000du}, {0x0202u, 0x00ffu},
    {0x0206u, 0x1000u}, {0x0212u, 0x00ffu}, {0x0216u, 0x1000u}, {0x0222u, 0x0110u},
    {0x0232u, 0x0110u}, {0x0252u, 0x0110u}, {0x02b2u, 0x0110u}, {0x0400u, 0x0480u},
    {0x0404u, 0x0040u}, {0x0414u, 0x003fu}, {0x0500u, 0x0480u}, {0x0504u, 0x0040u},
    {0x0514u, 0x003fu}, {0x060eu, 0x008fu}, {0x0640u, 0x0040u}, {0x0740u, 0x0080u},
    {0x0744u, 0x3040u}, {0x0746u, 0x0030u}, {0x0840u, 0x4444u}, {0x0842u, 0x4444u},
    {0x0844u, 0x4444u}, {0x0846u, 0x0444u}, {0x0848u, 0x4444u}, {0x084au, 0x0004u},
    {0x084cu, 0x4444u}, {0x084eu, 0x4444u}, {0x0850u, 0x4444u}, {0x0852u, 0x0444u},
    {0x0858u, 0x4444u}, {0x085au, 0x4444u}, {0x085cu, 0x4004u}, {0x085eu, 0x0044u},
    {0x0860u, 0x0440u}, {0x0862u, 0x4444u}, {0x0864u, 0x4040u}, {0x0868u, 0x4440u},
    {0x086eu, 0x4400u}, {0x0870u, 0x4444u}, {0x0902u, 0x000cu}, {0x090cu, 0x000cu},
    {0x0916u, 0x000cu}, {0x0920u, 0x000cu}, {0x092au, 0x000cu}, {0x0934u, 0x000cu},
    {0x093eu, 0x000cu}, {0x0948u, 0x000cu}, {0x0952u, 0x000cu}, {0x095cu, 0x000cu},
    {0x0966u, 0x000cu}, {0x0970u, 0x000cu}, {0x097au, 0x000cu}, {0x0984u, 0x000cu},
    {0x098eu, 0x000cu}, {0x0998u, 0x000cu}, {0x0c04u, 0xffffu}, {0x0c12u, 0xffffu},
    {0x0e00u, 0xc6ffu}, {0x0e0eu, 0x06c0u}, {0x0e10u, 0xffffu}, {0x0e1eu, 0xffffu},
    {0x0e20u, 0xf01eu}, {0x0e2eu, 0x601eu}, {0x0e30u, 0xffffu}, {0x0e3eu, 0x00c0u},
    {0x0e40u, 0x03ffu}, {0x0e4eu, 0x03ffu}, {0x0e50u, 0x313fu}, {0x0e60u, 0xf3c3u},
    {0x0e6eu, 0x03c0u}};

static uint16_t raw_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] |
                      ((uint16_t)cpu->data[(uint16_t)(address + 1u)] << 8u));
}

static void raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[(uint16_t)(address + 1u)] = (uint8_t)(value >> 8u);
}

static bool byte_queue_push(Dspic33ByteQueue* queue, uint8_t value) {
    uint16_t index;
    if (queue->count == sizeof(queue->bytes)) {
        return false;
    }
    index = (uint16_t)((queue->head + queue->count) % sizeof(queue->bytes));
    queue->bytes[index] = value;
    queue->count++;
    return true;
}

static bool byte_queue_pop(Dspic33ByteQueue* queue, uint8_t* value) {
    if (queue->count == 0u) {
        return false;
    }
    *value = queue->bytes[queue->head];
    queue->head = (uint16_t)((queue->head + 1u) % sizeof(queue->bytes));
    queue->count--;
    return true;
}

static bool can_queue_push(Dspic33CanQueue* queue, const Dspic33CanFrame* frame) {
    uint8_t index;
    if (queue->count == 64u) {
        return false;
    }
    index = (uint8_t)((queue->head + queue->count) % 64u);
    queue->frames[index] = *frame;
    queue->count++;
    return true;
}

static bool can_queue_pop(Dspic33CanQueue* queue, Dspic33CanFrame* frame) {
    if (queue->count == 0u) {
        return false;
    }
    *frame = queue->frames[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % 64u);
    queue->count--;
    return true;
}

static bool event_less(const Dspic33Event* left, const Dspic33Event* right) {
    return left->cycle < right->cycle ||
           (left->cycle == right->cycle && left->sequence < right->sequence);
}

static bool event_reserve(Dspic33EventQueue* queue) {
    Dspic33Event* items;
    size_t capacity;
    if (queue->count < queue->capacity) {
        return true;
    }
    capacity = queue->capacity == 0u ? 64u : queue->capacity * 2u;
    items = realloc(queue->items, capacity * sizeof(*items));
    if (items == NULL) {
        return false;
    }
    queue->items = items;
    queue->capacity = capacity;
    return true;
}

bool dspic33_schedule(Dspic33* cpu, Dspic33EventType type, uint16_t source,
                      uint32_t value, uint64_t delay) {
    Dspic33Event event;
    size_t index;
    size_t parent;
    if (!event_reserve(&cpu->events)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    event.cycle = cpu->cycles + delay;
    event.sequence = cpu->events.sequence++;
    event.value = value;
    event.source = source;
    event.type = type;
    index = cpu->events.count++;
    while (index != 0u) {
        parent = (index - 1u) / 2u;
        if (!event_less(&event, &cpu->events.items[parent])) {
            break;
        }
        cpu->events.items[index] = cpu->events.items[parent];
        index = parent;
    }
    cpu->events.items[index] = event;
    return true;
}

static Dspic33Event event_pop(Dspic33EventQueue* queue) {
    Dspic33Event result = queue->items[0];
    Dspic33Event tail = queue->items[--queue->count];
    size_t index = 0u;
    while (index * 2u + 1u < queue->count) {
        size_t child = index * 2u + 1u;
        if (child + 1u < queue->count &&
            event_less(&queue->items[child + 1u], &queue->items[child])) {
            child++;
        }
        if (!event_less(&queue->items[child], &tail)) {
            break;
        }
        queue->items[index] = queue->items[child];
        index = child;
    }
    if (queue->count != 0u) {
        queue->items[index] = tail;
    }
    return result;
}

void dspic33_raise_interrupt(Dspic33* cpu, uint16_t irq) {
    uint16_t address;
    uint16_t value;
    if (irq >= DSPIC33_IRQ_COUNT) {
        return;
    }
    address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    value = raw_word(cpu, address);
    value = (uint16_t)(value | (uint16_t)(1u << (irq % 16u)));
    raw_write_word(cpu, address, value);
}

static uint8_t interrupt_priority(const Dspic33* cpu, uint16_t irq) {
    uint16_t value = raw_word(cpu, (uint16_t)(0x0840u + (irq / 4u) * 2u));
    return (uint8_t)((value >> ((irq % 4u) * 4u)) & 0x07u);
}

static bool interrupt_enabled(const Dspic33* cpu, uint16_t irq) {
    uint16_t mask = (uint16_t)(1u << (irq % 16u));
    uint16_t offset = (uint16_t)((irq / 16u) * 2u);
    return (raw_word(cpu, (uint16_t)(0x0800u + offset)) & mask) != 0u &&
           (raw_word(cpu, (uint16_t)(0x0820u + offset)) & mask) != 0u;
}

bool dspic33_device_service_interrupt(Dspic33* cpu) {
    uint8_t current = (uint8_t)((cpu->sr >> 5u) & 0x07u);
    uint8_t best_priority = current;
    uint16_t next_priority;
    uint16_t best_irq = DSPIC33_IRQ_COUNT;
    size_t log_index;
    uint16_t irq;
    uint16_t stacked_high;
    if ((raw_word(cpu, 0x08c2u) & 0x8000u) == 0u || (cpu->corcon & 0x0008u) != 0u) {
        return false;
    }
    for (irq = 0u; irq < DSPIC33_IRQ_COUNT; irq++) {
        uint8_t priority;
        if (!interrupt_enabled(cpu, irq)) {
            continue;
        }
        priority = interrupt_priority(cpu, irq);
        if (cpu->disicnt != 0u && priority < 7u) {
            continue;
        }
        if (priority > best_priority) {
            best_priority = priority;
            best_irq = irq;
        }
    }
    if (best_irq == DSPIC33_IRQ_COUNT) {
        return false;
    }
    dspic33_write_word(cpu, cpu->w[15],
                       (uint16_t)((cpu->pc & 0xfffeu) | ((cpu->corcon >> 2u) & 1u)));
    cpu->w[15] += 2u;
    stacked_high = (uint16_t)(((cpu->sr & 0x00ffu) << 8u) |
                              ((cpu->corcon & 0x0008u) != 0u ? 0x0080u : 0u) |
                              ((cpu->pc >> 16u) & 0x007fu));
    dspic33_write_word(cpu, cpu->w[15], stacked_high);
    cpu->w[15] += 2u;
    cpu->corcon &= (uint16_t)~0x0004u;
    next_priority = (raw_word(cpu, 0x08c0u) & 0x8000u) != 0u
                        ? UINT16_C(0x00e0)
                        : (uint16_t)((uint16_t)best_priority << 5u);
    cpu->sr = (uint16_t)((cpu->sr & ~0x00e0u) | next_priority);
    log_index = (size_t)(cpu->interrupt_count % 16u);
    cpu->interrupt_log_irq[log_index] = best_irq;
    cpu->interrupt_log_entry[log_index] = cpu->pc;
    cpu->interrupt_log_return[log_index] = 0u;
    cpu->pc = cpu->program[(0x0014u + best_irq * 2u) / 2u] & 0x007ffffeu;
    cpu->last_interrupt = best_irq;
    cpu->interrupt_count++;
    cpu->interrupt_depth++;
    return true;
}

void dspic33_device_return_interrupt(Dspic33* cpu) {
    uint16_t high;
    uint16_t low;
    cpu->w[15] -= 2u;
    high = dspic33_read_word(cpu, cpu->w[15]);
    cpu->w[15] -= 2u;
    low = dspic33_read_word(cpu, cpu->w[15]);
    cpu->pc = ((uint32_t)(high & 0x007fu) << 16u) | (low & 0xfffeu);
    cpu->last_interrupt_return = cpu->pc;
    if (cpu->interrupt_count != 0u) {
        cpu->interrupt_log_return[(cpu->interrupt_count - 1u) % 16u] = cpu->pc;
    }
    cpu->sr = (uint16_t)((cpu->sr & 0xff00u) | (high >> 8u));
    if ((high & 0x0080u) != 0u) {
        cpu->corcon |= 0x0008u;
    } else {
        cpu->corcon &= (uint16_t)~0x0008u;
    }
    cpu->corcon = (uint16_t)((cpu->corcon & ~0x0004u) | ((low & 1u) << 2u));
    if (cpu->interrupt_depth != 0u) {
        cpu->interrupt_depth--;
    }
}

static void run_dma(Dspic33* cpu, uint8_t channel) {
    uint16_t base;
    uint16_t control;
    uint16_t count;
    uint16_t pad;
    uint32_t start;
    uint32_t address;
    uint16_t index;
    uint8_t width;
    bool direction;
    if (channel >= DSPIC33_DMA_COUNT) {
        return;
    }
    base = (uint16_t)(0x0b00u + channel * 0x10u);
    control = raw_word(cpu, base);
    if ((control & 0x8000u) == 0u) {
        return;
    }
    count = raw_word(cpu, (uint16_t)(base + 0x0eu));
    pad = raw_word(cpu, (uint16_t)(base + 0x0cu));
    start = ((uint32_t)raw_word(cpu, (uint16_t)(base + 0x06u)) << 16u) |
            raw_word(cpu, (uint16_t)(base + 0x04u));
    index = cpu->io.dma_index[channel];
    width = (control & 0x4000u) != 0u ? 1u : 2u;
    direction = (control & 0x2000u) != 0u;
    address = (start + (uint32_t)index * width) % DSPIC33_DATA_SIZE;
    if (direction) {
        if (width == 1u) {
            dspic33_write_byte(cpu, pad, cpu->data[address]);
        } else {
            uint16_t value = (uint16_t)(cpu->data[address] |
                                        ((uint16_t)cpu->data[address + 1u] << 8u));
            dspic33_write_word(cpu, pad, value);
        }
    } else if (width == 1u) {
        cpu->data[address] = dspic33_read_byte(cpu, pad);
    } else {
        uint16_t value = dspic33_read_word(cpu, pad);
        cpu->data[address] = (uint8_t)value;
        cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    }
    if (index >= count) {
        cpu->io.dma_index[channel] = 0u;
        dspic33_raise_interrupt(cpu, dma_irqs[channel]);
        if ((control & 0x0001u) == 0u) {
            raw_write_word(cpu, base, (uint16_t)(control & ~0x8000u));
        }
    } else {
        cpu->io.dma_index[channel]++;
    }
}

static void run_uart(Dspic33* cpu, uint8_t channel) {
    uint16_t base;
    uint16_t status;
    uint8_t value;
    if (channel >= DSPIC33_UART_COUNT) {
        return;
    }
    base = uart_bases[channel];
    status = raw_word(cpu, (uint16_t)(base + 2u));
    if (byte_queue_pop(&cpu->io.uart_rx[channel], &value)) {
        raw_write_word(cpu, (uint16_t)(base + 6u), value);
        raw_write_word(cpu, (uint16_t)(base + 2u), (uint16_t)(status | 0x0001u));
        dspic33_raise_interrupt(cpu, uart_rx_irqs[channel]);
    }
}

static void run_spi(Dspic33* cpu, uint8_t channel, uint16_t fallback) {
    uint16_t base;
    uint16_t value = fallback;
    uint8_t low;
    uint8_t high;
    if (channel >= DSPIC33_SPI_COUNT) {
        return;
    }
    base = spi_bases[channel];
    if (byte_queue_pop(&cpu->io.spi_rx[channel], &low)) {
        value = low;
        if (byte_queue_pop(&cpu->io.spi_rx[channel], &high)) {
            value |= (uint16_t)high << 8u;
        }
    }
    raw_write_word(cpu, (uint16_t)(base + 8u), value);
    raw_write_word(cpu, base, (uint16_t)(raw_word(cpu, base) | 0x0001u));
    dspic33_raise_interrupt(cpu, spi_irqs[channel]);
}

static void run_adc(Dspic33* cpu, uint8_t module) {
    uint16_t control = module == 0u ? 0x0320u : 0x0360u;
    uint16_t buffer = module == 0u ? 0x0300u : 0x0340u;
    uint16_t channels = module == 0u ? 16u : 16u;
    uint16_t index;
    for (index = 0u; index < channels; index++) {
        raw_write_word(cpu, (uint16_t)(buffer + index * 2u), cpu->io.adc[index]);
    }
    raw_write_word(cpu, control, (uint16_t)(raw_word(cpu, control) | 0x0001u));
    dspic33_raise_interrupt(cpu, module == 0u ? 13u : 21u);
}

static void run_can(Dspic33* cpu, uint8_t channel) {
    Dspic33CanFrame frame;
    uint16_t base;
    uint8_t index;
    if (channel >= DSPIC33_CAN_COUNT ||
        !can_queue_pop(&cpu->io.can_rx[channel], &frame)) {
        return;
    }
    base = channel == 0u ? 0x0400u : 0x0500u;
    raw_write_word(cpu, (uint16_t)(base + 4u), 0x0040u);
    raw_write_word(cpu, (uint16_t)(base + 0x20u),
                   (uint16_t)(raw_word(cpu, (uint16_t)(base + 0x20u)) | 1u));
    raw_write_word(cpu, (uint16_t)(base + 0x40u),
                   (uint16_t)((frame.identifier & 0x7ffu) << 2u));
    for (index = 0u; index < frame.length; index += 2u) {
        uint16_t value = frame.data[index];
        if (index + 1u < frame.length) {
            value |= (uint16_t)frame.data[index + 1u] << 8u;
        }
        raw_write_word(cpu, (uint16_t)(base + 0x42u + index), value);
    }
    dspic33_raise_interrupt(cpu, channel == 0u ? 34u : 55u);
}

static void process_event(Dspic33* cpu, const Dspic33Event* event) {
    switch (event->type) {
    case DSPIC33_EVENT_INTERRUPT:
        dspic33_raise_interrupt(cpu, event->source);
        break;
    case DSPIC33_EVENT_TIMER:
        dspic33_raise_interrupt(cpu, event->source);
        break;
    case DSPIC33_EVENT_DMA:
        run_dma(cpu, (uint8_t)event->source);
        break;
    case DSPIC33_EVENT_ADC:
        run_adc(cpu, (uint8_t)event->source);
        break;
    case DSPIC33_EVENT_UART:
        run_uart(cpu, (uint8_t)event->source);
        break;
    case DSPIC33_EVENT_SPI:
        run_spi(cpu, (uint8_t)event->source, (uint16_t)event->value);
        break;
    case DSPIC33_EVENT_CAN:
        run_can(cpu, (uint8_t)event->source);
        break;
    case DSPIC33_EVENT_USB:
        raw_write_word(cpu, 0x04c0u, (uint16_t)(raw_word(cpu, 0x04c0u) | 0x0008u));
        dspic33_raise_interrupt(cpu, 86u);
        break;
    case DSPIC33_EVENT_NVM:
        raw_write_word(cpu, 0x0728u, (uint16_t)(raw_word(cpu, 0x0728u) & ~0x8000u));
        break;
    case DSPIC33_EVENT_AUX_PLL:
        raw_write_word(cpu, 0x0758u, (uint16_t)(raw_word(cpu, 0x0758u) | 0x4000u));
        break;
    }
}

static void advance_timers(Dspic33* cpu, uint64_t cycles) {
    uint16_t enabled = cpu->io.timer_enabled;
    uint8_t timer = 0u;
    while (enabled != 0u) {
        if ((enabled & 1u) != 0u) {
            uint16_t control = raw_word(cpu, timer_controls[timer]);
            uint16_t prescale_bits = (uint16_t)((control >> 4u) & 3u);
            uint32_t prescale = prescale_bits == 0u   ? 1u
                                : prescale_bits == 1u ? 8u
                                : prescale_bits == 2u ? 64u
                                                      : 256u;
            uint64_t accumulated = cpu->io.timer_fraction[timer] + cycles;
            uint32_t ticks = (uint32_t)(accumulated / prescale);
            uint32_t value;
            uint32_t period;
            cpu->io.timer_fraction[timer] = (uint32_t)(accumulated % prescale);
            value = raw_word(cpu, timer_registers[timer]) + ticks;
            period = (uint32_t)raw_word(cpu, timer_periods[timer]) + 1u;
            if (period != 0u && value >= period) {
                value %= period;
                dspic33_raise_interrupt(cpu, timer_irqs[timer]);
            }
            raw_write_word(cpu, timer_registers[timer], (uint16_t)value);
        }
        enabled >>= 1u;
        timer++;
    }
}

bool dspic33_device_advance(Dspic33* cpu, uint64_t cycles) {
    cpu->cycles += cycles;
    if (cpu->disicnt > cycles) {
        cpu->disicnt = (uint16_t)(cpu->disicnt - cycles);
    } else {
        cpu->disicnt = 0u;
    }
    advance_timers(cpu, cycles);
    while (cpu->events.count != 0u && cpu->events.items[0].cycle <= cpu->cycles) {
        Dspic33Event event = event_pop(&cpu->events);
        process_event(cpu, &event);
        if (cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR) {
            return false;
        }
    }
    return true;
}

static void update_timer_control(Dspic33* cpu, uint16_t address) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        if ((address & 0xfffeu) == timer_controls[timer]) {
            if ((raw_word(cpu, timer_controls[timer]) & 0x8002u) == 0x8000u) {
                cpu->io.timer_enabled |= (uint16_t)(1u << timer);
            } else {
                cpu->io.timer_enabled &= (uint16_t)~(1u << timer);
            }
            return;
        }
    }
}

static void write_uart(Dspic33* cpu, uint16_t address) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t base = uart_bases[channel];
        if ((address & 0xfffeu) == base + 4u && (raw_word(cpu, base) & 0x8000u) != 0u &&
            (raw_word(cpu, (uint16_t)(base + 2u)) & 0x0400u) != 0u) {
            uint8_t value = (uint8_t)raw_word(cpu, (uint16_t)(base + 4u));
            byte_queue_push(&cpu->io.uart_tx[channel], value);
            raw_write_word(cpu, (uint16_t)(base + 2u),
                           (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~0x0100u));
            dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, uart_tx_irqs[channel], 0u,
                             1u);
            return;
        }
    }
}

static void write_spi(Dspic33* cpu, uint16_t address) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = spi_bases[channel];
        if ((address & 0xfffeu) == base + 8u && (raw_word(cpu, base) & 0x8000u) != 0u) {
            uint16_t value = raw_word(cpu, (uint16_t)(base + 8u));
            byte_queue_push(&cpu->io.spi_tx[channel], (uint8_t)value);
            byte_queue_push(&cpu->io.spi_tx[channel], (uint8_t)(value >> 8u));
            raw_write_word(cpu, base,
                           (uint16_t)((raw_word(cpu, base) & ~0x0001u) | 0x0002u));
            dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, value, 1u);
            return;
        }
    }
}

static void update_oscillator(Dspic33* cpu, uint16_t address) {
    uint16_t control;
    if (address != 0x0742u) {
        return;
    }
    control = raw_word(cpu, 0x0742u);
    if ((control & 0x0001u) != 0u) {
        control =
            (uint16_t)((control & 0x8fffu) | ((control & 0x0700u) << 4u) | 0x0020u);
        control &= (uint16_t)~0x0001u;
        raw_write_word(cpu, 0x0742u, control);
    }
}

void dspic33_device_write_byte(Dspic33* cpu, uint16_t address) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    uint8_t channel;
    if (base == 0x0758u) {
        raw_write_word(cpu, base,
                       (uint16_t)((raw_word(cpu, base) & 0x40ffu) | 0xa400u));
        if ((raw_word(cpu, base) & 0x8000u) != 0u &&
            (raw_word(cpu, base) & 0x4000u) == 0u) {
            dspic33_schedule(cpu, DSPIC33_EVENT_AUX_PLL, 0u, 0u, 32u);
        }
    }
    update_timer_control(cpu, base);
    update_oscillator(cpu, base);
    write_uart(cpu, base);
    write_spi(cpu, base);
    if (base == 0x0728u && (raw_word(cpu, base) & 0x8000u) != 0u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_NVM, 0u, 0u, 2u);
    }
    if (base == 0x0320u && (raw_word(cpu, base) & 0x8002u) == 0x8002u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_ADC, 0u, 0u, 1u);
    }
    if (base == 0x0360u && (raw_word(cpu, base) & 0x8002u) == 0x8002u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_ADC, 1u, 0u, 1u);
    }
    if (base >= 0x0b00u && base <= 0x0be0u && (base & 0x000fu) == 0u) {
        channel = (uint8_t)((base - 0x0b00u) / 0x10u);
        cpu->io.dma_index[channel] = 0u;
        if ((raw_word(cpu, base) & 0x8000u) != 0u) {
            cpu->io.dma_enabled |= (uint16_t)(1u << channel);
        } else {
            cpu->io.dma_enabled &= (uint16_t)~(1u << channel);
        }
    }
}

uint8_t dspic33_device_read_byte(Dspic33* cpu, uint16_t address, uint8_t value) {
    static const uint16_t port_addresses[DSPIC33_GPIO_PORT_COUNT] = {
        0x0e02u, 0x0e12u, 0x0e22u, 0x0e32u, 0x0e42u, 0x0e52u, 0x0e62u};
    static const uint16_t tris_addresses[DSPIC33_GPIO_PORT_COUNT] = {
        0x0e00u, 0x0e10u, 0x0e20u, 0x0e30u, 0x0e40u, 0x0e50u, 0x0e60u};
    static const uint16_t lat_addresses[DSPIC33_GPIO_PORT_COUNT] = {
        0x0e04u, 0x0e14u, 0x0e24u, 0x0e34u, 0x0e44u, 0x0e54u, 0x0e64u};
    uint8_t port;
    uint8_t channel;
    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        if ((address & 0xfffeu) == port_addresses[port]) {
            uint16_t tris = raw_word(cpu, tris_addresses[port]);
            uint16_t lat = raw_word(cpu, lat_addresses[port]);
            uint16_t pins = (uint16_t)((cpu->io.gpio[port] & tris) | (lat & ~tris));
            return (uint8_t)(pins >> ((address & 1u) * 8u));
        }
    }
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t base = uart_bases[channel];
        if ((address & 0xfffeu) == base + 6u && (address & 1u) != 0u) {
            raw_write_word(cpu, (uint16_t)(base + 2u),
                           (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~0x0001u));
        }
    }
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = spi_bases[channel];
        if ((address & 0xfffeu) == base + 8u && (address & 1u) != 0u) {
            raw_write_word(cpu, base, (uint16_t)(raw_word(cpu, base) & ~0x0001u));
        }
    }
    return value;
}

bool dspic33_uart_receive(Dspic33* cpu, uint8_t channel, uint8_t value,
                          uint64_t delay) {
    return channel < DSPIC33_UART_COUNT &&
           byte_queue_push(&cpu->io.uart_rx[channel], value) &&
           dspic33_schedule(cpu, DSPIC33_EVENT_UART, channel, 0u, delay);
}

bool dspic33_spi_receive(Dspic33* cpu, uint8_t channel, uint16_t value,
                         uint64_t delay) {
    return channel < DSPIC33_SPI_COUNT &&
           byte_queue_push(&cpu->io.spi_rx[channel], (uint8_t)value) &&
           byte_queue_push(&cpu->io.spi_rx[channel], (uint8_t)(value >> 8u)) &&
           dspic33_schedule(cpu, DSPIC33_EVENT_SPI, channel, value, delay);
}

bool dspic33_can_receive(Dspic33* cpu, uint8_t channel, const Dspic33CanFrame* frame,
                         uint64_t delay) {
    return channel < DSPIC33_CAN_COUNT &&
           can_queue_push(&cpu->io.can_rx[channel], frame) &&
           dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, 0u, delay);
}

bool dspic33_usb_receive(Dspic33* cpu, uint8_t endpoint, const uint8_t* data,
                         uint16_t size, uint64_t delay) {
    uint16_t offset = (uint16_t)endpoint * 64u;
    if (endpoint >= 16u || size > 64u || offset + size > sizeof(cpu->io.usb)) {
        return false;
    }
    memcpy(cpu->io.usb + offset, data, size);
    if (offset + size > cpu->io.usb_size) {
        cpu->io.usb_size = (uint16_t)(offset + size);
    }
    return dspic33_schedule(cpu, DSPIC33_EVENT_USB, endpoint, size, delay);
}

void dspic33_adc_input(Dspic33* cpu, uint8_t channel, uint16_t value) {
    if (channel < DSPIC33_ADC_CHANNEL_COUNT) {
        cpu->io.adc[channel] = value;
    }
}

void dspic33_gpio_input(Dspic33* cpu, uint8_t port, uint16_t value) {
    if (port < DSPIC33_GPIO_PORT_COUNT && cpu->io.gpio[port] != value) {
        cpu->io.gpio[port] = value;
        dspic33_raise_interrupt(cpu, 19u);
    }
}

void dspic33_device_reset(Dspic33* cpu) {
    size_t index;
    memset(&cpu->io, 0, sizeof(cpu->io));
    cpu->io.usb_size = sizeof(cpu->io.usb);
    for (index = 0u; index < sizeof(reset_values) / sizeof(reset_values[0]); index++) {
        raw_write_word(cpu, reset_values[index].address, reset_values[index].value);
    }
    raw_write_word(cpu, 0x0742u, 0x3020u);
    raw_write_word(cpu, 0x0758u, 0xa400u);
    raw_write_word(cpu, 0x08c2u, 0x8000u);
}
