if(NOT CMAKE_HOST_WIN32)
    message(FATAL_ERROR "XC16 firmware builds require Windows")
endif()

if(NOT CMAKE_GENERATOR MATCHES "^Ninja")
    message(FATAL_ERROR "XC16 firmware builds require Ninja")
endif()

if(NOT DSPIC33_SIM_PROCESSOR)
    message(FATAL_ERROR "DSPIC33_SIM_PROCESSOR is required")
endif()

if(DSPIC33_SIM_C_COMPILER AND NOT EXISTS "${DSPIC33_SIM_C_COMPILER}")
    unset(DSPIC33_SIM_C_COMPILER CACHE)
endif()

find_program(
    DSPIC33_SIM_C_COMPILER
    NAMES xc16-gcc.exe
    HINTS "C:/Program Files (x86)/Microchip/xc16/v1.35/bin"
          "C:/Program Files/Microchip/xc16/v1.35/bin"
)
if(NOT DSPIC33_SIM_C_COMPILER)
    message(
        FATAL_ERROR "Install Microchip XC16 v1.35 or add xc16-gcc.exe to PATH"
    )
endif()

get_filename_component(toolchain_bin "${DSPIC33_SIM_C_COMPILER}" DIRECTORY)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR "${DSPIC33_SIM_PROCESSOR}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER
    "${DSPIC33_SIM_C_COMPILER}"
    CACHE FILEPATH "" FORCE
)
set(CMAKE_ASM_COMPILER
    "${DSPIC33_SIM_C_COMPILER}"
    CACHE FILEPATH "" FORCE
)
