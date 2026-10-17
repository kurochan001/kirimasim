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
