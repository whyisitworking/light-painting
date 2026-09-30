# Host-side unit tests for the portable modules in lib/, built from the same
# module definitions as the firmware with the native compiler, independent of
# the Pico SDK:
#   cmake -S tests -B build-tests
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure
cmake_minimum_required(VERSION 3.25)

project(light-painting-tests C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
# For the editor (clangd, see .clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules.cmake)

# Test sources themselves: warnings, not errors
add_compile_options(
    -Wall
    -Wextra)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib lib)

enable_testing()

# The swapchain test runs its two sides on two threads
find_package(Threads REQUIRED)

# lp_add_test(<name> <target>...): test_<name>.c linked with the targets. A
# hang (e.g. an endless loop) fails quickly, not at ctest's default timeout
function(lp_add_test name)
    add_executable(test_${name} test_${name}.c)
    target_link_libraries(test_${name} PRIVATE ${ARGN})
    add_test(NAME ${name} COMMAND test_${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 30)
endfunction()

lp_add_test(fft fft)
lp_add_test(swapchain swapchain Threads::Threads)
lp_add_test(color color)
lp_add_test(spectrum spectrum)
lp_add_test(features features)
lp_add_test(palette effects)
lp_add_test(effects effects)
lp_add_test(layers effects)
lp_add_test(modes effects)
lp_add_test(visualizer visualizer)
lp_add_test(golden visualizer)
lp_add_test(latency visualizer)
lp_add_test(stats stats)
lp_add_test(settings settings)
lp_add_test(song song)
lp_add_test(parts spectrum features song)
lp_add_test(scene show)
lp_add_test(blocks show)
lp_add_test(rules show)
lp_add_test(show show)

# The preview tool's engine (tools/preview): the pipeline for every mode at
# once, also compiled to WebAssembly by tools/preview/build.sh
set(PREVIEW_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../tools/preview/preview_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../app/ui/ui_names.c)
add_library(preview_api STATIC ${PREVIEW_SOURCES})
set_source_files_properties(${PREVIEW_SOURCES}
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")
target_compile_options(preview_api PUBLIC
    -iquote${CMAKE_CURRENT_SOURCE_DIR}/../tools/preview
    -iquote${CMAKE_CURRENT_SOURCE_DIR}/../app/ui)
target_link_libraries(preview_api PUBLIC settings visualizer)
lp_add_test(preview preview_api)
