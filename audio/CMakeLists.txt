add_library(audio)

target_sources(audio
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/audio.c)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/audio.c
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")

target_include_directories(audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(audio fft pico_stdlib)
