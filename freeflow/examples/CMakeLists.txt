
# A helper macro for adding example programs.
macro(add_example target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} freeflow)
endmacro()


add_example(unix-echo-server unix-sockets/echo-server.cpp)

if(NOPROTO_USE_PCAP)
  add_example(pcap-read capture/reader.cpp)
endif()

