add_library(utilities utils.cpp utils.h)

target_include_directories(utilities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../plog/include)

target_link_libraries(utilities PUBLIC cblib jsoncpp ${Boost_LIBRARIES})
