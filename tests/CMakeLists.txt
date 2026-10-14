find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

add_subdirectory(unit)
add_subdirectory(suite)
add_subdirectory(perf_counts)
