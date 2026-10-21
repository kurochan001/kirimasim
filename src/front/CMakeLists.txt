# front: drives slang and lowers its elaboration result to SIR.
# May depend on: slang, sir.
add_library(kirimasim_front STATIC front.cpp lower.cpp lower_dump.cpp lower_expr.cpp lower_stmt.cpp
    lower_struct.cpp lower_sub.cpp type_name.cpp)
target_include_directories(kirimasim_front PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kirimasim_front PUBLIC kirimasim_sir PRIVATE slang::slang)
