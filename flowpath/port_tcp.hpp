#ifndef FP_PORT_TCP_HPP
#define FP_PORT_TCP_HPP

#include "port.hpp"
#include "packet.hpp"
#include "context.hpp"
#include "dataplane.hpp"

#include <string>

namespace fp
{

// TCP Port thread work function.
extern void* tcp_work_fn(void*);

class Port_tcp : public Port
{
public:
  using Port::Port;

  // Constructors/Destructor.
  Port_tcp(Port::Id, std::string const&, std::string const& = "");
  ~Port_tcp();

  // Packet related funtions.
  Context*  recv();
  int       send();

  // Port state functions.
  int     open();
  void    close();

  // Accessors.
  Function work_fn() { return tcp_work_fn; }

  // Data members.
  //
  // Socket file descriptor.
  int sock_fd_;
  int io_fd_;
  // Socket addresses.
  struct sockaddr_in src_addr_;
  struct sockaddr_in dst_addr_;
};

} // end namespace fp

#endif
