# sir: SIR definitions, verifier and text format. Depends on nothing.
add_library(haremasim_sir STATIC text.cpp types.cpp verify.cpp version.cpp)
target_include_directories(haremasim_sir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
