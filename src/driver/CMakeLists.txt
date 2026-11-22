# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# driver: the kirimasim executable (command line, build cache, tier choice).
# May depend on every library.
configure_file(build_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.h @ONLY)

# The installed executable can build a native module on its own. Embed the
# transitive headers of generated code and fingerprint the full public ABI.
set(kernel_headers "${CMAKE_CURRENT_SOURCE_DIR}/../kernel/include/kirimasim/kernel")
file(GLOB public_headers CONFIGURE_DEPENDS "${kernel_headers}/*.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${public_headers})
set(header_hashes "")
foreach(header IN LISTS public_headers)
    get_filename_component(name "${header}" NAME)
    file(SHA256 "${header}" digest)
    string(APPEND header_hashes "${name}:${digest}\n")
endforeach()
string(SHA256 public_digest "${header_hashes}")
set(embedded_headers "")
foreach(name IN ITEMS abi.h model_abi.h w4.h w4ops.h bits.h udp.h)
    file(READ "${kernel_headers}/${name}" content)
    string(APPEND embedded_headers "{\"${name}\", R\"KHDR(${content})KHDR\"},\n")
endforeach()
configure_file(native_headers.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/native_headers.h @ONLY)

add_executable(kirimasim main.cpp native_model.cpp race_files.cpp race_trace.cpp vpi_loader.cpp)
set_target_properties(kirimasim PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(kirimasim PRIVATE ${CMAKE_DL_LIBS})
target_include_directories(kirimasim PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
# sir and analysis come in through front, vm and codegen.
target_link_libraries(kirimasim PRIVATE
    kirimasim_front
    kirimasim_vm
    kirimasim_codegen
    kirimasim_kernel)
