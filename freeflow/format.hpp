
#ifndef NOCTL_FORMAT_HPP
#define NOCTL_FORMAT_HPP

#include "contrib/cppformat/format.h"

#include <iosfwd>

namespace ff 
{

// The format facility.
using fmt::format;


// The print facility.
using fmt::print;
using fmt::fprintf;
using fmt::sprintf;


// Text formatters.
using fmt::bin; 
using fmt::oct;
using fmt::hex;
using fmt::pad;


// Import the Writer class as an alternative to stringstream.
using fmt::Writer;


} // namespace

#endif
