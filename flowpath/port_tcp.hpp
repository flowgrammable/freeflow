#ifndef FP_PORT_TCP_HPP
#define FP_PORT_TCP_HPP

#include "port.hpp"
#include "packet.hpp"
#include "context.hpp"
#include "dataplane.hpp"

#include <string>

namespace fp
{

class Port_tcp : public Port
{
public:
  using Port::Port;

  // Constructors/Destructor.
  Port_tcp(Port::Id, std::string const&);
  ~Port_tcp();

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
};

} // end namespace fp

#endif
