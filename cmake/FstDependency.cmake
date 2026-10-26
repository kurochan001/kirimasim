# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Build the standalone permissive libfst tree, not GTKWave or simulator code.
# The pinned upstream Meson definitions are the build-metadata reference;
# no Meson, GTK/GLib or upstream test suite is needed by this CMake wrapper.
function(kirimasim_add_fst_dependency)
    find_package(ZLIB 1.2 REQUIRED)
    find_package(BZip2 1.0 REQUIRED)

    FetchContent_Declare(
        libfst
        GIT_REPOSITORY https://github.com/gtkwave/libfst.git
        GIT_TAG 17f6a5d27b2f576d65b550114fbde3c035cc74a8 # 2026-10-18, project version 1.0.0
        # Upstream uses Meson. Always use our small CMake target definition.
        SOURCE_SUBDIR kirimasim-cmake-wrapper
    )
    FetchContent_MakeAvailable(libfst)

    include(CheckSymbolExists)
    include(CheckCCompilerFlag)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
    check_symbol_exists(fseeko "stdio.h" KIRIMASIM_FST_HAVE_FSEEKO)
    check_symbol_exists(realpath "stdlib.h" KIRIMASIM_FST_HAVE_REALPATH)
    cmake_pop_check_state()
    set(HAVE_FSEEKO ${KIRIMASIM_FST_HAVE_FSEEKO})
    set(HAVE_REALPATH ${KIRIMASIM_FST_HAVE_REALPATH})
    set(fst_config_dir "${CMAKE_CURRENT_BINARY_DIR}/fst-config")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/fst_config.h.in"
                   "${fst_config_dir}/config.h")

    # Match upstream's narrowly scoped alignment exception for bundled FastLZ.
    # Every other sanitizer check, including all kirimasim checks, stays on.
    add_library(kirimasim_fst_fastlz OBJECT "${libfst_SOURCE_DIR}/src/fastlz.c")
    target_compile_definitions(kirimasim_fst_fastlz PRIVATE _GNU_SOURCE FST_INCLUDE_CONFIG)
    target_include_directories(kirimasim_fst_fastlz SYSTEM PRIVATE "${fst_config_dir}")
    check_c_compiler_flag(-fno-sanitize=alignment KIRIMASIM_FST_NO_ALIGNMENT_SANITIZER)
    if(KIRIMASIM_FST_NO_ALIGNMENT_SANITIZER)
        target_compile_options(kirimasim_fst_fastlz PRIVATE -fno-sanitize=alignment)
    endif()
    set_target_properties(kirimasim_fst_fastlz PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_library(kirimasim_fstapi STATIC
        "${libfst_SOURCE_DIR}/src/fstapi.c"
        "${libfst_SOURCE_DIR}/src/lz4.c"
        $<TARGET_OBJECTS:kirimasim_fst_fastlz>)
    add_library(kirimasim::fstapi ALIAS kirimasim_fstapi)
    target_include_directories(kirimasim_fstapi SYSTEM
        PUBLIC "${libfst_SOURCE_DIR}/src"
        PRIVATE "${fst_config_dir}")
    target_compile_definitions(kirimasim_fstapi PRIVATE _GNU_SOURCE FST_INCLUDE_CONFIG)
    target_link_libraries(kirimasim_fstapi PUBLIC ZLIB::ZLIB BZip2::BZip2)
    set_target_properties(kirimasim_fstapi PROPERTIES POSITION_INDEPENDENT_CODE ON)
    message(STATUS "FST dependency: libfst 17f6a5d27b2f; writer worker threads disabled")
endfunction()

kirimasim_add_fst_dependency()
