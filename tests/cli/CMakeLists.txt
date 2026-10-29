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
add_test(NAME cli/options
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_cli_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/options PROPERTIES LABELS Cli)

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
