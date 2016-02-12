
#ifndef FREEFLOW_SOCKET_HPP
#define FREEFLOW_SOCKET_HPP

// The socket module defines a low-level wrapper around the
// POSIX socket interface.

#include "system.hpp"

#include <cstddef>
#include <system_error>
#include <utility>

#include <sys/socket.h>
#include <netinet/in.h>


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
// Socket interface

// Create an unbound stream socket.
inline int
stream_socket(Family af, int proto = 0)
{
  return ::socket(af, SOCK_STREAM, proto);
}


// Create an unbound datagram socket.
inline int
datagram_socket(Family af, int proto = 0)
{
  return ::socket(af, SOCK_DGRAM, proto);
}


// Bind the interface to the given address.
template<typename Addr>
inline int
bind(int sd, Addr const& addr)
{
  return ::bind(sd, (sockaddr const*)&addr, sizeof(addr));
}


// Listen for connections.
inline int
listen(int sd)
{
  return ::listen(sd, SOMAXCONN);
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


// Connect to the given socket.
template<typename Addr>
inline int
connect(int sd, Addr const& addr)
{
  return ::connect(sd, (sockaddr const*)&addr, sizeof(addr));
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


// -------------------------------------------------------------------------- //
// Socket constructors

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
  if (bind(sd, addr) < 0)
    return -1;

  // Listen for connections.
  if (listen(sd) < 0)
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


// -------------------------------------------------------------------------- //
// Socket options

inline int
reuse_address(int sd, int val)
{
  return ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (void const*)&val, sizeof(int));
}


// -------------------------------------------------------------------------- //
// Socket class

// The socket class is a simple wrapper around a socket descriptor.
// It is a resource, meaning it is a move-only type. The socket
// is parameterized by its socket address type.
template<typename Addr>
class Socket
{
public:
  Socket(int kind, int proto = 0);

  explicit Socket(int);

  Socket(Socket&&);
  Socket& operator=(Socket&&);

  Socket(Socket const&) = delete;
  Socket& operator=(Socket const&) = delete;

  ~Socket();

  int fd() const { return fd; }

private:
  int fd_;
};


template<typename Addr>
inline
Socket<Addr>::Socket(int kind, int proto)
  : fd_(::socket(Addr::family(), kind, proto))
{
  if (fd_ < 0)
    throw std::system_error(errno, std::system_category());
}


template<typename Addr>
inline
Socket<Addr>::~Socket()
{
  if (fd_ >= 0)
    ::close(fd_);
}


template<typename Addr>
inline
Socket<Addr>::Socket(Socket&& s)
  : fd_(s.fd_)
{
  s.fd_ = -1;
}


template<typename Addr>
inline Socket<Addr>&
Socket<Addr>::operator=(Socket&& s)
{
  if (fd_ >= 0)
    ::close(fd_);
  fd_ = s.fd_;
  s.fd_ = -1;
  return *this;
}


// -------------------------------------------------------------------------- //
// Stream socket class

template<typename Addr>
struct Stream_socket : Socket<Addr>
{
  using Socket<Addr>::Socket;
  using Socket<Addr>::operator=;

  Stream_socket(int proto = 0)
    : Socket<Addr>(SOCK_STREAM, proto)
  { }

  bool listen(Addr const&);
  bool connect(Addr const&);

  Stream_socket&& accept();
  Stream_socket&& accept(Addr&);
};


template<typename Addr>
inline bool
Stream_socket<Addr>::listen(Addr const& addr)
{
  if (bind(this->fd(), addr) < 0)
    return false;
  if (listen(this->fd()) < 0)
    return false;
  return true;
}



} // namespace ff

#endif
