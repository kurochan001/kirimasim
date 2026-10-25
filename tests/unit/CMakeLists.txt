# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(GoogleTest)

add_executable(kirimasim_unit_tests managed_event_runtime_test.cpp managed_event_test.cpp activation_test.cpp activation_scheduler_test.cpp activation_sir_test.cpp activation_environment_test.cpp scoped_reference_test.cpp and4_test.cpp array_port_sir_test.cpp context_test.cpp delayed_side_test.cpp container_sir_test.cpp counters_test.cpp dump_test.cpp format_test.cpp heap_test.cpp heap_conversion_test.cpp pattern_test.cpp pattern_sir_test.cpp scan_test.cpp scheduler_test.cpp sir_test.cpp verify_matrix_test.cpp strength_test.cpp timescale_test.cpp
    storage_width_test.cpp strength_drivers_test.cpp strength_network_sir_test.cpp strength_selection_wait_test.cpp strength_store_test.cpp strength_value_test.cpp strength_sir_test.cpp mos_test.cpp event_activation_test.cpp event_sir_test.cpp vm_event_test.cpp wide_strength_sir_test.cpp wide_udp_sir_test.cpp udp_test.cpp urandom_test.cpp valsem_test.cpp vm_block_argument_test.cpp vm_container_test.cpp vm_frame_value_test.cpp vm_activation_test.cpp w4ops_test.cpp wide_test.cpp)
target_include_directories(kirimasim_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/vm)
target_link_libraries(kirimasim_unit_tests PRIVATE
    kirimasim_vm kirimasim_kernel kirimasim_sir kirimasim_valsem GTest::gtest_main)
gtest_discover_tests(kirimasim_unit_tests)

# The kernel's public headers are read by generated code, which is C++20.
# This target fails to build if any of them needs a newer standard.
add_library(kirimasim_kernel_headers_cxx20 OBJECT kernel_headers_cxx20.cpp)
target_link_libraries(kirimasim_kernel_headers_cxx20 PRIVATE kirimasim_kernel)
set_target_properties(kirimasim_kernel_headers_cxx20 PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
