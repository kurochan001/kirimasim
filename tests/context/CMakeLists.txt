# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# The tests of state and Contexts (RUNTIME_ABI.md 9.8; TEST_SUITE.md, list
# of required tests). Every preset runs them; the tsan preset runs only
# these (CMakePresets.json).
find_package(Threads REQUIRED)

add_executable(kirimasim_context_tests context_test.cpp)
target_link_libraries(kirimasim_context_tests PRIVATE
    kirimasim_front kirimasim_vm kirimasim_kernel kirimasim_sir Threads::Threads)
target_compile_definitions(kirimasim_context_tests PRIVATE
    KIRIMASIM_CONTEXT_DESIGNS="${CMAKE_CURRENT_SOURCE_DIR}/designs")

foreach(mode sequential parallel churn)
    add_test(NAME context_${mode} COMMAND kirimasim_context_tests ${mode})
    set_tests_properties(context_${mode} PROPERTIES LABELS Context)
endforeach()
# front on several threads at once (RUNTIME_ABI.md 9.7); it fails under
# ThreadSanitizer while slang's Driver writes its colour flags (#32).
add_test(NAME front_parallel COMMAND kirimasim_context_tests front)
set_tests_properties(front_parallel PROPERTIES LABELS Context)

# The stop before the stack of the running thread runs out (RUNTIME_ABI.md
# 5): each case runs on a thread of the stack size it names.
add_executable(kirimasim_stack_tests stack_test.cpp)
target_link_libraries(kirimasim_stack_tests PRIVATE
    kirimasim_front kirimasim_vm kirimasim_kernel kirimasim_sir Threads::Threads)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stack)
target_compile_definitions(kirimasim_stack_tests PRIVATE
    KIRIMASIM_STACK_DIR="${CMAKE_CURRENT_BINARY_DIR}/stack")
foreach(case front_small front_large function_small function_large task_small task_large
        guard_small guard_large deferred_small deferred_large default_thread
        class_static_unit_thread class_static_package_thread class_static_module_thread
        front_first guard_first class_static_first)
    add_test(NAME stack_${case} COMMAND kirimasim_stack_tests ${case})
    # Recursion 10000 deep is slow with the fake stacks of AddressSanitizer's
    # check of use after return (as in tests/suite/CMakeLists.txt).
    set_tests_properties(stack_${case} PROPERTIES LABELS Context
        ENVIRONMENT "ASAN_OPTIONS=detect_stack_use_after_return=0")
endforeach()
