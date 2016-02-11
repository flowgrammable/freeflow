
#include "port_tcp.hpp"
#include "types.hpp"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <system_error>


namespace fp
{

// This is conveniently large enough to hold an Ethernet frame.
static constexpr int TCP_BUF_SIZE = 2048;


// TCP Port constructor. Parses the TCP address and port from
// the input string given, allocates a new internal ID.
Port_tcp::Port_tcp(Port::Id id, ff::Ipv4_address addr, ff::Ip_port port)
  : Port(id), src_(addr, port)
{
  fd_ = ff::stream_socket(src_.family());
  if (fd_ < 0)
    throw std::runtime_error("create port");

  // Additional socket options.
  int optval = 1;
  setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
}


// Open the port. Creates a socket and binds it to the
// sockaddr data member. Note that the port is not up
// until a connection is made.
//
// FIXME: Throw an exception?
bool
Port_tcp::open()
{
  // Bind to the specified address.
  //
  // TODO: Write a wrapper in freeflow.
  if (::bind(fd_, (sockaddr const*)&src_, sizeof(src_)) < 0)
    return false;

  // Listen for connections.
  if (::listen(fd_, SOMAXCONN) < 0)
    return false;

  return true;
}


// Close the port. Shut the port down and close it.
bool
Port_tcp::close()
{
  down();
  if (::close(fd_) < 0)
    perror(__func__); // FIXME: Do better.
  return true;
}



// Read a packet from the input buffer, and perform basic initialization
// of the context.
bool
Port_tcp::recv(Context& cxt)
{
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
}


// Write a packet to the output buffer.
bool
Port_tcp::send(Context const& cxt)
{
  assert(is_up());

#if 0
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
  return true;
}


} // end namespace fp
