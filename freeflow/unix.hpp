
#ifndef FREEFLOW_UNIX_HPP
#define FREEFLOW_UNIX_HPP

// The UNIX module defines facilities for working with UNIX
// sockets. 

#include "socket.hpp"

#include <cassert>
#include <cstring>
#include <string>

#include <sys/un.h>

namespace ff
{

namespace un
{

// A UNIX socket address denotes an IPC endpoint for a local 
// application. The address is specified by a path name,
// which is a null-terminated string.
struct Socket_address : sockaddr_un
{
  Socket_address();
  Socket_address(char const*);
  Socket_address(std::string const&);

  Family      family() const { return sun_family; }
  char const* path() const   { return sun_path; }
};


// Default initialize the socket address. The default value
// is indeterminate.
inline
Socket_address::Socket_address()
  : sockaddr_un()
{
  sun_family = AF_UNIX;
}


// Initialize the socket address to use the path name given
// by `path`. The `path` argument shall not be null.
inline
Socket_address::Socket_address(char const* path)
{
  assert(path);
  sun_family = AF_UNIX;
  std::strcpy(sun_path, path);
}


// Initialize the socket address to use the path name given
// by `str`. The length of the string must be able to fit
// within the path allowable by a UNIX socket (108 bytes on
// Linux systems).
inline
Socket_address::Socket_address(std::string const& str)
{
  assert(str.size() < sizeof(sockaddr_un) - sizeof(sa_family_t));
  sun_family = AF_UNIX;
  std::strcpy(sun_path, str.c_str());
}


// -------------------------------------------------------------------------- //
//                          Address families


// Unlink the file used by a UNIX socket. This is provided for
// convenience since it is common to remove UNIX sockets prior
// to re-establishing connections.
inline int 
unlink(Socket_address const& addr)
{
  return ::unlink(addr.path());
}

} // namespace un


// Convenience names.
using Unix_socket_address = un::Socket_address;

} // namespace ff

#endif
