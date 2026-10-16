cmake_minimum_required(VERSION 3.28)

project(kirimasim VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# kirimasim does not use C++20 modules. Without this, CMake 3.28+ makes Clang
# scan for module dependencies and puts @*.modmap response files, which exist
# only after a full build, into compile_commands.json; clang-tidy then fails.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(cmake/Dependencies.cmake)

# Commit of the kirimasim sources, reported by `kirimasim --version`.
execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE KIRIMASIM_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT KIRIMASIM_GIT_COMMIT)
    set(KIRIMASIM_GIT_COMMIT "unknown")
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
