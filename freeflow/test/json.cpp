
#include "freeflow/json.hpp"
#include "freeflow/format.hpp"

#include <cassert>
#include <iostream>

using namespace ff;
using namespace ff::json;

char buf[1024];

void
test_1()
{
  char const* str = R"({"a":1, "b":2, "c":3})";
  Buffer_allocator alloc(buf, 1024);
  Document doc(alloc);
  
  Value* v = doc.parse(str);
  Object& o = *cast<Object>(v);
  std::cout << o["a"] << '\n';
  std::cout << o["b"] << '\n';
  std::cout << o["c"] << '\n';
  assert(o["d"] == nullptr);
}


void
test_2()
{
  char const* str = "[4, 5, 6]";
  Buffer_allocator alloc(buf, 1024);
  Document doc(alloc);
  
  Value* v = doc.parse(str);
  Array& a = *cast<Array>(v);
  std::cout << a[0] << '\n';
  std::cout << a[1] << '\n';
  std::cout << a[2] << '\n';
  assert(a[3] == nullptr);
}

int 
main()
{
  test_1();
  test_2();
}
