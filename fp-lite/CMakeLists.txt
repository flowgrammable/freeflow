project(flowpath-rt CXX)

# Configure compiler.
# We effectively require a functioning C++11 implementation.
# For Clang, use the libc++ as the standard library.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8")
    message(FATAL_ERROR "Requires GCC version 4.8 or greater")
  endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

# C++ build flags.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pedantic -Wall -pthread")

# Require Boost C++ Libraries.
find_package(Boost 1.55.0 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Allow includes to find from headers from this dir.
include_directories(.)

# The flowpath runtime library.
add_library(fp-lite-rt SHARED
  types.cpp
  context.cpp
  port.cpp
  port_tcp.cpp
  application.cpp)
target_link_libraries(fp-lite-rt freeflow)

# Drivers.
add_subdirectory(drivers)

# Applications.
add_subdirectory(apps)

# Tests.
# add_subdirectory(tests)
