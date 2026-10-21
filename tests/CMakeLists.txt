# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# The tools use the union types of Python 3.10 in their annotations.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

add_subdirectory(unit)
add_subdirectory(context)
add_subdirectory(suite)
add_subdirectory(dump_sir)
add_subdirectory(cli)
add_subdirectory(perf_counts)
add_subdirectory(tools)
