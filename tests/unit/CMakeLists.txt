include(GoogleTest)

add_executable(kirimasim_unit_tests and4_test.cpp counters_test.cpp format_test.cpp scheduler_test.cpp sir_test.cpp timescale_test.cpp
    valsem_test.cpp w4ops_test.cpp)
target_link_libraries(kirimasim_unit_tests PRIVATE
    kirimasim_kernel kirimasim_sir kirimasim_valsem GTest::gtest_main)
gtest_discover_tests(kirimasim_unit_tests)

# The kernel's public headers are read by generated code, which is C++20.
# This target fails to build if any of them needs a newer standard.
add_library(kirimasim_kernel_headers_cxx20 OBJECT kernel_headers_cxx20.cpp)
target_link_libraries(kirimasim_kernel_headers_cxx20 PRIVATE kirimasim_kernel)
set_target_properties(kirimasim_kernel_headers_cxx20 PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
