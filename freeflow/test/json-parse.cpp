
#include "freeflow/json.hpp"
#include "freeflow/format.hpp"

#include <iostream>

using namespace ff;


int 
main(int argc, char** argv)
{
  if (argc != 2) {
    std::cerr << "usage: json-parse [json-text]\n";
    return -1;
  }

  char const* str = argv[1];

  print("input: '{}'\n", str);
  Sequential_allocator alloc(4096);
  json::Document doc(alloc);
  json::Value* v = doc.parse(str);
  print("output: '{}'\n", *v);
  std::cout << v << '\n';
}
