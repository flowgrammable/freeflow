#include "port_udp.hpp"
#include "packet.hpp"
#include "context.hpp"

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

  // Set the address, if given. Otherwise it is set to the loopback device.
  src_addr_.sin_family = AF_INET;
  if (addr.length() > 0) {
    if (inet_pton(AF_INET, addr.c_str(), &src_addr_.sin_addr) < 0)
      perror(std::string("port[" + std::to_string(id_) + "] set src addr").c_str());
  }
  else {
    if (inet_pton(AF_INET, "127.0.0.1", &src_addr_.sin_addr) < 0)
      perror(std::string("port[" + std::to_string(id_) + "] set src addr").c_str());
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
  if ((fd_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror(std::string("port[" + std::to_string(id_) + "] socket").c_str());
    throw std::string("Port_udp::Ctor");
  }

  // Additional socket options.
  int optval = 1;
  setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval,
    sizeof(int));

  // FIXME: For now, we will hardcode a destination address for the UDP ports.
  // This needs to be fixed with some sort of configuration and/or discovery
  // (probably discovery).
  //
  // Possibly look at this:
  // http://stackoverflow.com/questions/5281409/get-destination-address-of-a-received-udp-packet
  dst_addr_.sin_family = AF_INET;
  if (inet_pton(AF_INET, "127.0.0.1", &dst_addr_.sin_addr) < 0)
    perror(std::string("port[" + std::to_string(id_) + "] set dst addr").c_str());
  dst_addr_.sin_port = htons(5003);
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
}


// UDP Port destructor. Releases the allocated ID.
Port_udp::~Port_udp()
{ }


// Open the port. Creates a socket and binds it to the
// sockaddr data member.
int
Port_udp::open()
{
  // Bind the socket to its source address.
  if (::bind(fd_, (struct sockaddr*)&src_addr_, sizeof(src_addr_)) < 0) {
    perror(std::string("port[" + std::to_string(id_) + "] bind src addr").c_str());
    return -1;
  }

  // Set the socket to be non-blocking.
  int flags = fcntl(fd_, F_GETFL, 0);
  fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

  // NOTE: UDP sockets cannot be put into a 'listening' state, as they are
  // connectionless. Normally we would set the into a listening state at this
  // point.

  // Setup recvmmsg.
  //
  // Clear message container.
  memset(messages_, 0, sizeof(struct mmsghdr) * UDP_BUF_SIZE);

  // Initialize buffer.
  for (int i = 0; i < UDP_BUF_SIZE; i++) {
    iovecs_[i].iov_base  = buffer_[i];
    iovecs_[i].iov_len   = UDP_MSG_SIZE;
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
  ::close((int)fd_);
}


// Read packets from the socket.
int
Port_udp::recv()
{
  // Check if port is usable.
  if (config_.down)
    throw std::string("Port down");

  // Receive data. Wait for UDP_BUF_SIZE messages to arrive or timeout at 1 second.
  int num_msg = recvmmsg(fd_, messages_, UDP_BUF_SIZE, 0, &timeout_);

  // Check for errors.
  if (num_msg == -1) {
    if (errno != EAGAIN || errno != EWOULDBLOCK)
      perror(std::string("port[" + std::to_string(id_) + "] recvmmsg").c_str());
    return -1;
  }


  // Copy the buffer so that we guarantee that we don't
  // accidentally overwrite it when we have multiple
  // readers.
  //
  // FIXED: fp::Buffer copies the data.
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
  // NOTE: This should be done in the config() function.
  int max_headers = 0;
  int max_fields = 0;
  uint64_t bytes = 0;
  // Process messages.
  for (int i = 0; i < num_msg; i++) {
    // Update num bytes received.
    bytes += messages_[i].msg_len;

    // Create a new packet, leaving the data in the original buffer.
    Packet* pkt = packet_create((unsigned char*)buffer_[i],
      messages_[i].msg_len, 0, nullptr, FP_BUF_ALLOC);

    // Create a new context associated with the packet.
    Context* cxt = new Context(pkt, id_, id_, 0, max_headers, max_fields);
    
    // Create the context associated with the packet and send it through
    // the pipeline.
    thread_pool.app()->lib().pipeline(cxt);
  }

  // Update port stats.
  stats_.pkt_rx += num_msg;
  stats_.byt_rx += bytes;

  return bytes;
}


// Send the packet to its destination.
int
Port_udp::send()
{
  // Check that this port is usable.
  if (config_.down)
    throw std::string("port down");
  uint64_t bytes = 0;
  uint64_t pkts = tx_queue_.size();
  // Send packets as long as the queue has work.
  for (int i = 0; i < pkts; i++) {
    // Get the next context, incr packet counter.
    Context* cxt = tx_queue_.front();

    // Send the packet data associated with the context.
    //
    // TODO: Change this to use sendmmsg, or maybe decide based on
    // the size of the queue?
    long res = sendto(fd_, cxt->packet()->data(), cxt->packet()->size(),
      0, (struct sockaddr*)&dst_addr_, sizeof(dst_addr_));

    // Error check, else update num bytes sent.
    if (res < 0) {
      perror(std::string("port[" + std::to_string(id_) + "] sendto").c_str());
    }
    else
      bytes += res;

    // Release resources.
    delete cxt->packet();
    delete cxt;
    tx_queue_.pop();
  }

  // Update port stats.
  stats_.pkt_tx += pkts;
  stats_.byt_tx += bytes;

  // Return number of bytes sent.
  return bytes;
}


} // end namespace fp
