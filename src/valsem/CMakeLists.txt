# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# valsem: reference implementations of value semantics. Depends on nothing.
add_library(kirimasim_valsem STATIC containers.cpp logic.cpp mos.cpp passive.cpp strength.cpp udp.cpp value.cpp)
target_include_directories(kirimasim_valsem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
