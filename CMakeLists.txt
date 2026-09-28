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
# own stack in main RAM: the SDK's default one sits in the 4 KB SCRATCH_X.
# The SDK's stack guard is not installed on either core: core 1 installs it
# on its own stack (app/ui/ui.c), with the function the define below asks
# the SDK to compile (2.3.1 compiles it anyway, the define says we rely on
# it). PICO_USE_STACK_GUARDS would also limit core 0
# to 2 KB (PICO_STACK_SIZE), where today it may use all 8 KB of SCRATCH_X
# and SCRATCH_Y, unmeasured
add_compile_definitions(
    PICO_FLASH_ASSUME_CORE0_SAFE=1
    PICO_CORE1_STACK_SIZE=0
    PICO_RUNTIME_NO_INIT_PER_CORE_INSTALL_STACK_GUARD=0)

add_executable(light-painting)

# lib/ is portable, platform/ is the Pico hardware, app/ puts them together
add_subdirectory(lib)
add_subdirectory(platform)

# LVGL for the menu on core 1, a git submodule pinned to v9.6.0. Its options
# are forced into the cache: under the old policies of LVGL's own
# cmake_minimum_required(), a plain variable would not override them.
# Neither its examples, demos nor ThorVG (vector graphics, C++) are needed
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/app/ui/lv_conf.h
    CACHE PATH "lv_conf.h" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
# Its headers are system headers to us: our warnings do not apply to them
add_subdirectory(third_party/lvgl SYSTEM)
# C11, the standard LVGL is tested with, rather than our C23
set_target_properties(lvgl PROPERTIES C_STANDARD 11)
# For size: it runs from RAM, and a menu needs no speed
target_compile_options(lvgl PRIVATE -Os)

set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app/diagnostics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/persist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/tuning_link.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/ui/ui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/ui/ui_menu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/ui/ui_names.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/ui/ui_port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/ui/ui_status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/ui/ui_theme.c)
target_sources(light-painting PRIVATE ${APP_SOURCES})
set_source_files_properties(${APP_SOURCES}
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")
# Quoted includes only, like the modules (see lp_add_module)
target_compile_options(light-painting
    PRIVATE
        -iquote${CMAKE_CURRENT_SOURCE_DIR}/app
        -iquote${CMAKE_CURRENT_SOURCE_DIR}/app/ui)
target_link_libraries(light-painting
    PRIVATE
        i2s
        ws2812
        joystick
        settings
        stats
        st7789
        storage
        visualizer
        lvgl
        pico_multicore
        pico_stdlib)

# Opt-in, e.g. -DPRINT_DIAGNOSTICS=ON: core 1 also prints the diagnostics
# over USB, every DIAGNOSTICS_PERIOD_MS (app/config.h), even without the LCD
option(PRINT_DIAGNOSTICS "Print the diagnostics over USB" OFF)
if (PRINT_DIAGNOSTICS)
    target_compile_definitions(light-painting PRIVATE PRINT_DIAGNOSTICS)
endif()

# Opt-in, e.g. -DBOOT_BUTTON_SHUFFLE=ON: each press of the BOOT button shows
# a random look, for demos
option(BOOT_BUTTON_SHUFFLE "BOOT shuffles the look" OFF)
if (BOOT_BUTTON_SHUFFLE)
    target_compile_definitions(light-painting PRIVATE BOOT_BUTTON_SHUFFLE)
    target_link_libraries(light-painting PRIVATE boot_button pico_rand)
endif()

# Copied from flash into RAM at boot, code and read-only data alike
pico_set_binary_type(light-painting copy_to_ram)
target_link_options(light-painting PRIVATE -Wl,--print-memory-usage)

pico_add_extra_outputs(light-painting)
pico_enable_stdio_usb(light-painting ON)
pico_enable_stdio_uart(light-painting OFF)