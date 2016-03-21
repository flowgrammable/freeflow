#include "packet.hpp"

namespace fp
{


// Packet constructor.
Packet::Packet(uint8_t* data, int size, uint64_t time, void* buf_handle, Buffer::BUF_TYPE buf_type)
  : timestamp_(time), buf_handle_(buf_handle)
{
  switch (buf_type) {
  case Buffer::BUF_TYPE::FP_BUF_ODP:
    // placement new in reserved (aligned_union) space for buffer struct
    new (&buf_) Buffer::Odp(data, size);
    break;
  case Buffer::BUF_TYPE::FP_BUF_SIMPLE:
    // placement new
    new (&buf_) Buffer::Simple(data, size);
    break;
  default:
    throw "Unknown buffer type, can't construct Packet";
  }
}


// Packet destructor.
Packet::~Packet()
{
  Buffer::Base* b = reinterpret_cast<Buffer::Base*>(&buf_);

  // RETHINK: replace type functionality with a dynamic_cast != NULL?

  switch (b->type()) {
  case Buffer::BUF_TYPE::FP_BUF_ODP:
    // placement new in reserved (aligned_union) space for buffer struct
    dynamic_cast<Buffer::Odp*>(b)->~Odp();
    break;
  case Buffer::BUF_TYPE::FP_BUF_SIMPLE:
    // placement new
    dynamic_cast<Buffer::Simple*>(b)->~Simple();
    break;
  }
}


// Constructs a new packet with the given buffer, size, timestamp,
// buffer handle, and buffer type. Returns a pointer to the packet.
//
// TODO: Implement a packet (de)allocation scheme in the buffer file.
// We don't necessarily want to always allocate new memory for new
// packets, there should be a way to re-use that space.
Packet*
packet_create(unsigned char* buf, int size, uint64_t time, void* buf_handle)
{
  Packet* p = new Packet(buf, size, time, buf_handle, Buffer::BUF_TYPE::FP_BUF_SIMPLE);
	return p;
}


} // end namespace fp
