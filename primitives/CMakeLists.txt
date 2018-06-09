add_library(primitives transaction.cpp transaction.h)

target_link_libraries(primitives PUBLIC libzcash)
