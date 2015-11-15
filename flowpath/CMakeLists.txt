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

# Applications.
add_subdirectory(apps)

# Drivers.
add_subdirectory(drivers)

# Tests.
add_subdirectory(tests)

# Lang.
add_subdirectory(lang)

# The flowpath runtime library.
add_library(flowpath-rt SHARED
    application.cpp
    application_library.cpp
    buffer.cpp
    dataplane.cpp
    flow.cpp
    context.cpp
    nbi.cpp
    packet.cpp
    port.cpp
    port_table.cpp
    port_udp.cpp
    sbi.cpp
    system.cpp
    table.cpp
    thread.cpp
    types.cpp
    queue.cpp)