#ifndef FP_CONTEXT_HPP
#define FP_CONTEXT_HPP

#include "packet.hpp"
#include "port.hpp"
#include "types.hpp"

#include <cstdint>
#include <utility>

namespace fp
{

#define MAX_BINDINGS 4

struct Table;
struct Flow;
struct Port;

struct Binding
{
  Binding()
    : len(0), byte_offset(0)
  { }

  Binding(uint16_t len, uint16_t off)
    : len(len), byte_offset(off)
  { }
  // each field, even if they are the same kind of field,
  // have potentially different lengths. we must keep track of
  // all of them
  uint16_t len;
  uint16_t byte_offset;
};


// Bindings keep track of the location for each field/header
// Bindings are actually a stack of active bindings
// Each field/header can reoccur at most MAX_BINDINGS
// number of times. Any more recurrences are undefined behavior
struct Binding_list {
  Binding_list()
    : top(-1)
  { }

  Binding const* get_top() const { return &bindings[top]; }
  bool     is_empty() const { return (top < 0) ? true : false; }
  bool     is_full() const { return (top == MAX_BINDINGS - 1) ? true : false; }

  void insert(uint16_t len, uint16_t byte_offset)
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
  Binding  bindings[MAX_BINDINGS];
};


template<unsigned int N>
struct Environment {
  Binding_list const& bindings(int n) const { return bindings_[n]; }
  // Environments keep a set of bindings
  // The bindings array is of fixed size
  // and is large enough to contain the maximum
  // amount of fields/headers
  Binding_list bindings_[N];
};


// Metadata fields
// Could be larger amount of data, but 64-bit
// is the openflow standard
struct Metadata {
  uint64_t data;
};


struct Context_current {
  // Current offset position
  uint16_t pos;
  // Pointers to current flow and table
  Table* table;
  Flow* flow;
};


// Context visible to the dataplane
struct Context {
  Context(Packet*, uint32_t, uint32_t, int);
  virtual ~Context() { }

  Packet const* packet() const { return packet_; }
  Metadata const& metadata() const { return metadata_; }

  uint16_t current_pos() const { return current_.pos; }
  Table* current_table() const { return current_.table; }
  Flow* current_flow() const { return current_.flow; }

  Byte* get_current_byte() { return nullptr; }

  void set_pos(uint16_t n) { current_.pos = n; }

  void write_metadata(uint64_t);
  Metadata const& read_metadata();

  // only the application context defines these.
  // these virtual functions merely provide an interface

  // a lookup returns a Byte* and a length value
  virtual std::pair<Byte*, int>     read_field(uint32_t f) { return {nullptr, 0}; }
  virtual std::pair<Byte*, int>     read_header(uint32_t h) { return {nullptr, 0}; }

  virtual void      add_field_binding(uint32_t f, uint16_t o, uint16_t l) { }
  virtual void      pop_field_binding(uint32_t f) { }
  virtual void      add_header_binding(uint32_t h, uint16_t o, uint16_t l) { }
  virtual void      pop_header_binding(uint32_t h) { }

  Packet* packet_;
  Metadata metadata_;
  Context_current current_;
  uint32_t in_port;
  uint32_t in_phy_port;
  int   tunnel_id;
  uint32_t out_port;
};

} // end namespace fp

#endif
