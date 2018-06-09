file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.h)
file(GLOB_RECURSE SRCS2 *.tcc)
file(GLOB_RECURSE HDRS2 *.hpp)

add_library(libsnark ${SRCS} ${HDRS} ${SRCS2} ${HDRS2})

target_link_libraries(libsnark libgmp ${PROCPS_LIBRARIES})

target_include_directories(libsnark PUBLIC libsnark ${DEPENDS_DIR}/libsnark depends depends/libff)