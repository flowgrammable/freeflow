#ifndef FP_PORT_UDP_HPP
#define FP_PORT_UDP_HPP

#include "port.hpp"
#include "packet.hpp"
#include "context.hpp"

#include <string>

namespace fp
{

class Port_udp : public Port
{
public:
  using Port::Port;
  static const int MSG_SIZE = 128;
  static const int BUF_SIZE = 128;
  // Ctor/Dtor.
  Port_udp(Port::Id, std::string const&);
  ~Port_udp();

  // Packet I/O functions.
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
  // Message containers.
  struct mmsghdr     messages_[MSG_SIZE];
  struct iovec       iovecs_[MSG_SIZE];
  char               buffer_[MSG_SIZE][BUF_SIZE];
  struct timespec    timeout_;
};

} // end namespace fp

#endif
