
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
  sock_.close();
  return true;
}


// Read a packet from the input stream.
bool
Port_tcp::recv(Context& cxt)
{
  // Get data.
  Packet p = cxt.packet();
  int n = sock_.recv(p.data(), p.size());
  if (n <= 0) {
    sock_.close();
    down();
    return n < 0;
  }
  return true;
}


// Writes a packet to the output stream.
bool
Port_tcp::send(Context const& cxt)
{
  return true;

#if 0
  assert(is_up());
  int bytes = write(fd_, cxt->packet()->data(), cxt->packet()->size_);

    if (bytes < 0)
      continue;

    // TODO: What do we do if we send 0 bytes?
    if (bytes == 0)
      continue;

    // Destroy the packet data.
    delete cxt->packet();
    tx_queue_.pop();
    // Destroy the packet context.
    delete cxt;

    bytes += l_bytes;
  }
  // Return number of bytes sent.
  return bytes;
#endif
}


} // end namespace fp
