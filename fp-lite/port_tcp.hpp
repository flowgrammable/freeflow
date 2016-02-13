#ifndef FP_PORT_TCP_HPP
#define FP_PORT_TCP_HPP

#include "port.hpp"

#include <freeflow/ip.hpp>

namespace fp
{

class Context;


// A TCP port is a connect TCP stream socket.
//
// TODO: We are currently using TCP ports to emulate Ethernet
// ports, but we could also emulate optical ports, and we could
// use UDP to emulate wireless devices.
class Port_tcp : public Port
{
public:
  Port_tcp(int);

  // Returns the underlying socket.
  ff::Ipv4_stream_socket const& socket() const { return sock_; }
  ff::Ipv4_stream_socket&       socket()       { return sock_; }

  // Returns the ports connected file descriptor.
  int fd() const { return sock_.fd(); }

  void attach(ff::Ipv4_stream_socket&&);
  void detach();

  // Packet related funtions.
  bool open();
  bool close();
  bool send(Context const&);
  bool recv(Context&);

  ff::Ipv4_stream_socket sock_;
};


// Initialize the port to have the given id. Note that the
// link is physically down until attached to a connected
// socket.
//
// TODO: Surely there are some other initializable properties here.
inline
Port_tcp::Port_tcp(int id)
  : Port(id), sock_(ff::uninitialized)
{
  state_.link_down = true;
}


// Attach the port to a connected socket. Set the link-down
// state to false.
inline void
Port_tcp::attach(ff::Ipv4_stream_socket&& s)
{
  sock_ = std::move(s);
  state_.link_down = false;
}


// Detach the port from its connected socket. Set the link-down
// to true.
//
// TODO: Should this close the socket or let the system do that?
inline void
Port_tcp::detach()
{
  sock_.close();
  state_.link_down = true;
}


} // end namespace fp

#endif
