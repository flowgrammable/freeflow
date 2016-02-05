#ifndef FP_PORT_UDP_HPP
#define FP_PORT_UDP_HPP

#include "port.hpp"
#include "packet.hpp"
#include "context.hpp"
#include "dataplane.hpp"

#include <string>

namespace fp
{

const int UDP_BUF_SIZE = 512;
const int UDP_MSG_SIZE = 128;

class Port_udp : public Port
{
public:
  using Port::Port;

  // Constructors/Destructor.
  Port_udp(Port::Id, std::string const&);
  ~Port_udp();

  // Packet related funtions.
  Context*  recv();
  int       send();

  // Port state functions.
  int     open();
  void    close();

  // Data members.
  //
  // Socket addresses.
  struct sockaddr_in src_addr_;
  struct sockaddr_in dst_addr_;

  // IO Buffers.
  struct mmsghdr  messages_[UDP_BUF_SIZE];
  struct iovec    iovecs_[UDP_BUF_SIZE];
  char            buffer_[UDP_BUF_SIZE][UDP_MSG_SIZE];
  struct timespec timeout_;
};

} // end namespace fp

#endif
