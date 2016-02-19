
# CMake configuration
cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})


# Project declaration
project(freeflow C CXX)


# Options
option(FREEFLOW_USE_PCAP "Enable PCAP" ON)


# Compiler config
# We effectively require a functioning C++11 implementation.
# For Clang, use the libc++ as the standard library.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8")
    message(FATAL_ERROR "Requires GCC version 4.8 or greater")
  endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments -stdlib=libc++")
endif()


# C/C++ build flags.
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -pthread -O3")


# Require Boost C++ Libraries.
find_package(Boost 1.55.0 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})


if(FREEFLOW_USE_PCAP)
  find_package(PCAP REQUIRED)
endif()


# Allow includes to find from headers from this dir.
include_directories(.)

add_subdirectory(freeflow)
add_subdirectory(fp-lite)
add_subdirectory(util)

# add_subdirectory(flowctl)
# add_subdirectory(flowmgr)
# add_subdirectory(flowpath)
