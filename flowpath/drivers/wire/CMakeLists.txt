#add_executable(wire-driver-tcp wire-tcp.cpp)
#target_link_libraries(wire-driver-tcp flowpath-rt ${CMAKE_DL_LIBS})

#add_executable(wire-driver-udp wire-udp.cpp)
#target_link_libraries(wire-driver-udp flowpath-rt ${CMAKE_DL_LIBS})

if (ENABLE_ODP)
  add_definitions(-DFP_ENABLE_ODP)
  add_executable(wire-driver-odp wire-odp.cpp)
  target_link_libraries(wire-driver-odp flowpath-rt ${CMAKE_DL_LIBS})
endif()

