#include "port_udp.hpp"
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


// UDP Port constructor. Parses the UDP address and port from
// the input string given, allocates a new internal ID.
Port_udp::Port_udp(Port::Id id, std::string const& args)
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
  if ((sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror(std::string("port[" + std::to_string(id_) + "] socket").c_str());
    throw std::string("Port_udp::Ctor");
  }

  // Additional socket options.
  int optval = 1;
  setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval,
    sizeof(int));
}


// UDP Port destructor. Releases the allocated ID.
Port_udp::~Port_udp()
{ }


// Open the port. Creates a socket and binds it to the
// sockaddr data member.
int
Port_udp::open()
{
  // Bind the socket to its address.
  if (::bind(sock_fd_, (struct sockaddr*)&src_addr_, sizeof(src_addr_)) < 0) {
    perror(std::string("port[" + std::to_string(id_) + "] bind").c_str());
    return -1;
  }

  // Set the socket to be non-blocking.
  int flags = fcntl(sock_fd_, F_GETFL, 0);
  fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

  // NOTE: UDP sockets cannot be put into a 'listening' state, as they are
  // connectionless. Normally we would set the into a listening state at this
  // point.

  // Setup recvmmsg.
  //
  // Clear message container.
  memset(messages_, 0, sizeof(messages_));

  // Initialize IOVecs
  for (int i = 0; i < 16; i++) {
    iovecs_[i].iov_base  = in_buffers_[i];
    iovecs_[i].iov_len   = 512;
    messages_[i].msg_hdr.msg_iov = &iovecs_[i];
    messages_[i].msg_hdr.msg_iovlen  = 1;
  }

  // Initialize timeout timespec.
  timeout_.tv_sec = 1;
  timeout_.tv_nsec = 0;

  return 0;
}


// Close the port (socket).
void
Port_udp::close()
{
  ::close((int)sock_fd_);
}


// Read a packet from the input buffer.
Context*
Port_udp::recv()
{
  if (config_.down)
    throw std::string("Port down");

  // Receive data. Wait for 16 messages to arrive or timeout at 1 second.
  int num_msg = recvmmsg(sock_fd_, messages_, 16, 0, &timeout_);

  // Check for errors.
  if (num_msg == -1) {
    if (errno != EAGAIN || errno != EWOULDBLOCK)
      perror(std::string("port[" + std::to_string(id_) + "] recvmmsg").c_str());
    return nullptr;
  }


  // Copy the buffer so that we guarantee that we don't
  // accidentally overwrite it when we have multiple
  // readers.
  //
  // TODO: We should probably have a better buffer management
  // framework so that we don't have to copy each time we
  // create a packet.
  //
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

  // Process messages.
  for (int i = 0; i < num_msg; i++) {
    Packet* pkt = packet_create((unsigned char*)&in_buffers_[i], 512, 0, nullptr, FP_BUF_ALLOC);
    Context* cxt = new Context(pkt, id_, id_, 0, max_headers, max_fields);
    thread_pool.assign(new Task("pipeline", cxt));
  }

  return nullptr;
}


// Write a packet to the output buffer.
int
Port_udp::send()
{
  // Check that this port is usable.
  if (config_.down)
    throw std::string("port down");
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



// UDP Port thread work function. Expects the internal port id to be passed
// as an argument, which is used to drive that port.
void*
udp_work_fn(void* arg)
{
  // Grab a pointer to the port object you are driving.
  Port_udp* self = (Port_udp*)port_table.find(*((int*)arg));
  int sock_fd = self->sock_fd_;

  // Run while the FD is valid.
  while (fcntl(sock_fd, F_GETFD) != EBADF) {
    // Receive.
    self->recv();
    // Send.
    self->send();
  }
  return 0;
}


} // end namespace fp
