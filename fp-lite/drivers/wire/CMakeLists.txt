
macro(add_driver target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} fp-lite-rt ${CMAKE_DL_LIBS})
endmacro()

# Single-threaded select
add_driver(fp-wire-select-st wire-select-st.cpp)

add_driver(fp-wire-poll  wire-poll.cpp)

# Don't build epoll apps for Mac.
if (NOT APPLE)
  add_driver(fp-wire-epoll-sta wire-epoll-sta.cpp)
  add_driver(fp-wire-epoll-tpp wire-epoll-tpp.cpp)
endif()