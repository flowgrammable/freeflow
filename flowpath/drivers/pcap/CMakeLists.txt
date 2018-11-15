#add_executable(wire-driver-tcp wire-tcp.cpp)
#target_link_libraries(wire-driver-tcp flowpath-rt ${CMAKE_DL_LIBS})

#find_package(ZLIB REQUIRED)

# Temporarily disable verbose warnings for this project...
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable -Wno-reorder -Wno-unused-function")

add_executable(caida-pcap caida-pcap.cpp util_caida.cpp util_extract.cpp)
add_executable(flow-count flow-count.cpp util_caida.cpp util_extract.cpp util_io.cpp)
add_executable(flow-records flow-records.cpp util_caida.cpp util_extract.cpp util_io.cpp)

target_link_libraries(caida-pcap flowpath-rt flowpath-applib ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})
target_link_libraries(flow-count flowpath-rt flowpath-applib absl::container ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})
target_link_libraries(flow-records flowpath-rt flowpath-applib absl::container ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})

#if (ENABLE_ODP)
#  add_definitions(-DFP_ENABLE_ODP)
#  add_executable(pcap-driver-odp caida-odp.cpp)
#  target_link_libraries(pcap-driver-odp flowpath-rt flowpath-applib ${CMAKE_DL_LIBS})
#endif()

# Require Boost C++ Libraries.
find_package(Boost 1.55.0 COMPONENTS program_options iostreams REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(caida-pcap ${Boost_LIBRARIES})
target_link_libraries(flow-count ${Boost_LIBRARIES})
target_link_libraries(flow-records ${Boost_LIBRARIES})

