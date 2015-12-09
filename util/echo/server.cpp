#include <iostream>
#include <string>
#include <cstdlib>
#include <signal.h>
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
  Socket* sock = new Server_socket(atoi(argv[1]));
  // Bind.
  sock->open();

  // Start processing loop.
  signal(SIGINT, sig_handler);
  running = true;
  std::cerr << "Server up...\n";
  while(running)
  {
    // Call 'send' if the call to 'recv' was successful (a message was found).
    if (sock->recv())
      sock->send();
  }
  std::cerr << "Server down...\n";
  // Cleanup.
  sock->close();
  delete sock;
  return 0;
}
