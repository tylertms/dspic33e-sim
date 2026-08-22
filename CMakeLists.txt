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

add_library(dspic33ep_mu_simulator STATIC src/device.c src/dspic33.c
                                          src/dspic33ep_mu_data.c src/i2c.c)
add_library(dspic33ep_mu::simulator ALIAS dspic33ep_mu_simulator)

add_library(dspic33ep_mu_firmware_image STATIC src/elf_image.c
                                               src/dspic33_firmware_image.c)
add_library(dspic33ep_mu::firmware_image ALIAS dspic33ep_mu_firmware_image)

add_executable(dspic33ep_mu_firmware_runner src/main.c)
add_executable(dspic33ep_mu::firmware_runner ALIAS dspic33ep_mu_firmware_runner)

target_include_directories(dspic33ep_mu_simulator PUBLIC src)
target_include_directories(dspic33ep_mu_firmware_image PUBLIC src)
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
    target_include_directories(${target} PRIVATE tests/support src)
    target_link_libraries(${target} PRIVATE dspic33ep_mu_simulator)
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4 /WX)
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}")
  endfunction()

  dspic33ep_mu_add_test(event_scheduler tests/core/event_scheduler_test.c
                        "core;integration")
  dspic33ep_mu_add_test(core_api tests/core/api_test.c "core;unit;integration")
  dspic33ep_mu_add_test(processor_fault tests/core/processor_fault_test.c
                        "core;integration")
  dspic33ep_mu_add_test(processor_timing tests/core/processor_timing_test.c
                        "core;integration")
  dspic33ep_mu_add_test(processor_data tests/core/processor_data_test.c
                        "core;integration")
  dspic33ep_mu_add_test(processor_control tests/core/processor_control_test.c
                        "core;integration")

  dspic33ep_mu_add_test(adc tests/device/adc_test.c "device;integration")
  dspic33ep_mu_add_test(auxiliary_clock tests/device/auxiliary_clock_test.c
                        "device;integration")
  dspic33ep_mu_add_test(can tests/device/can_test.c "device;integration")
  dspic33ep_mu_add_test(comparator tests/device/comparator_test.c
                        "device;integration")
  dspic33ep_mu_add_test(core_sfr tests/device/core_sfr_test.c
                        "device;integration")
  dspic33ep_mu_add_test(crc tests/device/crc_test.c "device;integration")
  dspic33ep_mu_add_test(dci tests/device/dci_test.c "device;integration")
  dspic33ep_mu_add_test(device_data tests/device/device_data_test.c
                        "device;unit")
  dspic33ep_mu_add_test(dma tests/device/dma_test.c "device;integration")
  dspic33ep_mu_add_test(gpio tests/device/gpio_test.c "device;integration")
  dspic33ep_mu_add_test(i2c tests/device/i2c_test.c "device;integration")
  dspic33ep_mu_add_test(input_capture tests/device/input_capture_test.c
                        "device;integration")
  dspic33ep_mu_add_test(interrupt_control tests/device/interrupt_control_test.c
                        "device;integration")
  dspic33ep_mu_add_test(nvm tests/device/nvm_test.c "device;integration")
  target_link_libraries(dspic33ep_mu_nvm_test
                        PRIVATE dspic33ep_mu_firmware_image)
  dspic33ep_mu_add_test(oscillator tests/device/oscillator_test.c
                        "device;integration")
  dspic33ep_mu_add_test(
    output_compare_basic tests/device/output_compare_basic_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    output_compare_fault tests/device/output_compare_fault_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    output_compare_cascade tests/device/output_compare_cascade_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    output_compare_power tests/device/output_compare_power_test.c
    "device;integration")
  dspic33ep_mu_add_test(
    output_compare_sync tests/device/output_compare_sync_test.c
    "device;integration")
  dspic33ep_mu_add_test(pmp tests/device/pmp_test.c "device;integration")
  dspic33ep_mu_add_test(pps tests/device/pps_test.c "device;integration")
  dspic33ep_mu_add_test(pwm tests/device/pwm_test.c "device;integration")
  dspic33ep_mu_add_test(qei tests/device/qei_test.c "device;integration")
  dspic33ep_mu_add_test(reset tests/device/reset_test.c "device;integration")
  dspic33ep_mu_add_test(rtcc tests/device/rtcc_test.c "device;integration")
  dspic33ep_mu_add_test(sfr_access tests/device/sfr_access_test.c
                        "device;integration")
  dspic33ep_mu_add_test(sfr_reset tests/device/sfr_reset_test.c
                        "device;integration")
  dspic33ep_mu_add_test(spi tests/device/spi_test.c "device;integration")
  dspic33ep_mu_add_test(timer tests/device/timer_test.c "device;integration")
  dspic33ep_mu_add_test(uart tests/device/uart_test.c "device;integration")
  dspic33ep_mu_add_test(usb tests/device/usb_test.c "device;integration")
  dspic33ep_mu_add_test(watchdog tests/device/watchdog_test.c
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
