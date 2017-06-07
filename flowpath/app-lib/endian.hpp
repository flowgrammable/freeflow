#ifndef FP_ENDIAN_HPP
#define FP_ENDIAN_HPP

// Module for detecting native order endianess and converting to the
// appropriate order.

//#include <boost/endian/conversion.hpp>
#include <algorithm>

#include "../types.hpp" // TODO: FIXME


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
// Big endian is network byte order so no reverse is necessary
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

#endif
