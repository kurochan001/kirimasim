# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# The libFuzzer target of front and tier 1, built by the fuzz preset only
# (KIRIMASIM_FUZZ; CONTRIBUTING.md, "Building and testing"). It needs a
# clang with libFuzzer: an LLVM from Homebrew or apt.llvm.org, not Apple's.
find_package(Threads REQUIRED)

add_executable(kirimasim_fuzz_compile fuzz_compile.cpp)
target_link_libraries(kirimasim_fuzz_compile PRIVATE
    kirimasim_front kirimasim_vm kirimasim_kernel kirimasim_sir Threads::Threads)
target_link_options(kirimasim_fuzz_compile PRIVATE -fsanitize=fuzzer)

# Each test of tests/suite/ once through the target, as files given to it
# run once each: the inputs the nightly run starts from compile and run
# without a finding. What it finds goes to the build directory.
file(GLOB_RECURSE suite_sources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/suite/*.sv)
# Passing the whole growing suite in one argv exceeds macOS ARG_MAX. Batch
# positional files without turning the directory into a mutation corpus.
configure_file(run_fuzz_suite.cmake.in run_fuzz_suite.cmake @ONLY)
add_test(NAME fuzz_compile_suite
    COMMAND ${CMAKE_COMMAND} -DFUZZ_BINARY=$<TARGET_FILE:kirimasim_fuzz_compile>
        -DARTIFACT_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_BINARY_DIR}/run_fuzz_suite.cmake)
set_tests_properties(fuzz_compile_suite PROPERTIES LABELS Fuzz)
