
#include "port_tcp.hpp"
#include "types.hpp"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <system_error>
#include <iostream>

namespace fp
{

// This is conveniently large enough to hold an Ethernet frame.
static constexpr int TCP_BUF_SIZE = 2048;


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
  char buf[2048];
  int n = sock_.recv(buf);

  // On error or closure, down the port.
  if (n <= 0) {
    down();
    return n < 0;
  }

  buf[n] = 0;
  std::cout << "[flowpath] " << buf;
  return true;

  /*
  assert(is_up());

  // Receive data.
  //
  // FIXME: Receive directly into the packet buffer of
  // the context. That way, we don't have to think about
  // memory management here.
  Byte buf[TCP_BUF_SIZE];
  int bytes = ff::recv(fd_, buf);

  // Handle exceptional cases. If there is an error, or the
  // remote host has closed the connection, shut down the
  // port.
  if (bytes <= 0) {
    if (bytes < 0)
      perror(__func__); // FIXME: Do better.
    close();
    return false;
  }

  // Copy the buffer so that we guarantee that we don't
  // accidentally overwrite it when we have multiple
  // readers.
  //
  // TODO: We should probably have a better buffer management
  // framework so that we don't have to copy each time we
  // create a packet.
  // Packet* pkt = packet_create(buf, bytes, 0, nullptr, FP_BUF_ALLOC);

  // TODO: We should call functions which ask the application
  // for the maximum desired number of headers and fields
  // that can be extracted so we can produce a context
  // which takes up a minimal amount of space.
  // And probably move these to become global values instead
  // of locals to reduce function calls.
  //
  // NOTE: This should be done in the config() function? Or it could just be done
  // at link time when we are giving definitions to other unknowns.
  // int max_headers = 0;
  // int max_fields = 0;
  // thread_pool.app()->lib().pipeline(new Context(pkt, id_, id_, 0, max_headers, max_fields));
  return true;
  */
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
