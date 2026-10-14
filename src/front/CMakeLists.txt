# front: drives slang and lowers its elaboration result to SIR.
# May depend on: slang, sir.
add_library(haremasim_front STATIC front.cpp)
target_include_directories(haremasim_front PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(haremasim_front PUBLIC haremasim_sir PRIVATE slang::slang)
