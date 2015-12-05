
#include <freeflow/unix.hpp>

#include <cstdlib>
#include <iostream>

// This is a simple example of a UNIX socket echo server. 
// You can connect to the server using the command
//
//    nc -U echo-socket
//
// where `ehco-socket` is the path to the UNIX socket
// created by this server. Note that this progra servers
// only a single connecto
//
// TODO: Allow the path to be configured.
//
// TODO: Implement graceful shutdown.

using namespace ff;


void serve(int);


int main()
{
  Unix_socket_address addr = "echo-socket";

  // Remove the file before creating the server. Otherwise, 
  // we get EINVAL when we try to bind.
  if (unlink(addr) == 0)
    std::cout << "* removing existing socket\n";


  // Create the socket.
  int sd = server_socket(addr);
  if (sd < 0) {
    std::cerr << "socket:" << std::strerror(errno) << '\n';
    return -1;
  }
  std::cout << "* waiting for connection\n";
  
  while (1) {
    // Accept a connection.
    int cd = accept(sd);
    if (cd < 0) {
      std::cerr << "accept: " << std::strerror(errno) << '\n';
      return -1;
    }
    std::cout << "* accepted connection\n";

    // And service the connection.
    serve(cd);
  }
}


// Run the echo server for a single connection.
void
serve(int cd)
{
  // Read data until the client closes.
  while (1) {
    char buf[1024];

    // Receive data.
    int nread = recv(cd, buf);
    if (nread <= 0) {
      if (nread < 0)
        std::cerr << "recv: " << std::strerror(errno) << '\n';
      close(cd);
      break;
    }
    buf[nread] = 0;

    std::cout << "* received\n* ----\n" << buf << "* ----\n";

    // Echo the message back.
    int nsent = send(cd, buf, nread);
    if (nsent < 0) {
      std::cerr << "send: " << std::strerror(errno) << '\n';
      close(cd);
      break;
    }
  }
  std::cout << "* closed connection\n";
}
