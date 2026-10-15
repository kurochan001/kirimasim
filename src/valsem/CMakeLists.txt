# valsem: reference implementations of value semantics. Depends on nothing.
add_library(haremasim_valsem STATIC logic.cpp value.cpp)
target_include_directories(haremasim_valsem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
