
#ifndef FP_FLOW_HPP
#define FP_FLOW_HPP

#include "types.hpp"

namespace fp
{

struct Dataplane;
struct Context;

// Flow instructions are a pointer to a function to
// be executed on match.
using Flow_instructions = void (*)(Dataplane&, Context&);


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
  std::size_t       pri_;
  Flow_counters     count_;
  Flow_instructions instr_;
  Flow_timeouts     time_;
  std::size_t       cookie_;
  std::size_t       flags;
};


} // namespace fp

#endif
