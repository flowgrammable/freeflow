
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

// Read an ethernet frame from the stream.
bool
Port_eth_tcp::recv(Context& cxt)
{
  Socket& sock = socket();
  Packet& p = cxt.packet();
  int n = sock.recv(p.data(), p.size());
  if (n <= 0)
    return false;

  // Adjust the packet size to the number of bytes read.
  p.size_ = n;

  // Set up the input context.
  //
  // FIXME: Pretty this up.
  cxt.input_ = {
    this, // In port
    this, // In physical port
    0     // Tunnel id
  };

  return true;
}


// Read an ethernet frame from the stream. This recv function utilizes a
// simple protocol to establish the length of the frame being received. We
// establish the length with a 2-byte (short) integer value, in network byte
// order, at the head of the message. If there is not a 2-byte header present,
// undefined behavior ensues.
bool
Port_eth_tcp::recv_exact(Context& cxt)
{
  Socket& sock = socket();
  Packet& p = cxt.packet();

  // Receive the 2-byte (short) header.
  short recv_size = 0;
  int n = sock.recv((char*)&recv_size, 2);
  // If we don't receive the 2-byte header, or encounter an error, return.
  if (n <= 0 || n != 2)
    return false;

  // Transform the network short to host byte order.
  recv_size = ntohs(recv_size);
  // FIXME: Currently limiting size of frames to size of local buffer
  // in driver.
  recv_size = recv_size > 2048 ? 2048 : recv_size;
  // Receive until recv_size bytes have been read.
  Byte* data_ptr = p.data();
  n = recv_size;
  while (recv_size) {
    int k = sock.recv(data_ptr, recv_size);
    if (k <= 0)
      break;
    recv_size -= k;
    data_ptr += k;
  }

  // Adjust the packet size to the number of bytes read.
  p.set_size(n);

  // Set up the input context.
  cxt.set_input(this, this, 0);

  // Update port stats.
  stats_.packets_rx++;
  stats_.bytes_rx += n;

  // Return recv status.
  return true;
}


// Writes a packet to the output stream.
bool
Port_eth_tcp::send(Context const& cxt)
{
  Socket& sock = socket();
  Packet const& p = cxt.packet();
  int n = sock.send(p.data(), p.size());
  if (n <= 0) {
    detach();
    return n < 0;
  }

  // Update port stats.
  stats_.packets_tx++;
  stats_.bytes_tx += n;

  // Return send status.
  return true;
}


} // end namespace fp
