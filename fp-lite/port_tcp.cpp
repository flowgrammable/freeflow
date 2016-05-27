
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

  // Receive the 4-byte header.
  char hdr[4];
  int n = sock.recv(hdr, 4);
  
  // If we don't receive the 4-byte header, or if we encounter an error
  // then give up.
  if (n <= 0 || n != 4)
    return false;

  // Transform the network integer to host byte order.
  int32_t expect;
  memcpy(&expect, hdr, sizeof(int32_t));
  expect = ntohl(expect);
 
  // Receive until recv_size bytes have been read.
  Byte* ptr = p.data();
  int32_t rem = expect;
  while (rem != 0) {
    int k = sock.recv(ptr, rem);
    if (k <= 0) {
      if (k == 0)
        return false;
      if (k < 0 && errno != EAGAIN) {
        state_.link_down = true;
        return false;
      }
      else
        continue;
    }
    rem -= k;
    ptr += k;
  }

  // Adjust the packet size to the number of bytes read.
  p.set_size(expect);

  // Set up the input context.
  cxt.set_input(this, this, 0);

  // Update port stats.
  stats_.packets_rx++;
  stats_.bytes_rx += n;

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

  // Build the send buffer (badly).
  char buf[4096];
  std::uint32_t len = htons(p.size());
  memcpy(buf, &len, 4);
  memcpy(buf + 4, p.data(), p.size());

  // Send the number of bytes in the packet.
  int k = sock.send(buf, p.size() + 4);
  if (k <= 0) {
    state_.link_down = true;
    return false;
  }

  /*
  int send_size = p.size();
  int n = send_size;
  while (send_size) {
    // Send the packet.
    int k = sock.send(data_ptr, send_size);
    if (k <= 0) {
      if (k < 0)
        perror("send");
        std::cout << strerror(errno) << '\n';
      return false;
    }

    send_size -= k;
    data_ptr += k;
  }
  */

  // Update port stats.
  stats_.packets_tx++;
  stats_.bytes_tx += k;

  return true;
}


} // end namespace fp
