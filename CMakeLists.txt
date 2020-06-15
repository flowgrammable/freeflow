cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0042 NEW)

include(ExternalProject)

project(freeflow C CXX)

# Configure compiler.
# We effectively require a functioning C++11 implementation.
# For Clang, use the libc++ as the standard library.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "7.3")
    message(FATAL_ERROR "Requires GCC version 7.3 or greater")
  endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments -stdlib=libc++")
endif()


# C/C++ build flags.
# - Use CMAKE_BUILD_TYPE={Debug, Release, RelWithDebInfo} to set optimization.
set(OPT_FLAGS "-march=native")
set(ASAN_FLAGS "-fsanatize=address")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${OPT_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 ${OPT_FLAGS} -Wall -Werror=return-type")

# Extra build flag for link-time optimization.
# - LTO can mess with gdb pretty-printing.
option(ENABLE_LTO "Enables Link-time Optimization" OFF)
if (ENABLE_LTO)
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -flto")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
endif()

# Extra build flags for profiling.
option(ENABLE_PERF "Enables frame-pointer for profiling with Perf" OFF)
if (ENABLE_PERF)
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
endif()

# Extra build flags for address sanitation.
option(ENABLE_ASAN "Enables address sanitization checks" OFF)
if (ENABLE_ASAN)
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fsanitize=address")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

# CMAKE find package modules.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# PCAP C Library.
find_package(PCAP REQUIRED)
find_package(Mathematica)
cmake_policy(SET CMP0057 NEW)
find_package(Filesystem REQUIRED Final)	# Requires NEW CMP0057 behavior

include_directories(${PCAP_INCLUDE_DIR})

# Include Abseil-CPP Library.
add_subdirectory(abseil-cpp.git)

# Require Boost C++ Libraries.
#find_package(Boost 1.55.0 REQUIRED)
#include_directories(${Boost_INCLUDE_DIRS})


# ODP Linking Options.
# 1) Look for release build of ODP in system path
# 2) Build ODP using External Project and statically link
#   - This allows compiling for release or debug mode
# 3) Build ODP-DPDK using External Project and statically link
option(ENABLE_ODP "Enable ODP Ports" OFF)
option(BUILD_ODP "Build ODP Library Locally" OFF)
option(BUILD_ODP_DPDK "Enable Build of ODP DPDK Ports" OFF)

# ODP External Project Module.
# - This module doesn't appear to get added when options are changed...
# -- Thus, need to always include incase it is needed (doesn't actually
#    build unless BUILD_ODP* options are enabled).
ExternalProject_Add(odp-linux
  PREFIX odp.git
  GIT_REPOSITORY http://git.linaro.org/lng/odp.git
  GIT_TAG v1.7.0.0
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-debug "CFLAGS=${CMAKE_C_FLAGS} -fPIC -g"
  INSTALL_DIR odp-sysroot
  UPDATE_COMMAND ""
  EXCLUDE_FROM_ALL 1
  BUILD_IN_SOURCE 1
#  UPDATE_DISCONNECTED 1
  LOG_DOWNLOAD 1
  LOG_UPDATE 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
)
ExternalProject_Add_Step(odp-linux bootstrap
  COMMAND <SOURCE_DIR>/bootstrap
  COMMENT "Performing odp specific bootstrap step"
  WORKING_DIRECTORY <SOURCE_DIR>
  DEPENDERS configure
  DEPENDEES download update
  LOG 1
)

# ODP-DPDK External Project Module.
ExternalProject_Add(odp-dpdk
  PREFIX odp-dpdk.git
  GIT_REPOSITORY http://git.linaro.org/lng/odp-dpdk.git
  GIT_TAG v1.6.0.0_DPDK_2.2.0
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-platform=linux-dpdk --with-sdk-install-path=<INSTALL_DIR>/../dpdk.git/src/dpdk/x86_64-native-linuxapp-gcc --with-debug "CFLAGS=${CMAKE_C_FLAGS} -fPIC -g"
# TODO: fix dpdk's build / install directory ^^
  INSTALL_DIR odp-dpdk-sysroot
  UPDATE_COMMAND ""
  EXCLUDE_FROM_ALL 1
  BUILD_IN_SOURCE 1
#  UPDATE_DISCONNECTED 1
  LOG_DOWNLOAD 1
  LOG_UPDATE 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
)
ExternalProject_Add_Step(odp-dpdk bootstrap
  COMMENT "Performing odp specific bootstrap step"
  COMMAND <SOURCE_DIR>/bootstrap
  WORKING_DIRECTORY <SOURCE_DIR>
  DEPENDERS configure
  DEPENDEES download update
  LOG 1
)

# DPDK External Project Module (required by ODP-DPDK).
ExternalProject_Add(dpdk
  PREFIX dpdk.git
  GIT_REPOSITORY http://dpdk.org/git/dpdk
  GIT_TAG v2.2.0
  CONFIGURE_COMMAND $(MAKE) config T=x86_64-native-linuxapp-gcc O=x86_64-native-linuxapp-gcc
  BUILD_COMMAND $(MAKE) T=x86_64-native-linuxapp-gcc O=x86_64-native-linuxapp-gcc "EXTRA_CFLAGS=-fPIC -g"
  INSTALL_COMMAND ""
  UPDATE_COMMAND ""
  EXCLUDE_FROM_ALL 1
  BUILD_IN_SOURCE 1
#  UPDATE_DISCONNECTED 1
  LOG_DOWNLOAD 1
  LOG_UPDATE 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
)
ExternalProject_Add_Step(dpdk confpatch
  COMMENT "Applying patch to DPDK configuration needed by ODP-DPDK"
  COMMAND sed -i "s,CONFIG_RTE_BUILD_COMBINE_LIBS=.*,CONFIG_RTE_BUILD_COMBINE_LIBS=y," x86_64-native-linuxapp-gcc/.config
  COMMAND sed -i "s,CONFIG_RTE_BUILD_SHARED_LIB=.*,CONFIG_RTE_BUILD_SHARED_LIB=n," x86_64-native-linuxapp-gcc/.config
  COMMAND sed -i "s,CONFIG_RTE_LIBRTE_PMD_PCAP=.*,CONFIG_RTE_LIBRTE_PMD_PCAP=y," x86_64-native-linuxapp-gcc/.config
  WORKING_DIRECTORY <SOURCE_DIR>
  DEPENDERS build
  DEPENDEES configure
  LOG 1
)


# Allow includes to find from headers from this dir.
include_directories(.)

add_subdirectory(freeflow)
add_subdirectory(flowctl)
add_subdirectory(flowmgr)
add_subdirectory(flowpath)
add_subdirectory(util)

