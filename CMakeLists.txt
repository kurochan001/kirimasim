# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.28)

project(kirimasim VERSION 0.0.0 LANGUAGES C CXX)

# kirimasim is written in C++23 and uses no C++26 feature. The nightly job
# cxx26 also builds it with -DCMAKE_CXX_STANDARD=26, to see when it can move
# to C++26; no other standard is accepted. The kernel-header and FST-interface
# checks use C++20 (tests/).
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
elseif(NOT CMAKE_CXX_STANDARD MATCHES "^(23|26)$")
    message(FATAL_ERROR "kirimasim is built as C++23 (CMAKE_CXX_STANDARD 23, "
                        "or 26 for the nightly check), not ${CMAKE_CXX_STANDARD}")
endif()
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

# Warnings for kirimasim's own code; the dependencies above keep their own
# flags. An aggregate whose later members have default member initializers
# is often written without them, so that is not warned of. With
# KIRIMASIM_WERROR the warnings are errors: CI's clang builds and
# tools/local_ci.py set it, while GCC builds only show them.
option(KIRIMASIM_WERROR "Treat warnings in kirimasim's own code as errors" OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
    # No fused multiply-add, so that a real gives the same bits on every OS
    # and tier (CODEGEN_SPEC.md "Floating point"). tools/check_release_flags.py
    # checks it.
    add_compile_options(-ffp-contract=off)
    if(KIRIMASIM_WERROR)
        add_compile_options(-Werror)
    endif()
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
