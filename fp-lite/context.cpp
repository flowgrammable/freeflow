#include "context.hpp"

namespace fp
{

void
Context::write_metadata(uint64_t meta)
{
  metadata_.data = meta;
}


Metadata const&
Context::read_metadata()
{
  return this->metadata_;
}


// -------------------------------------------------------------------------- //
// Evaluation of actions

namespace
{

inline void
apply(Context& cxt, Set_action a)
{

}


inline void
apply(Context& cxt, Copy_action a)
{
}


inline void
apply(Context& cxt, Output_action a)
{
  // FIXME: This requires a lookup.
  // cxt.ctrl_.out_port = a.port;
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

fp::Port*
fp_context_get_input_port(fp::Context* cxt)
{
  return cxt->dataplane()->get_port(cxt->input_port_id());
}


void
fp_context_set_output_port(fp::Context* cxt, fp::Port* p)
{
  cxt->set_output_port(p->id());
}


} // extern "C"
