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
  Port_tcp(int id)
    : Port(id), sock_(ff::uninitialized)
  { }

  // Returns the underlying socket.
  ff::Ipv4_stream_socket const& socket() const { return sock_; }
  ff::Ipv4_stream_socket&       socket()       { return sock_; }

  // Returns the ports connected file descriptor.
  int fd() const { return sock_.fd(); }

  void bind(ff::Ipv4_stream_socket&&);

  // Packet related funtions.
  bool open();
  bool close();
  bool send(Context const&);
  bool recv(Context&);

  ff::Ipv4_stream_socket sock_;
};


// Bind the port to a connected socket, ensuring the port
// is put into an up state.
void
Port_tcp::bind(ff::Ipv4_stream_socket&& s)
{
  up();
  sock_ = std::move(s);
}


} // end namespace fp

#endif
