
#include "freeflow/memory.hpp"
#include "freeflow/json.hpp"

// This benchmarking program was copied from one of those
// shown in the blog article posted here:
//
//   https://blog.thousandeyes.com/efficiency-comparison-c-json-libraries/
//
// TODO: Make different versions of this program and compare
// the results using different allocators.

#include <chrono>
#include <iostream>
#include <fstream>

using namespace std;
using namespace std::chrono;
using namespace ff;

static constexpr int ntimes = 10;

int 
main(int argc, char** argv) 
{
  if (argc < 2)
    return -1;
  
  ifstream input(argv[1]);
  string text;

  input.seekg(0, ios::end);   
  text.reserve(input.tellg());
  input.seekg(0, ios::beg);

  text.assign((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());

  // Test with a buffer allocator. Tests show that the allocated
  // json structures are about 90% the size of the original text.
  // So a buffer that is the same size should suffice for most
  // cases.
  //
  // See the buffer declaration below.
  char* buf = new char[text.size()];
  
  steady_clock::time_point start_time = steady_clock::now();
  for (int i = 0; i < ntimes; ++i) {

      // Test with new/delete
      // Allocator alloc;
    
      // Test with a sequential allocator with the given page size.
      // Sequential_allocator alloc(1 << 16);

      // Test the buffer allcoatr.
      Buffer_allocator alloc(buf, text.size());

      json::Document doc(alloc);
      char const* first = text.c_str();
      char const* last = first + text.size();
    
      doc.parse(first, last);

      // int jbytes = alloc.allocated();
      // int cbytes = text.size();
      // std::cout << "Allocated: " << jbytes << '\n';
      // std::cout << "Ratio: " << (double)jbytes / (double)cbytes << '\n';
  }
  steady_clock::time_point end_time = steady_clock::now();

  // Print the average parse time in millisecond.  
  milliseconds ms = duration_cast<milliseconds>(end_time - start_time);
  cout << "Average parse time: " << (ms.count() / ntimes) << "ms\n";
}
