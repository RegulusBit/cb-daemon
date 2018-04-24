file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.h)

add_library(script script.cpp ${SRCS} ${HDRS})
