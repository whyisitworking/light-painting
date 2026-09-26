# Host-side unit tests for the hardware independent modules.
# Built with the native compiler, independent of the Pico SDK:
#   cmake -S tests -B build-tests
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure
cmake_minimum_required(VERSION 3.25)

project(light-painting-tests C)

set(CMAKE_C_STANDARD 17)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_compile_options(
    -Wall
    -Wextra)

enable_testing()

add_library(fft_host STATIC ${ROOT}/fft/fft.c)
target_include_directories(fft_host PUBLIC ${ROOT}/fft)
target_link_libraries(fft_host PUBLIC m)

add_executable(test_fft test_fft.c)
target_link_libraries(test_fft fft_host)
add_test(NAME fft COMMAND test_fft)

add_library(swapchain_host STATIC ${ROOT}/swapchain/swapchain.c)
target_include_directories(swapchain_host PUBLIC ${ROOT}/swapchain)

add_executable(test_swapchain test_swapchain.c)
target_link_libraries(test_swapchain swapchain_host)
add_test(NAME swapchain COMMAND test_swapchain)

add_executable(test_color test_color.c)
target_include_directories(test_color PRIVATE ${ROOT}/util)
# color.h defines non-inline static helpers, as does the firmware build
target_compile_options(test_color PRIVATE -Wno-unused-function)
add_test(NAME color COMMAND test_color)

add_library(audio_host STATIC ${ROOT}/audio/audio.c)
target_include_directories(audio_host PUBLIC ${ROOT}/audio)
# Same as the firmware build
target_compile_definitions(audio_host PUBLIC AUDIO_ENVELOPE)
target_link_libraries(audio_host PUBLIC fft_host)

add_executable(test_audio test_audio.c)
target_link_libraries(test_audio audio_host)
add_test(NAME audio COMMAND test_audio)

# A hang (e.g. an endless loop) should fail quickly, not at ctest's default
set_tests_properties(fft swapchain color audio PROPERTIES TIMEOUT 30)
