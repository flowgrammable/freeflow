#include "port_odp.hpp"
#include "port_table.hpp"

//#include <arpa/inet.h>
//#include <netinet/in.h>
#include <cstdio>
#include <cstring>

//#include <unistd.h>
//#include <netdb.h>
//#include <sys/epoll.h>
//#include <fcntl.h>
#include <iostream>

#include <exception>


// The UDP port module.

namespace fp
{

extern Port_table port_table;


const int INIT_BUFF_SIZE = 2048;


// UDP Port constructor. Parses the UDP address and port from
// the input string given, allocates a new internal ID.
Port_odp::Port_odp(Port::Id id, std::string const& args)
  : Port(id, "")
{
  auto name_off = args.find(';');

  std::string dev_name = args.substr(0, name_off);
  std::string port_name = args.substr(name_off + 1, args.length());

  // Set the port name.
  name_ = port_name;
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
  if (::bind(sock_fd_, (struct sockaddr*)&src_addr_, sizeof(src_addr_)) < 0) {
    perror(std::string("port[" + std::to_string(id_) + "] bind src addr").c_str());
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
  memset(messages_, 0, sizeof(struct mmsghdr) * 512);
  memset(iovecs_, 0, sizeof(struct iovec) * 512);

    // Initialize buffer.
  buffer_ = new char*[512];
  for (int i = 0; i < 512; i++)
    buffer_[i] = new char[128];

  // Initialize IOVecs
  for (int i = 0; i < 512; i++) {
    iovecs_[i].iov_base  = buffer_[i];
    iovecs_[i].iov_len   = 1;
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


// Read packets from the socket.
Context*
Port_udp::recv()
{
  // Check if port is usable.
  if (config_.down)
    throw std::string("Port down");

  // Receive data. Wait for 512 messages to arrive or timeout at 1 second.
  //
  // FIXME: don't hardcode the number of messages to receive at a time.
  int num_msg = recvmmsg(sock_fd_, messages_, 512, 0, &timeout_);

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
  // NOTE: We are not currently copying the packet data from the buffer.
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
  uint64_t bytes = 0;
  // Process messages.
  for (int i = 0; i < num_msg; i++) {
    // Update num bytes received.
    bytes += messages_[i].msg_len;
    
    // Create a new packet, leaving the data in the original buffer.
    Packet* pkt = packet_create((unsigned char*)&buffer_[i], 
      messages_[i].msg_len, 0, nullptr, FP_BUF_ALLOC);
    
    // Create and associate a new context for the packet.
    Context* cxt = new Context(pkt, id_, id_, 0, max_headers, max_fields);

    // Send the packet through the pipeline.
    thread_pool.assign(new Task("pipeline", cxt));
  }

  // Update port stats.
  stats_.pkt_rx += num_msg;
  stats_.byt_rx += bytes;

  // FIXME: change this to the number of bytes received?
  return nullptr;
}


// Send the packet to its destination.
int
Port_udp::send()
{
  // Check that this port is usable.
  if (config_.down)
    throw std::string("port down");
  uint64_t bytes = 0;
  uint64_t pkts = 0;
  // Send packets as long as the queue has work.
  if (tx_queue_.size()) {
    // Get the next context, incr packet counter.
    Context* cxt = tx_queue_.dequeue();
    pkts++;
    
    // Send the packet data associated with the context.
    //
    // TODO: Change this to use sendmmsg, or maybe decide based on
    // the size of the queue?
    long res = sendto(send_fd_, cxt->packet_->data(), sizeof(cxt->packet_->data()),
      0, (struct sockaddr*)&dst_addr_, sizeof(dst_addr_));

    // Error check, else update num bytes sent.
    if (res < 0) {
      perror(std::string("port[" + std::to_string(id_) + "] sendto").c_str());
    }
    else
      bytes += res;

    // Release resources.
    packet_destroy(cxt->packet_);
    delete cxt;
  }

  // Update port stats.
  stats_.pkt_tx += pkts;
  stats_.byt_tx += bytes;
  
  // Return number of bytes sent.
  return bytes;
}



// UDP Port thread work function. Expects the internal port id to be passed
// as an argument, which is used to drive that port.
void*
odp_work_fn(void* arg)
{
  // Grab a pointer to the port object you are driving.
  Port_udp* self = (Port_udp*)port_table.find(*((int*)arg));

  // Execute port work.
  while (!self->config_.down) {
    // Receive.
    self->recv();
    // Send.
    self->send();
  }
  return 0;
}


} // end namespace fp
