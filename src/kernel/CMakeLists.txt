# kernel: the execution kernel shared by every execution tier.
# Depends on nothing else in haremasim. Its public headers are C++20.
add_library(haremasim_kernel STATIC abi.cpp counters.cpp format.cpp sim.cpp store.cpp strings.cpp)
target_include_directories(haremasim_kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
