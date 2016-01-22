add_executable(wire-driver-tcp wire-tcp.cpp)
target_link_libraries(wire-driver-tcp flowpath-rt ${CMAKE_DL_LIBS})

add_executable(wire-driver-udp wire-udp.cpp)
target_link_libraries(wire-driver-udp flowpath-rt ${CMAKE_DL_LIBS})