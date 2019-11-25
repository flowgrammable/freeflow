#add_executable(wire-driver-tcp wire-tcp.cpp)
#target_link_libraries(wire-driver-tcp flowpath-rt ${CMAKE_DL_LIBS})

#find_package(ZLIB REQUIRED)

# Temporarily disable verbose warnings for this project...
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable -Wno-reorder -Wno-unused-function")

add_executable(caida-pcap caida-pcap.cpp util_caida.cpp util_extract.cpp)
add_executable(flow-count flow-count.cpp util_caida.cpp util_extract.cpp util_io.cpp)
add_executable(flow-records flow-records.cpp util_caida.cpp util_extract.cpp util_io.cpp)
add_executable(flow-min flow-min.cpp sim_min.cpp sim_lru.cpp util_caida.cpp util_extract.cpp util_io.cpp)
add_executable(flow-link flow-link.cpp sim_min.cpp sim_opt.cpp sim_lru.cpp util_caida.cpp util_extract.cpp util_io.cpp)
add_executable(flow-stats flow-stats.cpp sim_min.cpp sim_lru.cpp util_caida.cpp util_extract.cpp util_io.cpp)

target_link_libraries(caida-pcap flowpath-rt flowpath-applib ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})
target_link_libraries(flow-count flowpath-rt flowpath-applib absl::container ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})
target_link_libraries(flow-records flowpath-rt flowpath-applib absl::container ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})
target_link_libraries(flow-min flowpath-rt flowpath-applib absl::container ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})
target_link_libraries(flow-link flowpath-rt flowpath-applib wstp-link absl::container ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})
target_link_libraries(flow-stats flowpath-rt flowpath-applib wstp-link absl::container ${PCAP_LIBRARY} ${CMAKE_DL_LIBS})

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
target_link_libraries(flow-min ${Boost_LIBRARIES})
target_link_libraries(flow-link ${Boost_LIBRARIES})
target_link_libraries(flow-stats ${Boost_LIBRARIES})

# Set runtime library search path:
#set_target_properties(flow-link PROPERTIES INSTALL_RPATH "${Mathematica_RUNTIME_LIBRARY_DIRS}")
#set_target_properties(flow-link PROPERTIES FOLDER "WSTP")

# Fix WSTP shared library references under Mac OS X
#Mathematica_ABSOLUTIZE_LIBRARY_DEPENDENCIES(flow-link)
#if (DEFINED Mathematica_USERBASE_DIR)
#  foreach (_systemID ${Mathematica_SYSTEM_IDS})
#    install(TARGETS ${_Targets}
#      RUNTIME DESTINATION
#        "${Mathematica_USERBASE_DIR}/SystemFiles/Kernel/Binaries/${_systemID}"
#      CONFIGURATIONS "Release")
#  endforeach()
#endif()
