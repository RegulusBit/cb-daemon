add_library(utilities utils.cpp logger.cpp)

target_include_directories(utilities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../plog/include)

target_link_libraries(utilities PUBLIC zclib jsoncpp ${Boost_LIBRARIES})
