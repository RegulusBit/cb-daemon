add_library(utilities utils.cpp logger.cpp)

target_include_directories(utilities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(utilities zclib ${Boost_LIBRARIES})
