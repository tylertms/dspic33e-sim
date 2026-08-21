# dsPIC33E Simulator

C simulator for the Microchip dsPIC33E architecture and dsPIC33EP512MU810 microcontroller.

## Features

- CPU Core: dsPIC33E instruction set (integer, DSP, branch, table, repeat, DO-loops, traps, interrupts).
- dsPIC33EP512MU810 Peripherals: SFR memory map, DMA controllers, timers, ADC, UART, SPI, I2C, USB, CAN, PWM, PPS.

## Build

```
cmake -S . -B build/simulator -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/simulator --parallel
```

### Build Targets

| Target | Type | Description |
| :--- | :--- | :--- |
| `dspic33e::simulator` | Static Library | Core CPU and peripheral simulator. |
| `dspic33e::firmware_image` | Static Library | ELF and raw binary image loader. |
| `dspic33e::firmware_runner` | Executable | CLI tool to load and run firmware images. |

## Run Firmware

```
dspic33e_firmware_runner <IMAGE> --reset-address <ADDRESS> [OPTIONS]
```

### Runner Options

| Option | Description |
| :--- | :--- |
| `--reset-address <ADDR>` | Entry point / reset address (required). |
| `--stop-address <ADDR>` | Execution stop address. |
| `--max-instructions <N>` | Maximum instruction count. |
| `--max-cycles <N>` | Maximum clock cycle limit. |
| `--program-word <ADDR> <VAL>` | Write word to program memory before execution. |

## Use in CMake Projects

```cmake
add_subdirectory(sim/dspic33e-sim EXCLUDE_FROM_ALL)
target_link_libraries(your_target PRIVATE dspic33e::simulator)
```

## Run Tests

Run all unit and device tests:

```
ctest --test-dir build/simulator --output-on-failure --parallel
```

Run specific test groups:

```
ctest --test-dir build/simulator -L core --output-on-failure --parallel
ctest --test-dir build/simulator -L device --output-on-failure --parallel
ctest --test-dir build/simulator -L system --output-on-failure --parallel
```
