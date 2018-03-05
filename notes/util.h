#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdbool.h>

// Common types with guaranteed widths and signed-ness
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

// Ethernet Ethertype Constants:
#define VLAN 0x8100
#define IPV4 0x0800

// IPV4 Protocol Constants
#define TCP 0x06
#define UDP 0x11

// View structure.  Keeps track of safe range of access within packet.
typedef struct{
  void* begin;
  void* end;
} View;

// Network byte-order translations
// - messy run-time translations...  need to fix with LLVM intrinsic
bool littleEndian;
inline void endianTest() {
	volatile u16 endianTest = 0xABCD;
	u8 end = *((volatile u8*)(&endianTest));
	littleEndian = (end == 0xCD);
}

u16 betoh16(u16 n) {
	if (littleEndian)
		return (n & 0x00FFU)<<8 | (n & 0xFF00U)>>8;
	else
		return n;
}
u32 betoh32(u32 n) {
	if (littleEndian)
		return (n & 0x000000FFUl)<<24 | (n & 0xFF000000Ul)>>24 |
           (n & 0x0000FF00Ul)<<8  | (n & 0x00FF0000Ul)>>8;
	else
		return n;
}
u64 betoh64(u64 n) {
	if (littleEndian)
		return (n&0x00000000000000FFUll)<<56 | (n&0x000000000000FF00Ull)<<40 |
           (n&0x0000000000FF0000Ull)<<24 | (n&0x00000000FF000000Ull)<<8  |
		  		 (n&0x000000FF00000000Ull)>>8  | (n&0x0000FF0000000000Ull)>>24 |
			  	 (n&0x00FF000000000000Ull)>>40 | (n&0xFF00000000000000Ull)>>56;
	else
		return n;
}

#endif // UTIL_H

