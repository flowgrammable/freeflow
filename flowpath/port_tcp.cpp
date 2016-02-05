#include "port_tcp.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstring>

#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <iostream>

#include <exception>


// The UDP port module.

namespace fp
{

const int TCP_BUF_SIZE = 2048;


// TCP Port constructor. Parses the TCP address and port from
// the input string given, allocates a new internal ID.
Port_tcp::Port_tcp(Port::Id id, std::string const& args)
  : Port(id, "")
{
  auto bind_port = args.find(':');
  auto name_off = args.find(';');
  // Check length of address.
  if (bind_port == std::string::npos)
    throw std::string("bad address form");

  std::string addr = args.substr(0, bind_port);
  std::string port = args.substr(bind_port + 1, args.find(';'));
  std::string name = args.substr(name_off + 1, args.length());
  // Set the address, if given. Otherwise it is set to INADDR_ANY.
  if (addr.length() > 0) {
    if (inet_pton(AF_INET, addr.c_str(), &src_addr_) < 0)
      perror("set socket address failed");
  }
  else {
    src_addr_.sin_family = AF_INET;
    src_addr_.sin_addr.s_addr = htons(INADDR_ANY);
  }

  // Check length of port arg.
  if (port.length() < 2)
    throw std::string("bad port form");

  // Set the port.
  int p = std::stoi(port, nullptr);
  src_addr_.sin_port = htons(p);

  // Set the port name.
  name_ = name;

  // Create the socket.
  if ((fd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror(std::string("port[" + std::to_string(id_) + "] socket").c_str());
    throw std::string("Port_tcp::Ctor");
  }

  // Additional socket options.
  int optval = 1;
  setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval,
    sizeof(int));
}


// UDP Port destructor. Releases the allocated ID.
Port_tcp::~Port_tcp()
{ }


// Read a packet from the input buffer.
Context*
Port_tcp::recv()
{
  if (config_.down)
    throw std::string("Port down");

  char buff[TCP_BUF_SIZE];
  // Receive data.
  int bytes = read(fd_, buff, TCP_BUF_SIZE);

  if (bytes < 0) {
    perror(std::string("port[" + std::to_string(id_) + "] recvfrom").c_str());
    return nullptr;
  }

  // If we receive a 0-byte packet, the dest has closed.
  // Set the port config to reflect this and return nullptr.
  if (bytes == 0) {
    config_.down = 1;
    throw std::string("Connection closed");
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
  //
  // NOTE: This should be done in the config() function? Or it could just be done
  // at link time when we are giving definitions to other unknowns.
  int max_headers = 0;
  int max_fields = 0;
  return new Context(pkt, id_, id_, 0, max_headers, max_fields);
}


// Write a packet to the output buffer.
int
Port_tcp::send()
{
  // Check that this port is usable.
  if (config_.down)
    throw("port down");
  int bytes = 0;

  // Get the next packet.
  Context* cxt = nullptr;
  while ((cxt = tx_queue_.dequeue())) {
    // Send the packet.
    int l_bytes = write(fd_, cxt->packet_->data(), cxt->packet_->size_);

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
Port_tcp::open()
{
  // Bind the socket to its address.
  if (::bind(fd_, (struct sockaddr*)&src_addr_, sizeof(src_addr_)) < 0) {
    perror(std::string("port[" + std::to_string(id_) + "] bind").c_str());
    return -1;
  }

  // Set the socket to be non-blocking.
  int flags = fcntl(fd_, F_GETFL, 0);
  fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

  // Put the socket into a listening state.
  listen(fd_, 10);

  return 0;
}


// Close the port (socket).
void
Port_tcp::close()
{
  ::close((int)fd_);
}


} // end namespace fp
