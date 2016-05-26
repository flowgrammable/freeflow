


# C++ build flags.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -pthread")

# Allow includes to find from headers from this dir.
include_directories(.)


# The flowpath runtime library.
add_library(fp-lite-rt SHARED
  types.cpp
  context.cpp
  port.cpp
  port_tcp.cpp
  port_drop.cpp
  flow.cpp
  table.cpp
  application.cpp
  dataplane.cpp
  system.cpp
  thread.cpp
  queue.cpp
  buffer.cpp)
target_link_libraries(fp-lite-rt freeflow)


# Drivers.
add_subdirectory(drivers)


# Applications.
add_subdirectory(apps)


# Tests.
# add_subdirectory(tests)
