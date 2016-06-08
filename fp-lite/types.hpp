#ifndef FP_TYPES_HPP
#define FP_TYPES_HPP

// Defines common types in the system.

#include <cstddef>
#include <cstdint>


namespace fp
{

// An integer value of 8 bits.
using Byte = uint8_t;


// packed 24 bit integer
// wraps a 32 bit integer
#pragma pack(push, 1)
struct uint24_t {
  uint24_t()
    : i(0)
  { }

  uint24_t(unsigned int i)
    : i(i)
  { }

  unsigned int i : 24;
};
#pragma pack(pop)


// packed 48 bit integer
// wraps a 64 bit integer
#pragma pack(push, 1)
struct uint48_t {
  uint48_t()
    : i(0)
  { }

  uint48_t(uint64_t i)
    : i(i)
  { }

  uint64_t i : 48;
};
#pragma pack(pop)


} // end namespace fp


#endif
