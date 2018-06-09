cmake_minimum_required(VERSION 3.5)
project(daemon)


set(
        DEPENDS_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/snark"
        CACHE
        STRING
        "Optionally specify the dependency installation directory relative to the source directory (default: inside dependency folder)"
)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wall")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/modules")


include_directories(.
		db
		snark
		crypto
		support
		primitives
		script
		snark/libsnark
		snark/libsnark
		snark/libsnark/common
		${OPENSSL_INCLUDE_DIRS}
		${GOBJECT_INCLUDE_DIR}
		snark/gtest/googletest/include)

include_directories(${LIBMONGOCXX_INCLUDE_DIR})
include_directories(${LIBBSONCXX_INCLUDE_DIR})

#it's not good to hard code absolute paths but it's needed
#https://stackoverflow.com/questions/37210716/using-the-mongodb-cxx-driver-in-a-cmake-c-project?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa

include_directories("/usr/local/include/mongocxx/v_noabi")
include_directories("/usr/local/include/bsoncxx/v_noabi")


ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)


add_subdirectory(src/utilities)
add_subdirectory(src/cblib)
add_subdirectory(src/zmq)
add_subdirectory(zcash)
add_subdirectory(primitives)
add_subdirectory(script)
add_subdirectory(crypto)
add_subdirectory(consensus)
add_subdirectory(snark)
add_subdirectory(support)
add_subdirectory(db)
add_subdirectory(zmqpp)
add_subdirectory(src/hashidsxx)

## load in pkg-config support
find_package(PkgConfig)
find_package(Boost REQUIRED COMPONENTS log filesystem system thread program_options)
find_package(Threads)
pkg_check_modules(JSONCPP jsoncpp)


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


find_package(GMP REQUIRED)
find_package(Procps REQUIRED)
find_package(JNI REQUIRED)
find_package(libmongocxx REQUIRED)
find_package(libbsoncxx REQUIRED)

pkg_search_module(OPENSSL REQUIRED openssl)

#this line is required for linking to libprocps
pkg_check_modules(PROCPS REQUIRED libprocps)

add_library(testlib SHARED IMPORTED)
add_library(testlib2 SHARED IMPORTED)
add_library(libgmp SHARED IMPORTED)

set_property(TARGET testlib PROPERTY IMPORTED_LOCATION "/usr/local/lib/libsecp256k1.so")
set_property(TARGET testlib2 PROPERTY IMPORTED_LOCATION "/usr/local/lib/libsodium.so")

#this config is important and is not in goog shape:
#TODO: repair the GMP configuration
set_property(TARGET libgmp PROPERTY IMPORTED_LOCATION "/usr/lib/x86_64-linux-gnu/libgmpxx.a")



add_executable(daemon
		src/cb_tests.cpp
		src/cb_tests.h
		src/serverd.cpp
		serialize.h
		uint252.h
		uint256.cpp
		uint256.h
		streams.h
		hash.h
		hash.cpp
		version.h
		amount.cpp
		amount.h
		random.cpp
		random.h
		tinyformat.h
		utilstrencodings.cpp
		utilstrencodings.h
		sync.cpp
		sync.h
		threadsafety.h
		utiltime.cpp
		utiltime.h
		pubkey.cpp
		pubkey.h
		util.h
		util.cpp
		key.h
		key.cpp
		keystore.cpp
		keystore.h
		arith_uint256.cpp
		arith_uint256.h
		basic-config.h)


target_link_libraries(daemon
		utilities
		mongo_db
		cblib
		zmq
		zmqpp-static
		primitives
		${JSONCPP_LIBRARIES}
		${Boost_LIBRARIES}
		${ZeroMQ_LIBRARY}
		${CMAKE_THREAD_LIBS_INIT}
		libzcash
		script
		libcrypto
		testlib
		testlib2
		libgmp
		support
		consensus
		libsnark
		hashidsxx
		${Boost_LIBRARIES}
		${CMAKE_THREAD_LIBS_INIT}
		secp256k1
		${OPENSSL_LIBRARIES}
		${GMP_LIBRARY}
		${PROCPS_LIBRARIES}
		${JNI_LIBRARIES}
		${LIBMONGOCXX_LIBRARIES}
		${LIBBSONCXX_LIBRARIES})

## add the include directory to our compile directives
target_include_directories(daemon PUBLIC
		src/plog/include
		src
		${ZeroMQ_INCLUDE_DIR}
		${Boost_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        ${DEPENDS_DIR}
        ${DEPENDS_DIR}/snark
        ${DEPENDS_DIR}/depends
        ${DEPENDS_DIR}/crypto
        ${DEPENDS_DIR}/support
        ${DEPENDS_DIR}/consensus
        ${DEPENDS_DIR}/primitives
        ${DEPENDS_DIR}/script
        ${PROCPS_INCLUDE_DIRS}
        ${JNI_INCLUDE_DIRS})

#
#dependencies:
#  sudo apt-get install libjsoncpp-dev (cpp JSON Parser)
#  sudo apt-get install libzmq3-dev
#
#
#
#sudo apt-get install libsodium-dev
#sudo apt-get install libgmp-dev
# also needs libprocps
#never set a duplicate name for STATIC libraries!

#
# mongoDB installation process:
# [1...3] is deprecated:
# 1) sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 7F0CEB10
# 2) echo "deb http://repo.mongodb.org/apt/ubuntu xenial/mongodb-org/3.2 multiverse" | sudo tee /etc/apt/sources.list.d/mongodb-org-3.2.list
# 3) sudo apt-get update
# it's restricted in Iran! so just install it from the standard repositories:
# 4) sudo apt-get install mongodb
# 5) sudo service mongodb start
#
# https://www.tutorialspoint.com/mongodb/mongodb_data_modeling.htm
#
# install c and cpp drivers:
# 1) wget https://github.com/mongodb/libbson/releases/download/1.9.3/libbson-1.9.3.tar.gz
#    tar -xzf libbson-1.9.3.tar.gz
#    cd libbson-1.9.3/
#    ./configure
#    wget https://github.com/mongodb/mongo-c-driver/releases/download/1.9.3/mongo-c-driver-1.9.3.tar.gz
#    tar xzf mongo-c-driver-1.9.3.tar.gz
#    cd mongo-c-driver-1.9.3
#    ./configure --disable-automatic-init-and-cleanup
# 2) curl -OL https://github.com/mongodb/mongo-cxx-driver/archive/r3.1.2.tar.gz
# 3) tar -xzf r3.1.2.tar.gz
# 4) cd mongo-cxx-driver-r3.1.2/build
# 5) cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
# 6) make && sudo make install
#
# Note: there was an error in build process. you should add the #include<cassert>
#       in the mock.hh. the path to file is provided in build error log in bash
#
# Note: error while loading shared libraries: libmongocxx.so._noabi: cannot open shared object file
#       solution: sudo /sbin/ldconfig -v
#
#
# mandatory: install libzmq and in some cases czmq
