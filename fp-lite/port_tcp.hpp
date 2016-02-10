#ifndef FP_PORT_TCP_HPP
#define FP_PORT_TCP_HPP

#include "port.hpp"

#include <freeflow/ip.hpp>

namespace fp
{

struct Context;


class Port_tcp : public Port
{
public:
  // Constructors/Destructor.
  Port_tcp(Port::Id, ff::Ipv4_address, ff::Ip_port);

  // Packet related funtions.
  bool open();
  bool close();
  bool send(Context const&);
  bool recv(Context&);

  ff::Ipv4_socket_address src_;
  ff::Ipv4_socket_address dst_;
};


} // end namespace fp

#endif
