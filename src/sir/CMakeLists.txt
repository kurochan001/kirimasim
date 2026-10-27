# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# sir: SIR definitions, verifier and text format. Depends on nothing.
add_library(kirimasim_sir STATIC activation.cpp nba.cpp pattern.cpp strength.cpp text.cpp timescale.cpp types.cpp verify.cpp version.cpp)
target_include_directories(kirimasim_sir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
