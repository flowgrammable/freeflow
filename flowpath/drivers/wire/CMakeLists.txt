add_executable(wire-driver wire.cpp)
target_link_libraries(wire-driver flowpath-rt ${CMAKE_DL_LIBS})