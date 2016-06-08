
add_executable(flowcap 
  main.cpp 
  forward.cpp
  replay.cpp
  expect.cpp
  fetch.cpp)
target_link_libraries(flowcap freeflow pcap)
