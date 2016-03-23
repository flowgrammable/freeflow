
macro(add_driver target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} fp-lite-rt ${CMAKE_DL_LIBS})
endmacro()

add_driver(fp-wire-select wire-select.cpp)
add_driver(fp-wire-poll  wire-poll.cpp)
add_driver(fp-wire-epoll-sta wire-epoll-sta.cpp)
add_driver(fp-wire-epoll-tpp wire-epoll-tpp.cpp)
