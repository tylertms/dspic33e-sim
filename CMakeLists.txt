cmake_minimum_required(VERSION 3.21)

project(dspic33ep_mu_sim LANGUAGES C)

if(PROJECT_IS_TOP_LEVEL)
  option(BUILD_TESTING "Build the simulator tests" ON)
  option(DSPIC33EP_MU_ENABLE_COVERAGE "Enable simulator source coverage" OFF)
  set(CMAKE_CTEST_ARGUMENTS --output-on-failure --parallel)
  set(CMAKE_SKIP_TEST_ALL_DEPENDENCY FALSE)
endif()

if(DSPIC33EP_MU_ENABLE_COVERAGE)
  if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Simulator source coverage requires GCC")
  endif()
  add_compile_options(--coverage)
  add_link_options(--coverage)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Build type" FORCE)
endif()

if(PROJECT_IS_TOP_LEVEL AND NOT DSPIC33EP_MU_ENABLE_COVERAGE)
  find_program(dspic33ep_mu_coverage_compiler gcc)
  find_program(dspic33ep_mu_gcov gcov)
  set(dspic33ep_mu_coverage_build_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/build/test-coverage")

  if(dspic33ep_mu_coverage_compiler AND dspic33ep_mu_gcov)
    add_custom_target(
      test-coverage
      COMMAND "${CMAKE_COMMAND}" -E rm -rf
              "${dspic33ep_mu_coverage_build_directory}"
      COMMAND
        "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_SOURCE_DIR}" -B
        "${dspic33ep_mu_coverage_build_directory}" -G Ninja
        -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
        -DDSPIC33EP_MU_ENABLE_COVERAGE=ON
        "-DCMAKE_C_COMPILER=${dspic33ep_mu_coverage_compiler}"
      COMMAND "${CMAKE_COMMAND}" --build
              "${dspic33ep_mu_coverage_build_directory}" --parallel
      COMMAND
        "${CMAKE_CTEST_COMMAND}" --test-dir
        "${dspic33ep_mu_coverage_build_directory}" --output-on-failure
        --parallel
      COMMAND
        "${CMAKE_COMMAND}"
        "-DCOVERAGE_BUILD_DIRECTORY=${dspic33ep_mu_coverage_build_directory}"
        "-DCOVERAGE_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DCOVERAGE_TARGET_PREFIX=dspic33ep_mu"
        "-DGCOV_EXECUTABLE=${dspic33ep_mu_gcov}" -P
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Coverage.cmake"
      COMMENT "Testing the dsPIC33EP-MU simulator with source coverage"
      USES_TERMINAL VERBATIM)
  else()
    add_custom_target(
      test-coverage
      COMMAND "${CMAKE_COMMAND}" -E echo
              "Simulator source coverage requires GCC and gcov"
      COMMAND "${CMAKE_COMMAND}" -E false
      USES_TERMINAL VERBATIM)
  endif()
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
function(dspic33ep_mu_enable_strict_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

set(dspic33ep_mu_targets dspic33ep_mu_simulator dspic33ep_mu_firmware_image
                         dspic33ep_mu_firmware_runner)
target_compile_definitions(
  dspic33ep_mu_firmware_image
  PRIVATE $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
foreach(simulator_target IN LISTS dspic33ep_mu_targets)
  dspic33ep_mu_enable_strict_warnings(${simulator_target})
endforeach()

include(tests/CMakeLists.txt)
