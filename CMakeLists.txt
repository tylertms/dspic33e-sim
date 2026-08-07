cmake_minimum_required(VERSION 3.20)

project(opentec_firmware_simulator LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE
        Release
        CACHE STRING "Build type" FORCE
    )
endif()

add_executable(
    opentec_firmware_simulator
    src/device.c
    src/dspic33.c
    src/elf_image.c
    src/firmware_image.c
    src/firmware_runner.c
    src/hex_image.c
    src/json.c
    src/main.c
    src/scenario_stream.c
)

target_compile_features(opentec_firmware_simulator PRIVATE c_std_11)

if(MSVC)
    target_compile_options(opentec_firmware_simulator PRIVATE /W4 /WX)
else()
    target_compile_options(
        opentec_firmware_simulator PRIVATE -Wall -Wextra -Wpedantic -Werror
    )
endif()

if(WIN32)
    target_link_libraries(opentec_firmware_simulator PRIVATE advapi32)
endif()
