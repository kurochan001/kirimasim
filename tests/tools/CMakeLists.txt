# The judging tools must fail when they should (TEST_SUITE.md).
add_test(NAME tools/negative_controls
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_tool_tests.py
        --kirimasim $<TARGET_FILE:kirimasim>)
set_tests_properties(tools/negative_controls PROPERTIES LABELS Tools)

# Generated tests must match their generator (TEST_SUITE.md).
add_test(NAME tools/gen_net_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_net_matrix.py --check)
set_tests_properties(tools/gen_net_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_sign_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_sign_matrix.py --check)
set_tests_properties(tools/gen_sign_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_assign_op_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_assign_op_matrix.py --check)
set_tests_properties(tools/gen_assign_op_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_resolution_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_resolution_matrix.py --check)
set_tests_properties(tools/gen_resolution_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_violation_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_violation_matrix.py --check)
set_tests_properties(tools/gen_violation_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_range_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_range_matrix.py --check)
set_tests_properties(tools/gen_range_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_default_init_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_default_init_matrix.py --check)
set_tests_properties(tools/gen_default_init_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_scope_name_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_scope_name_matrix.py --check)
set_tests_properties(tools/gen_scope_name_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_aggregate_assign_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_aggregate_assign_matrix.py --check)
set_tests_properties(tools/gen_aggregate_assign_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_gate_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_gate_matrix.py --check)
set_tests_properties(tools/gen_gate_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_override_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_override_matrix.py --check)
set_tests_properties(tools/gen_override_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_fork_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_fork_matrix.py --check)
set_tests_properties(tools/gen_fork_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_argument_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_argument_matrix.py --check)
set_tests_properties(tools/gen_argument_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_udp_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_udp_matrix.py --check)
set_tests_properties(tools/gen_udp_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_delay_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_delay_matrix.py --check)
set_tests_properties(tools/gen_delay_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_alias_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_alias_matrix.py --check)
set_tests_properties(tools/gen_alias_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_driver_start_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_driver_start_matrix.py --check)
set_tests_properties(tools/gen_driver_start_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_wait_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_wait_matrix.py --check)
set_tests_properties(tools/gen_wait_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_delay_value_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_delay_value_matrix.py --check)
set_tests_properties(tools/gen_delay_value_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_param_array_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_param_array_matrix.py --check)
set_tests_properties(tools/gen_param_array_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_postponed_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_postponed_matrix.py --check)
set_tests_properties(tools/gen_postponed_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_disable_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_disable_matrix.py --check)
set_tests_properties(tools/gen_disable_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_deferred_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_deferred_matrix.py --check)
set_tests_properties(tools/gen_deferred_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_typename_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_typename_matrix.py --check)
set_tests_properties(tools/gen_typename_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_aggregate_call_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_aggregate_call_matrix.py --check)
set_tests_properties(tools/gen_aggregate_call_matrix PROPERTIES LABELS Tools)

# The benchmark designs still run, and tools/bench.py still measures them
# (PERFORMANCE.md 5). A small count keeps it quick.
add_test(NAME tools/bench_smoke
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/bench.py
        --kirimasim $<TARGET_FILE:kirimasim> --n 100 --runs 1
        --out ${CMAKE_CURRENT_BINARY_DIR}/bench_smoke)
set_tests_properties(tools/bench_smoke PROPERTIES LABELS Tools)

add_test(NAME tools/gen_program_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_program_matrix.py --check)
set_tests_properties(tools/gen_program_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_process_lifetime_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_process_lifetime_matrix.py --check)
set_tests_properties(tools/gen_process_lifetime_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_wide_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_wide_matrix.py --check)
set_tests_properties(tools/gen_wide_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_illegal_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_illegal_matrix.py --check)
set_tests_properties(tools/gen_illegal_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_logic_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_logic_matrix.py --check)
set_tests_properties(tools/gen_logic_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_loop_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_loop_matrix.py --check)
set_tests_properties(tools/gen_loop_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_real_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_real_matrix.py --check)
set_tests_properties(tools/gen_real_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_string_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_string_matrix.py --check)
set_tests_properties(tools/gen_string_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_string_format_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_string_format_matrix.py --check)
set_tests_properties(tools/gen_string_format_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_real_convert_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_real_convert_matrix.py --check)
set_tests_properties(tools/gen_real_convert_matrix PROPERTIES LABELS Tools)

add_test(NAME tools/gen_rtoi_matrix
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen_rtoi_matrix.py --check)
set_tests_properties(tools/gen_rtoi_matrix PROPERTIES LABELS Tools)
