cmake_minimum_required(VERSION 3.20)

project(dspic33_sim LANGUAGES C)

include(CTest)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE
        Release
        CACHE STRING "Build type" FORCE
    )
endif()

add_library(dspic33_simulator STATIC src/device.c src/dspic33.c src/i2c.c)
add_library(dspic33::simulator ALIAS dspic33_simulator)

add_executable(
    dspic33_firmware_runner
    src/elf_image.c
    src/firmware_image.c
    src/firmware_runner.c
    src/hex_image.c
    src/json.c
    src/main.c
    src/scenario_stream.c
)
add_executable(dspic33::firmware_runner ALIAS dspic33_firmware_runner)

add_executable(dspic33_event_conformance conformance/event_scheduler.c)
add_executable(dspic33_processor_conformance conformance/processor.c)

add_executable(dspic33_dma_conformance conformance/dma.c)
add_executable(dspic33_timer_conformance conformance/timer.c)
add_executable(dspic33_adc_conformance conformance/adc.c)
add_executable(dspic33_pwm_conformance conformance/pwm.c)
add_executable(dspic33_spi_conformance conformance/spi.c)
add_executable(dspic33_can_conformance conformance/can.c)
add_executable(dspic33_usb_conformance conformance/usb.c)
add_executable(dspic33_uart_conformance conformance/uart.c)
add_executable(dspic33_i2c_conformance conformance/i2c.c)
add_executable(
    dspic33_nvm_conformance conformance/nvm.c src/elf_image.c src/hex_image.c
)
add_executable(dspic33_crc_conformance conformance/crc.c)
add_executable(dspic33_pmp_conformance conformance/pmp.c)
add_executable(dspic33_input_capture_conformance conformance/input_capture.c)
add_executable(dspic33_output_compare_conformance conformance/output_compare.c)
add_executable(dspic33_comparator_conformance conformance/comparator.c)
add_executable(dspic33_rtcc_conformance conformance/rtcc.c)
add_executable(dspic33_qei_conformance conformance/qei.c)
add_executable(dspic33_dci_conformance conformance/dci.c)
add_executable(dspic33_gpio_conformance conformance/gpio.c)
add_executable(dspic33_pps_conformance conformance/pps.c)
add_executable(
    dspic33_auxiliary_clock_conformance conformance/auxiliary_clock.c
)
add_executable(dspic33_oscillator_conformance conformance/oscillator.c)
add_executable(dspic33_watchdog_conformance conformance/watchdog.c)
add_executable(
    dspic33_interrupt_control_conformance conformance/interrupt_control.c
)
add_executable(dspic33_core_sfr_conformance conformance/core_sfr.c)
add_executable(dspic33_reset_conformance conformance/reset.c)
add_executable(dspic33_sfr_reset_conformance conformance/sfr_reset.c)
add_executable(dspic33_sfr_access_conformance conformance/sfr_access.c)

target_include_directories(
    dspic33_simulator
    PUBLIC src
    PRIVATE generated
)
set(dspic33_conformance_targets
    dspic33_event_conformance
    dspic33_processor_conformance
    dspic33_dma_conformance
    dspic33_timer_conformance
    dspic33_adc_conformance
    dspic33_pwm_conformance
    dspic33_spi_conformance
    dspic33_can_conformance
    dspic33_usb_conformance
    dspic33_uart_conformance
    dspic33_i2c_conformance
    dspic33_nvm_conformance
    dspic33_crc_conformance
    dspic33_pmp_conformance
    dspic33_input_capture_conformance
    dspic33_output_compare_conformance
    dspic33_comparator_conformance
    dspic33_rtcc_conformance
    dspic33_qei_conformance
    dspic33_dci_conformance
    dspic33_gpio_conformance
    dspic33_pps_conformance
    dspic33_auxiliary_clock_conformance
    dspic33_oscillator_conformance
    dspic33_watchdog_conformance
    dspic33_interrupt_control_conformance
    dspic33_core_sfr_conformance
    dspic33_reset_conformance
    dspic33_sfr_reset_conformance
    dspic33_sfr_access_conformance
)

target_link_libraries(dspic33_firmware_runner PRIVATE dspic33_simulator)
foreach(target IN LISTS dspic33_conformance_targets)
    target_link_libraries(${target} PRIVATE dspic33_simulator)
endforeach()
target_include_directories(dspic33_processor_conformance PRIVATE generated)
target_include_directories(dspic33_sfr_access_conformance PRIVATE generated)

set(dspic33_targets dspic33_simulator dspic33_firmware_runner
                    ${dspic33_conformance_targets}
)

foreach(target IN LISTS dspic33_targets)
    target_compile_features(${target} PRIVATE c_std_11)
endforeach()

if(BUILD_TESTING)
    foreach(target IN LISTS dspic33_conformance_targets)
        add_test(NAME ${target} COMMAND ${target})
    endforeach()
endif()

if(MSVC)
    foreach(target IN LISTS dspic33_targets)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endforeach()
else()
    foreach(target IN LISTS dspic33_targets)
        target_compile_options(
            ${target} PRIVATE -Wall -Wextra -Wpedantic -Werror
        )
    endforeach()
endif()

if(WIN32)
    target_link_libraries(dspic33_firmware_runner PRIVATE advapi32)
endif()
