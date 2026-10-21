# kernel: the execution kernel shared by every execution tier.
# Depends on nothing else in kirimasim. Its public headers are C++20.
add_library(kirimasim_kernel STATIC abi.cpp counters.cpp dump.cpp files.cpp format.cpp memfile.cpp random.cpp scan.cpp sim.cpp
    store.cpp strings.cpp udp.cpp wide.cpp)
target_include_directories(kirimasim_kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
