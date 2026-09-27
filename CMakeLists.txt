cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
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

# The Waveshare RP2350-LCD-1.47-A, from our own header in boards/
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)
set(PICO_BOARD waveshare_rp2350_lcd_1.47 CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(light-painting C CXX ASM)
include(cmake/modules.cmake)

# Opt-in, e.g. -DWAIT_FOR_USB_HOST=ON: at startup, wait up to 2 s for a USB
# serial host so the startup and error messages can be read. Leave it off for
# demos, the lights then start right away
option(WAIT_FOR_USB_HOST "Wait up to 2 s for a USB serial host at startup" OFF)
if (WAIT_FOR_USB_HOST)
    add_compile_definitions(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=2000)
endif()

pico_sdk_init()

# For all our targets. The SDK adds these through its INTERFACE libraries,
# which lib/ does not link: without them the portable modules keep their
# unused functions and cannot inline sqrtf and friends as FPU instructions
if (NOT PICO_NO_GC_SECTIONS)
    add_compile_options(-ffunction-sections -fdata-sections)
endif()
add_compile_options(-fno-math-errno)

# The firmware runs from RAM (copy_to_ram below), so core 0 never reads the
# flash and can keep running while core 1 erases or programs it. pico_flash
# compiles into every target that links it, hence global. Core 1 gets its
# own stack in main RAM: the SDK's default one sits in the 4 KB SCRATCH_X
add_compile_definitions(
    PICO_FLASH_ASSUME_CORE0_SAFE=1
    PICO_CORE1_STACK_SIZE=0)

add_executable(light-painting)

# lib/ is portable, platform/ is the Pico hardware, app/ puts them together
add_subdirectory(lib)
add_subdirectory(platform)

set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/perf.c)
target_sources(light-painting PRIVATE ${APP_SOURCES})
set_source_files_properties(${APP_SOURCES}
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")
# Quoted includes only, like the modules (see lp_add_module)
target_compile_options(light-painting
    PRIVATE
        -iquote${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(light-painting
    PRIVATE
        i2s
        ws2812
        visualizer
        pico_stdlib)

# Opt-in, e.g. -DPERF_STATS=ON: print main loop timings and audio/LED
# counters over USB about once per second
option(PERF_STATS "Print performance statistics once per second" OFF)
if (PERF_STATS)
    target_compile_definitions(light-painting PRIVATE PERF_STATS)
endif()

# Copied from flash into RAM at boot, code and read-only data alike
pico_set_binary_type(light-painting copy_to_ram)
target_link_options(light-painting PRIVATE -Wl,--print-memory-usage)

pico_add_extra_outputs(light-painting)
pico_enable_stdio_usb(light-painting ON)
pico_enable_stdio_uart(light-painting OFF)