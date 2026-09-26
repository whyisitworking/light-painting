add_library(effects)

target_sources(effects
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/effects.c
        ${CMAKE_CURRENT_SOURCE_DIR}/palette.c)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/effects.c
    ${CMAKE_CURRENT_SOURCE_DIR}/palette.c
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")

target_include_directories(effects
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(effects features util)
