# dsPIC33E Simulator

This repository provides a native C simulator for the dsPIC33E architecture.

The CPU model implements the dsPIC33E instruction set. The device model implements the dsPIC33EP512MU810 B1 revision.

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
- `dspic33e::firmware_runner` is the firmware scenario runner.

## Use from CMake

Add this repository as a Git submodule.

Add the submodule to the parent build:

```cmake
add_subdirectory(third_party/dspic33e-sim EXCLUDE_FROM_ALL)
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

- `cpu/processor` contains separate fault, timing, data-encoding, and control-encoding tests.
- `cpu/event_scheduler_test.c` contains event and public-state tests.
- `device` contains register and peripheral tests.
- `support` contains shared test data and the small assertion helper.
- `runner_smoke.hex` is the firmware-runner fixture.

The device tests include interrupt, reset, copy, power, lifecycle, and error cases.

CTest stops an ordinary test after 60 seconds. It gives each exhaustive processor test 10 minutes.

Run one test group:

```
ctest --test-dir build/simulator -L unit --parallel --output-on-failure
ctest --test-dir build/simulator -L cpu --parallel --output-on-failure
ctest --test-dir build/simulator -L device --parallel --output-on-failure
ctest --test-dir build/simulator -L system --parallel --output-on-failure
```

## Device data

Special function registers (SFRs) control the dsPIC device.

`src/dspic33ep512mu810_data.c` contains the implemented SFR addresses and reset values. The tests use fixed hashes and independent access expectations.
