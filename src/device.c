#include "device.h"

#include <stdlib.h>
#include <string.h>

static const uint16_t timer_registers[DSPIC33_TIMER_COUNT] = {
    0x0100u, 0x0106u, 0x010au, 0x0114u, 0x0118u, 0x0122u, 0x0126u, 0x0130u, 0x0134u};
static const uint16_t timer_periods[DSPIC33_TIMER_COUNT] = {
    0x0102u, 0x010cu, 0x010eu, 0x011au, 0x011cu, 0x0128u, 0x012au, 0x0136u, 0x0138u};
static const uint16_t timer_controls[DSPIC33_TIMER_COUNT] = {
    0x0104u, 0x0110u, 0x0112u, 0x011eu, 0x0120u, 0x012cu, 0x012eu, 0x013au, 0x013cu};
static const uint16_t timer_holding_registers[4] = {0x0108u, 0x0116u, 0x0124u, 0x0132u};
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
static const uint16_t adc_buffers[DSPIC33_ADC_COUNT] = {0x0300u, 0x0340u};
static const uint16_t adc_controls[DSPIC33_ADC_COUNT] = {0x0320u, 0x0360u};
static const uint8_t adc_irqs[DSPIC33_ADC_COUNT] = {13u, 21u};

typedef struct {
    uint16_t address;
    uint16_t value;
} Dspic33ResetValue;

typedef struct {
    uint16_t address;
    uint16_t writable;
} Dspic33RegisterMask;

enum {
    TIMER_ON = 0x8000u,
    TIMER_STOP_IDLE = 0x2000u,
    TIMER_GATE = 0x0040u,
    TIMER_PRESCALE_MASK = 0x0030u,
    TIMER_32_BIT = 0x0008u,
    TIMER_SYNC = 0x0004u,
    TIMER_EXTERNAL = 0x0002u,
    DMA_CHANNEL_BASE = 0x0b00u,
    DMA_CHANNEL_STRIDE = 0x0010u,
    DMA_CON_CHEN = 0x8000u,
    DMA_CON_SIZE_BYTE = 0x4000u,
    DMA_CON_RAM_TO_PERIPHERAL = 0x2000u,
    DMA_CON_HALF = 0x1000u,
    DMA_CON_NULL_WRITE = 0x0800u,
    DMA_CON_AMODE_MASK = 0x0030u,
    DMA_CON_AMODE_FIXED = 0x0010u,
    DMA_CON_AMODE_PERIPHERAL = 0x0020u,
    DMA_CON_MODE_MASK = 0x0003u,
    DMA_CON_MODE_ONE_SHOT = 0x0001u,
    DMA_CON_MODE_PING_PONG = 0x0002u,
    DMA_REQ_FORCE = 0x8000u,
    DMA_REQ_SOURCE_MASK = 0x00ffu,
    DMA_EVENT_FORCE = 0x00010000u,
    DMA_EVENT_GENERATION_SHIFT = 17u,
    DMA_EVENT_GENERATION_MASK = 0x7fffu,
    DMA_CHANNEL_MASK = 0x7fffu,
    DMA_PWC = 0x0bf0u,
    DMA_RQC = 0x0bf2u,
    DMA_PPS = 0x0bf4u,
    DMA_LCA = 0x0bf6u,
    DMA_SADRL = 0x0bf8u,
    DMA_SADRH = 0x0bfau,
    ADC_ON = 0x8000u,
    ADC_STOP_IDLE = 0x2000u,
    ADC_BUFFER_ORDER = 0x1000u,
    ADC_12_BIT = 0x0400u,
    ADC_FORMAT_MASK = 0x0300u,
    ADC_TRIGGER_MASK = 0x00f0u,
    ADC_SIMULTANEOUS = 0x0008u,
    ADC_AUTO_SAMPLE = 0x0004u,
    ADC_SAMPLE = 0x0002u,
    ADC_DONE = 0x0001u,
    ADC_SCAN = 0x0400u,
    ADC_CHANNELS_MASK = 0x0300u,
    ADC_SECOND_BUFFER = 0x0080u,
    ADC_BUFFER_SPLIT = 0x0002u,
    ADC_ALTERNATE = 0x0001u,
    ADC_DMA_ENABLE = 0x0100u,
    ADC_DMA_LENGTH_MASK = 0x0007u,
    ADC_EVENT_COMPLETE = 0x00000100u,
    ADC_EVENT_SOURCE_MASK = 0x000000ffu,
    ADC_EVENT_GENERATION_SHIFT = 16u
};

static const Dspic33RegisterMask register_masks[] = {
    {0x0046u, 0xcfffu}, {0x0048u, 0xfffeu}, {0x004au, 0xfffeu}, {0x004cu, 0xfffeu},
    {0x004eu, 0xfffeu}, {0x0050u, 0xffffu}, {0x0104u, 0xa076u}, {0x0110u, 0xa07au},
    {0x0112u, 0xa072u}, {0x011eu, 0xa07au}, {0x0120u, 0xa072u}, {0x012cu, 0xa07au},
    {0x012eu, 0xa072u}, {0x013au, 0xa07au}, {0x013cu, 0xa072u}, {0x0680u, 0x3f3fu},
    {0x0682u, 0x3f3fu}, {0x0684u, 0x3f3fu}, {0x0686u, 0x3f3fu}, {0x0688u, 0x3f3fu},
    {0x068au, 0x3f3fu}, {0x068cu, 0x3f3fu}, {0x068eu, 0x3f3fu}, {0x0690u, 0x3f3fu},
    {0x0692u, 0x3f3fu}, {0x0696u, 0x3f3fu}, {0x0698u, 0x3f3fu}, {0x069au, 0x3f3fu},
    {0x069cu, 0x3f3fu}, {0x069eu, 0x3f3fu}, {0x06a0u, 0x7f00u}, {0x06a2u, 0x7f7fu},
    {0x06a4u, 0x7f7fu}, {0x06a6u, 0x7f7fu}, {0x06a8u, 0x7f7fu}, {0x06aau, 0x7f7fu},
    {0x06acu, 0x7f7fu}, {0x06aeu, 0x7f7fu}, {0x06b0u, 0x7f7fu}, {0x06b2u, 0x7f7fu},
    {0x06b4u, 0x7f7fu}, {0x06b6u, 0x7f7fu}, {0x06b8u, 0x7f7fu}, {0x06bau, 0x7f7fu},
    {0x06bcu, 0x7f7fu}, {0x06beu, 0x7f7fu}, {0x06c0u, 0x7f7fu}, {0x06c2u, 0x7f7fu},
    {0x06c4u, 0x7f7fu}, {0x06c6u, 0x7f7fu}, {0x06c8u, 0x7f7fu}, {0x06cau, 0x007fu},
    {0x06ceu, 0x007fu}, {0x06d0u, 0x7f7fu}, {0x06d2u, 0x007fu}, {0x06d4u, 0x7f7fu},
    {0x06d6u, 0x7f7fu}, {0x06d8u, 0x7f7fu}, {0x06dau, 0x7f7fu}, {0x06dcu, 0x007fu},
    {0x06deu, 0x7f7fu}, {0x06e0u, 0x007fu}, {0x06e2u, 0x7f7fu}, {0x06e4u, 0x7f7fu},
    {0x06e6u, 0x7f7fu}, {0x06e8u, 0x7f7fu}, {0x06eau, 0x7f7fu}, {0x06ecu, 0x7f7fu},
    {0x06eeu, 0x7f7fu}, {0x06f0u, 0x7f7fu}, {0x06f2u, 0x007fu}, {0x06f4u, 0x7f7fu},
    {0x06f6u, 0x007fu}, {0x0744u, 0xffdfu}, {0x0746u, 0x01ffu}, {0x0748u, 0x003fu},
    {0x074eu, 0xbf00u}, {0x075au, 0x183fu}, {0x0760u, 0xffffu}, {0x0762u, 0xffffu},
    {0x0764u, 0xf7abu}, {0x0766u, 0x0021u}, {0x0768u, 0xffffu}, {0x076au, 0x3f03u},
    {0x076cu, 0x00f0u}, {0x0800u, 0xffffu}, {0x0802u, 0xffffu}, {0x0804u, 0xffffu},
    {0x0806u, 0x7fffu}, {0x0808u, 0x0afeu}, {0x080au, 0xdfeeu}, {0x080cu, 0xc3efu},
    {0x080eu, 0xffc0u}, {0x0810u, 0x7fdfu}, {0x0820u, 0xffffu}, {0x0822u, 0xffffu},
    {0x0824u, 0xffffu}, {0x0826u, 0x7fffu}, {0x0828u, 0x0afeu}, {0x082au, 0xdfeeu},
    {0x082cu, 0xffefu}, {0x082eu, 0xffc0u}, {0x0830u, 0x001fu}, {0x0840u, 0x7777u},
    {0x0842u, 0x7777u}, {0x0844u, 0x7777u}, {0x0846u, 0x7777u}, {0x0848u, 0x7777u},
    {0x084au, 0x7777u}, {0x084cu, 0x7777u}, {0x084eu, 0x7777u}, {0x0850u, 0x7777u},
    {0x0852u, 0x7777u}, {0x0854u, 0x7777u}, {0x0856u, 0x7777u}, {0x0858u, 0x7777u},
    {0x085au, 0x7777u}, {0x085cu, 0x7777u}, {0x085eu, 0x0777u}, {0x0860u, 0x7770u},
    {0x0862u, 0x7777u}, {0x0864u, 0x7070u}, {0x0868u, 0x7770u}, {0x086au, 0x7700u},
    {0x086cu, 0x7777u}, {0x086eu, 0x7777u}, {0x0870u, 0x7777u}, {0x087au, 0x7700u},
    {0x087cu, 0x7777u}, {0x087eu, 0x7777u}, {0x0880u, 0x7777u}, {0x0882u, 0x7707u},
    {0x0884u, 0x7777u}, {0x0886u, 0x0777u}, {0x0e00u, 0xc6ffu}, {0x0e04u, 0xc6ffu},
    {0x0e06u, 0xc03fu}, {0x0e08u, 0xc6ffu}, {0x0e0au, 0xc6ffu}, {0x0e0cu, 0xc6ffu},
    {0x0e0eu, 0x06c0u}, {0x0e10u, 0xffffu}, {0x0e14u, 0xffffu}, {0x0e18u, 0xffffu},
    {0x0e1au, 0xffffu}, {0x0e1cu, 0xffffu}, {0x0e1eu, 0xffffu}, {0x0e20u, 0xf01eu},
    {0x0e24u, 0xf01eu}, {0x0e28u, 0xf01eu}, {0x0e2au, 0xf01eu}, {0x0e2cu, 0xf01eu},
    {0x0e2eu, 0x601eu}, {0x0e30u, 0xffffu}, {0x0e34u, 0xffffu}, {0x0e36u, 0xff3fu},
    {0x0e38u, 0xffffu}, {0x0e3au, 0xffffu}, {0x0e3cu, 0xffffu}, {0x0e3eu, 0x00c0u},
    {0x0e40u, 0x03ffu}, {0x0e44u, 0x03ffu}, {0x0e48u, 0x03ffu}, {0x0e4au, 0x03ffu},
    {0x0e4cu, 0x03ffu}, {0x0e4eu, 0x03ffu}, {0x0e50u, 0x313fu}, {0x0e54u, 0x313fu},
    {0x0e56u, 0x313fu}, {0x0e58u, 0x313fu}, {0x0e5au, 0x313fu}, {0x0e5cu, 0x313fu},
    {0x0e60u, 0xf3c3u}, {0x0e64u, 0xf3c3u}, {0x0e66u, 0xf003u}, {0x0e68u, 0xf3cfu},
    {0x0e6au, 0xf3c3u}, {0x0e6cu, 0xf3c3u}, {0x0e6eu, 0x03c0u}};

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

static bool register_write_mask(uint16_t address, uint16_t* writable) {
    size_t first = 0u;
    size_t count = sizeof(register_masks) / sizeof(register_masks[0]);

    while (count != 0u) {
        size_t step = count / 2u;
        size_t index = first + step;

        if (register_masks[index].address < address) {
            first = index + 1u;
            count -= step + 1u;
        } else {
            count = step;
        }
    }
    if (first == sizeof(register_masks) / sizeof(register_masks[0]) ||
        register_masks[first].address != address) {
        return false;
    }
    *writable = register_masks[first].writable;
    return true;
}

static bool adc_register_write_mask(uint16_t address, uint16_t* writable) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        uint16_t control = adc_controls[module];
        uint16_t buffer = adc_buffers[module];
        if (address >= buffer && address < buffer + 0x20u) {
            *writable = 0u;
            return true;
        }
        if (address == control) {
            *writable = module == 0u ? 0xb7feu : 0xb3feu;
            return true;
        }
        if (address == control + 2u) {
            *writable = module == 0u ? 0xe77fu : 0xe73fu;
            return true;
        }
        if (address == control + 4u) {
            *writable = 0x9fffu;
            return true;
        }
        if (address == control + 6u) {
            *writable = 0x0707u;
            return true;
        }
        if (address == control + 8u) {
            *writable = 0x9f9fu;
            return true;
        }
        if ((module == 0u && (address == 0x032eu || address == 0x0330u)) ||
            (module == 1u && address == 0x0370u)) {
            *writable = 0xffffu;
            return true;
        }
        if ((module == 0u && address == 0x0332u) ||
            (module == 1u && address == 0x0372u)) {
            *writable = 0x0107u;
            return true;
        }
    }
    return false;
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
    if (left->cycle != right->cycle) {
        return left->cycle < right->cycle;
    }
    if (left->type == DSPIC33_EVENT_DMA && right->type == DSPIC33_EVENT_DMA &&
        left->source != right->source) {
        return left->source < right->source;
    }
    return left->sequence < right->sequence;
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
    if (delay > UINT64_MAX - cpu->cycles) {
        return false;
    }
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

static bool interrupt_deferred(const Dspic33* cpu, uint16_t irq) {
    return (cpu->interrupt_deferred[irq / 16u] & (uint16_t)(1u << (irq % 16u))) != 0u;
}

void dspic33_device_latch_interrupt(Dspic33* cpu, uint8_t vector, uint8_t priority) {
    raw_write_word(cpu, 0x08c8u, (uint16_t)(((uint16_t)priority << 8u) | vector));
}

static bool service_interrupt(Dspic33* cpu, bool waking) {
    uint8_t current = (uint8_t)((cpu->sr >> 5u) & 0x07u);
    uint8_t best_priority = current;
    uint16_t next_priority;
    uint16_t best_irq = DSPIC33_IRQ_COUNT;
    size_t log_index;
    uint16_t irq;
    uint16_t stacked_high;
    uint16_t group;
    if (((raw_word(cpu, 0x08c2u) & 0x8000u) == 0u && cpu->gie_disable_deferred == 0u) ||
        (cpu->corcon & 0x0008u) != 0u) {
        return false;
    }
    for (group = 0u; group < (DSPIC33_IRQ_COUNT + 15u) / 16u; group++) {
        uint16_t offset = (uint16_t)(group * 2u);
        if ((raw_word(cpu, (uint16_t)(0x0800u + offset)) &
             raw_word(cpu, (uint16_t)(0x0820u + offset))) != 0u) {
            break;
        }
    }
    if (group == (DSPIC33_IRQ_COUNT + 15u) / 16u) {
        return false;
    }
    for (irq = 0u; irq < DSPIC33_IRQ_COUNT; irq++) {
        uint8_t priority;
        if (!interrupt_enabled(cpu, irq) || interrupt_deferred(cpu, irq)) {
            continue;
        }
        priority = interrupt_priority(cpu, irq);
        if (!waking && cpu->disicnt != 0u && priority < 7u) {
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
    dspic33_device_latch_interrupt(cpu, (uint8_t)(best_irq + 8u), best_priority);
    return true;
}

bool dspic33_device_service_interrupt(Dspic33* cpu) {
    return service_interrupt(cpu, false);
}

bool dspic33_device_wake(Dspic33* cpu) {
    uint16_t irq;
    for (irq = 0u; irq < DSPIC33_IRQ_COUNT; irq++) {
        if (interrupt_enabled(cpu, irq) && !interrupt_deferred(cpu, irq) &&
            interrupt_priority(cpu, irq) != 0u) {
            service_interrupt(cpu, true);
            return true;
        }
    }
    return false;
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

static uint16_t dma_channel_base(uint8_t channel) {
    return (uint16_t)(DMA_CHANNEL_BASE + channel * DMA_CHANNEL_STRIDE);
}

static uint16_t dma_channel_bit(uint8_t channel) { return (uint16_t)(1u << channel); }

static uint32_t dma_start_address(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.dma_bank & dma_channel_bit(channel)) != 0u
               ? cpu->io.dma_start_b[channel]
               : cpu->io.dma_start_a[channel];
}

static uint32_t dma_transfer_address(const Dspic33* cpu, uint8_t channel,
                                     uint16_t control, uint16_t indirect_address) {
    uint32_t start = dma_start_address(cpu, channel);
    uint16_t mode = control & DMA_CON_AMODE_MASK;
    if (mode == DMA_CON_AMODE_PERIPHERAL) {
        return start | indirect_address;
    }
    return cpu->io.dma_address[channel];
}

static bool dma_memory_address_valid(uint32_t address, uint8_t width) {
    return address < DSPIC33_DATA_SIZE &&
           (width == 1u || address + 1u < DSPIC33_DATA_SIZE);
}

static bool dma_write_cycle_matches(const Dspic33* cpu) {
    return cpu->io.cpu_write_valid && (cpu->io.cpu_write_cycle == cpu->cycles ||
                                       (cpu->io.cpu_write_cycle != UINT64_MAX &&
                                        cpu->io.cpu_write_cycle + 1u == cpu->cycles));
}

static bool dma_cpu_wrote_byte(const Dspic33* cpu, uint32_t address) {
    return dma_write_cycle_matches(cpu) && address >= cpu->io.cpu_write_address &&
           address < cpu->io.cpu_write_address + cpu->io.cpu_write_width;
}

static uint8_t dma_read_memory_byte(const Dspic33* cpu, uint32_t address) {
    if (dma_cpu_wrote_byte(cpu, address)) {
        uint8_t shift = (uint8_t)((address - cpu->io.cpu_write_address) * 8u);
        return (uint8_t)(cpu->io.cpu_write_previous >> shift);
    }
    return cpu->data[address];
}

static uint16_t dma_read_memory(const Dspic33* cpu, uint32_t address, uint8_t width) {
    if (!dma_memory_address_valid(address, width)) {
        return 0u;
    }
    if (width == 1u) {
        return dma_read_memory_byte(cpu, address);
    }
    return (uint16_t)(dma_read_memory_byte(cpu, address) |
                      ((uint16_t)dma_read_memory_byte(cpu, address + 1u) << 8u));
}

static void dma_write_memory(Dspic33* cpu, uint32_t address, uint8_t width,
                             uint16_t value) {
    if (!dma_memory_address_valid(address, width)) {
        return;
    }
    if (!dma_cpu_wrote_byte(cpu, address)) {
        cpu->data[address] = (uint8_t)value;
    }
    if (width == 2u && !dma_cpu_wrote_byte(cpu, address + 1u)) {
        cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    }
}

static void dma_record_transfer(Dspic33* cpu, uint8_t channel, uint16_t control,
                                uint32_t address) {
    uint16_t bit = dma_channel_bit(channel);
    uint16_t base = dma_channel_base(channel);
    uint16_t start_offset = (cpu->io.dma_bank & bit) != 0u ? 8u : 4u;
    uint16_t ping_pong = raw_word(cpu, DMA_PPS);
    if ((cpu->io.dma_bank & bit) != 0u) {
        ping_pong |= bit;
    } else {
        ping_pong &= (uint16_t)~bit;
    }
    raw_write_word(cpu, DMA_PPS, (uint16_t)(ping_pong & DMA_CHANNEL_MASK));
    raw_write_word(cpu, DMA_LCA, channel);
    raw_write_word(cpu, DMA_SADRL, (uint16_t)address);
    raw_write_word(cpu, DMA_SADRH, (uint16_t)((address >> 16u) & 0x00ffu));
    if ((control & DMA_CON_AMODE_MASK) == DMA_CON_AMODE_PERIPHERAL) {
        raw_write_word(cpu, (uint16_t)(base + start_offset), (uint16_t)address);
        raw_write_word(cpu, (uint16_t)(base + start_offset + 2u),
                       (uint16_t)((address >> 16u) & 0x00ffu));
    }
}

static bool dma_write_collision(const Dspic33* cpu, uint16_t pad, uint8_t width) {
    uint32_t previous_end;
    uint32_t dma_end;
    if (!dma_write_cycle_matches(cpu)) {
        return false;
    }
    previous_end = cpu->io.cpu_write_address + cpu->io.cpu_write_width;
    dma_end = (uint32_t)pad + width;
    return cpu->io.cpu_write_address < dma_end && pad < previous_end;
}

static void dma_peripheral_write_collision(Dspic33* cpu, uint8_t channel) {
    uint16_t status = raw_word(cpu, DMA_PWC);
    uint16_t bit = dma_channel_bit(channel);
    if ((status & bit) == 0u) {
        raw_write_word(cpu, DMA_PWC, (uint16_t)(status | bit));
        dspic33_raise_dma_collision_trap(cpu);
    }
}

static void dma_disable_channel(Dspic33* cpu, uint8_t channel, uint16_t control) {
    uint16_t base = dma_channel_base(channel);
    uint16_t bit = dma_channel_bit(channel);
    raw_write_word(cpu, base, (uint16_t)(control & ~DMA_CON_CHEN));
    raw_write_word(cpu, (uint16_t)(base + 2u),
                   (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
    cpu->io.dma_enabled &= (uint16_t)~bit;
    cpu->io.dma_forced_pending &= (uint16_t)~bit;
    cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
    cpu->io.dma_generation[channel]++;
}

static void dma_complete_block(Dspic33* cpu, uint8_t channel, uint16_t control) {
    uint16_t bit = dma_channel_bit(channel);
    uint16_t mode = control & DMA_CON_MODE_MASK;
    bool secondary = (cpu->io.dma_bank & bit) != 0u;
    if ((control & DMA_CON_HALF) == 0u) {
        dspic33_raise_interrupt(cpu, dma_irqs[channel]);
    }
    cpu->io.dma_index[channel] = 0u;
    cpu->io.dma_half_raised &= (uint16_t)~bit;
    if ((mode & DMA_CON_MODE_ONE_SHOT) != 0u &&
        ((mode & DMA_CON_MODE_PING_PONG) == 0u || secondary)) {
        dma_disable_channel(cpu, channel, control);
        return;
    }
    if ((mode & DMA_CON_MODE_PING_PONG) != 0u) {
        cpu->io.dma_bank ^= bit;
    }
    cpu->io.dma_address[channel] = dma_start_address(cpu, channel);
}

static void run_dma(Dspic33* cpu, uint8_t channel, uint32_t event_value) {
    uint16_t base;
    uint16_t bit;
    uint16_t control;
    uint16_t count;
    uint16_t pad;
    uint16_t index;
    uint16_t transferred;
    uint16_t half;
    uint16_t value;
    uint16_t generation;
    uint32_t address;
    uint8_t width;
    bool forced;
    if (channel >= DSPIC33_DMA_COUNT) {
        return;
    }
    bit = dma_channel_bit(channel);
    forced = (event_value & DMA_EVENT_FORCE) != 0u;
    generation = (uint16_t)((event_value >> DMA_EVENT_GENERATION_SHIFT) &
                            DMA_EVENT_GENERATION_MASK);
    if (generation != (cpu->io.dma_generation[channel] & DMA_EVENT_GENERATION_MASK)) {
        return;
    }
    if (forced) {
        cpu->io.dma_forced_pending &= (uint16_t)~bit;
    } else {
        cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
    }
    base = dma_channel_base(channel);
    control = raw_word(cpu, base);
    if ((control & DMA_CON_CHEN) == 0u || (raw_word(cpu, DMA_PWC) & bit) != 0u) {
        if (forced) {
            raw_write_word(
                cpu, (uint16_t)(base + 2u),
                (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
        }
        return;
    }
    count = raw_word(cpu, (uint16_t)(base + 0x0eu));
    pad = raw_word(cpu, (uint16_t)(base + 0x0cu));
    index = cpu->io.dma_index[channel];
    width = (control & DMA_CON_SIZE_BYTE) != 0u ? 1u : 2u;
    address = dma_transfer_address(cpu, channel, control, (uint16_t)event_value);
    if (!dma_memory_address_valid(address, width)) {
        dspic33_raise_dma_address_trap(cpu);
        if (forced) {
            raw_write_word(
                cpu, (uint16_t)(base + 2u),
                (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
        }
        return;
    }
    dma_record_transfer(cpu, channel, control, address);
    cpu->io.dma_transfer_active = true;
    if ((control & DMA_CON_RAM_TO_PERIPHERAL) != 0u) {
        value = dma_read_memory(cpu, address, width);
        if (dma_write_collision(cpu, pad, width)) {
            dma_peripheral_write_collision(cpu, channel);
        } else if (width == 1u) {
            dspic33_write_byte(cpu, pad, (uint8_t)value);
        } else {
            dspic33_write_word(cpu, pad, value);
        }
    } else {
        value = width == 1u ? dspic33_read_byte(cpu, pad) : dspic33_read_word(cpu, pad);
        dma_write_memory(cpu, address, width, value);
        if ((control & DMA_CON_NULL_WRITE) != 0u) {
            if (dma_write_collision(cpu, pad, width)) {
                dma_peripheral_write_collision(cpu, channel);
            } else if (width == 1u) {
                dspic33_write_byte(cpu, pad, 0u);
            } else {
                dspic33_write_word(cpu, pad, 0u);
            }
        }
    }
    cpu->io.dma_transfer_active = false;
    if ((control & DMA_CON_AMODE_MASK) == 0u) {
        cpu->io.dma_address[channel] += width;
    }
    if (forced) {
        raw_write_word(
            cpu, (uint16_t)(base + 2u),
            (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
    }
    transferred = (uint16_t)(index + 1u);
    half = (uint16_t)(((uint32_t)count + 2u) / 2u);
    if ((control & DMA_CON_HALF) != 0u && transferred == half &&
        (cpu->io.dma_half_raised & bit) == 0u) {
        cpu->io.dma_half_raised |= bit;
        dspic33_raise_interrupt(cpu, dma_irqs[channel]);
    }
    if (index >= count) {
        dma_complete_block(cpu, channel, control);
    } else {
        cpu->io.dma_index[channel]++;
    }
}

static uint32_t dma_event_value(const Dspic33* cpu, uint8_t channel,
                                uint16_t indirect_address, bool forced) {
    uint32_t value = indirect_address;
    value |= ((uint32_t)cpu->io.dma_generation[channel] & DMA_EVENT_GENERATION_MASK)
             << DMA_EVENT_GENERATION_SHIFT;
    if (forced) {
        value |= DMA_EVENT_FORCE;
    }
    return value;
}

static bool schedule_dma_channel(Dspic33* cpu, uint8_t channel,
                                 uint16_t indirect_address, bool forced,
                                 uint64_t delay) {
    uint16_t bit = dma_channel_bit(channel);
    uint16_t* pending =
        forced ? &cpu->io.dma_forced_pending : &cpu->io.dma_peripheral_pending;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_DMA, channel,
                          dma_event_value(cpu, channel, indirect_address, forced),
                          delay)) {
        return false;
    }
    *pending |= bit;
    return true;
}

static void dma_request_collision(Dspic33* cpu, uint8_t channel) {
    uint16_t status = raw_word(cpu, DMA_RQC);
    uint16_t bit = dma_channel_bit(channel);
    if ((status & bit) == 0u) {
        raw_write_word(cpu, DMA_RQC, (uint16_t)(status | bit));
        dspic33_raise_dma_collision_trap(cpu);
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

static uint16_t adc_register(const Dspic33* cpu, uint8_t module, uint16_t offset) {
    return raw_word(cpu, (uint16_t)(adc_controls[module] + offset));
}

static bool adc_12_bit(const Dspic33* cpu, uint8_t module) {
    return module == 0u && (adc_register(cpu, module, 0u) & ADC_12_BIT) != 0u;
}

static bool adc_power_enabled(const Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_register(cpu, module, 0u);
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    if (cpu->power_state == DSPIC33_POWER_IDLE) {
        return (control & ADC_STOP_IDLE) == 0u;
    }
    return (adc_register(cpu, module, 4u) & 0x8000u) != 0u;
}

static uint8_t adc_channel_count(const Dspic33* cpu, uint8_t module) {
    uint16_t selection;
    if (adc_12_bit(cpu, module)) {
        return 1u;
    }
    selection = adc_register(cpu, module, 2u) & ADC_CHANNELS_MASK;
    if (selection == 0u) {
        return 1u;
    }
    return selection == 0x0100u ? 2u : 4u;
}

static uint8_t adc_next_scan_channel(Dspic33* cpu, uint8_t module) {
    uint8_t limit = module == 0u ? 32u : 16u;
    uint8_t offset;
    uint32_t selected = module == 0u ? ((uint32_t)raw_word(cpu, 0x032eu) << 16u) |
                                           raw_word(cpu, 0x0330u)
                                     : raw_word(cpu, 0x0370u);
    if (selected == 0u) {
        return (uint8_t)(adc_register(cpu, module, 8u) & 0x001fu);
    }
    for (offset = 0u; offset < limit; offset++) {
        uint8_t channel = (uint8_t)((cpu->io.adc_scan_index[module] + offset) % limit);
        if ((selected & ((uint32_t)1u << channel)) != 0u) {
            cpu->io.adc_scan_index[module] = (uint8_t)((channel + 1u) % limit);
            return channel;
        }
    }
    return 0u;
}

static uint8_t adc_positive_channel(Dspic33* cpu, uint8_t module, uint8_t lane,
                                    bool mux_b) {
    uint16_t channels;
    if (lane == 0u) {
        if (!mux_b && (adc_register(cpu, module, 2u) & ADC_SCAN) != 0u) {
            return adc_next_scan_channel(cpu, module);
        }
        channels = adc_register(cpu, module, 8u);
        return (uint8_t)((channels >> (mux_b ? 8u : 0u)) & 0x001fu);
    }
    channels = adc_register(cpu, module, 6u);
    return (uint8_t)(lane - 1u +
                     (((channels >> (mux_b ? 8u : 0u)) & 1u) != 0u ? 3u : 0u));
}

static uint8_t adc_negative_channel(const Dspic33* cpu, uint8_t module, uint8_t lane,
                                    bool mux_b) {
    uint16_t channels;
    uint16_t negative;
    if (lane == 0u) {
        channels = adc_register(cpu, module, 8u);
        return (channels & (mux_b ? 0x8000u : 0x0080u)) != 0u ? 1u : UINT8_MAX;
    }
    channels = adc_register(cpu, module, 6u);
    negative = (channels >> (mux_b ? 9u : 1u)) & 3u;
    if (negative < 2u) {
        return UINT8_MAX;
    }
    return (uint8_t)((negative == 2u ? 6u : 9u) + lane - 1u);
}

static uint16_t adc_input_code(const Dspic33* cpu, uint8_t module, uint8_t positive,
                               uint8_t negative) {
    uint16_t high = positive < DSPIC33_ADC_CHANNEL_COUNT ? cpu->io.adc[positive] : 0u;
    uint16_t low = negative < DSPIC33_ADC_CHANNEL_COUNT ? cpu->io.adc[negative] : 0u;
    uint16_t difference = high > low ? (uint16_t)(high - low) : 0u;
    return adc_12_bit(cpu, module) ? (uint16_t)(difference & 0x0fffu)
                                   : (uint16_t)((difference >> 2u) & 0x03ffu);
}

static uint16_t adc_format_code(const Dspic33* cpu, uint8_t module, uint16_t code) {
    uint8_t bits = adc_12_bit(cpu, module) ? 12u : 10u;
    uint8_t shift = (uint8_t)(16u - bits);
    uint16_t sign = (uint16_t)(1u << (bits - 1u));
    uint16_t mask = (uint16_t)((1u << bits) - 1u);
    uint16_t format = adc_register(cpu, module, 0u) & ADC_FORMAT_MASK;
    if (format == 0u) {
        return code;
    }
    if (format == 0x0200u) {
        return (uint16_t)(code << shift);
    }
    code ^= sign;
    if ((code & sign) != 0u) {
        code |= (uint16_t)~mask;
    }
    return format == 0x0100u ? code : (uint16_t)(code << shift);
}

static uint64_t adc_clock_cycles(const Dspic33* cpu, uint8_t module) {
    uint16_t timing = adc_register(cpu, module, 4u);
    return (timing & 0x8000u) != 0u ? 1u : (uint64_t)(timing & 0x00ffu) + 1u;
}

static uint64_t adc_sample_cycles(const Dspic33* cpu, uint8_t module) {
    uint64_t cycles = ((adc_register(cpu, module, 4u) >> 8u) & 0x001fu) *
                      adc_clock_cycles(cpu, module);
    return cycles == 0u ? 1u : cycles;
}

static uint64_t adc_conversion_cycles(const Dspic33* cpu, uint8_t module) {
    return (adc_12_bit(cpu, module) ? 14u : 12u) * adc_clock_cycles(cpu, module);
}

static uint32_t adc_event_value(const Dspic33* cpu, uint8_t module, uint8_t source,
                                bool complete) {
    uint32_t value = source;
    value |= (uint32_t)cpu->io.adc_generation[module] << ADC_EVENT_GENERATION_SHIFT;
    if (complete) {
        value |= ADC_EVENT_COMPLETE;
    }
    return value;
}

static uint8_t adc_increment_threshold(const Dspic33* cpu, uint8_t module) {
    uint8_t value = (uint8_t)((adc_register(cpu, module, 2u) >> 2u) & 0x001fu);
    uint16_t dma = module == 0u ? raw_word(cpu, 0x0332u) : raw_word(cpu, 0x0372u);
    if (module != 0u || (dma & ADC_DMA_ENABLE) == 0u) {
        value &= 0x0fu;
    }
    return (uint8_t)(value + 1u);
}

static uint16_t adc_dma_address(Dspic33* cpu, uint8_t module, uint8_t channel) {
    uint16_t control = adc_register(cpu, module, 0u);
    uint16_t dma = module == 0u ? raw_word(cpu, 0x0332u) : raw_word(cpu, 0x0372u);
    uint8_t length = (uint8_t)(dma & ADC_DMA_LENGTH_MASK);
    uint8_t slot = cpu->io.adc_dma_sample[module][channel];
    uint16_t address;
    if ((control & ADC_BUFFER_ORDER) != 0u) {
        return (uint16_t)(cpu->io.adc_buffer_index[module] * 2u);
    }
    address = (uint16_t)(channel * ((uint16_t)2u << length) + slot * 2u);
    cpu->io.adc_dma_sample[module][channel] =
        (uint8_t)((slot + 1u) & ((1u << length) - 1u));
    return address;
}

static void adc_increment_boundary(Dspic33* cpu, uint8_t module) {
    uint16_t control2 = adc_register(cpu, module, 2u);
    cpu->io.adc_sample_count[module] = 0u;
    cpu->io.adc_scan_index[module] = 0u;
    if ((control2 & ADC_BUFFER_SPLIT) != 0u) {
        control2 ^= ADC_SECOND_BUFFER;
        cpu->io.adc_buffer_index[module] =
            (control2 & ADC_SECOND_BUFFER) != 0u ? 8u : 0u;
        raw_write_word(cpu, (uint16_t)(adc_controls[module] + 2u), control2);
    } else {
        cpu->io.adc_buffer_index[module] = 0u;
    }
    dspic33_raise_interrupt(cpu, adc_irqs[module]);
}

static void adc_store_result(Dspic33* cpu, uint8_t module, uint8_t channel,
                             uint16_t result) {
    uint16_t dma = module == 0u ? raw_word(cpu, 0x0332u) : raw_word(cpu, 0x0372u);
    uint16_t indirect = 0u;
    if ((dma & ADC_DMA_ENABLE) != 0u) {
        indirect = adc_dma_address(cpu, module, channel);
        raw_write_word(cpu, adc_buffers[module], result);
        dspic33_dma_request(cpu, adc_irqs[module], indirect, 0u);
    } else {
        raw_write_word(cpu,
                       (uint16_t)(adc_buffers[module] +
                                  (cpu->io.adc_buffer_index[module] & 0x0fu) * 2u),
                       result);
    }
    cpu->io.adc_buffer_index[module] =
        (uint8_t)((cpu->io.adc_buffer_index[module] + 1u) & 0x0fu);
    cpu->io.adc_sample_count[module]++;
    if (cpu->io.adc_sample_count[module] >= adc_increment_threshold(cpu, module)) {
        adc_increment_boundary(cpu, module);
    }
}

static void adc_begin_sampling(Dspic33* cpu, uint8_t module);

static void adc_complete_conversion(Dspic33* cpu, uint8_t module) {
    uint16_t control;
    uint8_t index;
    if (!adc_power_enabled(cpu, module)) {
        return;
    }
    control = adc_register(cpu, module, 0u);
    if ((control & ADC_ON) == 0u) {
        return;
    }
    for (index = 0u; index < cpu->io.adc_latched_count[module]; index++) {
        adc_store_result(cpu, module, cpu->io.adc_latched_channel[module][index],
                         cpu->io.adc_latched[module][index]);
    }
    raw_write_word(cpu, adc_controls[module], (uint16_t)(control | ADC_DONE));
    if ((control & ADC_AUTO_SAMPLE) != 0u) {
        adc_begin_sampling(cpu, module);
    }
}

static void adc_start_conversion(Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_register(cpu, module, 0u);
    uint16_t control2 = adc_register(cpu, module, 2u);
    uint8_t count;
    uint8_t index;
    bool mux_b;
    if ((control & (ADC_ON | ADC_SAMPLE)) != (ADC_ON | ADC_SAMPLE) ||
        !adc_power_enabled(cpu, module)) {
        return;
    }
    count = adc_channel_count(cpu, module);
    mux_b = (cpu->io.adc_mux_b & (uint8_t)(1u << module)) != 0u;
    for (index = 0u; index < count; index++) {
        uint8_t positive = adc_positive_channel(cpu, module, index, mux_b);
        uint8_t negative = adc_negative_channel(cpu, module, index, mux_b);
        cpu->io.adc_latched_channel[module][index] = positive;
        cpu->io.adc_latched[module][index] = adc_format_code(
            cpu, module, adc_input_code(cpu, module, positive, negative));
    }
    cpu->io.adc_latched_count[module] = count;
    if ((control2 & ADC_ALTERNATE) != 0u) {
        cpu->io.adc_mux_b ^= (uint8_t)(1u << module);
    } else {
        cpu->io.adc_mux_b &= (uint8_t)~(1u << module);
    }
    raw_write_word(cpu, adc_controls[module],
                   (uint16_t)(control & ~(ADC_SAMPLE | ADC_DONE)));
    dspic33_schedule(cpu, DSPIC33_EVENT_ADC, module,
                     adc_event_value(cpu, module,
                                     (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u),
                                     true),
                     adc_conversion_cycles(cpu, module));
}

static void adc_begin_sampling(Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_register(cpu, module, 0u);
    uint8_t source;
    if ((control & ADC_ON) == 0u || !adc_power_enabled(cpu, module)) {
        return;
    }
    cpu->io.adc_generation[module]++;
    control |= ADC_SAMPLE;
    raw_write_word(cpu, adc_controls[module], control);
    source = (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u);
    if (source == 7u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_ADC, module,
                         adc_event_value(cpu, module, source, false),
                         adc_sample_cycles(cpu, module));
    }
}

static void run_adc(Dspic33* cpu, uint8_t module, uint32_t event_value) {
    uint16_t generation;
    uint8_t source;
    if (module >= DSPIC33_ADC_COUNT) {
        return;
    }
    generation = (uint16_t)(event_value >> ADC_EVENT_GENERATION_SHIFT);
    source = (uint8_t)(event_value & ADC_EVENT_SOURCE_MASK);
    if ((event_value & ADC_EVENT_COMPLETE) != 0u) {
        if (generation == cpu->io.adc_generation[module]) {
            adc_complete_conversion(cpu, module);
        }
        return;
    }
    if (generation != UINT16_MAX && generation != cpu->io.adc_generation[module]) {
        return;
    }
    if (((adc_register(cpu, module, 0u) & ADC_TRIGGER_MASK) >> 4u) == source) {
        adc_start_conversion(cpu, module);
    }
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

static bool timer_is_type_b(uint8_t timer) { return timer >= 1u && (timer & 1u) != 0u; }

static bool timer_is_type_c(uint8_t timer) { return timer >= 2u && (timer & 1u) == 0u; }

static bool timer_pair_enabled(const Dspic33* cpu, uint8_t timer) {
    return timer_is_type_b(timer) &&
           (raw_word(cpu, timer_controls[timer]) & TIMER_32_BIT) != 0u;
}

static bool timer_is_paired_high(const Dspic33* cpu, uint8_t timer) {
    return timer_is_type_c(timer) && timer_pair_enabled(cpu, (uint8_t)(timer - 1u));
}

static uint32_t timer_prescale(uint16_t control) {
    switch (control & TIMER_PRESCALE_MASK) {
    case 0x0010u:
        return 8u;
    case 0x0020u:
        return 64u;
    case 0x0030u:
        return 256u;
    default:
        return 1u;
    }
}

static bool timer_power_enabled(const Dspic33* cpu, uint8_t timer, bool external) {
    uint16_t control = raw_word(cpu, timer_controls[timer]);
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    if (cpu->power_state == DSPIC33_POWER_IDLE && (control & TIMER_STOP_IDLE) == 0u &&
        (!timer_pair_enabled(cpu, timer) ||
         (raw_word(cpu, timer_controls[timer + 1u]) & TIMER_STOP_IDLE) == 0u)) {
        return true;
    }
    return external && timer == 0u && (control & TIMER_SYNC) == 0u;
}

typedef struct {
    uint64_t value;
    uint64_t matches;
} TimerAdvance;

static TimerAdvance advance_counter(uint64_t current, uint64_t period, uint64_t maximum,
                                    uint64_t ticks) {
    TimerAdvance result = {current, 0u};
    uint64_t cycle = period + 1u;
    uint64_t first_match;
    uint64_t first_reset;
    uint64_t remaining;
    if (ticks == 0u) {
        return result;
    }
    if (current == period) {
        first_match = cycle;
        first_reset = 1u;
    } else if (current < period) {
        first_match = period - current;
        first_reset = first_match + 1u;
    } else {
        first_match = maximum - current + 1u + period;
        first_reset = first_match + 1u;
    }
    if (ticks >= first_match) {
        result.matches = 1u + (ticks - first_match) / cycle;
    }
    if (ticks < first_reset) {
        result.value = (current + ticks) & maximum;
        return result;
    }
    remaining = ticks - first_reset;
    result.value = remaining % cycle;
    return result;
}

static void signal_timer_period(Dspic33* cpu, uint8_t timer, uint64_t matches,
                                bool gated) {
    if (matches != 0u) {
        dspic33_dma_request(cpu, timer_irqs[timer], 0u, 0u);
        if (!gated) {
            cpu->io.timer_interrupt_pending |= (uint16_t)(1u << timer);
        }
    }
}

static void advance_timer_ticks(Dspic33* cpu, uint8_t timer, uint64_t ticks) {
    uint16_t control = raw_word(cpu, timer_controls[timer]);
    bool paired = timer_pair_enabled(cpu, timer);
    bool gated = (control & TIMER_GATE) != 0u && (control & TIMER_EXTERNAL) == 0u;
    if (ticks == 0u) {
        return;
    }
    if (paired) {
        uint64_t current =
            raw_word(cpu, timer_registers[timer]) |
            ((uint64_t)raw_word(cpu, timer_registers[timer + 1u]) << 16u);
        uint64_t period = raw_word(cpu, timer_periods[timer]) |
                          ((uint64_t)raw_word(cpu, timer_periods[timer + 1u]) << 16u);
        TimerAdvance result = advance_counter(current, period, UINT32_MAX, ticks);
        raw_write_word(cpu, timer_registers[timer], (uint16_t)result.value);
        raw_write_word(cpu, timer_registers[timer + 1u],
                       (uint16_t)(result.value >> 16u));
        if (period != 0u) {
            signal_timer_period(cpu, (uint8_t)(timer + 1u), result.matches, gated);
        }
    } else {
        uint16_t period = raw_word(cpu, timer_periods[timer]);
        TimerAdvance result = advance_counter(raw_word(cpu, timer_registers[timer]),
                                              period, UINT16_MAX, ticks);
        raw_write_word(cpu, timer_registers[timer], (uint16_t)result.value);
        if (period != 0u) {
            signal_timer_period(cpu, timer, result.matches, gated);
        }
    }
}

static void clock_timer(Dspic33* cpu, uint8_t timer, uint64_t clocks) {
    uint16_t control = raw_word(cpu, timer_controls[timer]);
    uint32_t prescale = timer_prescale(control);
    uint64_t accumulated = cpu->io.timer_fraction[timer] + clocks;
    cpu->io.timer_fraction[timer] = (uint32_t)(accumulated % prescale);
    advance_timer_ticks(cpu, timer, accumulated / prescale);
}

static void pulse_timer(Dspic33* cpu, uint8_t timer, uint32_t pulses) {
    uint16_t bit;
    uint16_t control;
    if (timer >= DSPIC33_TIMER_COUNT || pulses == 0u ||
        timer_is_paired_high(cpu, timer)) {
        return;
    }
    bit = (uint16_t)(1u << timer);
    control = raw_word(cpu, timer_controls[timer]);
    if ((cpu->io.timer_enabled & bit) == 0u || (control & TIMER_EXTERNAL) == 0u ||
        !timer_power_enabled(cpu, timer, true)) {
        return;
    }
    if ((timer == 0u || timer_is_type_b(timer)) &&
        (cpu->io.timer_external_started & bit) == 0u) {
        cpu->io.timer_external_started |= bit;
        pulses--;
    }
    clock_timer(cpu, timer, pulses);
}

static void set_timer_gate(Dspic33* cpu, uint8_t timer, bool high) {
    uint16_t bit;
    bool previous;
    uint16_t control;
    if (timer >= DSPIC33_TIMER_COUNT || timer_is_paired_high(cpu, timer)) {
        return;
    }
    bit = (uint16_t)(1u << timer);
    previous = (cpu->io.timer_gate & bit) != 0u;
    if (high) {
        cpu->io.timer_gate |= bit;
    } else {
        cpu->io.timer_gate &= (uint16_t)~bit;
    }
    control = raw_word(cpu, timer_controls[timer]);
    if (previous && !high && (cpu->io.timer_enabled & bit) != 0u &&
        (control & (TIMER_GATE | TIMER_EXTERNAL)) == TIMER_GATE &&
        timer_power_enabled(cpu, timer, false)) {
        uint8_t interrupt_timer =
            timer_pair_enabled(cpu, timer) ? (uint8_t)(timer + 1u) : timer;
        uint32_t period = raw_word(cpu, timer_periods[timer]);
        if (timer_pair_enabled(cpu, timer)) {
            period |= (uint32_t)raw_word(cpu, timer_periods[timer + 1u]) << 16u;
        }
        if (period != 0u) {
            dspic33_raise_interrupt(cpu, timer_irqs[interrupt_timer]);
        }
    }
}

static void process_event(Dspic33* cpu, const Dspic33Event* event) {
    switch (event->type) {
    case DSPIC33_EVENT_INTERRUPT:
        dspic33_raise_interrupt(cpu, event->source);
        break;
    case DSPIC33_EVENT_TIMER:
        pulse_timer(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_TIMER_GATE:
        set_timer_gate(cpu, (uint8_t)event->source, event->value != 0u);
        break;
    case DSPIC33_EVENT_DMA:
        run_dma(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_ADC:
        run_adc(cpu, (uint8_t)event->source, event->value);
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
        dspic33_complete_nvm(cpu);
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
    if (cycles != 0u) {
        uint16_t pending = cpu->io.timer_interrupt_pending;
        cpu->io.timer_interrupt_pending = 0u;
        while (pending != 0u) {
            if ((pending & 1u) != 0u) {
                dspic33_raise_interrupt(cpu, timer_irqs[timer]);
            }
            pending >>= 1u;
            timer++;
        }
        timer = 0u;
    }
    while (enabled != 0u) {
        if ((enabled & 1u) != 0u) {
            uint16_t control = raw_word(cpu, timer_controls[timer]);
            bool gated = (control & TIMER_GATE) != 0u;
            bool gate_high = (cpu->io.timer_gate & (uint16_t)(1u << timer)) != 0u;
            if (!timer_is_paired_high(cpu, timer) && (control & TIMER_EXTERNAL) == 0u &&
                timer_power_enabled(cpu, timer, false) && (!gated || gate_high)) {
                clock_timer(cpu, timer, cycles);
            }
        }
        enabled >>= 1u;
        timer++;
    }
}

bool dspic33_device_advance(Dspic33* cpu, uint64_t cycles) {
    size_t group;
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
    for (group = 0u; group < DSPIC33_IRQ_GROUP_COUNT; group++) {
        cpu->interrupt_deferred[group] = cpu->interrupt_deferred_next[group];
        cpu->interrupt_deferred_next[group] = 0u;
    }
    cpu->gie_disable_deferred = cpu->gie_disable_deferred_next;
    cpu->gie_disable_deferred_next = 0u;
    return true;
}

static void update_timer_register(Dspic33* cpu, uint16_t address) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        if ((address & 0xfffeu) == timer_controls[timer]) {
            uint16_t bit = (uint16_t)(1u << timer);
            if ((raw_word(cpu, timer_controls[timer]) & TIMER_ON) != 0u) {
                cpu->io.timer_enabled |= (uint16_t)(1u << timer);
            } else {
                cpu->io.timer_enabled &= (uint16_t)~(1u << timer);
            }
            cpu->io.timer_fraction[timer] = 0u;
            cpu->io.timer_external_started &= (uint16_t)~bit;
            return;
        }
        if ((address & 0xfffeu) == timer_registers[timer]) {
            uint16_t bit = (uint16_t)(1u << timer);
            cpu->io.timer_fraction[timer] = 0u;
            cpu->io.timer_external_started &= (uint16_t)~bit;
            if (timer_pair_enabled(cpu, timer)) {
                raw_write_word(cpu, timer_registers[timer + 1u],
                               raw_word(cpu, timer_holding_registers[timer / 2u]));
            }
            return;
        }
    }
}

static void update_adc_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        uint16_t control_address = adc_controls[module];
        uint16_t control;
        if (address != control_address && address != control_address + 2u &&
            address != control_address + 6u) {
            continue;
        }
        control = raw_word(cpu, control_address);
        if (address == control_address) {
            bool was_on = (previous & ADC_ON) != 0u;
            bool on;
            bool was_sampling = (previous & ADC_SAMPLE) != 0u;
            bool sampling;
            uint8_t source;
            if ((requested & ADC_DONE) == 0u) {
                control &= (uint16_t)~ADC_DONE;
            }
            if (was_on && module == 0u && ((control ^ previous) & ADC_12_BIT) != 0u) {
                control = (uint16_t)((control & ~ADC_12_BIT) | (previous & ADC_12_BIT));
            }
            if (module == 0u && (control & ADC_12_BIT) != 0u) {
                control &= (uint16_t)~ADC_SIMULTANEOUS;
                raw_write_word(
                    cpu, (uint16_t)(control_address + 2u),
                    (uint16_t)(adc_register(cpu, module, 2u) & ~ADC_CHANNELS_MASK));
                raw_write_word(cpu, (uint16_t)(control_address + 6u), 0u);
            }
            raw_write_word(cpu, control_address, control);
            on = (control & ADC_ON) != 0u;
            sampling = (control & ADC_SAMPLE) != 0u;
            source = (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u);
            if (!on) {
                cpu->io.adc_generation[module]++;
                cpu->io.adc_latched_count[module] = 0u;
                raw_write_word(cpu, control_address,
                               (uint16_t)(control & ~(ADC_SAMPLE | ADC_DONE)));
                return;
            }
            if (!was_on) {
                cpu->io.adc_buffer_index[module] = 0u;
                cpu->io.adc_sample_count[module] = 0u;
                cpu->io.adc_scan_index[module] = 0u;
                cpu->io.adc_mux_b &= (uint8_t)~(1u << module);
                memset(cpu->io.adc_dma_sample[module], 0,
                       sizeof(cpu->io.adc_dma_sample[module]));
            }
            if (was_on && was_sampling && !sampling && source == 0u) {
                raw_write_word(cpu, control_address, (uint16_t)(control | ADC_SAMPLE));
                adc_start_conversion(cpu, module);
                return;
            }
            if (sampling && (!was_sampling || !was_on ||
                             ((control ^ previous) & ADC_TRIGGER_MASK) != 0u)) {
                adc_begin_sampling(cpu, module);
                return;
            }
            if ((control & ADC_AUTO_SAMPLE) != 0u &&
                (!was_on || (previous & ADC_AUTO_SAMPLE) == 0u)) {
                adc_begin_sampling(cpu, module);
                return;
            }
            if (was_sampling && !sampling) {
                cpu->io.adc_generation[module]++;
            }
            return;
        }
        if (module == 0u && (control & ADC_12_BIT) != 0u) {
            if (address == control_address + 2u) {
                raw_write_word(cpu, address,
                               (uint16_t)(raw_word(cpu, address) & ~ADC_CHANNELS_MASK));
            } else if (address == control_address + 6u) {
                raw_write_word(cpu, address, 0u);
            }
        }
        return;
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

static bool protect_oscillator_write(Dspic33* cpu, uint16_t address,
                                     uint16_t previous) {
    uint8_t value;
    uint8_t first;
    uint8_t second;
    uint8_t ready;

    if (address != 0x0742u && address != 0x0743u) {
        return false;
    }
    value = cpu->data[address];
    if (address == 0x0742u) {
        first = 0x46u;
        second = 0x57u;
        ready = 2u;
    } else {
        first = 0x78u;
        second = 0x9au;
        ready = 4u;
    }
    if (value == first) {
        cpu->io.oscillator_unlock = (uint8_t)(ready - 1u);
        raw_write_word(cpu, 0x0742u, previous);
        return true;
    }
    if (cpu->io.oscillator_unlock == ready - 1u && value == second) {
        cpu->io.oscillator_unlock = ready;
        raw_write_word(cpu, 0x0742u, previous);
        return true;
    }
    if (cpu->io.oscillator_unlock == ready) {
        cpu->io.oscillator_unlock = 0u;
        return false;
    }
    cpu->io.oscillator_unlock = 0u;
    raw_write_word(cpu, 0x0742u, previous);
    return true;
}

static bool dma_register_write_mask(uint16_t address, uint16_t* writable) {
    static const uint16_t channel_masks[] = {0xf833u, 0x80ffu, 0xffffu, 0x00ffu,
                                             0xffffu, 0x00ffu, 0xffffu, 0x3fffu};
    if (address >= DMA_CHANNEL_BASE &&
        address < DMA_CHANNEL_BASE + DSPIC33_DMA_COUNT * DMA_CHANNEL_STRIDE) {
        *writable = channel_masks[((address - DMA_CHANNEL_BASE) & 0x000fu) / 2u];
        return true;
    }
    if (address >= DMA_PWC && address <= DMA_SADRH) {
        *writable = 0u;
        return true;
    }
    return false;
}

static uint32_t dma_register_address(const Dspic33* cpu, uint16_t base,
                                     uint16_t low_offset) {
    return ((uint32_t)(raw_word(cpu, (uint16_t)(base + low_offset + 2u)) & 0x00ffu)
            << 16u) |
           raw_word(cpu, (uint16_t)(base + low_offset));
}

static void initialize_dma_channel(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dma_channel_base(channel);
    uint16_t bit = dma_channel_bit(channel);
    cpu->io.dma_index[channel] = 0u;
    cpu->io.dma_start_a[channel] = dma_register_address(cpu, base, 4u);
    cpu->io.dma_start_b[channel] = dma_register_address(cpu, base, 8u);
    cpu->io.dma_address[channel] = cpu->io.dma_start_a[channel];
    cpu->io.dma_bank &= (uint16_t)~bit;
    cpu->io.dma_half_raised &= (uint16_t)~bit;
    cpu->io.dma_forced_pending &= (uint16_t)~bit;
    cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
    cpu->io.dma_enabled |= bit;
    cpu->io.dma_generation[channel]++;
    raw_write_word(cpu, DMA_PPS, (uint16_t)(raw_word(cpu, DMA_PPS) & ~bit));
}

static void update_dma_control(Dspic33* cpu, uint8_t channel, uint16_t previous) {
    uint16_t base = dma_channel_base(channel);
    uint16_t current = raw_word(cpu, base);
    uint16_t bit = dma_channel_bit(channel);
    bool was_enabled = (previous & DMA_CON_CHEN) != 0u;
    bool enabled = (current & DMA_CON_CHEN) != 0u;
    if (enabled && !was_enabled) {
        initialize_dma_channel(cpu, channel);
    } else if (!enabled && was_enabled) {
        cpu->io.dma_enabled &= (uint16_t)~bit;
        cpu->io.dma_forced_pending &= (uint16_t)~bit;
        cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
        cpu->io.dma_generation[channel]++;
        raw_write_word(
            cpu, (uint16_t)(base + 2u),
            (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
    }
}

static void update_dma_request(Dspic33* cpu, uint8_t channel, uint16_t previous) {
    uint16_t base = dma_channel_base(channel);
    uint16_t address = (uint16_t)(base + 2u);
    uint16_t request = raw_word(cpu, address);
    uint16_t bit = dma_channel_bit(channel);
    if ((raw_word(cpu, base) & DMA_CON_CHEN) == 0u) {
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((previous & DMA_REQ_FORCE) != 0u) {
        raw_write_word(cpu, address, (uint16_t)(request | DMA_REQ_FORCE));
        return;
    }
    if ((request & DMA_REQ_FORCE) == 0u) {
        return;
    }
    if ((cpu->io.dma_peripheral_pending & bit) != 0u) {
        dma_request_collision(cpu, channel);
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((cpu->io.dma_forced_pending & bit) == 0u &&
        !schedule_dma_channel(cpu, channel, 0u, true, 1u)) {
        raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
    }
}

void dspic33_device_write_byte(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    uint16_t requested = raw_word(cpu, base);
    uint16_t writable;
    uint8_t channel;
    if (protect_oscillator_write(cpu, address, previous)) {
        return;
    }
    if (base >= 0x0680u && base <= 0x06f6u &&
        (raw_word(cpu, 0x0742u) & 0x0040u) != 0u) {
        raw_write_word(cpu, base, previous);
        return;
    }
    if (register_write_mask(base, &writable) ||
        adc_register_write_mask(base, &writable) ||
        dma_register_write_mask(base, &writable)) {
        raw_write_word(cpu, base,
                       (uint16_t)((previous & ~writable) | (requested & writable)));
    }
    if (base >= 0x0800u && base < 0x0800u + DSPIC33_IRQ_GROUP_COUNT * 2u) {
        uint16_t group = (uint16_t)((base - 0x0800u) / 2u);
        uint16_t current = raw_word(cpu, base);
        uint16_t cleared = (uint16_t)(previous & ~current);
        cpu->interrupt_deferred[group] &= (uint16_t)~cleared;
        cpu->interrupt_deferred_next[group] =
            (uint16_t)((cpu->interrupt_deferred_next[group] & ~cleared) |
                       (current & ~previous));
    }
    if (base == 0x08c8u) {
        raw_write_word(cpu, base, previous);
    }
    if (base == 0x08c2u) {
        uint16_t current = raw_word(cpu, base);
        if ((current & 0x8000u) != 0u) {
            cpu->gie_disable_deferred = 0u;
            cpu->gie_disable_deferred_next = 0u;
        } else if ((previous & 0x8000u) != 0u) {
            cpu->gie_disable_deferred_next = 1u;
        }
    }
    if (base == 0x08c0u && (previous & 0x0020u) != 0u &&
        (raw_word(cpu, base) & 0x0020u) == 0u) {
        raw_write_word(cpu, DMA_PWC, 0u);
        raw_write_word(cpu, DMA_RQC, 0u);
    }
    if (base == 0x0488u || base == 0x04c0u || base == 0x04c4u) {
        uint16_t mask = base == 0x04c4u ? 0x00ffu : 0x00fdu;
        raw_write_word(cpu, base, (uint16_t)(previous & ~(raw_word(cpu, base) & mask)));
    }
    if (base == 0x0758u) {
        raw_write_word(cpu, base,
                       (uint16_t)((raw_word(cpu, base) & 0x40ffu) | 0xa400u));
        if ((raw_word(cpu, base) & 0x8000u) != 0u &&
            (raw_word(cpu, base) & 0x4000u) == 0u) {
            dspic33_schedule(cpu, DSPIC33_EVENT_AUX_PLL, 0u, 0u, 32u);
        }
    }
    update_timer_register(cpu, base);
    update_adc_register(cpu, base, previous, requested);
    update_oscillator(cpu, base);
    write_uart(cpu, base);
    write_spi(cpu, base);
    if (base == 0x0728u && (raw_word(cpu, base) & 0x8000u) != 0u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_NVM, 0u, 0u, 2u);
    }
    if (base >= DMA_CHANNEL_BASE &&
        base < DMA_CHANNEL_BASE + DSPIC33_DMA_COUNT * DMA_CHANNEL_STRIDE) {
        channel = (uint8_t)((base - DMA_CHANNEL_BASE) / DMA_CHANNEL_STRIDE);
        if ((base & 0x000fu) == 0u) {
            update_dma_control(cpu, channel, previous);
        } else if ((base & 0x000fu) == 2u) {
            update_dma_request(cpu, channel, previous);
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
    static const uint16_t analog_addresses[DSPIC33_GPIO_PORT_COUNT] = {
        0x0e0eu, 0x0e1eu, 0x0e2eu, 0x0e3eu, 0x0e4eu, 0u, 0x0e6eu};
    uint8_t port;
    uint8_t channel;
    uint8_t timer;
    if (address == 0x08c3u) {
        return cpu->disicnt != 0u ? (uint8_t)(value | 0x40u)
                                  : (uint8_t)(value & ~0x40u);
    }
    if ((address & 1u) == 0u) {
        for (timer = 1u; timer < DSPIC33_TIMER_COUNT; timer += 2u) {
            if (address == timer_registers[timer] && timer_pair_enabled(cpu, timer)) {
                raw_write_word(cpu, timer_holding_registers[timer / 2u],
                               raw_word(cpu, timer_registers[timer + 1u]));
                break;
            }
        }
    }
    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        if ((address & 0xfffeu) == port_addresses[port]) {
            uint16_t tris = raw_word(cpu, tris_addresses[port]);
            uint16_t lat = raw_word(cpu, lat_addresses[port]);
            uint16_t pins = (uint16_t)((cpu->io.gpio[port] & tris) | (lat & ~tris));
            if (analog_addresses[port] != 0u) {
                pins &= (uint16_t)~raw_word(cpu, analog_addresses[port]);
            }
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

bool dspic33_dma_request(Dspic33* cpu, uint8_t request, uint16_t indirect_address,
                         uint64_t delay) {
    uint8_t channel;
    bool succeeded = true;
    for (channel = 0u; channel < DSPIC33_DMA_COUNT; channel++) {
        uint16_t base = dma_channel_base(channel);
        uint16_t bit = dma_channel_bit(channel);
        if ((raw_word(cpu, base) & DMA_CON_CHEN) == 0u ||
            (raw_word(cpu, (uint16_t)(base + 2u)) & DMA_REQ_SOURCE_MASK) != request ||
            (raw_word(cpu, DMA_PWC) & bit) != 0u) {
            continue;
        }
        if ((cpu->io.dma_peripheral_pending & bit) != 0u) {
            continue;
        }
        if ((cpu->io.dma_forced_pending & bit) != 0u) {
            dma_request_collision(cpu, channel);
        }
        if (!schedule_dma_channel(cpu, channel, indirect_address, false, delay)) {
            succeeded = false;
        }
    }
    return succeeded;
}

bool dspic33_timer_pulse(Dspic33* cpu, uint8_t timer, uint32_t pulses, uint64_t delay) {
    return timer < DSPIC33_TIMER_COUNT && pulses != 0u &&
           dspic33_schedule(cpu, DSPIC33_EVENT_TIMER, timer, pulses, delay);
}

bool dspic33_timer_gate(Dspic33* cpu, uint8_t timer, bool high, uint64_t delay) {
    return timer < DSPIC33_TIMER_COUNT &&
           dspic33_schedule(cpu, DSPIC33_EVENT_TIMER_GATE, timer, high ? 1u : 0u,
                            delay);
}

bool dspic33_adc_trigger(Dspic33* cpu, uint8_t module, uint8_t source, uint64_t delay) {
    uint32_t value;
    if (module >= DSPIC33_ADC_COUNT || source == 0u || source == 6u || source == 7u ||
        source >= 15u) {
        return false;
    }
    value = source | ((uint32_t)UINT16_MAX << ADC_EVENT_GENERATION_SHIFT);
    return dspic33_schedule(cpu, DSPIC33_EVENT_ADC, module, value, delay);
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
        cpu->io.adc[channel] = (uint16_t)(value & 0x0fffu);
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
    memset(cpu->interrupt_deferred, 0, sizeof(cpu->interrupt_deferred));
    memset(cpu->interrupt_deferred_next, 0, sizeof(cpu->interrupt_deferred_next));
    cpu->gie_disable_deferred = 0u;
    cpu->gie_disable_deferred_next = 0u;
    cpu->io.usb_size = sizeof(cpu->io.usb);
    for (index = 0u; index < sizeof(reset_values) / sizeof(reset_values[0]); index++) {
        raw_write_word(cpu, reset_values[index].address, reset_values[index].value);
    }
    raw_write_word(cpu, 0x0742u, 0x3020u);
    raw_write_word(cpu, 0x0758u, 0xa400u);
    raw_write_word(cpu, 0x08c2u, 0x8000u);
    raw_write_word(cpu, 0x08c8u, 0u);
    raw_write_word(cpu, DMA_LCA, 0x000fu);
}
