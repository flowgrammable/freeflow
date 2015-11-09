set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--no-as-needed -ldl")

add_executable(app-test app.cpp)
target_link_libraries(app-test fp-app-lib)