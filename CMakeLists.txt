cmake_minimum_required(VERSION 3.28)

project(haremasim VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/Dependencies.cmake)

# Commit of the haremasim sources, reported by `haremasim --version`.
execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE HAREMASIM_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT HAREMASIM_GIT_COMMIT)
    set(HAREMASIM_GIT_COMMIT "unknown")
endif()

add_subdirectory(src/sir)
add_subdirectory(src/kernel)
add_subdirectory(src/valsem)
add_subdirectory(src/analysis)
add_subdirectory(src/vm)
add_subdirectory(src/codegen)
add_subdirectory(src/front)
add_subdirectory(src/driver)

enable_testing()
add_subdirectory(tests)
