set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC")

add_library(flowpath-applib STATIC
  action.cpp
  binding.cpp
  endian.cpp
  libsteve.cpp
)
