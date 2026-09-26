add_library(features)

target_sources(features
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/features.c)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/features.c
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")

target_include_directories(features
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})
