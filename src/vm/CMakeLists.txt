# vm: tier 1, bytecode generation and interpretation.
# May depend on: sir, kernel.
add_library(haremasim_vm STATIC vm.cpp)
target_include_directories(haremasim_vm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(haremasim_vm PUBLIC haremasim_sir haremasim_kernel)
