set(CMAKE_CXX_FLAGS "-Wall -std=c++1y -fPIC")

add_library(flowpath-applib STATIC
  action.cpp
  binding.cpp
  endian.cpp
  libsteve.cpp
)
