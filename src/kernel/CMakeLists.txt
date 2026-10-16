# kernel: the execution kernel shared by every execution tier.
# Depends on nothing else in kirimasim. Its public headers are C++20.
add_library(kirimasim_kernel STATIC abi.cpp counters.cpp format.cpp sim.cpp store.cpp strings.cpp)
target_include_directories(kirimasim_kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
