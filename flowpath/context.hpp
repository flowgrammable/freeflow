#ifndef FP_CONTEXT_HPP
#define FP_CONTEXT_HPP

#include "packet.hpp"
#include "port.hpp"
#include "action.hpp"
#include "binding.hpp"
#include "types.hpp"

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


// Represents a header in the header stack. This
// is represented by its offset and a "link" to
// the next header in the stack. The last header
// has next == -1.
struct Header
{
  int offset;
  int next;
};


// A bounded stack of headers. This is the data structure
// manipulated by push/pop operations.
//
// TODO: Make this dynamically resizable?
//
// FIXME: If we insert a new header, then we may need to
// point to an address space that is neither metadata
// nor packet. Consider adding an address field to the
// Header structure.
struct Header_stack
{
  void push(int offset);

  Header hdrs[16];
  int    top;
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

  // Aciton interface
  void apply_action(Action a);
  void write_action(Action a);
  void apply_actions();
  void clear_actions();

  // FIXME: Implement me.
  void bind_header(int);
  void bind_field(int, std::uint16_t, std::uint16_t);
  Byte const* get_field(std::uint16_t) const;
  Byte*       get_field(std::uint16_t);
  Binding     get_field_binding(int) const;

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

inline void
Context::bind_header(int id)
{
  hdr_.push(id, offset());
}


inline void
Context::bind_field(int id, std::uint16_t off, std::uint16_t len)
{
  fld_.push(id, {off, len});
}


// Returns a pointer to a given field at the absolute offset.
inline Byte const*
Context::get_field(std::uint16_t off) const
{
  return packet_->data() + off;
}


// Returns a pointer to a given field at the absolute offset.
inline Byte*
Context::get_field(std::uint16_t off)
{
  return packet_->data() + off;
}


// Returns the binding for the given field.
inline Binding
Context::get_field_binding(int fld) const
{
  return fld_[fld].top();
}


} // namespace fp


#endif
