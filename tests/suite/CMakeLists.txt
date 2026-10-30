# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Each .sv file under tests/suite becomes one CTest test (tools/run_suite_test.py).
# Simulation tests that use constructs not supported yet are reported as skipped.
set(KIRIMASIM_SUITE_DIAGNOSTICS "pinned" CACHE STRING "Test-suite frontend diagnostic profile")
set_property(CACHE KIRIMASIM_SUITE_DIAGNOSTICS PROPERTY STRINGS pinned slang-main)
if(NOT KIRIMASIM_SUITE_DIAGNOSTICS MATCHES "^(pinned|slang-main)$")
    message(FATAL_ERROR "KIRIMASIM_SUITE_DIAGNOSTICS must be pinned or slang-main")
endif()
file(GLOB_RECURSE suite_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.sv)
foreach(sv ${suite_files})
    file(RELATIVE_PATH rel ${CMAKE_CURRENT_SOURCE_DIR} ${sv})
    string(REGEX REPLACE "\\.sv$" "" test_name "${rel}")
    add_test(NAME suite/${test_name}
        COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/run_suite_test.py
            --kirimasim $<TARGET_FILE:kirimasim>
            --diagnostic-profile ${KIRIMASIM_SUITE_DIAGNOSTICS} ${sv})
    set_tests_properties(suite/${test_name} PROPERTIES LABELS Suite SKIP_RETURN_CODE 77)
    # The matrix of nesting 1000 and 10000 deep (gen_nesting_depth_matrix.py):
    # with the fake stacks of AddressSanitizer's check of use after return,
    # on by default with GCC on Linux, each of the 10000 frames is slow to
    # make, and a test took more than its 300 seconds. The check stays on
    # for every other test.
    if(test_name MATCHES "^proc/nesting_depth_")
        set_tests_properties(suite/${test_name} PROPERTIES
            ENVIRONMENT "ASAN_OPTIONS=detect_stack_use_after_return=0")
    endif()
endforeach()
