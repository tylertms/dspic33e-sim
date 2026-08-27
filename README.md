# dsPIC33EP-MU Simulator

C simulator for the Microchip dsPIC33EP-MU microcontroller family and its dsPIC33E core.

## Features

- CPU Core: dsPIC33E instruction set (integer, DSP, branch, table, repeat, DO-loops, traps, interrupts).
- dsPIC33EP-MU Peripherals: device-specific flash, RAM, SFR maps, DMA controllers, timers, ADC, UART, SPI, I2C, USB, CAN, PWM, PPS.
- Device Profiles: dsPIC33EP256MU806, dsPIC33EP256MU810, dsPIC33EP256MU814, dsPIC33EP512MU810, and dsPIC33EP512MU814.

Device memory maps and reset values are based on Microchip's dsPIC33E Device Family Pack 1.7.401.

## Build

Requires GCC, Meson, and Ninja.

```
meson setup build/simulator --buildtype=release
meson compile -C build/simulator
```

### Build Targets

| Target | Type | Description |
| :--- | :--- | :--- |
| `dspic33ep_mu_simulator` | Static Library | dsPIC33EP-MU device and dsPIC33E core simulator. |
| `dspic33ep_mu_firmware_image` | Static Library | ELF and raw binary image loader. |
| `dspic33ep_mu_firmware_runner` | Executable | CLI tool to load and run firmware images. |
| `test` | Utility | Run all tests. |

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

## Use in Meson Projects

```meson
dspic33ep_mu = subproject('dspic33ep-mu-sim')
simulator = dspic33ep_mu.get_variable('dspic33ep_mu_simulator_dependency')

executable('your_target', 'main.c', dependencies: simulator)
```

## Run Tests

Run all tests:

```
meson test -C build/simulator
```

Run all tests with simulator source coverage:

```
meson setup build/test-coverage --buildtype=debug -Db_coverage=true
meson test -C build/test-coverage --num-processes 1 --no-suite census
meson compile -C build/test-coverage --ninja-args=coverage-text
```

The coverage report requires GCC, gcov, and gcovr. The coverage run excludes exhaustive census
tests because their billions of iterations overflow GCC's coverage counters. Normal test runs still
include them.
