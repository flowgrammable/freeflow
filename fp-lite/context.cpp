#include "context.hpp"
#include "endian.hpp"
#include "system.hpp"

namespace fp
{

Context::Context(Packet const& p, Dataplane* dp, unsigned int in, unsigned int in_phy, int tunnelid)
  : input_{in, in_phy, tunnelid}, ctrl_(), decode_(), packet_(p),
    dp_(dp)
{ }

Context::Context(Packet const& p, Dataplane* dp, Port* in, Port* in_phy, int tunnelid)
  : input_{in->id(), in_phy->id(), tunnelid}, ctrl_(), decode_(),
    packet_(p), dp_(dp)
{ }


// Sets the input port, physical input port, and tunnel id.
void
Context::set_input(Port* in, Port* in_phys, int tunnel)
{
  input_ = {
    in->id(),
    in_phys->id(),
    tunnel
  };
}


void
Context::write_metadata(uint64_t meta)
{
  metadata_.data = meta;
}


Metadata const&
Context::read_metadata()
{
  return metadata_;
}


// -------------------------------------------------------------------------- //
// Evaluation of actions

namespace
{

inline void
apply(Context& cxt, Set_action a)
{
  // Copy the new data into the packet at the appropriate location.
  Byte* p = cxt.get_field(a.field.offset);
  Byte* val = a.value;
  int len = a.field.length;

  // Convert native to network order after copying.
  std::copy(val, val + len, p);
  native_to_network_order(p, len);
}


inline void
apply(Context& cxt, Copy_action a)
{
  // TODO: Implement me.
}


inline void
apply(Context& cxt, Output_action a)
{
  cxt.set_output_port(a.port);
}


inline void
apply(Context& cxt, Queue_action a)
{
  // TODO: Implement queues.
}


inline void
apply(Context& cxt, Group_action a)
{
  // TODO: Implement group actions.
}


} // namespace


void
Context::apply_action(Action a)
{
  switch (a.type) {
    case Action::SET: return apply(*this, a.value.set);
    case Action::COPY: return apply(*this, a.value.copy);
    case Action::OUTPUT: return apply(*this, a.value.output);
    case Action::QUEUE: return apply(*this, a.value.queue);
    case Action::GROUP: return apply(*this, a.value.group);
  }
}


} // namespace fp


// -------------------------------------------------------------------------- //
// Application interface

extern "C"
{

unsigned int
fp_context_get_input_port(fp::Context* cxt)
{
  return cxt->input_port_id();
}


void
fp_context_set_output_port(fp::Context* cxt, unsigned int p)
{
  cxt->set_output_port(p);
}


} // extern "C"
