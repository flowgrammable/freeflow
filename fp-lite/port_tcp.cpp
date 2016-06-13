
#include "port_tcp.hpp"
#include "context.hpp"
#include "types.hpp"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <system_error>
#include <iostream>

namespace fp
{

// Read an ethernet frame from the stream. This recv function utilizes a
// simple protocol to establish the length of the frame being received. We
// establish the length with a 4-byte integer value, in network byte
// order, at the head of the message. If there is not a 4-byte header present,
// undefined behavior ensues.
bool
Port_eth_tcp::recv(Context& cxt)
{
  Socket& sock = socket();
  Packet& p = cxt.packet();

  // Receive the 4-byte header and nativize it.
  // If we don't receive the 4-byte header, or if we encounter an error
  // then just give up. It's not worth trying to capture more.
  std::uint32_t hdr;
  int k1 = ::recv(sock.fd(), (Byte*)&hdr, 4, MSG_PEEK);  
  if (k1 <= 0 || k1 != 4)
    return false;
  hdr = ntohl(hdr);
  
  // Verify that we won't exceed the capacity of the packet.
  //
  // TODO: Rather than simply failing (hard), we should be able to request
  // a larger buffer for the packet. But now we're getting into serious
  // memory management strategies.
  if (hdr >= (std::size_t)p.capacity())
    return -1;

  // Read the rest of the message.
  //
  // TODO: Verify that the packet has capacity for the header.
  int k2 = sock.recv(p.data(), hdr + 4);
  if (k2 <= 0) {
    if (k2 == 0)
      return false;

    // TODO: If we actually get an EAGAIN, then what should we 
    // do? Continue receiving? Fail? Not quite sure.
    if (k2 < 0 && errno != EAGAIN) {
      state_.link_down = true;
      return false;
    }
  }
  p.limit(hdr);
 
  // Set up the input context.
  //
  // TODO: The physical port may not be this port.
  cxt.set_input(this, this, 0);

  // Update port stats.
  stats_.packets_rx++;
  stats_.bytes_rx += hdr;

  return true;
}


// Writes a packet to the output stream.
bool
Port_eth_tcp::send(Context& cxt)
{
  // Get the ports socket.
  Socket& sock = socket();
  
  // Get the packet from the context.
  Packet const& p = cxt.packet();

  // Send the header size.
  std::uint32_t hdr = ntohl(p.length());
  int k1 = sock.send((Byte*)&hdr, 4);
  if (k1 <= 0) {
    state_.link_down = true;
    return false;
  }

  // Send the remainder of the packet.
  int k2 = sock.send(p.data(), p.length());
  if (k2 <= 0) {
    state_.link_down = true;
    return false;
  }

  // Update port stats.
  stats_.packets_tx++;
  stats_.bytes_tx += k2;

  return true;
}


} // end namespace fp
