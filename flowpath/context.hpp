#ifndef FP_CONTEXT_HPP
#define FP_CONTEXT_HPP

#include "packet.hpp"
#include "port.hpp"
#include "action.hpp"
#include "binding.hpp"
#include "types.hpp"

#include <cstdint>
#include <utility>

namespace fp
{

struct Table;
struct Flow;
class Port;


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
//
// TODO: The use of member functions may prevent optimizations
// due to aliasing issues.
struct Context
{
  Context(Packet*, uint32_t, uint32_t, int, int, int);
  ~Context() { }

  Packet const* packet() const { return packet_; }
  Packet*       packet()       { return packet_; }

  Metadata const& metadata() const { return metadata_; }
  Metadata&       metadata()       { return metadata_; }

  // Packet header access.
  void          advance(std::uint16_t n);
  std::uint16_t offset() const;
  Byte const*   position() const;
  Byte*         position();

  Table*   current_table() const { return current_.table; }
  Flow*    current_flow() const  { return current_.flow; }

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
  void bind_header(int) { }
  void bind_field(int, std::uint16_t, std::uint16_t) { }
  std::uint16_t get_header(int) const { return 0; }
  std::uint16_t get_field(int) const { return 0; }

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
  Action_set actions_;

  // Header and field bindings.
  // FIXME: These data structures should be required by
  // the applicaiton, since a) not every data application
  // needs full support for rebinding and b) making that
  // assumption will be an unfortunate pessimization.
  Environment hdr_;
  Environment fld_;
};


// Advance the current header offset by n bytes.
inline void
Context::advance(std::uint16_t n)
{
  current_.pos += n;
}


// Returns the current header offset within the packet.
inline std::uint16_t
Context::offset() const
{
  return current_.pos;
}


// Returns a pointer to the current header.
inline Byte const*
Context::position() const
{
  return packet_->data() + offset();
}


// Returns a pointer to the current header.
inline Byte*
Context::position()
{
  return packet_->data() + offset();
}


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
