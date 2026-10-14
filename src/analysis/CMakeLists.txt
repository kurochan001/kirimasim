# analysis: X-reachability analysis, static island detection, levelization, observability.
# May depend on: sir.
add_library(haremasim_analysis STATIC analysis.cpp)
target_include_directories(haremasim_analysis PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(haremasim_analysis PUBLIC haremasim_sir)
