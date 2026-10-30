# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(GoogleTest)

add_executable(kirimasim_unit_tests vm_nba_event_test.cpp nba_sir_test.cpp nba_monitor_test.cpp vm_nba_origin_test.cpp nba_event_test.cpp nba_payload_test.cpp process_limit_test.cpp vm_event_gc_test.cpp event_transfers_test.cpp event_temporaries_test.cpp managed_event_runtime_test.cpp managed_event_test.cpp activation_test.cpp activation_scheduler_test.cpp activation_sir_test.cpp activation_environment_test.cpp scoped_reference_test.cpp and4_test.cpp array_port_sir_test.cpp context_test.cpp delayed_side_test.cpp container_sir_test.cpp counters_test.cpp dump_test.cpp format_test.cpp heap_test.cpp heap_conversion_test.cpp pattern_test.cpp pattern_sir_test.cpp scan_test.cpp scheduler_test.cpp sir_test.cpp verify_matrix_test.cpp strength_test.cpp timescale_test.cpp
    storage_width_test.cpp passive_sir_test.cpp passive_network_test.cpp strength_drivers_test.cpp strength_network_sir_test.cpp strength_selection_wait_test.cpp strength_store_test.cpp strength_value_test.cpp strength_sir_test.cpp mos_test.cpp event_activation_test.cpp event_sir_test.cpp vm_event_test.cpp wide_strength_sir_test.cpp wide_udp_sir_test.cpp udp_test.cpp urandom_test.cpp valsem_test.cpp vm_block_argument_test.cpp vm_container_test.cpp vm_frame_value_test.cpp vm_activation_test.cpp w4ops_test.cpp wide_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE clocking_test.cpp cond_x_test.cpp fixed_array_conditional_test.cpp query_validation_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE init_values_test.cpp init_values_sir_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE report_pulses_test.cpp report_pulses_vm_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE mixed_foreach_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE runtime_array_query_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE strength_force_value_test.cpp strength_force_runtime_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE struct_event_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE struct_array_member_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE event_nba_ref_array_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE event_nba_ref_window_test.cpp)
target_sources(kirimasim_unit_tests PRIVATE delayed_ref_nba_test.cpp)
target_include_directories(kirimasim_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/vm)
target_link_libraries(kirimasim_unit_tests PRIVATE
    kirimasim_front kirimasim_vm kirimasim_kernel kirimasim_sir kirimasim_valsem GTest::gtest_main)
gtest_discover_tests(kirimasim_unit_tests)

# The kernel's public headers are read by generated code, which is C++20.
# This target fails to build if any of them needs a newer standard.
add_library(kirimasim_kernel_headers_cxx20 OBJECT kernel_headers_cxx20.cpp)
target_link_libraries(kirimasim_kernel_headers_cxx20 PRIVATE kirimasim_kernel)
set_target_properties(kirimasim_kernel_headers_cxx20 PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
