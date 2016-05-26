// Copyright (c) 2015 Flowgrammable.org
// All rights reserved

#ifndef FP_ACTION_HPP
#define FP_ACTION_HPP

#include "types.hpp"

#include <vector>


namespace fp
{

struct Context;


// Defined address spaces for fields.
constexpr int Packet_memory   = 0;
constexpr int Metadata_memory = 1;


// A field defines the offset and length of a value in
// memory within some address space. Currently, there
// are only two address spaces: packet and metadata.
//
// If the field refers to packet memory, the offset is
// relative to the current header.
//
// FIXME: Preserve the address space or no?
struct Field
{
  std::uint8_t  address;
  std::uint16_t offset;
  std::uint16_t length;
};


// -------------------------------------------------------------------------- //
// Actions
//
// TODO: Define actions for TTL operations, pushing, and
// popping headers.


// Copies a value into the given field. Note that
// value + field.length must be within the range
// of memory designated by field.address.
struct Set_action
{
  Set_action()
    : field {0, 0, 0}, value(nullptr)
  { }

  Set_action(std::uint8_t addr, std::uint16_t off, std::uint16_t len, Byte* v)
    : field{addr, off, len}, value(new Byte[len])
  {
    std::copy(v, v + len, value);
  }

  Set_action(Set_action const& s)
    : field(s.field), value(new Byte[s.field.length])
  {
    std::copy(s.value, s.value + s.field.length, value);
  }

  ~Set_action() {
    if (value)
      delete [] value;
  }

  Field       field;
  Byte*       value;
};



// Copies a field from a source address space to
// an offset in the other address space.
//
// TODO: What if we have >2 address spaces?
struct Copy_action
{
  Field        field;
  std::uint16_t offset;
};


// Set the output port for the current packet.
struct Output_action
{
  std::uint32_t port;
};


// Sets the output queue for the current packet.
struct Queue_action
{
  std::uint32_t queue;
};


// Sets the group action to apply to this packet.
struct Group_action
{
  std::uint32_t group;
};


// Represents one of a set of actions. Abstactly:
//
//    action ::= set <field> <value>
//               copy <field> <address>
//               output <port>
//               queue <queue>
//               group <group>
struct Action
{
  enum Type : std::uint8_t
  {
    SET, COPY, OUTPUT, QUEUE, GROUP, ACTION
  };

  Action() { }
  Action(Set_action const& s) : value(s), type(SET) { }
  Action(Copy_action const& c) : value(c), type(COPY) { }
  Action(Output_action const& o) : value(o), type(OUTPUT) { }
  Action(Queue_action const& q) : value(q), type(QUEUE) { }
  Action(Group_action const& g) : value(g), type(GROUP) { }
  Action(Action const&);

  ~Action()
  {
    clear();
  }

  void clear();

  union Action_data
  {
    Action_data() { }
    Action_data(Set_action const& s) : set(s) { }
    Action_data(Copy_action const& c) : copy(c) { }
    Action_data(Output_action const& o) : output(o) { }
    Action_data(Queue_action const& q) : queue(q) { }
    Action_data(Group_action const& g) : group(g) { }

    ~Action_data() { }

    Set_action    set;
    Copy_action   copy;
    Output_action output;
    Queue_action  queue;
    Group_action  group;
  };

  Action_data value;
  std::uint8_t type;
};


inline void
Action::clear()
{
  if (type == SET)
    value.set.~Set_action();
}

inline
Action::Action(Action const& a)
{
  type = a.type;
  switch (a.type)
  {
    case SET:
      value = Action_data(a.value.set);
      break;
    case COPY:
      value = Action_data(a.value.copy);
      break;
    case OUTPUT:
      value = Action_data(a.value.output);
      break;
    case QUEUE:
      value = Action_data(a.value.queue);
      break;
    case GROUP:
      value = Action_data(a.value.group);
      break;
  }
}


// A list of actions.
using Action_list = std::vector<Action>;


// The action set maintains a sequence of instructions
// to be executed on a packet (context) prior to egress.
//
// FIXME: This is a highly structured list of actions,
// and the order in which those actions are applied matters.
struct Action_set : Action_list
{

};


} // namespace fp


#endif
