
macro(add_driver target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} fp-lite-rt ${CMAKE_DL_LIBS})
endmacro()

add_driver(wire wire.cpp)
