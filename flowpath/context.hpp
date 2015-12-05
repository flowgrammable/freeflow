#ifndef FP_CONTEXT_HPP
#define FP_CONTEXT_HPP

#include "packet.hpp"
#include "action.hpp"
#include "port.hpp"
#include "types.hpp"

#include <cstdint>
#include <utility>

namespace fp
{

#define MAX_BINDINGS 4

struct Table;
struct Flow;
class Port;


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


// Packet metadata. This is an unstructured blob
// to be used as scratch data by the application.
//
// FIXME: This should be dynamically allocated on
// application load and indexed by fields.
struct Metadata
{
  uint64_t data;
};


// FIXME: This is a terribly named class.
struct Context_current
{
  uint16_t pos;  // The current header offset?
  Table* table;
  Flow*  flow;
};


// Context visible to the dataplane.
struct Context
{
  Context(Packet*, uint32_t, uint32_t, int, int, int);
  ~Context() { }

  Packet const* packet() const { return packet_; }
  Packet*       packet()       { return packet_; }

  Metadata const& metadata() const { return metadata_; }
  Metadata&       metadata()       { return metadata_; }

  std::uint16_t get_pos() const          { return current_.pos; }
  void          set_pos(std::uint16_t n) { current_.pos = n; }

  Table*   current_table() const { return current_.table; }
  Flow*    current_flow() const  { return current_.flow; }

  Byte* get_current_byte() { return nullptr; }

  void            write_metadata(uint64_t);
  Metadata const& read_metadata();

  // only the application context defines these.
  // these virtual functions merely provide an interface

  // FIXME: Implement these.
  std::pair<Byte*, int> read_field(uint32_t f) { return {}; }
  std::pair<Byte*, int> read_header(uint32_t h) { return {}; }

  // Aciton interface
  void apply_action(Action a);
  void write_action(Action a);
  void apply_actions();
  void clear_actions();

  // FIXME: Implement me.
  void add_field_binding(uint32_t f, uint16_t o, uint16_t l) { }
  void pop_field_binding(uint32_t f) { }
  void add_header_binding(uint32_t h, uint16_t o, uint16_t l) { }
  void pop_header_binding(uint32_t h) { }

  Packet*         packet_;
  Metadata        metadata_;
  Context_current current_;

  // Input context.
  uint32_t in_port;
  uint32_t in_phy_port;
  int      tunnel_id;
  uint32_t out_port;

  // Actions
  Action_list actions_;

  // Header and field bindings
  Environment hdr_;
  Environment fld_;
};


// Add the given action to the context's action set.
// These actions are applied prior to egress.
inline void
Context::write_action(Action a)
{
  actions_.push_back(a);
}

// Apply all of the saved actions.
inline void
Context::apply_actions()
{
  for (Action const& a : actions_)
    apply_action(a);
}


// Reset the action list.
inline void
Context::clear_actions()
{
  actions_.clear();
}



} // namespace fp


#endif
