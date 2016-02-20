
# Needed for Boost.functional/hash.
find_package(Boost)

include_directories(${Boost_INCLUDE_DIRS})

# Source for 3rd party libraries.
set(contrib-src
  contrib/cppformat/format.cc)


if(FREEFLOW_USE_PCAP)
  set(pcap-src capture.cpp)
endif()


# The main freeflow library.
add_library(freeflow SHARED
  ${contrib-src}
  format.cpp
  system.cpp
  time.cpp
  memory.cpp
  async.cpp
  poll.cpp
  epoll.cpp
  select.cpp
  socket.cpp
  ip.cpp
  unix.cpp
  json.cpp
  ${pcap-src})

if(FREEFLOW_USE_PCAP)
  target_link_libraries(freeflow ${PCAP_LIBRARIES})
endif()

add_subdirectory(test)
add_subdirectory(examples)
