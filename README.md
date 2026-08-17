# dsPIC33 Simulator

This repository contains a native C simulator for the dsPIC33E architecture.

The CPU engine implements the dsPIC33E instruction set. The device model targets the dsPIC33EP512MU810 B1 revision.

## Build

```powershell
cmake -S . -B build/simulator -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/simulator --parallel
```

The build provides these targets:

- `dspic33::simulator`: static simulator library
- `dspic33::firmware_runner`: firmware scenario runner

## Use from CMake

Add this repository as a Git submodule. Then add the simulator to the parent build:

```cmake
add_subdirectory(third_party/dspic33-sim EXCLUDE_FROM_ALL)
target_link_libraries(your_target PRIVATE dspic33::simulator)
```

The parent build does not build the standalone simulator tests.

## Tests

The test suite uses CTest and native C executables.

```powershell
ctest --test-dir build/simulator --output-on-failure
```

The `tests` directory has these groups:

- `cpu`: CPU and event integration tests
- `device`: device unit and integration tests
- `system`: firmware-runner system tests
- `support`: shared test utilities

Use a CTest label to run one test level or subsystem:

```powershell
ctest --test-dir build/simulator -L unit --output-on-failure
ctest --test-dir build/simulator -L device --output-on-failure
ctest --test-dir build/simulator -L system --output-on-failure
```

The device data in `src/dspic33ep512mu810_data.c` defines the implemented SFR addresses and master-clear reset values.
