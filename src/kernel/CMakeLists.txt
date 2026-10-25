# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# kernel: the execution kernel shared by every execution tier.
# Depends on nothing else in kirimasim. Its public headers are C++20.
add_library(kirimasim_kernel STATIC abi.cpp activation.cpp counters.cpp dump.cpp files.cpp format.cpp heap.cpp managed_event.cpp managed_event_runtime.cpp memfile.cpp pattern.cpp random.cpp scan.cpp sim.cpp stack.cpp
    store.cpp strings.cpp udp.cpp wide.cpp)
target_include_directories(kirimasim_kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
