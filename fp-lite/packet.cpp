#include "packet.hpp"

namespace fp
{

// Constructs a new packet with the given buffer, size, timestamp,
// buffer handle, and buffer type. Returns a pointer to the packet.
//
// TODO: Implement a packet (de)allocation scheme in the buffer file.
// We don't necessarily want to always allocate new memory for new
// packets, there should be a way to re-use that space.
Packet*
packet_create(unsigned char* buf, int size, uint64_t time, void* buf_handle,
	Buff_t buf_type)
{
	Packet* p = new Packet(buf, size, time, buf_handle, buf_type);
	return p;
}


} // end namespace fp
