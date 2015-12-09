
#ifndef FREEFLOW_SOCKET_HPP
#define FREEFLOW_SOCKET_HPP

// The socket module defines a low-level wrapper around the 
// POSIX socket interface.

#include "system.hpp"

#include <cstddef>

#include <sys/socket.h>

namespace ff
{

// -------------------------------------------------------------------------- //
//                          Address families

// The Family type is one of the address families supported 
//  by the host system.
using Family = sa_family_t;

// Families currently supported.
constexpr Family IPv4 = AF_INET;
constexpr Family IPv6 = AF_INET6;
constexpr Family UNIX = AF_UNIX;


// -------------------------------------------------------------------------- //
//                       Generic Socket address

// The generic socket address is used as an input
// to most of the low-level socket interface. Generic
// socket addresses cannot be constructed or copied.
struct Socket_address : sockaddr
{
  Socket_address() = delete;
  Socket_address(Socket_address const&) = delete;
  Socket_address& operator=(Socket_address const&) = delete;

  Family family() const { return sa_family; }
};


// -------------------------------------------------------------------------- //
//                       Socket address storage

// The socket address storage class is an address object
// large enough to contain any address family supported
// by the host sytem.
struct Socket_address_store : sockaddr_storage
{
  Socket_address_store()
    : sockaddr_storage()
  { }

  Family family() const { return ss_family; }
};


// -------------------------------------------------------------------------- //
//                        Socket interface

// Socket constructors
int stream_socket(Family, int = 0);
int datagram_socket(Family, int = 0);


// Create a stream socket, bind it to the given address
// and put it in a listening state. Use the specified
// protocol, if given.
template<typename Addr>
int
server_socket(Addr const& addr, int proto = 0)
{
  // Create the socket.
  int sd = stream_socket(addr.family(), proto);
  if (sd < 0)
    return sd;

  // Bind to the specified address.
  if (::bind(sd, (sockaddr const*)&addr, sizeof(addr)) < 0)
    return -1;

  // Listen for connections.
  if (::listen(sd, SOMAXCONN) < 0)
    return -1;

  return sd;
}


// Create a stream socket and connect it to the
// given address. Use the specified protocol if
// specified.
template<typename Addr>
int 
client_socket(Addr const& addr, int proto = 0)
{
  // Create the socket.
  int sd = stream_socket(addr.family(), proto);
  if (sd < 0)
    return sd;

  // Bind to the specified address.
  if (::connect(sd, (sockaddr const*)&addr, sizeof(addr)) < 0)
    return -1;

  return sd;
}


// Accept a connection. This overload does not retain the
// peer address of the connecting client.
inline int
accept(int sd)
{
  return ::accept(sd, nullptr, nullptr);
}


// Accept a conection and save the result in the given
// address. This returns a descriptor for the accepted
// connection.
template<typename Addr>
inline int 
accept(int sd, Addr& addr)
{
  socklen_t len = sizeof(addr);
  return ::accept(sd, (sockaddr*)&addr, &len);
}


// Receive at most `len` bytes of data on the socket and copy
// it into the object pointed to by `buf`.
inline int
recv(int sd, void* buf, std::size_t len)
{
  return ::recv(sd, buf, len, 0);
}


// Receive data into an array of objects of bound N. Note
// that the call may not fully initialize each object in
// the buffer. In general, T should be a narrow character
// type.
template<typename T, int N>
inline int
recv(int sd, T(&buf)[N])
{
  return ::recv(sd, buf, sizeof(buf), 0);
}


// Send at most `len` bytes of data on the socket from the
// object pointed to by `buf`.
inline int
send(int sd, void const* buf, std::size_t len)
{
  return ::send(sd, buf, len, 0);
}


// Send data from an array of objects of bound N. Note
// that the call may not fully transmit each object in
// the buffer. In general, T should be a narrow character
// type.
template<typename T, int N>
inline int
send(int sd, T const (&buf)[N])
{
  return ::send(sd, buf, sizeof(buf), 0);
}


// Send a string through the conncted socket.
//
// TODO: Ensure that we send the entire string.
inline int
send(int sd, std::string const& str)
{
  return ::send(sd, str.c_str(), str.size(), 0);
}


} // namespace ff

#endif
