
enable_testing()

# Add a unit test the given target and file name. Note that unit tests
# must be prefixed sys_${mod}, where ${mod} is the name of the module
# for which the test is written.
#
# Necessary libraries are listed following the name of the source
# file. For example:
#
#     add_unit_test(test_string string.cpp freeflow freeflow-ofp)
#
# Libraries can also be passed as a list variable.
macro(add_unit_test tgt file)
  add_executable(${tgt} ${file})
  target_link_libraries(${tgt} ${ARGN})
  add_test(test_${tgt} ${tgt})
endmacro()

#add_subdirectory(hub)
add_subdirectory(wire)
add_subdirectory(pcap)

