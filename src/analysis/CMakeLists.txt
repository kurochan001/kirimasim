# analysis: X-reachability analysis, static island detection, levelization, observability.
# May depend on: sir.
add_library(kirimasim_analysis STATIC analysis.cpp)
target_include_directories(kirimasim_analysis PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kirimasim_analysis PUBLIC kirimasim_sir)
