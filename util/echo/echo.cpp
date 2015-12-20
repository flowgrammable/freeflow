#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include "socket.hpp"

// Running flag.
bool running;

// Signal handler, for more graceful termination
void sig_handler(int sig)
{
  running = false;
}

// The UDP echo server main program. Creates a server socket, opens it, and
// runs until it receives an interrupt.
int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "Invalid arguments. Usage: " << argv[0] <<
      " [port]\n";
    return -1;
  }

  // Create the socket on the port.
  Socket* sock = new Echo_socket(argv[1], atoi(argv[2]));
  // Bind.
  sock->open();

  // Setup select.
  fd_set fdset;
  struct timeval tv;

  FD_ZERO(&fdset);
  FD_SET(sock->sock_, &fdset);

  tv.tv_sec = 4;
  tv.tv_usec = 0;

  // Start processing loop.
  signal(SIGINT, sig_handler);
  running = true;
  std::cerr << "Echo up...\n";
  while(running)
  {
    if (select(1, &fdset, nullptr, nullptr, &tv)) {
      sock->recv();
      sock->send();
    }
  }
  std::cerr << "Echo down...\n";
  // Cleanup.
  sock->close();
  delete sock;
  return 0;
}
