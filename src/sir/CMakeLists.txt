# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# sir: SIR definitions, verifier and text format. Depends on nothing.
add_library(kirimasim_sir STATIC activation.cpp class_observation.cpp classes.cpp frame_layout.cpp module_path.cpp monitor_reads.cpp nba.cpp pattern.cpp process_policy.cpp records.cpp strength.cpp systasks.cpp text.cpp text_v1.cpp timescale.cpp types.cpp verified.cpp verify.cpp verify_syscall.cpp version.cpp)
target_include_directories(kirimasim_sir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
