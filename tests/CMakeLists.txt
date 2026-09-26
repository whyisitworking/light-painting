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
