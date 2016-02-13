
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

// This doesn't actually do anything.
bool
Port_tcp::open()
{
  return true;
}


// Terminate the connection from the dataplane side.
bool
Port_tcp::close()
{
  return true;
}


// Read a packet from the input stream.
bool
Port_tcp::recv(Context& cxt)
{
  // Get data.
  Packet& p = cxt.packet();
  int n = sock_.recv(p.data(), p.size());
  if (n <= 0)
    return false;

  // Adjust the packet size to the number of bytes read.
  //
  // FIXME: This is gross.
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


// Writes a packet to the output stream.
bool
Port_tcp::send(Context const& cxt)
{
  Packet const& p = cxt.packet();
  int n = sock_.send(p.data(), p.size());
  if (n <= 0) {
    detach();
    return n < 0;
  }
  return true;
}


} // end namespace fp
