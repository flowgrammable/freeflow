#ifndef FP_CONTEXT_HPP
#define FP_CONTEXT_HPP

#include "packet.hpp"
#include "port.hpp"
#include "app-lib/action.hpp"  // TODO: Refactor this out
#include "app-lib/binding.hpp" // TODO: Refactor this out
#include "types.hpp"

#include <utility>

namespace fp
{
class Table;
struct Flow;
class Port;


// Stores information about the ingress of a packet
// into a dataplane.
//
// TODO: Save the time stamp here?
struct Ingress_info
{
  unsigned int in_port;
  unsigned int in_phy_port;
  int   tunnel_id;
};


// Maintains information about the control flow of a
// context through a pipeline.
//
// FIXME: Am I actually using the table and flow?
struct Control_info
{
  unsigned int out_port; // The selected output port.
  Table* table;
  Flow*  flow;
};


// Maintains information about the current decoding
// of the packet.
//
// FIXME: These data structures should be required by
// the application, since a) not every data application
// needs full support for rebinding and b) making that
// assumption will be an unfortunate pessimization.
struct Decoding_info
{
  uint16_t pos;
  Environment hdrs;
  Environment flds;
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
//struct Context_current
//{
//  uint16_t pos;  // The current header offset?
//  Table* table;
//  Flow*  flow;
//};


// Represents a header in the header stack. This
// is represented by its offset and a "link" to
// the next header in the stack. The last header
// has next == -1.
//struct Header
//{
//  int offset;
//  int next;
//};


// A bounded stack of headers. This is the data structure
// manipulated by push/pop operations.
//
// TODO: Make this dynamically resizable?
//
// FIXME: If we insert a new header, then we may need to
// point to an address space that is neither metadata
// nor packet. Consider adding an address field to the
// Header structure.
//struct Header_stack
//{
//  void push(int offset);

//  Header hdrs[16];
//  int    top;
//};


// Context visible to the dataplane.
//
// TODO: The use of member functions may prevent optimizations
// due to aliasing issues.
class Context
{
public:
  Context() = default;
//  Context(Packet*, uint32_t, uint32_t, int, int, int);

  // Iniitalize the context with a packet.
  Context(Packet* p, Dataplane* dp)
    : input_(), ctrl_(), decode_(), packet_(p), dp_(dp)
  { }
  Context(Packet* p, Dataplane* dp, unsigned int in, unsigned int in_phy, int tunnelid)
    : input_{in, in_phy, tunnelid}, ctrl_(), decode_(), packet_(p), dp_(dp)
  { }

  ~Context() = default;

  // Disable copy construction and copy assignment:
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  // Default move constructiona and move assignment:
  Context(Context&&) = default;
  Context& operator=(Context&&) = default;

  // Returns the packet owned by the context.
  Packet const& packet() const { return *packet_; }
  Packet&       packet()       { return *packet_; }

  // Returns a pointer to the dataplane which created the context.
  Dataplane const* dataplane() const { return dp_; }

  // Returns the metadata owned by the context.
  Metadata const& metadata() const { return metadata_; }
  Metadata&       metadata()       { return metadata_; }

  // Packet header access.
  void          advance(std::uint16_t n);
  std::uint16_t offset() const;
  Byte const*   position() const;
  Byte*         position();
  int           size() const { return packet_->size(); }

  // Returns the input and output ports associated with
  // the context.
  // Port*   input_port() const          { return input_.in_port; }
  // Port*   input_physical_port() const { return input_.in_phy_port; }
  unsigned int output_port() const { return ctrl_.out_port; }
  unsigned int in_port()     const { return input_.in_port; }
  unsigned int in_phy_port() const { return input_.in_phy_port; }

  // Sets the output port.
  void set_output_port(unsigned int p) { ctrl_.out_port = p; }

  // Returns the current
  Table*   current_table() const { return ctrl_.table; }
  Flow*    current_flow() const  { return ctrl_.flow; }

  void            write_metadata(uint64_t);
  Metadata const& read_metadata();

  // Aciton interface
  void apply_action(Action a);
  void write_action(Action a);
  void apply_actions();
  void clear_actions();

  void bind_header(int);
  void bind_field(int, std::uint16_t, std::uint16_t);
  Byte const* get_field(std::uint16_t) const;
  Byte*       get_field(std::uint16_t);

  Binding const& get_field_binding(int) const;
  Binding&       get_field_binding(int);

  Ingress_info  input_;
  Control_info  ctrl_;
  Decoding_info decode_;

  // Packet data and context local data.
  //
  // TODO: I suspect that metadata should also be a pointer.
  Packet*   packet_;
  Metadata metadata_;

  // The action set.
  Action_set actions_;

  // A pointer to the dataplane which constructed the context.
  Dataplane* dp_; // TODO: factor me out?
};


/// Advance the current header offset by n bytes.
inline void
Context::advance(std::uint16_t n)
{
  decode_.pos += n;

  if (decode_.pos > size())
    throw std::exception();
}


// Returns the current header offset within the packet.
inline std::uint16_t
Context::offset() const
{
  return decode_.pos;
}


inline void
Context::bind_header(int id)
{
  decode_.hdrs.push(id, offset());
}


inline void
Context::bind_field(int id, std::uint16_t off, std::uint16_t len)
{
  decode_.flds.push(id, {off, len});
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


// Returns the binding for the given field.
inline Binding const&
Context::get_field_binding(int fld) const
{
  return decode_.flds[fld].top();
}


// Returns the binding for the given field.
inline Binding&
Context::get_field_binding(int fld)
{
  return decode_.flds[fld].top();
}

} // namespace fp


// -------------------------------------------------------------------------- //
// Application interface

extern "C"
{

unsigned int fp_context_get_input_port(fp::Context*);
void         fp_context_set_output_port(fp::Context*, unsigned int);

}


#endif
