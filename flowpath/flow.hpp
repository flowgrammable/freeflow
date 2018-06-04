
#ifndef FP_FLOW_HPP
#define FP_FLOW_HPP

#include "types.hpp"

namespace fp
{

class Dataplane;
class Context;
class Table;

// Flow instructions are a pointer to a function to
// be executed on match.
using Flow_instructions = void (*)(Table*, Context*);

// Default miss case.
void Drop_miss(Table*, Context*);


// The flow counters mantain counts on matches.
//
// TODO: Implement me.
struct Flow_counters
{
};


// TODO: Implement me.
struct Flow_timeouts
{
};


// A flow is an entry in a flow table.
struct Flow
{
  Flow()
    : pri_(0), count_(), instr_(Drop_miss), time_(), cookie_(0), flags_(0)
  { }

  Flow(std::size_t pri, Flow_counters count, Flow_instructions instr,
       Flow_timeouts time, std::size_t cookie, std::size_t flags)
    : pri_(pri), count_(count), instr_(instr), time_(time), cookie_(cookie),
      flags_(flags)
  { }

  std::size_t       pri_;
  Flow_counters     count_;
  Flow_instructions instr_;
  Flow_timeouts     time_;
  std::size_t       cookie_;
  std::size_t       flags_;
};


} // namespace fp

#endif
