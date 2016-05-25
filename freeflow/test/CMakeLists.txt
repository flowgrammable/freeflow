
# A helper macro for adding test programs.
macro(add_tester target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} freeflow)
endmacro()

macro(add_test_program target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} freeflow)
  add_test(test-${target} ${target})
endmacro()

# Spot-test JSON parsing
# add_tester(json-parse json-parse.cpp)
# add_test(test-json-1 json-parse "true")
# add_test(test-json-2 json-parse "false")
# add_test(test-json-3 json-parse "null")
# add_test(test-json-4 json-parse "0")
# add_test(test-json-5 json-parse "\"\"")
# add_test(test-json-6 json-parse "\"string\"")
# add_test(test-json-7 json-parse "[]")
# add_test(test-json-8 json-parse "[1, 2, 3]")
# add_test(test-json-9 json-parse "{}")
# add_test(test-json-10 json-parse "{\"a\":1, \"b\":2, \"c\":3}")

# TODO: This should be in a performance testing framework.
add_tester(json-bench json-bench.cpp)

add_test_program(json json.cpp)
