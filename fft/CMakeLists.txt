project(fft C)
add_library(fft)

target_sources(fft
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/fft.c
)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/fft.c
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")

target_include_directories(fft
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

