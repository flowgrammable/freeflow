#include <iostream>
#include <string>
#include <cstdlib>
#include <signal.h>
#include "socket.hpp"

// Running flag.
bool running;

// Signal handler, for a more graceful termination.
void sig_handler(int sig)
{
  running = false;
}

// The UDP echo client main program. Creates a client socket bound to the given
// server address, and transmits messages from stdin to the server. When an
// interrupt is received, it will shutdown.
int main(int argc, char* argv[])
{
  if (argc != 3) {
    printf("Invalid arguments. Usage: %s <address> <port>\n", argv[0]);
    return 0;
  }

  // Create the socket.
  Socket* sock = new Client_socket(argv[1], atoi(argv[2]));
  sock->open();

  // Establish handler, setup run flag.
  signal(SIGINT, sig_handler);
  running = true;
  while(running)
  {
    // Send the message.
    sock->send();
    // Get it back.
    sock->recv();
  }
  // Cleanup.
  sock->close();
  delete sock;
  return 0;
}
