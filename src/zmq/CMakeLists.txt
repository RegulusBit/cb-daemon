add_library(zmq messaging_server.cpp zmsg.hpp)

target_include_directories(zmq PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../plog/include)

target_link_libraries(zmq utilities)
