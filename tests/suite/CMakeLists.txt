# Each .sv file under tests/suite becomes one CTest test. At this stage the
# test only checks `haremasim parse`; expected_stdout is not compared yet.
file(GLOB_RECURSE suite_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.sv)
foreach(sv ${suite_files})
    file(RELATIVE_PATH rel ${CMAKE_CURRENT_SOURCE_DIR} ${sv})
    string(REGEX REPLACE "\\.sv$" "" test_name "${rel}")
    add_test(NAME suite/${test_name}
        COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/run_suite_test.py
            --haremasim $<TARGET_FILE:haremasim> ${sv})
    set_tests_properties(suite/${test_name} PROPERTIES LABELS Suite)
endforeach()
