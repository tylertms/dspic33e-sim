project(
  'dspic33ep-mu-sim',
  'c',
  default_options: [
    'buildtype=release',
    'c_std=c11',
    'warning_level=3',
    'werror=true',
  ],
  meson_version: '>=1.11.0',
)

cc = meson.get_compiler('c')
public_include = include_directories('include')
private_include = include_directories('src')
build_by_default = not meson.is_subproject()

simulator_sources = files(
  'src/architecture/dspic33/coverage.c',
  'src/architecture/dspic33/execution/addressing.c',
  'src/architecture/dspic33/execution/instructions.c',
  'src/architecture/dspic33/execution/reset.c',
  'src/architecture/dspic33/execution/run.c',
  'src/architecture/dspic33/execution/state.c',
  'src/architecture/dspic33/execution/traps.c',
  'src/device/dspic33ep_mu/analog/adc.c',
  'src/device/dspic33ep_mu/analog/comparator.c',
  'src/device/dspic33ep_mu/common.c',
  'src/device/dspic33ep_mu/communication/can/bus.c',
  'src/device/dspic33ep_mu/communication/can/controller.c',
  'src/device/dspic33ep_mu/communication/can/registers.c',
  'src/device/dspic33ep_mu/communication/dci.c',
  'src/device/dspic33ep_mu/communication/i2c/controller.c',
  'src/device/dspic33ep_mu/communication/i2c/slave.c',
  'src/device/dspic33ep_mu/communication/pmp.c',
  'src/device/dspic33ep_mu/communication/serial/dma.c',
  'src/device/dspic33ep_mu/communication/serial/read_completion.c',
  'src/device/dspic33ep_mu/communication/spi.c',
  'src/device/dspic33ep_mu/communication/uart.c',
  'src/device/dspic33ep_mu/communication/usb/registers.c',
  'src/device/dspic33ep_mu/communication/usb/runtime.c',
  'src/device/dspic33ep_mu/control/input_capture.c',
  'src/device/dspic33ep_mu/control/output_compare/runtime.c',
  'src/device/dspic33ep_mu/control/output_compare/state.c',
  'src/device/dspic33ep_mu/control/pwm.c',
  'src/device/dspic33ep_mu/control/qei.c',
  'src/device/dspic33ep_mu/data.c',
  'src/device/dspic33ep_mu/memory/crc.c',
  'src/device/dspic33ep_mu/memory/dma/registers.c',
  'src/device/dspic33ep_mu/memory/dma/runtime.c',
  'src/device/dspic33ep_mu/memory/registers.c',
  'src/device/dspic33ep_mu/peripherals.c',
  'src/device/dspic33ep_mu/platform/gpio/inputs.c',
  'src/device/dspic33ep_mu/platform/gpio/runtime.c',
  'src/device/dspic33ep_mu/platform/register_updates.c',
  'src/device/dspic33ep_mu/platform/write_access.c',
  'src/device/dspic33ep_mu/runtime.c',
  'src/device/dspic33ep_mu/timing/oscillator.c',
  'src/device/dspic33ep_mu/timing/rtcc.c',
  'src/device/dspic33ep_mu/timing/timers.c',
)

simulator = static_library(
  'dspic33ep_mu_simulator',
  simulator_sources,
  build_by_default: build_by_default,
  include_directories: [public_include, private_include],
)
dspic33ep_mu_simulator_dependency = declare_dependency(
  include_directories: public_include,
  link_with: simulator,
)

firmware_image = static_library(
  'dspic33ep_mu_firmware_image',
  files('src/image/dspic33_firmware_image.c', 'src/image/elf_image.c'),
  build_by_default: build_by_default,
  dependencies: dspic33ep_mu_simulator_dependency,
  include_directories: public_include,
)
dspic33ep_mu_firmware_image_dependency = declare_dependency(
  dependencies: dspic33ep_mu_simulator_dependency,
  include_directories: public_include,
  link_with: firmware_image,
)

dspic33ep_mu_firmware_runner = executable(
  'dspic33ep_mu_firmware_runner',
  'src/runner/main.c',
  build_by_default: build_by_default,
  dependencies: dspic33ep_mu_firmware_image_dependency,
)

if not meson.is_subproject()
  subdir('tests')
endif
