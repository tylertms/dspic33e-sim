cmake_minimum_required(VERSION 3.20)

project(opentec_firmware_simulator LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE
        Release
        CACHE STRING "Build type" FORCE
    )
endif()

add_library(opentec_simulator_core STATIC src/device.c src/dspic33.c src/i2c.c)

add_executable(
    opentec_firmware_simulator
    src/elf_image.c
    src/firmware_image.c
    src/firmware_runner.c
    src/hex_image.c
    src/json.c
    src/main.c
    src/scenario_stream.c
)

add_executable(
    opentec_simulator_event_conformance conformance/event_scheduler.c
)
add_executable(opentec_simulator_processor_conformance conformance/processor.c)

add_executable(opentec_simulator_dma_conformance conformance/dma.c)
add_executable(opentec_simulator_timer_conformance conformance/timer.c)
add_executable(opentec_simulator_adc_conformance conformance/adc.c)
add_executable(opentec_simulator_pwm_conformance conformance/pwm.c)
add_executable(opentec_simulator_spi_conformance conformance/spi.c)
add_executable(opentec_simulator_can_conformance conformance/can.c)
add_executable(opentec_simulator_usb_conformance conformance/usb.c)
add_executable(opentec_simulator_uart_conformance conformance/uart.c)
add_executable(opentec_simulator_i2c_conformance conformance/i2c.c)
add_executable(opentec_simulator_nvm_conformance conformance/nvm.c)
add_executable(opentec_simulator_sfr_reset_conformance conformance/sfr_reset.c)
add_executable(
    opentec_simulator_sfr_access_conformance conformance/sfr_access.c
)

target_include_directories(opentec_simulator_core PUBLIC src)
target_link_libraries(opentec_firmware_simulator PRIVATE opentec_simulator_core)
target_link_libraries(
    opentec_simulator_event_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_processor_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_dma_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_timer_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_adc_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_pwm_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_spi_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_can_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_usb_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_uart_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_i2c_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_nvm_conformance PRIVATE opentec_simulator_core
)
target_link_libraries(
    opentec_simulator_sfr_reset_conformance PRIVATE opentec_simulator_core
)
target_include_directories(
    opentec_simulator_sfr_access_conformance PRIVATE generated
)
target_link_libraries(
    opentec_simulator_sfr_access_conformance PRIVATE opentec_simulator_core
)

set(opentec_simulator_targets
    opentec_simulator_core
    opentec_firmware_simulator
    opentec_simulator_event_conformance
    opentec_simulator_processor_conformance
    opentec_simulator_dma_conformance
    opentec_simulator_timer_conformance
    opentec_simulator_adc_conformance
    opentec_simulator_pwm_conformance
    opentec_simulator_spi_conformance
    opentec_simulator_can_conformance
    opentec_simulator_usb_conformance
    opentec_simulator_uart_conformance
    opentec_simulator_i2c_conformance
    opentec_simulator_nvm_conformance
    opentec_simulator_sfr_reset_conformance
    opentec_simulator_sfr_access_conformance
)

foreach(target IN LISTS opentec_simulator_targets)
    target_compile_features(${target} PRIVATE c_std_11)
endforeach()

if(MSVC)
    foreach(target IN LISTS opentec_simulator_targets)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endforeach()
else()
    foreach(target IN LISTS opentec_simulator_targets)
        target_compile_options(
            ${target} PRIVATE -Wall -Wextra -Wpedantic -Werror
        )
    endforeach()
endif()

if(WIN32)
    target_link_libraries(opentec_firmware_simulator PRIVATE advapi32)
endif()
