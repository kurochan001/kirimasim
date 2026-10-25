# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# vm: tier 1, bytecode generation and interpretation.
# May depend on: sir, kernel.
add_library(kirimasim_vm STATIC vm.cpp activation_environment.cpp)
target_include_directories(kirimasim_vm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kirimasim_vm PUBLIC kirimasim_sir kirimasim_kernel)
