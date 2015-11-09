#ifndef STEVE_ENV_BUILTIN_HPP
#define STEVE_ENV_BUILTIN_HPP

#include "app_context.hpp"
#include "table.hpp"
#include "types.hpp"

#include <vector>
#include <iostream>

struct Match_table
{
  virtual ~Match_table() { }
};

// Define an exact match table which can keep track
// of which field elements that it matches against
struct Exact_table : Match_table {
  using Key_vector = std::vector<int>;

  Exact_table(Key_vector key_vec, fp::Hash_table ht)
    : key_vec {key_vec}, table_{ht}
  { }

  ~Exact_table() { }

  Key_vector const& key_vector() const { return key_vec; }
  fp::Hash_table const& table() const { return table_; }

  Key_vector key_vec;
  fp::Hash_table table_;
};


// Send a context to a table
void __match(fp::Context&, Exact_table&);

// Dispatch a context to a decoder
// Decoder is a function
template<typename Cxt, typename Decoder>
void __decode(Cxt& _cxt_, Decoder decode)
{
  decode(_cxt_);
}

// Bind the header and its starting offset
void __bind_header(fp::Context&, int, int);

// Bind a field, its starting offset, and its length
void __bind_offset(fp::Context&, int, int);

// Advances a context's current lookahead byte by n
void __advance(fp::Context&, int);


// default flow instruction for now
inline void log(fp::Dataplane& dp, fp::Context& _cxt_);


#endif