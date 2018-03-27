cmake_minimum_required(VERSION 3.5)
project(tutserver)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)

add_subdirectory(src/utilities)
add_subdirectory(src/zclib)
add_subdirectory(src/zmq)

## load in pkg-config support
find_package(PkgConfig)
find_package(Boost REQUIRED COMPONENTS log)
find_package(Threads)


## use pkg-config to get hints for 0mq locations
pkg_check_modules(PC_ZeroMQ QUIET zmq)

## use the hint from above to find where 'zmq.hpp' is located
find_path(ZeroMQ_INCLUDE_DIR
        NAMES zmq.hpp
        PATHS ${PC_ZeroMQ_INCLUDE_DIRS}
        )

## use the hint from about to find the location of libzmq
find_library(ZeroMQ_LIBRARY
        NAMES zmq
        PATHS ${PC_ZeroMQ_LIBRARY_DIRS}
        )




add_executable(tutserver src/serverd.cpp)
target_link_libraries(tutserver utilities zclib zmq ${Boost_LIBRARIES} ${ZeroMQ_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

## add the include directory to our compile directives
target_include_directories(tutserver PUBLIC ${ZeroMQ_INCLUDE_DIR})

#
#
#
#sudo apt-get install libzmq3-dev
#
#
#
