#include "builtin.hpp"

#include <iostream>

inline void log(fp::Dataplane& dp, fp::Context& _cxt_)
{
  std::cout << "logging this packet\n";
}


// Gathers all appropriate keys into a byte array
inline fp::Key 
gather(fp::Context& _cxt_, std::vector<int> const& key_vector)
{
  int size = 0;
  // allocate maximum size for array
  fp::Byte key[128];

  for (int i : key_vector) {
    // fetch the bytes and the length from the context
    std::pair<fp::Byte*, int> lookup = _cxt_.read_field(i);

    std::copy(lookup.first, lookup.first + lookup.second, &key[size]);
    size += lookup.second;
  }

  return fp::Key(key, size);
}


// Dispatch a context to a table
void __match(fp::Context& _cxt_, Exact_table& t) 
{
  fp::Key key = gather(_cxt_, t.key_vector());

  auto flow = t.table().find(key);
}


// Bind the header and its starting offset
void __bind_header(fp::Context& _cxt_, int header, int len) 
{
  _cxt_.add_header_binding(header, _cxt_.current_pos(), len);
}


// Bind a field, its starting offset, and its length
void __bind_offset(fp::Context& _cxt_, int field, int len)
{
  _cxt_.add_field_binding(field, _cxt_.current_pos(), len);
}


// Advances a context's current lookahead byte by n
void __advance(fp::Context& _cxt_, int n)
{
  _cxt_.set_pos(n);
}