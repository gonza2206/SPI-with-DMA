# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
SET(DEVICE_ID 0x4b534352)
SET(BOOTLOADER_VERSION 1)
SET(SEAL_VERSION 1)
SET(HARDWARE_VERSION 1)

# Include build functions from Pico SDK

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

# Set name of project (as PROJECT_NAME) and C/C++ standards
project(Spi-with-DMA C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_CXX_ENABLE_RTTI 1)


# Tell CMake where to find the executable source file
set(SPIDMA keyscanner)

add_executable(${SPIDMA}
        main.cpp
        )

pico_set_program_name(${SPIDMA} "Spi-with-DMA")
pico_set_program_version(${SPIDMA} "0.1")

# Enable usb output, disable uart output
pico_enable_stdio_uart(${SPIDMA} 0)
pico_enable_stdio_usb(${SPIDMA} 0)

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(${SPIDMA}
        pico_stdlib
        pico_multicore
        hardware_exception
        hardware_dma
        hardware_flash
        hardware_sleep
        pico_unique_id
        hardware_spi
)
# Add any user requested libraries
#[[target_link_libraries(${SPIDMA}
)]]

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${SPIDMA})
add_definitions(-DKEYSCANNER)
target_compile_options(${SPIDMA} PRIVATE -O0)
target_compile_definitions(${SPIDMA} PUBLIC SPIDMA=1 FMW_VERSION=${VERSION})

if (CMAKE_BUILD_TYPE MATCHES Debug)
    #Change this to 0 if you want to debug
    pico_enable_stdio_usb(${SPIDMA} 1)

    math(EXPR FLASH_OFFSET_MAINPROGRAM "0")

    target_compile_definitions(${SPIDMA} PUBLIC
            DBG_PRINT_ENABLED
            DBG_PRINT_ERROR_ENABLED
            DBG_PRINT_TRACE_ENABLED)
endif ()