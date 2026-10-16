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
