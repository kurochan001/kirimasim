# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# driver: the kirimasim executable (command line, build cache, tier choice).
# May depend on every library.
configure_file(build_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.h @ONLY)

add_executable(kirimasim main.cpp)
target_include_directories(kirimasim PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
# sir and analysis come in through front, vm and codegen.
target_link_libraries(kirimasim PRIVATE
    kirimasim_front
    kirimasim_vm
    kirimasim_codegen
    kirimasim_kernel
    kirimasim_valsem)
