include_directories(../..)

# Thread test.
add_executable(thread-single thread.cpp)
target_link_libraries(thread-single flowpath-rt ${CMAKE_DL_LIBS})

# Thread pool test.
add_executable(thread-pool pool.cpp)
target_link_libraries(thread-pool flowpath-rt ${CMAKE_DL_LIBS})