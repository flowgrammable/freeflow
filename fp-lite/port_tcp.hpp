#ifndef FP_PORT_TCP_HPP
#define FP_PORT_TCP_HPP

#include "port.hpp"

#include <freeflow/ip.hpp>

namespace fp
{

class Context;


// A TCP port is a connect TCP stream socket.
class Port_tcp : public Port
{
public:
  // Constructors/Destructor.
  Port_tcp(int id, ff::Ipv4_stream_socket&& s)
    : Port(id), sock_(std::move(s))
  { }

  // Returns the underlying socket.
  ff::Ipv4_stream_socket const& socket() const { return sock_; }
  ff::Ipv4_stream_socket&       socket()       { return sock_; }


  // Packet related funtions.
  bool open();
  bool close();
  bool send(Context const&);
  bool recv(Context&);

  ff::Ipv4_stream_socket sock_;
};


} // end namespace fp

#endif
