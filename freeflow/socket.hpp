
#ifndef FREEFLOW_SOCKET_HPP
#define FREEFLOW_SOCKET_HPP

// The socket module defines a low-level wrapper around the
// POSIX socket interface.
//
// TODO: It might be worthwhile to parameterize the socket
// address by the address family and then specialize for each
// supported protocol.

#include "system.hpp"

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <utility>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>

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
recv(int sd, char* buf, std::size_t len)
{
  return ::recv(sd, buf, len, 0);
}


inline int
recv(int sd, unsigned char* buf, std::size_t len)
{
  return ::recv(sd, buf, len, 0);
}


// Receive data into an array of objects of bound N. Note
// that the call may not fully initialize each object in
// the buffer.
template<int N>
inline int
recv(int sd, char (&buf)[N])
{
  return ::recv(sd, buf, N, 0);
}


template<int N>
inline int
recv(int sd, unsigned char (&buf)[N])
{
  return ::recv(sd, buf, N, 0);
}


// Send at most `len` bytes of data on the socket from the
// object pointed to by `buf`.
inline int
send(int sd, char const* buf, std::size_t len)
{
  return ::send(sd, buf, len, 0);
}


inline int
send(int sd, unsigned char const* buf, std::size_t len)
{
  return ::send(sd, buf, len, 0);
}


// Send data from an array of objects of bound N. Note
// that the call may not fully transmit each object in
// the buffer. In general, T should be a narrow character
// type.
template<int N>
inline int
send(int sd, char const (&buf)[N])
{
  return ::send(sd, buf, sizeof(buf), 0);
}


template<int N>
inline int
send(int sd, unsigned char const (&buf)[N])
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


struct boolean_option
{
  boolean_option(bool b)
    : value(b)
  { }

  int value;
};


struct reuse_address : boolean_option
{
  using boolean_option::boolean_option;
};


struct nonblocking : boolean_option
{
  using boolean_option::boolean_option;
};

struct nodelay : boolean_option
{
  using boolean_option::boolean_option;
};


// TODO: This could be a made a template, with each of the
// types above modeling some concept, but I don't feel like
// working through it right now.
inline int
set_option(int sd, reuse_address opt)
{
// FIXME: Do better than this.
#ifdef __linux__
  return ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt.value, sizeof(opt.value));
#else
  return 0;
#endif
}


inline int
set_option(int sd, nonblocking opt)
{
  return fcntl(sd, F_SETFL, fcntl(sd, F_GETFL, 0) | O_NONBLOCK);
}


inline int
set_option(int sd, nodelay opt)
{
// FIXME: Do better than this.
#ifdef __linux__
  return ::setsockopt(sd, SOL_TCP, TCP_NODELAY, &opt.value, sizeof(opt.value));
#else
  return 0;
#endif
}


template<typename Opt, typename... Opts>
inline int
set_options(int sd, Opt opt, Opts... opts)
{
  int n = set_option(sd, opt);
  if (n < 0)
    return n;
  set_options(sd, opts...);
}



// -------------------------------------------------------------------------- //
// Socket class


struct give_t
{
  int fd;
};


// Returns an object that gives a file descriptor to an owner.
inline give_t
give(int fd)
{
  return {fd};
}


// A special value (of enum type) that requests initialization
// later (e.g., by move).
using uninit_t = void(*)();

// Look aways.
inline void uninitialized() { }


// The socket class is a simple wrapper around a socket descriptor.
// It is a resource, meaning it is a move-only type. The socket
// is parameterized by its socket address type.
//
// TODO: Throw exceptions on failure?
template<typename Addr>
class Socket
{
public:
  Socket(int kind, int proto = 0);
  Socket(give_t);
  Socket(uninit_t);

  Socket(Socket&&);
  Socket& operator=(Socket&&);

  Socket(Socket const&) = delete;
  Socket& operator=(Socket const&) = delete;

  ~Socket();

  // Returns true if the file descriptor is valid.
  explicit operator bool() const { return fd_ != -1; }

  // Returns the underlying file descriptor.
  int fd() const { return fd_; }
  int fd()       { return fd_; }

  void close();

  int recv(char*, int);
  int recv(unsigned char*, int);
  int send(char const*, int);
  int send(unsigned char const*, int);

  template<int N> int recv(char (&)[N]);
  template<int N> int recv(unsigned char (&)[N]);
  template<int N> int send(char const (&)[N]);
  template<int N> int send(unsigned char const (&)[N]);

private:
  int fd_;
};


template<typename Addr>
inline
Socket<Addr>::Socket(int kind, int proto)
  : fd_(::socket(Addr::address_family(), kind, proto))
{
  if (fd_ < 0)
    throw std::system_error(errno, std::system_category());
}


template<typename Addr>
Socket<Addr>::Socket(give_t given)
  : fd_(given.fd)
{ }


template<typename Addr>
Socket<Addr>::Socket(uninit_t)
  : fd_(-1)
{ }


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


template<typename Addr>
inline void
Socket<Addr>::close()
{
  ::close(fd_);
  fd_ = -1;
}


template<typename Addr>
inline int
Socket<Addr>::recv(char* buf, int n)
{
  return ff::recv(fd_, buf, n);
}


template<typename Addr>
inline int
Socket<Addr>::recv(unsigned char* buf, int n)
{
  return ff::recv(fd_, buf, n);
}


template<typename Addr>
inline int
Socket<Addr>::send(char const* buf, int n)
{
  return ff::send(fd_, buf, n);
}


template<typename Addr>
inline int
Socket<Addr>::send(unsigned char const* buf, int n)
{
  return ff::send(fd_, buf, n);
}


template<typename Addr>
template<int N>
inline int
Socket<Addr>::recv(char (&buf)[N])
{
  return ff::recv(fd_, buf);
}


template<typename Addr>
template<int N>
inline int
Socket<Addr>::recv(unsigned char (&buf)[N])
{
  return ff::recv(fd_, buf);
}


template<typename Addr>
template<int N>
inline int
Socket<Addr>::send(char const (&buf)[N])
{
  return ff::send(fd_, buf);
}


template<typename Addr>
template<int N>
inline int
Socket<Addr>::send(unsigned char const (&buf)[N])
{
  return ff::send(fd_, buf);
}


// -------------------------------------------------------------------------- //
// Stream socket class

template<typename Addr>
struct Stream_socket : Socket<Addr>
{
  using Socket<Addr>::Socket;
  using Socket<Addr>::operator=;

  Stream_socket(int proto = 0);
  Stream_socket(Addr const& addr);

  bool listen(Addr const&);
  bool connect(Addr const&);

  Stream_socket accept();
  Stream_socket accept(Addr&);
};


template<typename Addr>
inline
Stream_socket<Addr>::Stream_socket(int proto)
  : Socket<Addr>(SOCK_STREAM, proto)
{ }


// Construct a stream socket, bind it to the given address
// and put it in listening model.
//
// TODO: This isn't the absolute best interface. What if I
// want a bound socket that I'm going to connect with?
template<typename Addr>
inline
Stream_socket<Addr>::Stream_socket(Addr const& addr)
  : Stream_socket()
{
  if (!listen(addr))
    throw std::system_error(errno, std::system_category());
}


template<typename Addr>
inline bool
Stream_socket<Addr>::listen(Addr const& addr)
{
  if (ff::bind(this->fd(), addr) < 0)
    return false;
  if (ff::listen(this->fd()) < 0)
    return false;
  return true;
}


// Initiate a connection to the application given by addr. Returns
// false, if an error occurred.
template<typename Addr>
inline bool
Stream_socket<Addr>::connect(Addr const& addr)
{
  return ff::connect(this->fd(), addr) == 0;
}


// Accepts a connection, returning a new socket object. If
// the accept fails, the resulting socket will be invalid.
template<typename Addr>
inline Stream_socket<Addr>
Stream_socket<Addr>::accept()
{
  int cd = ff::accept(this->fd());
  return Stream_socket(give(cd));
}


// Accepts a connection, returning a new socket object. If
// the accept fails, the resulting socket will be invalid.
template<typename Addr>
inline Stream_socket<Addr>
Stream_socket<Addr>::accept(Addr& addr)
{
  int cd = ff::accept(this->fd(), addr);
  return Stream_socket(give(cd));
}



} // namespace ff

#endif
