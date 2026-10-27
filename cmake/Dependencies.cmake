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

# slang's master branch at e2d1021 (2026-10-27), 24 commits after
# the previous pin 16bd3b9. It includes the fixes of slang issues #2023,
# #2029, #2036, #2041, #2042 and #2048, plus #2064's invalid class-specifier
# crash fix and subsequent literal, selection, pattern and name-binding fixes.
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
    GIT_TAG e2d1021661f78b3f17d1edf7ea17f9d6f95fb168 # master, after 12.1, 2026-10-27
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
