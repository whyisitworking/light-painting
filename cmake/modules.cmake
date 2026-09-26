# Module definitions shared by the firmware (CMakeLists.txt) and the host
# tests (tests/CMakeLists.txt), so that both build every module the same way
include_guard(GLOBAL)

# Warnings for our own sources only, as errors. Pico SDK libraries are
# INTERFACE libraries whose sources compile inside our targets, so this is set
# per source file (set_source_files_properties) rather than per target
set(PROJECT_WARNINGS
    -Wall
    -Wextra
    -Werror
    -Wno-unused-function
    -Wpointer-arith
    -Wcast-align)
# GCC only, clang rejects unknown warning options under -Werror
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    list(APPEND PROJECT_WARNINGS -Wno-maybe-uninitialized)
endif()

# lp_add_module(<name> SOURCES <file>... [DEPS <target>...]
#               [OPTIONS <flag>...] [MATH])
#
# A static library of the sources in the calling directory, which is also its
# public include directory. OPTIONS are extra compile flags for its sources,
# MATH links libm on the host (the firmware toolchain provides it)
function(lp_add_module name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "MATH" "" "SOURCES;DEPS;OPTIONS")

    list(TRANSFORM ARG_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

    add_library(${name} STATIC ${ARG_SOURCES})
    target_include_directories(${name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PUBLIC ${ARG_DEPS})
    set_source_files_properties(${ARG_SOURCES}
        PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS};${ARG_OPTIONS}")

    if (ARG_MATH AND NOT CMAKE_CROSSCOMPILING)
        target_link_libraries(${name} PUBLIC m)
    endif()
endfunction()
