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

#set(ODP_DEBUG true)
# Find system Open Data Plane (ODP) Libraries.
if (NOT ODP_DEBUG)
  find_library(LIB_ODP odp)
  if (NOT LIB_ODP)
    message(FATAL_ERROR "Cannot find lib odp")
  endif()
  message(STATUS "LIB_ODP: ${LIB_ODP}")

  find_path(LIB_ODP_INCLUDE_DIR "odp.h")
  if (NOT LIB_ODP_INCLUDE_DIR)
    message(FATAL_ERROR "Cannot find odp headers")
  endif()
  message(STATUS "LIB_ODP_INCLUDE_DIR: ${LIB_ODP_INCLUDE_DIR}")

  include_directories("${LIB_ODP_INCLUDE_DIR}")
endif()

# Allow includes to find from headers from this dir.
include_directories(.)

# Applications.
add_subdirectory(apps)

# Drivers.
add_subdirectory(drivers)

# Tests.
add_subdirectory(tests)

# The flowpath runtime library.
add_library(flowpath-rt SHARED
    application.cpp
    buffer.cpp
    action.cpp
    binding.cpp
    context.cpp
    dataplane.cpp
    flow.cpp
    packet.cpp
    port.cpp
    port_table.cpp
    port_tcp.cpp
    port_udp.cpp
    port_odp.cpp
    queue.cpp
    system.cpp
    table.cpp
    thread.cpp
    types.cpp)

# Add ODP as dependancy to flowpath-rt.
if (NOT ODP_DEBUG)
  target_link_libraries(flowpath-rt
    ${LIB_ODP}
    crypto
    rt
  )
else()
  add_dependencies(flowpath-rt odp-linux)
  target_link_libraries(flowpath-rt 
    LINK_PRIVATE debug "${CMAKE_BINARY_DIR}/odp-sysroot/lib/libodp.a"
    LINK_PRIVATE debug "${CMAKE_BINARY_DIR}/odp-sysroot/lib/libodphelper.a"
    crypto
    rt
  )
endif()

