# The judging tools must fail when they should (TEST_SUITE.md).
add_test(NAME tools/negative_controls
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run_tool_tests.py
        --haremasim $<TARGET_FILE:haremasim>)
set_tests_properties(tools/negative_controls PROPERTIES LABELS Tools)
