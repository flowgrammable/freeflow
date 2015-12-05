// Copyright (c) 2015 Flowgrammable.org
// All rights reserved

#include "instruction.hpp"
#include "context.hpp"


namespace fp
{

// -------------------------------------------------------------------------- //
// Evaluation of actions

// FIXME: We should probably never get here.
inline void
evaluate(Context* cxt, Get_action a)
{
}


inline void
evaluate(Context* cxt, Set_action a)
{
}


inline void
evaluate(Context* cxt, Copy_action a)
{
}


inline void
evaluate(Context* cxt, Output_action a)
{
}


inline void
evaluate(Context* cxt, Queue_action a)
{
}


inline void
evaluate(Context* cxt, Group_action a)
{
}


inline void
evaluate(Context* cxt, Drop_action a)
{
}


void
evaluate(Context* cxt, Action a)
{
  switch (a.type) {
    case Action::GET: return evaluate(cxt, a.value.get);
    case Action::SET: return evaluate(cxt, a.value.set);
    case Action::COPY: return evaluate(cxt, a.value.copy);
    case Action::OUTPUT: return evaluate(cxt, a.value.output);
    case Action::QUEUE: return evaluate(cxt, a.value.queue);
    case Action::GROUP: return evaluate(cxt, a.value.group);
    case Action::DROP: return evaluate(cxt, a.value.drop);
  }
}


void
evaluate(Context* cxt, Action_list const& acts)
{
  for (Action const& a : acts)
    evaluate(cxt, a);
}


// -------------------------------------------------------------------------- //
// Evaluation of instructions

inline void
evaluate(Context* cxt, Apply_instruction i)
{
  return evaluate(cxt, i.action);
}


inline void
evaluate(Context* cxt, Write_instruction i)
{
}


inline void
evaluate(Context* cxt, Clear_instruction i)
{
}


inline void
evaluate(Context* cxt, Goto_instruction i)
{
}



void
evaluate(Context* cxt, Instruction i)
{
  switch (i.type) {
    case Instruction::APPLY: return evaluate(cxt, i.value.apply);
    case Instruction::WRITE: return evaluate(cxt, i.value.write);
    case Instruction::CLEAR: return evaluate(cxt, i.value.clear);
    case Instruction::GOTO: return evaluate(cxt, i.value.go);
  }
}


void
evaluate(Context* cxt, Instruction_list const& insns)
{
  for (Instruction const& i : insns)
    evaluate(cxt, i);
}


} // namespace fp
