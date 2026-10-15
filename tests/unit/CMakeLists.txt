include(GoogleTest)

add_executable(haremasim_unit_tests and4_test.cpp counters_test.cpp scheduler_test.cpp sir_test.cpp timescale_test.cpp
    valsem_test.cpp w4ops_test.cpp)
target_link_libraries(haremasim_unit_tests PRIVATE
    haremasim_kernel haremasim_sir haremasim_valsem GTest::gtest_main)
gtest_discover_tests(haremasim_unit_tests)

# The kernel's public headers are read by generated code, which is C++20.
# This target fails to build if any of them needs a newer standard.
add_library(haremasim_kernel_headers_cxx20 OBJECT kernel_headers_cxx20.cpp)
target_link_libraries(haremasim_kernel_headers_cxx20 PRIVATE haremasim_kernel)
set_target_properties(haremasim_kernel_headers_cxx20 PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
