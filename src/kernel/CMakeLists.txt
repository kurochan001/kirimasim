# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# kernel: the execution kernel shared by every execution tier.
# Depends on no other kirimasim library; FST is a private external dependency.
# Public headers remain C++20 and do not expose the external C API.
add_library(kirimasim_kernel STATIC abi.cpp activation.cpp clocking.cpp counters.cpp dump.cpp files.cpp format.cpp heap.cpp managed_event.cpp managed_event_runtime.cpp memfile.cpp nba.cpp nba_event.cpp pattern.cpp random.cpp scan.cpp sim.cpp stack.cpp
    store.cpp strength_network.cpp strength_drivers.cpp strength_wait.cpp strength_store.cpp strength_publication.cpp strings.cpp udp.cpp wide.cpp)
target_include_directories(kirimasim_kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kirimasim_kernel PRIVATE kirimasim::fstapi)
