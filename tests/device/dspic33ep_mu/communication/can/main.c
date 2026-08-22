#include "device/dspic33ep_mu/communication/can/internal.h"

typedef void (*CanTestGroup)(TestState* state, Dspic33* cpu);

static bool run_group(TestState* state, CanTestGroup group) {
    Dspic33 cpu;
    if (!dspic33_initialize(&cpu)) {
        return false;
    }
    group(state, &cpu);
    dspic33_release(&cpu);
    return true;
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    const CanTestGroup groups[] = {
        dspic33_can_test_register_groups,
        dspic33_can_test_receive_groups,
        dspic33_can_test_bus_groups,
        dspic33_can_test_error_groups,
    };
    for (size_t index = 0u; index < sizeof(groups) / sizeof(groups[0]); index++) {
        if (!run_group(&state, groups[index])) {
            fprintf(stderr, "[can-error] cannot initialize emulator\n");
            return 2;
        }
    }
    return test_finish(&state);
}
