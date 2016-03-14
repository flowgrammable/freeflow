// Module for detecting native order endianess and converting to the
// appropriate order.

#include <endian.h>
#include <algorithm>
#include <iostream>
#include "types.hpp"


namespace fp
{

#if __BYTE_ORDER == __BIG_ENDIAN

// Big endian is network byte order so no reverse is necessary
inline void
network_to_native_order(fp::Byte* buf, int len)
{
}

inline void
native_to_network_order(fp::Byte* buf, int len)
{
}

#endif


#if __BYTE_ORDER == __LITTLE_ENDIAN

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
