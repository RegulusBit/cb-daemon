add_library(zclib address.cpp)

target_include_directories(zclib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(zclib utilities)
