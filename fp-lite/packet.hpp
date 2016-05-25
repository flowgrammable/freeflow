#ifndef FP_PACKET_HPP
#define FP_PACKET_HPP

#include "types.hpp"

#include <cassert>
#include <cstdint>

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
  Packet();
  Packet(Byte*, int);
  Packet(Byte*, int, int);
  ~Packet() { }

  Packet& operator=(Packet const& other)
  {
    buf_ = other.buf_;
    size_ = other.size_;
    bytes_ = other.bytes_;
    timestamp_ = other.timestamp_;
    id_ = other.id_;
    return *this;
  }

  template<int N>
  Packet(Byte (&buf)[N])
    : Packet(buf, N)
  { }

  // Returns a pointer to the raw buffer.
  Byte const* data() const { return buf_; }
  Byte*       data()       { return buf_; }
  int         size() const { return size_; }
  int         id()   const { return id_; }

  void set_size(int size) { size_ = size; }

  void limit(int n);

  // Data members.
  Byte*     buf_;        // Packet buffer.
  int       size_;       // Total buffer size.
  int       bytes_;      // Total bytes in the packet.
  uint64_t  timestamp_;  // Time of packet arrival.
  int       id_;         // The packet id.
};


inline
Packet::Packet()
  : buf_(nullptr), size_(0), bytes_(0), timestamp_(0), id_(0)
{ }


inline
Packet::Packet(Byte* data, int id)
	: buf_(data)
  , size_(0)
  , timestamp_(0)
  , id_(id)
{ }


inline
Packet::Packet(Byte* data, int id, int size)
	: buf_(data)
  , size_(size)
  , timestamp_(0)
  , id_(id)
{ }


// Set the number of bytes to a smaller value.
inline void
Packet::limit(int n)
{
  assert(n <= size_);
  size_ = n;
}



Packet*   packet_create(Byte*, int, uint64_t, void*, Buff_t);

} // namespace fp


#endif
