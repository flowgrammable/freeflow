#include "port_udp.hpp"
#include "port_table.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>

// FIXME: Apple uses kqueue instead of epoll.
#if ! __APPLE__
#  include <sys/epoll.h>
#endif

#include <unistd.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <iostream>

#include <exception>


// The UDP port module.

namespace fp
{

extern Port_table port_table;


const int INIT_BUFF_SIZE = 2048;


// UDP Port constructor. Parses the UDP address and port from
// the input string given, allocates a new internal ID.
Port_udp::Port_udp(Port::Id id, std::string const& bind, std::string const& name)
  : Port(id, name)
{
  auto idx = bind.find(':');
  // Check length of address.
  if (idx == std::string::npos)
    throw std::string("bad address form");

  std::string addr = bind.substr(0, idx);
  std::string port = bind.substr(idx + 1, bind.length());
  // Set the address, if given. Otherwise it is set to INADDR_ANY.
  if (addr.length() > 0) {
    if (inet_pton(AF_INET, addr.c_str(), &src_addr_) < 0)
      perror("set socket address failed");
  }
  else {
    src_addr_.sin_family = AF_INET;
    src_addr_.sin_addr.s_addr = INADDR_ANY;
  }

  // Check length of port arg.
  if (port.length() < 2)
    throw std::string("bad port form");

  // Set the port.
  int p = std::stoi(port, nullptr);
  src_addr_.sin_port = htons(p);

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
  //
  // NOTE: This should be done in the config() function? Or it could just be done
  // at link time when we are giving definitions to other unknowns.
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
      (struct sockaddr*)&dst_addr_, sizeof(dst_addr_));

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
  if ((sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Socket");
    return -1;
  }

  // Bind the socket to its address.
  if (bind(sock_fd_, (struct sockaddr*)&src_addr_, sizeof(src_addr_)) < 0) {
    perror("Bind");
    return -1;
  }

  // Set the port to be non-blocking.
  int flags = fcntl(sock_fd_, F_GETFL, 0);
  fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

  // Set the port to listen for incoming connections.
  listen(sock_fd_, 1);

  return 0;
}


// Close the port (socket).
void
Port_udp::close()
{
  ::close((int)sock_fd_);
}


// UDP Port thread work function. Expects the internal port id to be passed
// as an argument, which is used to drive that port.
void*
udp_work_fn(void* arg)
{
  // Grab a pointer to the port object you are driving.
  Port_udp* self = (Port_udp*)port_table.find(*((int*)arg));

  // Setup epoll.
  struct epoll_event event, event_list[10];
  struct sockaddr_in addr = self->dst_addr_;
  socklen_t slen = sizeof(addr);
  int sock_fd, conn_fd, epoll_fd, num_fds;
  sock_fd = self->sock_fd_;

  if ((epoll_fd = epoll_create1(0)) == -1)
    throw std::string("Error: UDP Port epoll create");
  // Listen for input events.
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = sock_fd;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event) == -1)
    throw std::string("Error: UDP port epoll add");

  // Run while the FD is valid.
  while (fcntl(sock_fd, F_GETFD) != EBADF) {
    if ((num_fds = epoll_wait(epoll_fd, event_list, 10, -1)) == -1)
      throw std::string("Error: UDP port epoll wait");
    for (int i = 0; i < num_fds; i++) {
      if (event_list[i].data.fd == sock_fd) {
        // We have a new client connecting. Add its FD to the pollset.
        if ((conn_fd = accept(sock_fd, (struct sockaddr*)&addr, &slen)) == -1) {
          std::cerr << "port [" << self->id() << "] error: UDP port accept\n";
          continue;
        }
        // Set the socket to be non-blocking.
        int flags = fcntl(conn_fd, F_GETFL, 0);
        fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK);

        // Add the new client FD.
        event.events = EPOLLIN;
        event.data.fd = conn_fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &event) == -1) {
          std::cerr << "Error: UDP port epoll add client";
          continue;
        }
      }
      else {
        // We have a new message from an already connected client. Receive it.
        self->recv();
      }
      // Send any packets buffered for TX.
      self->send();
    }
  }
  return 0;
}


} // end namespace fp
