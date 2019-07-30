
include_directories(${Mathematica_INCLUDE_DIRS})

add_library(wstp-link SHARED
	wstp.cpp
	wstp_link.cpp
	wstp_server.cpp
)

target_link_libraries(wstp-link	${Mathematica_WSTP_LIBRARIES})

