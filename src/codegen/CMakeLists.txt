# codegen: tier 2, C++ generation and build.
# May depend on: sir, analysis.
add_library(kirimasim_codegen STATIC codegen.cpp)
target_include_directories(kirimasim_codegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kirimasim_codegen PUBLIC kirimasim_sir kirimasim_analysis)
