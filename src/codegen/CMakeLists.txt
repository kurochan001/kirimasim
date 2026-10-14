# codegen: tier 2, C++ generation and build.
# May depend on: sir, analysis.
add_library(haremasim_codegen STATIC codegen.cpp)
target_include_directories(haremasim_codegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(haremasim_codegen PUBLIC haremasim_sir haremasim_analysis)
