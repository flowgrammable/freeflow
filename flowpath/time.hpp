#ifndef FP_TIME_HPP
#define FP_TIME_HPP


// The Flowpath Time module. It gives the entire system a uniform
// view of what 'time' is, and provides time relation functionality
// such as timers...
//
// TODO: Implement a flow timer.
namespace fp
{

using Timestamp = uint64_t;

namespace Time
{

inline Timestamp current() const;

} // end namespace time

} // end namespace fp

#endif