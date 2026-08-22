cmake_minimum_required(VERSION 3.21)

project(dspic33ep_mu_sim LANGUAGES C)

if(PROJECT_IS_TOP_LEVEL)
  option(BUILD_TESTING "Build the simulator tests" ON)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Build type" FORCE)
endif()

set(dspic33ep_mu_simulator_sources
    src/architecture/dspic33/execution/addressing.c
    src/architecture/dspic33/execution/instructions.c
    src/architecture/dspic33/execution/reset.c
    src/architecture/dspic33/execution/run.c
    src/architecture/dspic33/execution/state.c
    src/architecture/dspic33/execution/traps.c
    src/device/dspic33ep_mu/analog/adc.c
    src/device/dspic33ep_mu/analog/comparator.c
    src/device/dspic33ep_mu/communication/can/bus.c
    src/device/dspic33ep_mu/communication/can/controller.c
    src/device/dspic33ep_mu/communication/can/registers.c
    src/device/dspic33ep_mu/communication/dci.c
    src/device/dspic33ep_mu/communication/i2c/controller.c
    src/device/dspic33ep_mu/communication/i2c/slave.c
    src/device/dspic33ep_mu/communication/pmp.c
    src/device/dspic33ep_mu/communication/serial/dma.c
    src/device/dspic33ep_mu/communication/serial/read_completion.c
    src/device/dspic33ep_mu/communication/spi.c
    src/device/dspic33ep_mu/communication/uart.c
    src/device/dspic33ep_mu/communication/usb/runtime.c
    src/device/dspic33ep_mu/communication/usb/registers.c
    src/device/dspic33ep_mu/control/input_capture.c
    src/device/dspic33ep_mu/control/output_compare/runtime.c
    src/device/dspic33ep_mu/control/output_compare/state.c
    src/device/dspic33ep_mu/control/pwm.c
    src/device/dspic33ep_mu/control/qei.c
    src/device/dspic33ep_mu/memory/crc.c
    src/device/dspic33ep_mu/memory/dma/runtime.c
    src/device/dspic33ep_mu/memory/dma/registers.c
    src/device/dspic33ep_mu/memory/registers.c
    src/device/dspic33ep_mu/platform/gpio/runtime.c
    src/device/dspic33ep_mu/platform/gpio/inputs.c
    src/device/dspic33ep_mu/platform/register_updates.c
    src/device/dspic33ep_mu/platform/write_access.c
    src/device/dspic33ep_mu/timing/rtcc.c
    src/device/dspic33ep_mu/timing/timers.c
    src/device/dspic33ep_mu/timing/oscillator.c
    src/device/dspic33ep_mu/common.c
    src/device/dspic33ep_mu/data.c
    src/device/dspic33ep_mu/peripherals.c
    src/device/dspic33ep_mu/runtime.c)

add_library(dspic33ep_mu_simulator STATIC ${dspic33ep_mu_simulator_sources})
add_library(dspic33ep_mu::simulator ALIAS dspic33ep_mu_simulator)

add_library(dspic33ep_mu_firmware_image STATIC
            src/image/dspic33_firmware_image.c src/image/elf_image.c)
add_library(dspic33ep_mu::firmware_image ALIAS dspic33ep_mu_firmware_image)

add_executable(dspic33ep_mu_firmware_runner src/runner/main.c)
add_executable(dspic33ep_mu::firmware_runner ALIAS dspic33ep_mu_firmware_runner)

target_include_directories(
  dspic33ep_mu_simulator
  PUBLIC include
  PRIVATE src)
target_include_directories(dspic33ep_mu_firmware_image PUBLIC include)
target_compile_features(dspic33ep_mu_simulator PUBLIC c_std_11)
target_compile_features(dspic33ep_mu_firmware_image PUBLIC c_std_11)
target_compile_features(dspic33ep_mu_firmware_runner PRIVATE c_std_11)

target_link_libraries(dspic33ep_mu_firmware_image PUBLIC dspic33ep_mu_simulator)
target_link_libraries(dspic33ep_mu_firmware_runner
                      PRIVATE dspic33ep_mu_firmware_image)
set(dspic33ep_mu_targets dspic33ep_mu_simulator dspic33ep_mu_firmware_image
                         dspic33ep_mu_firmware_runner)

if(MSVC)
  foreach(target IN LISTS dspic33ep_mu_targets)
    target_compile_options(${target} PRIVATE /W4 /WX)
  endforeach()
else()
  foreach(target IN LISTS dspic33ep_mu_targets)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endforeach()
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  enable_testing()

  function(dspic33ep_mu_add_test name source labels)
    set(target "dspic33ep_mu_${name}_test")
    add_executable(${target} ${source} ${ARGN})
    target_compile_features(${target} PRIVATE c_std_11)
    target_include_directories(${target} PRIVATE tests tests/support src
                                                 src/image)
    target_link_libraries(${target} PRIVATE dspic33ep_mu_simulator)
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4 /WX)
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}")
  endfunction()

  function(dspic33ep_mu_enable_allocation_failure target)
    target_sources(${target} PRIVATE tests/support/allocation_failure.c)
    if(NOT MSVC)
      target_link_options(${target} PRIVATE -Wl,--wrap=realloc)
      target_compile_definitions(${target}
                                 PRIVATE DSPIC33_TEST_ALLOCATION_FAILURE=1)
    endif()
  endfunction()

  dspic33ep_mu_add_test(
    event_scheduler tests/architecture/dspic33/timing/events_test.c
    "core;integration")
  dspic33ep_mu_add_test(core_api tests/architecture/dspic33/api_test.c
                        "core;unit;integration")
  dspic33ep_mu_add_test(
    processor_fault
    tests/architecture/dspic33/exceptions/address.c
    "core;integration"
    tests/architecture/dspic33/exceptions/exceptions.c
    tests/architecture/dspic33/exceptions/traps.c
    tests/architecture/dspic33/exceptions/main.c)
  dspic33ep_mu_add_test(
    processor_timing tests/architecture/dspic33/timing/instructions/cycles.c
    "core;integration" tests/architecture/dspic33/timing/instructions/errata.c
    tests/architecture/dspic33/timing/instructions/main.c)
  dspic33ep_mu_add_test(
    processor_data
    tests/architecture/dspic33/execution/data/loops.c
    "core;integration"
    tests/architecture/dspic33/execution/data/arithmetic.c
    tests/architecture/dspic33/execution/data/memory.c
    tests/architecture/dspic33/execution/data/dsp.c
    tests/architecture/dspic33/execution/data/edge.c
    tests/architecture/dspic33/execution/data/main.c)
  dspic33ep_mu_add_test(
    processor_control
    tests/architecture/dspic33/execution/control/branches.c
    "core;integration"
    tests/architecture/dspic33/execution/control/flow.c
    tests/architecture/dspic33/execution/control/system.c
    tests/architecture/dspic33/execution/control/main.c)
  dspic33ep_mu_add_test(
    processor_encoding_census
    tests/architecture/dspic33/execution/encoding_census_test.c
    "core;unit;integration")
  dspic33ep_mu_add_test(
    processor_state_census
    tests/architecture/dspic33/execution/state_census_test.c
    "core;unit;integration")
  dspic33ep_mu_add_test(
    peripheral_state_census
    tests/device/dspic33ep_mu/state_census_test.c
    "device;unit;integration")
  dspic33ep_mu_add_test(
    processor_boundary tests/architecture/dspic33/execution/boundary.c
    "core;unit")
  dspic33ep_mu_add_test(
    processor_addressing_boundary
    tests/architecture/dspic33/execution/data/addressing_boundary_test.c
    "core;unit")

  dspic33ep_mu_add_test(adc tests/device/dspic33ep_mu/analog/adc_test.c
                        "device;integration")
  dspic33ep_mu_add_test(
    auxiliary_clock tests/device/dspic33ep_mu/timing/auxiliary_clock_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    can
    tests/device/dspic33ep_mu/communication/can/registers.c
    "device;integration"
    tests/device/dspic33ep_mu/communication/can/receive.c
    tests/device/dspic33ep_mu/communication/can/bus.c
    tests/device/dspic33ep_mu/communication/can/errors.c
    tests/device/dspic33ep_mu/communication/can/boundary.c
    tests/device/dspic33ep_mu/communication/can/main.c)
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_can_test)
  dspic33ep_mu_add_test(
    comparator tests/device/dspic33ep_mu/analog/comparator/registers.c
    "device;integration" tests/device/dspic33ep_mu/analog/comparator/behavior.c)
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_comparator_test)
  dspic33ep_mu_add_test(
    core_sfr tests/device/dspic33ep_mu/platform/core_sfr_test.c
    "device;integration")
  dspic33ep_mu_add_test(common tests/device/dspic33ep_mu/common/boundary.c
                        "device;unit")
  dspic33ep_mu_add_test(crc tests/device/dspic33ep_mu/memory/crc_test.c
                        "device;integration")
  dspic33ep_mu_add_test(
    dci
    tests/device/dspic33ep_mu/communication/dci/registers.c
    "device;integration"
    tests/device/dspic33ep_mu/communication/dci/transfer.c
    tests/device/dspic33ep_mu/communication/dci/clock.c
    tests/device/dspic33ep_mu/communication/dci/boundary.c
    tests/device/dspic33ep_mu/communication/dci/state_census.c
    tests/device/dspic33ep_mu/communication/dci/state_matrix.c
    tests/device/dspic33ep_mu/communication/dci/main.c)
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_dci_test)
  dspic33ep_mu_add_test(
    device_data tests/device/dspic33ep_mu/memory/device_data_test.c
    "device;unit")
  dspic33ep_mu_add_test(
    dma tests/device/dspic33ep_mu/memory/dma_test.c "device;integration"
    tests/device/dspic33ep_mu/memory/dma/boundary.c)
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_dma_test)
  dspic33ep_mu_add_test(gpio tests/device/dspic33ep_mu/platform/gpio_test.c
                        "device;integration")
  dspic33ep_mu_add_test(
    i2c
    tests/device/dspic33ep_mu/communication/i2c/registers.c
    "device;integration"
    tests/device/dspic33ep_mu/communication/i2c/master.c
    tests/device/dspic33ep_mu/communication/i2c/slave.c
    tests/device/dspic33ep_mu/communication/i2c/pins.c
    tests/device/dspic33ep_mu/communication/i2c/boundary.c
    tests/device/dspic33ep_mu/communication/i2c/state_matrix.c)
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_i2c_test)
  dspic33ep_mu_add_test(
    input_capture tests/device/dspic33ep_mu/control/input_capture/registers.c
    "device;integration"
    tests/device/dspic33ep_mu/control/input_capture/events.c
    tests/device/dspic33ep_mu/control/input_capture/boundary.c)
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_input_capture_test)
  dspic33ep_mu_add_test(
    interrupt_control
    tests/device/dspic33ep_mu/platform/interrupt_control_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    peripheral_census
    tests/device/dspic33ep_mu/platform/peripheral_census_test.c
    "device;unit;integration")
  dspic33ep_mu_add_test(
    peripheral_api_boundary
    tests/device/dspic33ep_mu/platform/peripheral_api_boundary_test.c
    "device;unit")
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_peripheral_api_boundary_test)
  dspic33ep_mu_add_test(
    nvm
    tests/device/dspic33ep_mu/memory/nvm/registers.c
    "device;integration"
    tests/device/dspic33ep_mu/memory/nvm/commands.c
    tests/device/dspic33ep_mu/memory/nvm/flash.c
    tests/device/dspic33ep_mu/memory/nvm/main.c)
  target_link_libraries(dspic33ep_mu_nvm_test
                        PRIVATE dspic33ep_mu_firmware_image)
  dspic33ep_mu_add_test(
    oscillator
    tests/device/dspic33ep_mu/timing/oscillator/configuration.c
    "device;integration"
    tests/device/dspic33ep_mu/timing/oscillator/switching.c
    tests/device/dspic33ep_mu/timing/oscillator/main.c)
  dspic33ep_mu_add_test(
    output_compare_basic
    tests/device/dspic33ep_mu/control/output_compare_basic_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    output_compare_boundary
    tests/device/dspic33ep_mu/control/output_compare/boundary_test.c
    "device;unit")
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_output_compare_boundary_test)
  dspic33ep_mu_add_test(
    output_compare_register_matrix
    tests/device/dspic33ep_mu/control/output_compare/register_matrix_test.c
    "device;unit")
  dspic33ep_mu_add_test(
    output_compare_fault
    tests/device/dspic33ep_mu/control/output_compare_fault_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    output_compare_cascade
    tests/device/dspic33ep_mu/control/output_compare_cascade_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    output_compare_power
    tests/device/dspic33ep_mu/control/output_compare_power_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    output_compare_sync
    tests/device/dspic33ep_mu/control/output_compare_sync/source.c
    "device;integration"
    tests/device/dspic33ep_mu/control/output_compare_sync/synchronization.c
    tests/device/dspic33ep_mu/control/output_compare_sync/main.c)
  dspic33ep_mu_add_test(
    output_compare_state_census
    tests/device/dspic33ep_mu/control/output_compare/state_census_test.c
    "device;unit;integration")
  dspic33ep_mu_add_test(
    pmp tests/device/dspic33ep_mu/communication/pmp/registers.c
    "device;integration" tests/device/dspic33ep_mu/communication/pmp/master.c
    tests/device/dspic33ep_mu/communication/pmp/slave.c
    tests/device/dspic33ep_mu/communication/pmp/boundary.c
    tests/device/dspic33ep_mu/communication/pmp/state_matrix.c)
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_pmp_test)
  dspic33ep_mu_add_test(pps tests/device/dspic33ep_mu/platform/pps_test.c
                        "device;integration")
  dspic33ep_mu_add_test(
    pwm tests/device/dspic33ep_mu/control/pwm/configuration.c
    "device;integration" tests/device/dspic33ep_mu/control/pwm/waveforms.c)
  dspic33ep_mu_add_test(
    qei tests/device/dspic33ep_mu/control/qei/registers.c "device;integration"
    tests/device/dspic33ep_mu/control/qei/events.c)
  dspic33ep_mu_add_test(reset tests/device/dspic33ep_mu/platform/reset_test.c
                        "device;integration")
  dspic33ep_mu_add_test(rtcc tests/device/dspic33ep_mu/timing/rtcc_test.c
                        "device;integration")
  dspic33ep_mu_add_test(
    sfr_access tests/device/dspic33ep_mu/platform/sfr_access_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    sfr_reset tests/device/dspic33ep_mu/platform/sfr_reset_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    spi tests/device/dspic33ep_mu/communication/spi/registers.c
    "device;integration" tests/device/dspic33ep_mu/communication/spi/master.c
    tests/device/dspic33ep_mu/communication/spi/slave.c
    tests/device/dspic33ep_mu/communication/spi/boundary.c
    tests/device/dspic33ep_mu/communication/spi/state_matrix.c)
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_spi_test)
  dspic33ep_mu_add_test(timer tests/device/dspic33ep_mu/timing/timer_test.c
                        "device;integration")
  dspic33ep_mu_add_test(
    uart tests/device/dspic33ep_mu/communication/uart_test.c
    "device;integration" tests/device/dspic33ep_mu/communication/uart/boundary.c
    tests/device/dspic33ep_mu/communication/uart/state_matrix.c)
  dspic33ep_mu_enable_allocation_failure(dspic33ep_mu_uart_test)
  dspic33ep_mu_add_test(
    usb tests/device/dspic33ep_mu/communication/usb/registers.c
    "device;integration"
    tests/device/dspic33ep_mu/communication/usb/transactions.c
    tests/device/dspic33ep_mu/communication/usb/boundary.c)
  dspic33ep_mu_add_test(
    watchdog tests/device/dspic33ep_mu/timing/watchdog_test.c
    "device;integration")
  dspic33ep_mu_add_test(firmware_image tests/system/firmware_image_test.c
                        "runner;system")
  target_link_libraries(dspic33ep_mu_firmware_image_test
                        PRIVATE dspic33ep_mu_firmware_image)

  add_test(
    NAME dspic33ep_mu_firmware_runner_smoke_test
    COMMAND
      dspic33ep_mu_firmware_runner
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/system/runner_smoke.bin --reset-address
      0 --device dsPIC33EP256MU806 --program-word 0 0x2005a0 --program-word 2
      0xfe4000 --stop-address 2 --max-instructions 10)
  set_tests_properties(
    dspic33ep_mu_firmware_runner_smoke_test
    PROPERTIES LABELS "runner;system" PASS_REGULAR_EXPRESSION "W0=0x005a")

endif()
