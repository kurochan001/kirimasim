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

# slang's master branch at 8ffdcdd8 (checked 2026-11-13), 6 commits after
# the previous pin 62affeb0. It includes #2116's fix (in a ? b : c -> d the
# conditional operator binds tighter than the implication, as 11.3.2's table
# of precedence says), a fix of a use-after-free in DiagnosticVisitor for
# nested generic classes, and a faster constant evaluation of selects of
# large local variables. Its dependencies do not change.
# This is a development commit after 12.1, pinned by full SHA and used
# unmodified. The nightly job slang-main tests the upstream default branch.
# GIT_SHALLOW is not used because a shallow clone cannot check out an
# arbitrary SHA.
set(SLANG_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(SLANG_INCLUDE_TOOLS OFF CACHE BOOL "" FORCE)
set(SLANG_INCLUDE_INSTALL OFF CACHE BOOL "" FORCE)
# Always the pinned dependencies of slang, never a copy that happens to be
# installed on the machine (slang's FIND_PACKAGE_ARGS would prefer one).
set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER)
FetchContent_Declare(
    slang
    GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
    GIT_TAG 8ffdcdd8ad237823fbfd72d495082d76e92d4015 # master, after 12.1, checked 2026-11-13
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
