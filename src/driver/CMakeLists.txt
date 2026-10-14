# driver: the haremasim executable (command line, build cache, tier choice).
# May depend on every library.
configure_file(build_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.h @ONLY)

add_executable(haremasim main.cpp)
target_include_directories(haremasim PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
# sir and analysis come in through front, vm and codegen.
target_link_libraries(haremasim PRIVATE
    haremasim_front
    haremasim_vm
    haremasim_codegen
    haremasim_kernel
    haremasim_valsem)
