cmake_minimum_required(VERSION 2.8.12)

project(freeflow C CXX)

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

# C/C++ build flags.
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pedantic -Wall -pthread")

# Require Boost C++ Libraries.
find_package(Boost 1.55.0 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Allow includes to find from headers from this dir.
include_directories(.)

# Flowctl
add_subdirectory(flowctl)

# Flowmgr
add_subdirectory(flowmgr)

# Flowpath Runtime
add_subdirectory(flowpath)

# Utilities
add_subdirectory(util)