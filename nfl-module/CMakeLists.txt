cmake_minimum_required(VERSION 3.28)
project(nfl-aflnet C)
set(CMAKE_C_STANDARD 17)

# netfuzzlib's CMakeLists includes its helper module via CMAKE_SOURCE_DIR, which
# points at this project when it is pulled in with add_subdirectory. Put its cmake
# dir on the module path so include(nfl_helpers) resolves.
list(APPEND CMAKE_MODULE_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/netfuzzlib/cmake)

# Pull in netfuzzlib from the submodule (provides netfuzzlib_static + headers).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/netfuzzlib
                 ${CMAKE_BINARY_DIR}/netfuzzlib EXCLUDE_FROM_ALL)

add_library(nfl-aflnet SHARED
        callbacks.c
        config.c
        queue_logic.c
        afl_init.c
        child_tracker.c)

target_include_directories(nfl-aflnet PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_compile_definitions(nfl-aflnet PRIVATE _GNU_SOURCE)

target_link_libraries(nfl-aflnet PRIVATE
        netfuzzlib_headers
        -Wl,--whole-archive netfuzzlib_static -Wl,--no-whole-archive
        ${CMAKE_DL_LIBS})
