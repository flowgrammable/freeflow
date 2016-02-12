
#ifndef FREEFLOW_IP_HPP
#define FREEFLOW_IP_HPP

// The IP module defines facilities for working with IPv4
// and IPv6 addresses and sockets.
//
// TODO: Implement support for IPv6.

#include "socket.hpp"

#include <cstring>

#include <netinet/in.h>

namespace ff
{

namespace ip
{

// The Port type specifies the valid range of values
// for an IPv4 or IPv6 port. IP ports are numbered
// 1 to 64K, with the first 2K ports reserved for
// system use.
//
// Note that all port values must be in network byte
// order. Use `htons` to convert host 16-bit values
// to network-byte order 16-bit values.
using Port = in_port_t;


namespace v4
{

// -------------------------------------------------------------------------- //
//                            Network Address

// The address class defines an IP address. Ths is
// an opaque 32-bit structure stored in network byte
// order.
struct Address : in_addr
{
  Address() = default;
  Address(in_addr);
  explicit Address(in_addr_t);

  // Named addresses.
  static Address loopback()  { return Address(INADDR_LOOPBACK); }
  static Address any()       { return Address(INADDR_ANY); }
  static Address broadcast() { return Address(INADDR_BROADCAST); }
};


// Initialize the address as a copy of `addr`.
inline
Address::Address(in_addr addr)
  : in_addr{addr.s_addr}
{ }


// Initilialize this addrss with the given `addr` value. Note
// that `addr` must be in network byte order.
inline
Address::Address(in_addr_t addr)
  : in_addr{addr}
{ }


// -------------------------------------------------------------------------- //
//                            Socket Address


// A socket address denotes an endpoint for a network
// application. It is comprised of an IP address and the
// application's port number.
struct Socket_address : sockaddr_in
{
  Socket_address();
  Socket_address(Address, Port);

  Family  family() const  { return sin_family; }
  Port    port() const    { return ntohs(sin_port); }
  Address address() const { return sin_addr; }
};


// Initialize the socket address. The initial value is
// indeterminate.
inline
Socket_address::Socket_address()
  : sockaddr_in()
{
  sin_family = AF_INET;
}


// Initialize the socket address. Note that the address and
// port must be in network byte order.
inline
Socket_address::Socket_address(Address a, Port p)
  : sockaddr_in()
{
  sin_family = AF_INET;
  sin_port = htons(p);
  sin_addr = a;
}


} // namespace v4


} // namespace ip


// Convenience names.
using Ip_port             = ip::Port;
using Ipv4_address        = ip::v4::Address;
using Ipv4_socket_address = ip::v4::Socket_address;

} // namespace ff

#endif
