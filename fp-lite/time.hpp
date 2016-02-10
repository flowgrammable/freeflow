#ifndef FP_TIME_HPP
#define FP_TIME_HPP

// The Flowpath Time module. It gives the entire system a uniform
// view of what 'time' is, and provides time relation functionality
// such as timers...
//
// TODO: Implement a flow timer.

#include "types.hpp"

namespace fp
{

using Timestamp = std::uint64_t;


// FIXME: This namespace starts with a capital letter.
namespace Time
{

inline Timestamp current() const;

} // namespace time

} // namespace fp

#endif
