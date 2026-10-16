# vm: tier 1, bytecode generation and interpretation.
# May depend on: sir, kernel.
add_library(kirimasim_vm STATIC vm.cpp)
target_include_directories(kirimasim_vm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kirimasim_vm PUBLIC kirimasim_sir kirimasim_kernel)
