// Copyright (c) 2015 Flowgrammable.org
// All rights reserved

#ifndef FP_BINDING_HPP
#define FP_BINDING_HPP

#include "types.hpp"


namespace fp
{

#define MAX_BINDINGS 4


// A binding...
struct Binding
{
  Binding()
    : len(0), byte_offset(0)
  { }

  Binding(std::uint16_t len, std::uint16_t off)
    : len(len), byte_offset(off)
  { }
  // each field, even if they are the same kind of field,
  // have potentially different lengths. we must keep track of
  // all of them
  std::uint16_t len;
  std::uint16_t byte_offset;
};


// Bindings keep track of the location for each field/header
// Bindings are actually a stack of active bindings
// Each field/header can reoccur at most MAX_BINDINGS
// number of times. Any more recurrences are undefined behavior
struct Binding_list
{
  // Hard coded maximum amount of times a header can
  // reappear during decoding.
  //
  // FIXME: figure out how to make this a configuration thing
  constexpr static int max_dup = 4;

  Binding_list()
    : top(-1)
  { }

  Binding const* get_top() const { return &bindings[top]; }
  bool     is_empty() const { return (top < 0) ? true : false; }
  bool     is_full() const { return (top == max_dup - 1) ? true : false; }

  void insert(std::uint16_t len, std::uint16_t byte_offset)
  {
    top++;
    bindings[top] = Binding(len, byte_offset);
  }

  // we can leave whatever is there and overwrite later
  void pop()
  {
    top--;
  }

  // points to the top of the binding stack
  // we can use the bindings as a stack of active
  // bindings for a field/header
  // if top == -1 then the field hasn't been bound before
  int  top;
  Binding bindings[max_dup];
};


struct Environment
{
  Environment(int max_fields)
    : bindings_(new Binding_list[max_fields])
  { }

  Binding_list const& bindings(int n) const { return bindings_[n]; }
  // Environments keep a set of bindings
  // The bindings array is of fixed size
  // and is large enough to contain the maximum
  // amount of fields/headers
  Binding_list* bindings_;
};



} // namespace fp


#endif
