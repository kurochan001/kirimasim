# Command-line tests: options, exit codes and messages (CLI.md).
add_test(NAME cli/options
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_cli_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(cli/options PROPERTIES LABELS Cli)
