# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
