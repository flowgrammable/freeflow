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

# Require (for now) C ZLIB Libraries.
# TODO: cleanup pcap, zlib, crypto, and rt dependencies
find_package(ZLIB REQUIRED)

# Find system Open Data Plane (ODP) Libraries.
if (ENABLE_ODP AND NOT BUILD_ODP)
  find_library(LIB_ODP odp)
  if (NOT LIB_ODP)
    message(FATAL_ERROR "Cannot find lib odp in default paths")
  endif()
  message(STATUS "LIB_ODP: ${LIB_ODP}")

  find_path(LIB_ODP_INCLUDE_DIR "odp.h")
  if (NOT LIB_ODP_INCLUDE_DIR)
    message(FATAL_ERROR "Expected to find odp.h at root of: ${LIB_ODP}")
  endif()
  message(STATUS "LIB_ODP_INCLUDE_DIR: ${LIB_ODP_INCLUDE_DIR}")

  include_directories("${LIB_ODP_INCLUDE_DIR}")
endif()

# Allow includes to find from headers from this dir.
include_directories(.)

# Applications.
#add_subdirectory(apps)
add_subdirectory(app-lib)

# Drivers.
add_subdirectory(drivers)

# Tests.
#add_subdirectory(tests)

# The flowpath runtime library.
add_library(flowpath-rt SHARED
    application.cpp
    buffer.cpp
    context.cpp
    dataplane.cpp
    flow.cpp
    packet.cpp
    port.cpp
    port_table.cpp
#    port_tcp.cpp
#    port_udp.cpp
    port_odp.cpp
    port_pcap.cpp
    queue.cpp
    system.cpp
    table.cpp
    thread.cpp
    types.cpp
)

# Add C-Define to enable compilation of port_odp.cpp
if (ENABLE_ODP)
  add_definitions(-DFP_ENABLE_ODP)
endif()

# Add ODP as dependancy to flowpath-rt.
if (ENABLE_ODP AND NOT BUILD_ODP)
  # Use system's ODP libs.
  target_link_libraries(flowpath-rt
    ${LIB_ODP}
		${ZLIB_LIBRARIES}
    crypto
    rt
  )
elseif (ENABLE_ODP AND BUILD_ODP AND NOT BUILD_ODP_DPDK)
  # Include ODP compiled locally.
  add_dependencies(flowpath-rt odp-linux)
  target_link_libraries(flowpath-rt 
    LINK_PRIVATE debug "${CMAKE_BINARY_DIR}/odp-sysroot/lib/libodp.a"
    LINK_PRIVATE debug "${CMAKE_BINARY_DIR}/odp-sysroot/lib/libodphelper.a"
		${ZLIB_LIBRARIES}
    pcap
    crypto
    rt
  )
elseif (ENABLE_ODP AND BUILD_ODP AND BUILD_ODP_DPDK)
  # Include ODP-DPDK compiled locally.
  add_dependencies(odp-dpdk dpdk)
  add_dependencies(flowpath-rt odp-dpdk)
  target_link_libraries(flowpath-rt
    LINK_PRIVATE debug "${CMAKE_BINARY_DIR}/odp-dpdk-sysroot/lib/libodp.a"
    LINK_PRIVATE debug "${CMAKE_BINARY_DIR}/odp-dpdk-sysroot/lib/libodphelper.a"
    LINK_PRIVATE debug "${CMAKE_BINARY_DIR}/dpdk.git/src/dpdk/x86_64-native-linuxapp-gcc/lib/libdpdk.a"
# TODO: Make proper build / install paths for dpdk library ^^
		${ZLIB_LIBRARIES}
    pcap
    crypto
    rt
  )
else ()
  target_link_libraries(flowpath-rt
		${ZLIB_LIBRARIES}
    pcap
		crypto
#    rt
	)
endif()

