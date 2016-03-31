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


// Stores information about the ingress of a packet
// into a dataplane.
//
// TODO: Save the time stamp here?
struct Ingress_info
{
  Port* in_port;
  Port* in_phy_port;
  int   tunnel_id;
};


// Maintains information about the control flow of a
// context through a pipeline.
//
// FIXME: Am I actually using the table and flow?
struct Control_info
{
  Port*  out_port; // The selected output port.
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
// TODO: Multiple contexts can share a packet if none modify
// it's buffer.
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
class Context
{
public:
  
  Context()
    : input_(), ctrl_(), decode_(), packet_()
  { }

  // Iniitalize the context with a packet.
  Context(Packet p)
    : input_(), ctrl_(), decode_(), packet_(p)
  { }

  // Copy constructor.
  Context(Context const& other)
  {
    *this = other;
  }

  // Context dtor.
  ~Context() { }

  Context& operator=(Context const& other)
  {
    input_ = other.input_;
    ctrl_ = other.ctrl_;
    decode_ = other.decode_;
    packet_ = other.packet_;
    metadata_ = other.metadata_;
    actions_ = other.actions_;
    return *this;
  }

  // Returns the packet owned by the context.
  Packet const& packet() const { return packet_; }
  Packet&       packet()       { return packet_; }

  // Returns the metadata owned by the context.
  Metadata const& metadata() const { return metadata_; }
  Metadata&       metadata()       { return metadata_; }

  // Packet header access.
  void          advance(std::uint16_t n);
  std::uint16_t offset() const;
  Byte const*   position() const;
  Byte*         position();

  // Returns the input and output ports associated with
  // the context.
  Port*   input_port() const          { return input_.in_port; }
  Port*   input_physical_port() const { return input_.in_phy_port; }
  Port*   output_port() const         { return ctrl_.out_port; }

  // Sets the output port.
  void set_output_port(Port* p) { ctrl_.out_port = p; }

  // Sets the input port, physical input port, and tunnel id.
  void set_input(Port*, Port*, int);



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

  // FIXME: Implement me.
  void bind_header(int);
  void bind_field(int, std::uint16_t, std::uint16_t);
  Byte const* get_field(std::uint16_t) const;
  Byte*       get_field(std::uint16_t);
  Binding     get_field_binding(int) const;

  Ingress_info  input_;
  Control_info  ctrl_;
  Decoding_info decode_;

  // Packet data and context local data.
  //
  // TODO: I suspect that metadata should also be a pointer.
  Packet   packet_;
  Metadata metadata_;

  // The action set.
  Action_set actions_;
};


// Advance the current header offset by n bytes.
inline void
Context::advance(std::uint16_t n)
{
  decode_.pos += n;
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
  return packet_.data() + off;
}


// Returns a pointer to a given field at the absolute offset.
inline Byte*
Context::get_field(std::uint16_t off)
{
  return packet_.data() + off;
}


// Returns the binding for the given field.
inline Binding
Context::get_field_binding(int f) const
{
  return decode_.flds[f].top();
}


// Returns a pointer to the current header.
inline Byte const*
Context::position() const
{
  return packet_.data() + offset();
}


// Returns a pointer to the current header.
inline Byte*
Context::position()
{
  return packet_.data() + offset();
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


// Sets the input port, physical input port, and tunnel id.
inline void
Context::set_input(Port* in, Port* in_phys, int tunnel)
{
  input_ = {
    in,
    in_phys,
    tunnel
  };
}


} // namespace fp


// -------------------------------------------------------------------------- //
// Application interface

extern "C"
{

fp::Port* fp_context_get_input_port(fp::Context*);
void      fp_context_set_output_port(fp::Context*, fp::Port*);


} // extern "C"


#endif
