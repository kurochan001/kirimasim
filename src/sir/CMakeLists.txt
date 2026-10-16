# sir: SIR definitions, verifier and text format. Depends on nothing.
add_library(kirimasim_sir STATIC text.cpp timescale.cpp types.cpp verify.cpp version.cpp)
target_include_directories(kirimasim_sir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
