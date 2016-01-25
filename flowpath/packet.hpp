#ifndef FP_PACKET_HPP
#define FP_PACKET_HPP

#include "buffer.hpp"
#include "types.hpp"

#include <cstdint>

namespace fp
{

// Buffer type enumeration.
enum Buff_t {
  FP_BUF_NADK,
  FP_BUF_NETMAP,
  FP_BUF_ALLOC
};


// The base flowpath packet type.
//
// TODO: Should we expand the packet class to deal with different
// underlying buffer types, or is this sufficient?
//
// TODO: Should we take the timestamp as a constructor argument or
// create the timestamp inside the packet constructor?
// That is, should it be timestamp(time) or timestamp(get_time()) ?
struct Packet
{
  using Buffer = Buffer::Flowpath;

  Packet(unsigned char*, int, uint64_t, void*, Buff_t);
  ~Packet();

  void alloc_buff(unsigned char* data);

  // Returns a pointer to the raw buffer.
  Byte const* data() const { return buf_->data_; }
  Byte*       data()       { return buf_->data_; }

  // Data members.
  Buffer*   buf_;        // Packet buffer.
  int       size_;       // Number of bytes.
  uint64_t  timestamp_;  // Time of packet arrival.
  void*     buf_handle_; // [optional] port-specific buffer handle.
  Buff_t    buf_dev_;    // [optional] owner of buffer handle (dev*).
};

Packet*   packet_create(unsigned char*, int, uint64_t, void*, Buff_t);
void      packet_destroy(Packet*);

} // namespace fp


#endif
