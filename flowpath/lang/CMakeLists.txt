add_library(fp-app-lib SHARED
	app_context.cpp
	builtin.cpp
)

target_link_libraries(fp-app-lib flowpath-rt)