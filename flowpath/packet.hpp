#ifndef FP_PACKET_HPP
#define FP_PACKET_HPP

#include "buffer.hpp"
#include "types.hpp"

#include <type_traits>
#include <new>

namespace fp
{

// The base flowpath packet type.
//
// TODO: Should we expand the packet class to deal with different
// underlying buffer types, or is this sufficient?
//
// TODO: Should we take the timestamp as a constructor argument or
// create the timestamp inside the packet constructor?
// That is, should it be timestamp(time) or timestamp(get_time()) ?
class Packet
{
public:
  // Results in a POD type suitable for any (listed) Buffer type.
  // - type is uninitialized (must construct desired Buffer type via 'placemnt new')
  typedef std::aligned_union<0, Buffer::Base, Buffer::Simple, Buffer::Odp>::type Buffer_t;

  Packet(uint8_t*, int, uint64_t, void*, Buffer::BUF_TYPE);
  ~Packet();

  void alloc_buff(unsigned char* data);

  // Returns a pointer to the raw buffer.
  inline Byte const* data() const { return reinterpret_cast<const Buffer::Base*>(&buf_)->data_; }
  inline Byte*       data()       { return reinterpret_cast<const Buffer::Base*>(&buf_)->data_; }
  inline int         size() const { return reinterpret_cast<const Buffer::Base*>(&buf_)->bytes_; }
  inline Buffer::BUF_TYPE type() const { return reinterpret_cast<const Buffer::Base*>(&buf_)->type(); }

  // Packet accessors.
  inline void*       buf_handle() { return buf_handle_; }

private:
  // Data members.
  Buffer_t  buf_;        // Packet buffer.
  ///int       size_;       // Number of bytes.
  uint64_t  timestamp_;  // Time of packet arrival (ns).  TODO: change to struct timeval
  void*     buf_handle_; // [optional] port-specific buffer handle.
  ///Buff_t    buf_dev_;    // [optional] owner of buffer handle (dev*).
};

Packet* packet_create(unsigned char*, int, uint64_t, void* = nullptr);  // only for Buffer:Simple

} // namespace fp


#endif
