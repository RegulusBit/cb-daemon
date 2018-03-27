add_library(zmq messaging_server.cpp)

target_include_directories(zmq PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(zmq utilities)
