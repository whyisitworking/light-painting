project(fft C)
add_library(fft)

target_sources(fft
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/fft.c
)

# -fcx-limited-range: plain complex multiplications instead of __mulsc3 calls
# that handle NaN/Inf operands, which audio samples never are
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/fft.c
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS};-fcx-limited-range")

target_include_directories(fft
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

