#include <stdint.h>
#include <stdio.h>

#include "dspic33.h"

int main(void) {
    Dspic33 cpu;
    uint16_t address;

    if (!dspic33_initialize(&cpu)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    dspic33_reset(&cpu, 0u);
    printf("[sfr-reset-snapshot] ");
    for (address = 0u; address < 0x1000u; address += 2u) {
        printf("%04x", (unsigned)dspic33_read_word(&cpu, address));
    }
    putchar('\n');
    dspic33_destroy(&cpu);
    return 0;
}
