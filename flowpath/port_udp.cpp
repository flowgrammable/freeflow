#include "port_udp.hpp"
#include "port_table.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

// FIXME: Apple uses kqueue instead of epoll.
#if ! __APPLE__
#  include <sys/epoll.h>
#endif

#include <unistd.h>

#include <exception>


// The UDP port module.

namespace fp
{

extern Port_table port_table;


const int INIT_BUFF_SIZE = 2048;


// UDP Port constructor. Parses the UDP address and port from
// the input string given, allocates a new internal ID.
Port_udp::Port_udp(Port::Id id, std::string const& str)
  : Port(id)
{
  auto idx = str.find(':');

  // Check length of address.
  if (idx == std::string::npos)
    throw("bad address form");

  // Set the address, if given. Otherwise it is set to INADDR_ANY.
  if (idx > 0) {
    if (inet_pton(AF_INET, str.substr(0, idx).c_str(), &src_addr_) < 0)
      throw("set socket address failed");
  }
  else {
    src_addr_.sin_family = AF_INET;
    src_addr_.sin_addr.s_addr = INADDR_ANY;
  }

  // Check length of port arg.
  if (str.length() - idx < 2)
    throw("bad port form");

  // Set the port.
  int port = std::stoi(str.substr(idx + 1, str.length()), nullptr);
  src_addr_.sin_port = htons(port);

}


// UDP Port destructor. Releases the allocated ID.
Port_udp::~Port_udp()
{ }


// Read a packet from the input buffer.
Context*
Port_udp::recv()
{
  if (config_.down)
    throw("port down");

  char buff[INIT_BUFF_SIZE];
  socklen_t slen = sizeof(dst_addr_);
  // Receive data.
  int bytes = recvfrom(sock_fd_, buff, INIT_BUFF_SIZE, 0,
    (struct sockaddr*)&dst_addr_, &slen);

  if (bytes < 0)
    return nullptr;

  // If we receive a 0-byte packet, the dest has closed.
  // Set the port config to reflect this and return nullptr.
  if (bytes == 0) {
    config_.down = 1;
    return nullptr;
  }

  // Copy the buffer so that we guarantee that we don't
  // accidentally overwrite it when we have multiple
  // readers.
  //
  // TODO: We should probably have a better buffer management
  // framework so that we don't have to copy each time we
  // create a packet.
  Packet* pkt = packet_create((unsigned char*)buff, bytes, 0, nullptr, FP_BUF_ALLOC);
  // TODO: We should call functions which ask the application
  // for the maximum desired number of headers and fields
  // that can be extracted so we can produce a context
  // which takes up a minimal amount of space.
  // And probably move these to become global values instead
  // of locals to reduce function calls.
  int max_headers = 0;
  int max_fields = 0;
  return new Context(pkt, id_, id_, 0, max_headers, max_fields);
}


// Write a packet to the output buffer.
int
Port_udp::send()
{
  // Check that this port is usable.
  if (config_.down)
    throw("port down");
  int bytes = 0;

  // Get the next packet.
  Context* cxt = nullptr;
  while ((cxt = tx_queue_.dequeue())) {
    // Send the packet.
    int l_bytes = sendto(sock_fd_, cxt->packet_->buf_.data_, cxt->packet_->size_, 0,
      (struct sockaddr*)&dst_addr_, sizeof(struct sockaddr_in));

    if (bytes < 0)
      continue;

    // TODO: What do we do if we send 0 bytes?
    if (bytes == 0)
      continue;

    // Destroy the packet data.
    packet_destroy(cxt->packet_);

    // Destroy the packet context.
    delete cxt;

    bytes += l_bytes;
  }
  // Return number of bytes sent.
  return bytes;
}


// Open the port. Creates a socket and binds it to the
// sockaddr data member.
int
Port_udp::open()
{
  // Open the socket.
  if ((sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    return -1;

  // Bind the socket to its address.
  if (bind(sock_fd_, (struct sockaddr*)&src_addr_, sizeof(src_addr_) < 0))
    return -1;

  // TODO: Setup listen() and epoll() stuff...

  return 0;
}


// Close the port (socket).
void
Port_udp::close()
{
  ::close((int)sock_fd_);
  // TODO: Should we clear the value for fd_? We may not
  // need to since it would be re-assigned when opened.
}


} // end namespace fp
