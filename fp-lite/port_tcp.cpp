
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
    this->id(), // In port
    this->id(), // In physical port
    0           // Tunnel id
  };

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
  return true;
}


} // end namespace fp
