
#ifndef FREEFLOW_SYSTEM_HPP
#define FREEFLOW_SYSTEM_HPP

// The system module is a collection of miscellaneous POSIX
// facilities.
//
// This module provides the definitions in unistd.h.

#include <string>

#include <unistd.h>

namespace ff
{

// Unlink the path given in `str`.
inline int
unlink(std::string const& str)
{
  return ::unlink(str.c_str());
}


} // namespace ff

#endif
