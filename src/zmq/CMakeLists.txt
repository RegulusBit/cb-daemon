add_library(zmq messaging_server.cpp zmsg.hpp request.cpp)

target_include_directories(zmq PUBLIC . ${CMAKE_CURRENT_SOURCE_DIR} ../plog/include ../cblib ../zmq)

target_link_libraries(zmq utilities mongo_db)
