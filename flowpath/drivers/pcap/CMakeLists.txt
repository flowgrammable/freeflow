#add_executable(wire-driver-tcp wire-tcp.cpp)
#target_link_libraries(wire-driver-tcp flowpath-rt ${CMAKE_DL_LIBS})

#find_package(ZLIB REQUIRED)

add_executable(caida-pcap caida-pcap.cpp)
#target_link_libraries(caida-pcap flowpath-rt flowpath-applib ${CMAKE_DL_LIBS})
#target_link_libraries(caida-pcap flowpath-rt flowpath-applib ${ZLIB_LIBRARIES} ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})
target_link_libraries(caida-pcap flowpath-rt flowpath-applib ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})

#if (ENABLE_ODP)
#  add_definitions(-DFP_ENABLE_ODP)
#  add_executable(pcap-driver-odp caida-odp.cpp)
#  target_link_libraries(pcap-driver-odp flowpath-rt flowpath-applib ${CMAKE_DL_LIBS})
#endif()

