
# Needed for Boost.functional/hash.
find_package(Boost)

include_directories(${Boost_INCLUDE_DIRS})

# Source for 3rd party libraries.
set(contrib-src
  contrib/cppformat/format.cc)


if(NOPROTO_USE_PCAP)
  set(pcap-src capture.cpp)
endif()


# The main freeflow library.
add_library(freeflow STATIC
  ${contrib-src}
  format.cpp
  system.cpp
  memory.cpp
  async.cpp
  poll.cpp
  select.cpp
  socket.cpp
  ip.cpp
  unix.cpp
  json.cpp
  ${pcap-src})


if(NOPROTO_USE_PCAP)
  target_link_libraries(freeflow ${PCAP_LIBRARIES})
endif()

add_subdirectory(test)
add_subdirectory(examples)
