# dsPIC33E Simulator

This repository provides a native C simulator for the dsPIC33E architecture.

The CPU model implements the dsPIC33E instruction set. The device model
implements the dsPIC33EP512MU810 B1 revision.

## Supported behavior

The CPU supports integer, DSP, branch, table, stack, repeat, and DO-loop
instructions. It also supports interrupts, traps, reset states, power states,
event scheduling, and bounded execution.

The device model includes memory, clocks, DMA, GPIO, timers, analog units,
serial interfaces, USB, CAN, PWM, PPS, and other dsPIC33EP512MU810 peripherals.

The register model rejects unknown addresses and unsupported access widths.
Hardware tests must check electrical timing, analog tolerances, clock accuracy,
and silicon-specific behavior. The model does not replace these hardware tests.

## Build

Configure a Release build:

```
cmake -S . -B build/simulator -G Ninja -DCMAKE_BUILD_TYPE=Release
```

Build the simulator:

```
cmake --build build/simulator --parallel
```

The build provides these targets:

- `dspic33e::simulator` is the static simulator library.
- `dspic33e::firmware_image` loads ELF and raw binary images.
- `dspic33e::firmware_runner` loads and runs a firmware image.

The runner accepts a dsPIC ELF file or a raw binary file. It requires a reset
address and uses finite instruction and cycle limits by default:

```
dspic33e_firmware_runner IMAGE --reset-address ADDRESS \
  [--max-instructions COUNT] [--max-cycles COUNT] \
  [--stop-address ADDRESS] [--program-word ADDRESS VALUE]
```

Reset and stop addresses can be numeric values or ELF symbols. The
`--program-word` option supports CPU-specific runner tests.

## Use from CMake

Add this repository as a Git submodule.

Add the submodule to the parent build:

```cmake
add_subdirectory(sim/dspic33e-sim EXCLUDE_FROM_ALL)
target_link_libraries(your_target PRIVATE dspic33e::simulator)
```

The parent build does not build the standalone tests.

## Tests

CTest runs isolated native C executables. No external test framework is necessary.

Run all tests:

```
ctest --test-dir build/simulator --parallel --output-on-failure
```

The `tests` directory has this structure:

- `core` contains instruction, event, fault, timing, and public API tests.
- `device` contains register and peripheral tests.
- `system` contains firmware image and runner tests.
- `support` contains shared test data and the small assertion helper.

The device tests include interrupt, reset, copy, power, lifecycle, and error cases.

Run one test group:

```
ctest --test-dir build/simulator -L unit --parallel --output-on-failure
ctest --test-dir build/simulator -L core --parallel --output-on-failure
ctest --test-dir build/simulator -L device --parallel --output-on-failure
ctest --test-dir build/simulator -L system --parallel --output-on-failure
```

## Device data

Special function registers (SFRs) control the dsPIC device.

`src/dspic33ep512mu810_data.c` contains the implemented SFR addresses and reset values. The tests use fixed hashes and independent access expectations.
