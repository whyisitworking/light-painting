add_library(swapchain)

target_sources(swapchain
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/swapchain.c)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/swapchain.c
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")

target_include_directories(swapchain
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})