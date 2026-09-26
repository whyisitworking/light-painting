project(fft C)
add_library(fft)

target_sources(fft
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/fft.c
)

target_include_directories(fft
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

