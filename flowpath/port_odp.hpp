#ifndef FP_PORT_ODP_HPP
#define FP_PORT_ODP_HPP

#include "port.hpp"
#include "packet.hpp"
#include "context.hpp"
#include "dataplane.hpp"

#include <string>

namespace fp
{

// ODP Port thread work function.
extern void* odp_work_fn(void*);

class Port_odp : public Port
{
public:
  using Port::Port;

  // Constructors/Destructor.
  Port_odp(Port::Id, std::string const&);
  ~Port_odp();

  // Packet related funtions.
  Context*  recv();
  int       send();

  // Port state functions.
  int     open();
  void    close();

  // Accessors.
  Function work_fn() { return odp_work_fn; }

private:
  // Data members.
  //
  // Socket file descriptor.
  //int sock_fd_;
  //int send_fd_;
  // Socket addresses.
  //struct sockaddr_in src_addr_;
  //struct sockaddr_in dst_addr_;
  // Message containers.
  struct mmsghdr     messages_[512];
  struct iovec       iovecs_[512];
  char**             buffer_;
  //struct timespec    timeout_;
};

} // end namespace fp

#endif
