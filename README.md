# dsPIC33EP-MU Simulator

C simulator for the Microchip dsPIC33EP-MU microcontroller family and its dsPIC33E core.

## Features

- CPU Core: dsPIC33E instruction set (integer, DSP, branch, table, repeat, DO-loops, traps, interrupts).
- dsPIC33EP-MU Peripherals: device-specific flash, RAM, SFR maps, DMA controllers, timers, ADC, UART, SPI, I2C, USB, CAN, PWM, PPS.
- Device Profiles: dsPIC33EP256MU806, dsPIC33EP256MU810, dsPIC33EP256MU814, dsPIC33EP512MU810, and dsPIC33EP512MU814.

Device memory maps and reset values are based on Microchip's dsPIC33E Device Family Pack 1.7.401.

## Layout

- `include/` contains the public simulator and firmware-image APIs.
- `src/core/` contains the dsPIC33E processor implementation.
- `src/device/` contains the dsPIC33EP-MU device model and profiles.
- `src/image/` and `src/runner/` contain firmware loading and the command-line runner.
- `tests/` mirrors the core, device, and system boundaries.

## Build

```
cmake -S . -B build/simulator -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/simulator --parallel
```

### Build Targets

| Target | Type | Description |
| :--- | :--- | :--- |
| `dspic33ep_mu::simulator` | Static Library | dsPIC33EP-MU device and dsPIC33E core simulator. |
| `dspic33ep_mu::firmware_image` | Static Library | ELF and raw binary image loader. |
| `dspic33ep_mu::firmware_runner` | Executable | CLI tool to load and run firmware images. |

## Run Firmware

```
dspic33ep_mu_firmware_runner <IMAGE> --reset-address <ADDRESS> [OPTIONS]
```

### Runner Options

| Option | Description |
| :--- | :--- |
| `--reset-address <ADDR>` | Entry point / reset address (required). |
| `--device <DEVICE>` | Device profile. Defaults to `dsPIC33EP512MU810`. |
| `--stop-address <ADDR>` | Execution stop address. |
| `--max-instructions <N>` | Maximum instruction count. |
| `--max-cycles <N>` | Maximum clock cycle limit. |
| `--program-word <ADDR> <VAL>` | Write word to program memory before execution. |

## Use in CMake Projects

```cmake
add_subdirectory(sim/dspic33ep-mu-sim EXCLUDE_FROM_ALL)
target_link_libraries(your_target PRIVATE dspic33ep_mu::simulator)
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
