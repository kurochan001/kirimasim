# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_test(NAME cli/clocking
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_clocking_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/clocking PROPERTIES LABELS Cli)

add_test(NAME cli/dynamic_containers
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_dynamic_container_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/dynamic_containers PROPERTIES LABELS Cli)

add_test(NAME cli/queue_write_warnings
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_queue_write_warning_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/queue_write_warnings PROPERTIES LABELS Cli)

add_test(NAME cli/delayed_nba
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_delayed_nba_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/delayed_nba PROPERTIES LABELS Cli)

add_test(NAME cli/program
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_program_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/program PROPERTIES LABELS Cli)

add_test(NAME cli/binary_scan
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_binary_scan_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/binary_scan PROPERTIES LABELS Cli)

add_test(NAME cli/binary_format
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_binary_format_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/binary_format PROPERTIES LABELS Cli)

add_test(NAME cli/autoflush
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_autoflush_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/autoflush PROPERTIES LABELS Cli)

add_test(NAME cli/append_fifo
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_append_fifo_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/append_fifo PROPERTIES LABELS Cli SKIP_RETURN_CODE 77 TIMEOUT 60)

add_test(NAME cli/stop
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_stop_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/stop PROPERTIES LABELS Cli)

add_test(NAME cli/wall_time
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_wall_time_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/wall_time PROPERTIES LABELS Cli)

add_test(NAME cli/bounds_check
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_bounds_check_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/bounds_check PROPERTIES LABELS Cli)

add_test(NAME cli/bits_query
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_bits_query_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/bits_query PROPERTIES LABELS Cli)

add_test(NAME cli/runtime_array_query
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_runtime_array_query_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/runtime_array_query PROPERTIES LABELS Cli)

add_test(NAME cli/struct_events
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_struct_event_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/struct_events PROPERTIES LABELS Cli)

add_test(NAME cli/cond_x
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_cond_x_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/cond_x PROPERTIES LABELS Cli)

add_test(NAME cli/runtime_string_format
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_runtime_string_format_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/runtime_string_format PROPERTIES LABELS Cli)

add_test(NAME cli/runtime_plusargs
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_runtime_plusargs_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/runtime_plusargs PROPERTIES LABELS Cli)

add_test(NAME cli/runtime_scan
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_runtime_scan_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/runtime_scan PROPERTIES LABELS Cli)

# Command-line tests: options, exit codes and messages (CLI.md).
add_test(NAME cli/slang_warnings
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_slang_warning_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/slang_warnings PROPERTIES LABELS Cli)

add_test(NAME cli/options
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_cli_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/options PROPERTIES LABELS Cli)

add_test(NAME cli/common_input
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_common_input_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/common_input PROPERTIES LABELS Cli)

add_test(NAME cli/mintypmax
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_mintypmax_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/mintypmax PROPERTIES LABELS Cli)

# The default of --max-slot-events takes seconds to reach (CLI.md 2.6:
# about 7 in release and 32 in asan), so its case runs in the release
# configuration only.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME cli/slot_event_default
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_cli_tests.py
            --kirimasim $<TARGET_FILE:kirimasim> --slot-event-default)
    set_tests_properties(cli/slot_event_default PROPERTIES LABELS Cli)
endif()

add_test(NAME cli/process_limit
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_process_limit_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/process_limit PROPERTIES LABELS Cli)

add_test(NAME cli/init_values
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_init_values_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/init_values PROPERTIES LABELS Cli)

add_test(NAME cli/report_pulses
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_report_pulses_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/report_pulses PROPERTIES LABELS Cli)

add_test(NAME cli/mixed_foreach
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_mixed_foreach_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/mixed_foreach PROPERTIES LABELS Cli)

add_test(NAME cli/struct_array_members
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_struct_array_member_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/struct_array_members PROPERTIES LABELS Cli)

add_test(NAME cli/event_nba_ref_array
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_event_nba_ref_array_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/event_nba_ref_array PROPERTIES LABELS Cli)

add_test(NAME cli/event_nba_ref_window
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_event_nba_ref_window_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/event_nba_ref_window PROPERTIES LABELS Cli)

add_test(NAME cli/delayed_ref_nba
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_delayed_ref_nba_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/delayed_ref_nba PROPERTIES LABELS Cli)

add_test(NAME cli/delayed_ref_selection
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_delayed_ref_selection_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/delayed_ref_selection PROPERTIES LABELS Cli)

add_test(NAME cli/nba_arena
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_nba_arena_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/nba_arena PROPERTIES LABELS Cli)

add_test(NAME cli/ref_nba_selection
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_ref_nba_selection_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/ref_nba_selection PROPERTIES LABELS Cli)
