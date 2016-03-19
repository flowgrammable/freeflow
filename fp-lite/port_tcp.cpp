
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
// establish the length with a 2-byte (short) integer value, in network byte
// order, at the head of the message. If there is not a 2-byte header present,
// undefined behavior ensues.
bool
Port_eth_tcp::recv(Context& cxt)
{
  Socket& sock = socket();
  Packet& p = cxt.packet();

  // Receive the 4-byte (short) header.
  char hdr[4];
  int n = sock.recv(hdr, 4);
  // If we don't receive the 4-byte header, or encounter an error, return.
  if (n <= 0 || n != 4)
    return false;

  // Transform the network short to host byte order.
  int recv_size = ntohl(*((int*)hdr));

  // Receive until recv_size bytes have been read.
  Byte* data_ptr = p.data();
  n = recv_size;
  while (recv_size) {
    int k = sock.recv(data_ptr, recv_size);
    if (k <= 0) {
      if (k == 0)
        break;
      if (k < 0 && errno != EAGAIN)
        return false;
      else
        continue;
    }
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
Port_eth_tcp::send(Context cxt)
{
  Socket& sock = socket();
  Packet const& p = cxt.packet();
  int n = sock.send(p.data(), p.size());
  if (n <= 0) {
    if (errno == EAGAIN)
      return true;
    detach();
    return n < 0;
  }

  // Update port stats.
  stats_.packets_tx++;
  stats_.bytes_tx += n;
  return true;
}


} // end namespace fp
