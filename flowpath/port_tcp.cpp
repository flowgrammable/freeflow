#include "port_tcp.hpp"
#include "port_table.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstring>

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
  if ((sock_fd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror(std::string("port[" + std::to_string(id_) + "] socket").c_str());
    throw std::string("Port_tcp::Ctor");
  }

  // Additional socket options.
  int optval = 1;
  setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval,
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

  char buff[INIT_BUFF_SIZE];
  // Receive data.
  int bytes = read(io_fd_, buff, INIT_BUFF_SIZE);

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
    int l_bytes = write(io_fd_, cxt->packet_->data(), cxt->packet_->size_);

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
  if (::bind(sock_fd_, (struct sockaddr*)&src_addr_, sizeof(src_addr_)) < 0) {
    perror(std::string("port[" + std::to_string(id_) + "] bind").c_str());
    return -1;
  }

  // Set the socket to be non-blocking.
  int flags = fcntl(sock_fd_, F_GETFL, 0);
  fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

  // Put the socket into a listening state.
  listen(sock_fd_, 10);

  return 0;
}


// Close the port (socket).
void
Port_tcp::close()
{
  ::close((int)sock_fd_);
}


// TCP Port thread work function. Expects the internal port id to be passed
// as an argument, which is used to drive that port.
void*
tcp_work_fn(void* arg)
{
  // Grab a pointer to the port object you are driving.
  Port_tcp* self = (Port_tcp*)port_table.find(*((int*)arg));

  // Setup epoll.
  struct epoll_event event, event_list[10];
  struct sockaddr_in addr = self->dst_addr_;
  socklen_t slen = sizeof(addr);
  int sock_fd, conn_fd, epoll_fd, res;
  sock_fd = self->sock_fd_;

  if ((epoll_fd = epoll_create1(0)) == -1)
    perror(std::string("port[" + std::to_string(self->id()) +
      "] epoll_create").c_str());
  // Listen for input events.
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = sock_fd;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event) == -1)
    perror(std::string("port[" + std::to_string(self->id()) +
      "] epoll_ctl_add").c_str());

  // Run while the FD is valid.
  while (fcntl(sock_fd, F_GETFD) != EBADF) {
    res = epoll_wait(epoll_fd, event_list, 10, 1000);

    // Receive messages/report errors.
    if (res < 0) {
      perror(std::string("port[" + std::to_string(self->id()) +
        "]").c_str());
      continue;
    }
    else {
      for (int i = 0; i < res; i++) {
        if (event_list[i].data.fd == sock_fd) {
          // We have a new connection coming in.
          conn_fd = accept(sock_fd, (struct sockaddr*)&addr, &slen);
          if (conn_fd == -1) {
            perror(std::string("port[" + std::to_string(self->id()) +
              "]").c_str());
            continue;
          }
          else {
            // Set the socket to be non-blocking.
            int flags = fcntl(conn_fd, F_GETFL, 0);
            fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK);
            // Add to the epoll set.
            event.events = EPOLLIN | EPOLLET;
            event.data.fd = conn_fd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &event) == -1)
              perror(std::string("port[" + std::to_string(self->id()) +
                "]").c_str());
          }
        } // End if new connection.
        else {
          // A message from a known client has been received. Process it.
          self->io_fd_ = event_list[i].data.fd;
          thread_pool.assign(new Task("pipeline", self->recv()));
        } // End else known connection.
      }
    }
    // Send any packets buffered for TX.
    self->send();
  }
  return 0;
}


} // end namespace fp
