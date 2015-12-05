// Copyright (c) 2015 Flowgrammable.org
// All rights reserved

#include "instruction.hpp"
#include "context.hpp"


namespace fp
{

// -------------------------------------------------------------------------- //
// Evaluation of actions


inline void
evaluate(Context& cxt, Set_action a)
{
}


inline void
evaluate(Context& cxt, Copy_action a)
{
}


inline void
evaluate(Context& cxt, Output_action a)
{
}


inline void
evaluate(Context& cxt, Queue_action a)
{
}


inline void
evaluate(Context& cxt, Group_action a)
{
}


inline void
evaluate(Context& cxt, Drop_action a)
{
}


void
evaluate(Context& cxt, Action a)
{
  switch (a.type) {
    case Action::SET: return evaluate(cxt, a.value.set);
    case Action::COPY: return evaluate(cxt, a.value.copy);
    case Action::OUTPUT: return evaluate(cxt, a.value.output);
    case Action::QUEUE: return evaluate(cxt, a.value.queue);
    case Action::GROUP: return evaluate(cxt, a.value.group);
    case Action::DROP: return evaluate(cxt, a.value.drop);
  }
}


void
evaluate(Context& cxt, Action_list const& acts)
{
  for (Action const& a : acts)
    evaluate(cxt, a);
}


} // namespace fp
