cmake_minimum_required(VERSION 3.20)

set(OPENTEC_TOOLCHAIN xc16)
set(OPENTEC_PROCESSOR dsPIC33E)
set(OPENTEC_DEVICE 33EP512MU810)

include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/FirmwareToolchain.cmake")
project(dspic33-conformance LANGUAGES C ASM)

set(device_linker_script
    "${toolchain_bin}/../support/dsPIC33E/gld/p33EP512MU810.gld"
)

add_executable(
    dspic33-conformance
    source/arithmetic.s
    source/bits.s
    source/branches.s
    source/divides.s
    source/main.c
    source/extensions.s
    source/moves.s
    source/multiplies.s
    source/shifts.s
)

set_target_properties(dspic33-conformance PROPERTIES SUFFIX ".bin")
target_compile_features(dspic33-conformance PRIVATE c_std_99)
target_include_directories(dspic33-conformance PRIVATE source)
target_compile_options(
    dspic33-conformance
    PRIVATE -Wall
            -Werror
            -O1
            -mcpu=${OPENTEC_DEVICE}
            -mlarge-code
            -mlarge-data
            -mlarge-arrays
)
target_link_options(
    dspic33-conformance
    PRIVATE
    -mcpu=${OPENTEC_DEVICE}
    -mlarge-code
    -mlarge-data
    -mlarge-arrays
    -no-legacy-libc
    "-T${device_linker_script}"
    -Wl,--local-stack
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/dspic33-conformance.map"
)
