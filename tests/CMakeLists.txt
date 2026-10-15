find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

add_subdirectory(unit)
add_subdirectory(suite)
add_subdirectory(dump_sir)
add_subdirectory(cli)
add_subdirectory(perf_counts)
