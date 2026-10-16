# Each .sv file under tests/dump_sir is compared with the --dump-sir text in
# its .expected file (SIR_SPEC.md 9). The .expected files are protected
# expectations (TEST_SUITE.md).
file(GLOB dump_sir_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.sv)
foreach(sv ${dump_sir_files})
    get_filename_component(test_name ${sv} NAME_WE)
    add_test(NAME dump_sir/${test_name}
        COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/run_dump_sir_test.py
            --kirimasim $<TARGET_FILE:kirimasim> ${sv})
    set_tests_properties(dump_sir/${test_name} PROPERTIES LABELS DumpSir)
endforeach()
