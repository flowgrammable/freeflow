#include "packet.hpp"

namespace fp
{

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
  case Buffer::BUF_TYPE::FP_BUF_PCAP:
    // placement new
    dynamic_cast<Buffer::Pcap*>(b)->~Pcap();
    break;
  default:
    throw "Unknown buffer type, can't destruct Packet";
  }
}


// Constructs a new packet with the given buffer, size, timestamp,
// buffer handle, and buffer type. Returns a pointer to the packet.
//
// TODO: Implement a packet (de)allocation scheme in the buffer file.
// We don't necessarily want to always allocate new memory for new
// packets, there should be a way to re-use that space.
Packet*
packet_create(unsigned char* buf, int size, timespec time, void* buf_handle)
{
  Packet* p = new Packet(buf, size, time, buf_handle, Buffer::BUF_TYPE::FP_BUF_SIMPLE);
  return p;
}


} // end namespace fp
