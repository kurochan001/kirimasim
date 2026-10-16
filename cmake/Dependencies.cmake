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

# slang v12.0 (released 2026-09-29). The SHA is the commit the v12.0 tag
# points to. GIT_SHALLOW is not used because a shallow clone cannot check out
# an arbitrary SHA.
set(SLANG_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(SLANG_INCLUDE_TOOLS OFF CACHE BOOL "" FORCE)
set(SLANG_INCLUDE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    slang
    GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
    GIT_TAG 545f88b2ed3a2d20325fbd17413af177a6bca72f # v12.0
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

# slang issue #2023: constant evaluation of $rtoi converts a negative double
# straight to uint64_t, which is undefined behavior, during elaboration.
# Until a slang release fixes it, the ubsan preset does not check
# float-to-integer conversions inside slang's own code. haremasim's code is
# still checked in full. Remove this when slang is upgraded past the fix.
if(CMAKE_CXX_FLAGS_DEBUG MATCHES "fsanitize=undefined" AND TARGET slang_slang)
    target_compile_options(slang_slang PRIVATE -fno-sanitize=float-cast-overflow)
endif()
