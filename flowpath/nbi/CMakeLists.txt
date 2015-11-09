project(fpio CXX)

# C++ build flags.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pedantic -Wall -pthread -Wl,--no-as-needed -ldl")

include_directories(.)

add_executable(fpio nbi.cpp main.cpp)