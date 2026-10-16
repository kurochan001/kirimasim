add_test(NAME perfcount
    COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/perfcount_check.py
        --kirimasim $<TARGET_FILE:kirimasim>
        --expected ${CMAKE_CURRENT_SOURCE_DIR}/expected.json
        --root ${PROJECT_SOURCE_DIR})
set_tests_properties(perfcount PROPERTIES LABELS PerfCount)
