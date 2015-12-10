# Build application libraries directly in this location, making
# them easier to find by the linker.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

macro(add_application name)
  add_library(${name} SHARED ${ARGN})
  set_target_properties(${name} PROPERTIES
    PREFIX ""       # Don't prefix with "lib"
    SUFFIX ".app")  # Use .app as the extension
  target_link_libraries(${name} flowpath-rt)
endmacro()

# Disable hub for now
#add_subdirectory(hub)

add_subdirectory(wire)
