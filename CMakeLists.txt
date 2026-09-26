cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)

if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico2 CACHE STRING "Board Type")

include(pico_sdk_import.cmake)

project(light-painting C CXX ASM)
# Warnings for our own sources only, as errors. Pico SDK libraries are
# INTERFACE libraries whose sources compile inside our targets, so this is set
# per source file (set_source_files_properties) rather than per target
set(PROJECT_WARNINGS
    -Wall
    -Wextra
    -Werror
    -Wno-format
    -Wno-unused-function
    -Wno-maybe-uninitialized
    -Wpointer-arith
    -Wcast-align)
add_link_options(
    -Wall
    -Wextra
    -Werror
    -flto)
add_definitions(
    -DAUDIO_ENVELOPE)

# Opt-in, e.g. -DWAIT_FOR_USB_HOST=ON: at startup, wait up to 2 s for a USB
# serial host so the startup and error messages can be read. Leave it off for
# demos, the lights then start right away
option(WAIT_FOR_USB_HOST "Wait up to 2 s for a USB serial host at startup" OFF)
if (WAIT_FOR_USB_HOST)
    add_compile_definitions(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=2000)
endif()

pico_sdk_init()

add_executable(light-painting)

add_subdirectory(util)
add_subdirectory(fft)
add_subdirectory(swapchain)
add_subdirectory(drivers)
add_subdirectory(audio)

target_sources(light-painting
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/main.c
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")
target_link_libraries(light-painting
    PRIVATE
        i2s
        neopixel
        util
        audio
        swapchain
        pico_stdlib)

pico_add_extra_outputs(light-painting)
pico_enable_stdio_usb(light-painting ON)
pico_enable_stdio_uart(light-painting OFF)