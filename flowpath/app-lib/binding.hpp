// Copyright (c) 2015 Flowgrammable.org
// All rights reserved

#ifndef FP_BINDING_HPP
#define FP_BINDING_HPP

#include "types.hpp"

#include <cassert>


namespace fp
{

#define MAX_BINDINGS 4


// Represents a field binding within the packet. Each field
// is described by an offset/length pair. The absolute
// address of the field is relative to some address within
// the packet (usually the header offset).
//
// A partial binding is one that specifies an offset but
// not a length. This is useful for header bindings where
// the length is not calculated (e.g., the last header
// analyzed).
struct Binding
{
  Binding() = default;

  Binding(std::uint16_t o, std::uint16_t n)
    : offset(o), length(n)
  { assert(n != 0); }

  Binding(std::uint16_t o)
    : offset(o), length(0)
  { }

  bool is_partial() const { return length == 0; }

  std::uint16_t offset;
  std::uint16_t length;
};


// A binding list stores the innermost field binding
// for a particular field name. We allow multiple bindings
// of the same field to accommodate the decoding of packets
// with nested structures. Currently, we limit the total
// number of bindings to 4.
//
// FIXME: Support arbitrarily deep or shallow binding lists.
struct Binding_list
{
  constexpr static int max_length = 4;

  Binding_list()
    : current(-1)
  { }

  bool is_empty() const { return current == -1; }
  bool is_full() const  { return current == max_length - 1; }

  Binding const& top() const;
  Binding&       top();
  Binding const& bottom() const;
  Binding&       bottom();

  void push(Binding);
  void push(std::uint16_t);
  void pop();

  Binding bindings[max_length];
  int     current;
};


// Returns the innermost binding in the list.
inline Binding const&
Binding_list::top() const
{
  assert(!is_empty());
  return bindings[current];
}


inline Binding&
Binding_list::top()
{
  assert(!is_empty());
  return bindings[current];
}


// Returns the outermost binding in the list.
inline Binding const&
Binding_list::bottom() const
{
  assert(!is_empty());
  return bindings[0];
}


inline Binding&
Binding_list::bottom()
{
  assert(!is_empty());
  return bindings[0];
}


// Push a new field binding onto the binding list. Behavior
// is undefined if the pushing the entry would exceed the
// maximum length of the binding list.
inline void
Binding_list::push(Binding b)
{
  assert(!is_full());
  bindings[++current] = b;
}


// Push a partial binding into the binding list.
inline void
Binding_list::push(std::uint16_t n)
{
  assert(!is_full());
  bindings[++current] = Binding(n);
}


// Pop the innermost binding from the binding list.
// Behavior is undefined if the list.
inline void
Binding_list::pop()
{
  assert(!is_empty());
  --current;
}


// A binding environment associates protocol field names
// with the their location and length within a region
// of memory.
//
// Each protocol field is assigned, by the programmer, a
// unique integer value in the range [0, n] where n is
// the number of fields needed by the application. Those
// values must be "remembered" by the programmer.
//
// FIXME: Make the number of fields configurable.
//
// FIXME: What's the right query mechanism here?
struct Environment
{
  static constexpr int max_fields = 32;

  Binding_list const& operator[](int n) const { return fields[n]; }
  Binding_list&       operator[](int n)       { return fields[n]; }

  void push(int n, Binding b);
  void pop(int n);

  Binding_list fields[max_fields];
};


inline void
Environment::push(int n, Binding b)
{
  assert(0 <= n && n < max_fields);
  fields[n].push(b);
}


inline void
Environment::pop(int n)
{
  assert(0 <= n && n < max_fields);
  fields[n].pop();
}



} // namespace fp


#endif
