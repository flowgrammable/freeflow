// Module for detecting native order endianess and converting to the
// appropriate order.

#include "types.hpp"

#include <algorithm>

// Includes Boost configuration macros for testing endiannes, so
// we don't have to rely on 
#include <boost/endian/conversion.hpp>


namespace fp
{

#if BOOST_BIG_ENDIAN

// Big endian is network byte order so no reverse is necessary
inline void
network_to_native_order(fp::Byte* buf, int len)
{
}

inline void
native_to_network_order(fp::Byte* buf, int len)
{
}

#else

// Little endian requires a reversal of bytes.
inline void
network_to_native_order(fp::Byte* buf, int len)
{
  std::reverse(buf, buf + len);
}


inline void
native_to_network_order(fp::Byte* buf, int len)
{
  std::reverse(buf, buf + len);
}

#endif

} // namespace fp
