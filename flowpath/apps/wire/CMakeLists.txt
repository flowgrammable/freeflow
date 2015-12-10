add_library(wire SHARED wire.c)
set_target_properties(wire PROPERTIES
  PREFIX ""       # Don't prefix with "lib"
  SUFFIX ".app")  # Use .app as the extension
