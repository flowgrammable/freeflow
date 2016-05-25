#ifndef FP_PORT_TCP_HPP
#define FP_PORT_TCP_HPP

#include "port.hpp"

#include <freeflow/ip.hpp>

namespace fp
{

class Context;


// -------------------------------------------------------------------------- //
// TCP Port infrastructure

// A TCP port is a connected TCP stream socket.
//
// TODO: We are currently using TCP ports to emulate Ethernet
// ports, but we could also emulate optical ports, and we could
// use UDP to emulate wireless devices.
class Port_tcp : public Port
{
public:
  using Socket = ff::Ipv4_stream_socket;

  Port_tcp(int);

  // TODO: I'm not sure that we're using these any more.
  bool open() { return true; }
  bool close() { return true; }

  // Returns the underlying socket.
  Socket const& socket() const { return sock_; }
  Socket&       socket()       { return sock_; }

  // Returns the ports connected file descriptor.
  int fd() const { return sock_.fd(); }

  void   attach(Socket&&);
  Socket detach();


  Socket sock_;
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
Port_tcp::attach(Socket&& s)
{
  sock_ = std::move(s);
  state_.link_down = false;
}


// Detach the port from its connected socket. Set the link-down
// to true and move the socket out of the port.
inline Port_tcp::Socket
Port_tcp::detach()
{
  state_.link_down = true;
  return std::move(sock_);
}


// -------------------------------------------------------------------------- //
// Ethernet over TCP port.

// Represents a port that sends and recieves Ethernet frames
// over a connected TCP socket.
class Port_eth_tcp : public Port_tcp
{
public:
  using Port_tcp::Port_tcp;

  bool send(Context&);
  bool recv(Context&);
};


} // end namespace fp

#endif
