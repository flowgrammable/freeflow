#ifndef FP_PACKET_HPP
#define FP_PACKET_HPP

#include "types.hpp"

#include <cassert>
#include <cstdint>
#include <algorithm>

namespace fp
{

// Buffer type enumeration.
enum Buff_t {
  FP_BUF_NADK,
  FP_BUF_NETMAP,
  FP_BUF_ALLOC,
};


// The Packet type. This is a view on top of an externally allocated
// buffer. The packet does not manage that buffer.
//
// TODO: Should we take the timestamp as a constructor argument or
// create the timestamp inside the packet constructor?
// That is, should it be timestamp(time) or timestamp(get_time()) ?
//
// FIXME: The size of a packet's buffer is almost certainly
// larget than its payload.
struct Packet
{
  Packet(Byte* b, int n)
    : buf_(b), cap_(n), len_(0), ts_(), id_()
  { }

  template<int N>
  Packet(Byte (&buf)[N])
    : Packet(buf, N)
  { }

  // Returns a pointer to the raw buffer.
  Byte const* data() const { return buf_; }
  Byte*       data()       { return buf_; }
  
  // Returns the total capacity of the buffer containing the packe.
  int capacity() const { return cap_; }
  
  // Returns the number of bytes actually in the packet. Note that length
  // must always be less than capacity.
  int length() const   { return len_; }
  
  // Returns the id of the packet. 
  int id()   const { return id_; }

  uint64_t    timestamp() const { return ts_; }

  void limit(int n);

  // Data members.
  Byte*     buf_;        // Packet buffer.
  int       cap_;        // Total buffer size.
  int       len_;        // Total bytes in the packet.
  uint64_t  ts_;  // Time of packet arrival.
  int       id_;         // The packet id.
};


// Set the number of bytes to a smaller value.
inline void
Packet::limit(int n)
{
  assert(n <= cap_);
  len_ = n;
}


// Packet* packet_create(Byte*, int, uint64_t, void*, Buff_t);

} // namespace fp


#endif
