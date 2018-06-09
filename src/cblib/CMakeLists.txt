add_library(cblib address.cpp account.cpp request_methods.cpp process_request.cpp account.h process_request.h request_methods.h address.h note.cpp note.h)

target_include_directories(cblib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cblib PUBLIC utilities zmq mongo_db)
