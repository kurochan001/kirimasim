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
