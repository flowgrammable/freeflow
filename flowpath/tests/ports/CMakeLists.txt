include_directories(../..)

# UDP Port test.
add_executable(test-udp udp.cpp)

target_link_libraries(test-udp flowpath-rt ${CMAKE_DL_LIBS})