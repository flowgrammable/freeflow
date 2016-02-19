
#ifndef FREEFLOW_IP_HPP
#define FREEFLOW_IP_HPP

// The IP module defines facilities for working with IPv4
// and IPv6 addresses and sockets.
//
// TODO: Implement support for IPv6.

#include "socket.hpp"

#include <cstring>

#include <netinet/in.h>
#include <arpa/inet.h>


namespace ff
{

namespace ip
{

// The Port type specifies the valid range of values for an IPv4 or IPv6
// port. IP ports are numbered 1 to 64K, with the first 2K ports reserved
// for system use.
//
// Note that all port values must be in network byte order. Use `htons`
// to convert host 16-bit values to network-byte order 16-bit values.
// Interfaces that perform this conversion take a short int instead
// of a port.
using Port = in_port_t;


namespace v4
{

// -------------------------------------------------------------------------- //
//                            Network Address

// The address class defines an IP address. Ths is an opaque 32-bit
// structure stored in network byte order.
struct Address : in_addr
{
  Address() = default;
  Address(in_addr);
  explicit Address(in_addr_t);
  Address(char const*);
  Address(std::string const&);

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


// Initalize the address by converting the host string into
// an IP address. If that conversion fails, an exception is
// thrown.
inline
Address::Address(char const* host)
{
  int ret = inet_pton(IPv4, host, this);
  if (ret <= 0) {
    if (ret == 0)
      throw std::runtime_error("invalid hostname");
    else
      throw std::system_error(errno, std::system_category());
  }
}


inline
Address::Address(std::string const& host)
  : Address(host.c_str())
{ }


// -------------------------------------------------------------------------- //
//                            Socket Address


// A socket address denotes an endpoint for a network application. It is
// comprised of an IP address and the application's port number.
struct Socket_address : sockaddr_in
{
  using Address = v4::Address;

  Socket_address();
  Socket_address(Address, std::uint16_t);

  static constexpr Family address_family() { return AF_INET; }

  Family        family() const  { return sin_family; }
  std::uint16_t port() const    { return ntohs(sin_port); }
  Address       address() const { return sin_addr; }
};


// Initialize the socket address. The initial value is indeterminate.
inline
Socket_address::Socket_address()
  : sockaddr_in()
{
  sin_family = AF_INET;
}


// Initialize the socket address.
inline
Socket_address::Socket_address(Address a, std::uint16_t p)
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
using Ipv4_stream_socket = Stream_socket<Ipv4_socket_address>;

} // namespace ff

#endif
