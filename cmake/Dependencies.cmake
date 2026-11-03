# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# External dependencies, fetched with FetchContent and pinned to commit SHAs.
#
# No git submodules are used. Every dependency is pinned to a full commit SHA
# so that a configure always sees the same sources.
#
# To try a local patch to slang (for example, one you intend to send
# upstream), point FetchContent at your own clone instead of the pinned SHA:
#
#   cmake --preset dev -DFETCHCONTENT_SOURCE_DIR_SLANG=/path/to/your/slang
#
# The same mechanism works for GoogleTest via FETCHCONTENT_SOURCE_DIR_GOOGLETEST.

include(FetchContent)

# slang's master branch at d02b7cda (checked 2026-11-03), 25 commits after
# the previous pin e2d1021. It includes #2072's subroutine error-recovery fix,
# random seed binding, class and non-ANSI interface-port fixes. Its existing
# mimalloc dependency advances from 3.6.0 to 3.6.1.
# This is a development commit after 12.1, pinned by full SHA and used
# unmodified. The nightly job slang-main tests the upstream default branch.
# GIT_SHALLOW is not used because a shallow clone cannot check out an
# arbitrary SHA.
set(SLANG_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(SLANG_INCLUDE_TOOLS OFF CACHE BOOL "" FORCE)
set(SLANG_INCLUDE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    slang
    GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
    GIT_TAG d02b7cda02d3183f35c6c4d4db2b2fc03f19f4ae # master, after 12.1, checked 2026-11-03
)

# GoogleTest v1.18.0 (released 2026-08-10). Used only by the unit tests.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 063de7e9578f82b369302001269680b4b1553359 # v1.18.0
)

FetchContent_MakeAvailable(slang googletest)

# FST library preparation requested 2026-10-26; waveform output remains separate.
include(${CMAKE_CURRENT_LIST_DIR}/FstDependency.cmake)
