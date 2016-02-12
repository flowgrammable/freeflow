
#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"

#include <freeflow/socket.hpp>
#include <freeflow/poll.hpp>

#include <string>
#include <iostream>
#include <signal.h>


using namespace ff;

// Emulate a 2 port wire running over TCP ports.

// NOTE: Clang will optimize assume that the running loop
// never terminates if this is not declared volatile.
// Go figure.
static bool volatile running;

void
on_signal(int sig)
{
  running = false;
}


int
main()
{
  // TODO: Use sigaction.
  signal(SIGINT, on_signal);
  signal(SIGKILL, on_signal);
  signal(SIGHUP, on_signal);

  // Build a server socket that will accept network connections.
  Ipv4_socket_address addr(Ipv4_address::any(), 5000);
  Ipv4_stream_socket server(addr);


  // The two (possible) endpoints.
  // int ends[2] { -1, -1 };

  // TODO: Handle exceptions.

  // Create some ports. This is the "system" port table.
  // fp::Port_tcp p1(0, Ipv4_address::any(), 5000);
  // fp::Port_tcp p2(1, Ipv4_address::any(), 5001);

  // Configure the dataplane. Ports must be added
  // before applications are loaded.
  fp::Dataplane dp = "dp1";
  dp.add_drop_port();
  dp.load_application("apps/wire.app");
  dp.up();


  // Allocate the client sockets. We'll properly initialize them
  // when they are created.
  Ipv4_stream_socket ends[] {
    uninitialized,
    uninitialized
  };
  int nends = 0;

  // Set up the initial polling state.
  Poll_file fds[] {
    { server.fd(), POLLIN },
    { -1, 0 },
    { -1, 0 }
  };

  // Accept connections from the server socket.
  auto accept = [&](Ipv4_stream_socket& server) {
    Ipv4_socket_address addr;
    Ipv4_stream_socket client = server.accept(addr);
    if (!client)
      return; // TODO: Log the error.


    // If we already have two endpoints, just return, which
    // will cause the socket to be closed.
    if (nends == 2) {
      std::cout << "[flowpath] reject connection " << addr.port() << '\n';
      return;
    }
    std::cout << "[flowpath] accept connection " << addr.port() << '\n';

    client.send("hello\n");

    // Register the socket with the endpoint and poll sets.
    fds[nends + 1] = { client.fd(), POLLIN };
    ends[nends] = std::move(client);
    ++nends;

    // TODO: Actually add a new port to the system.
  };


  // Handle input from the client socket.
  auto input = [&](Ipv4_stream_socket& client) {
    char buf[2048];
    int n = client.recv(buf);
    if (n <= 0) {
      std::cout << "[flowpath] closing endpoint " << nends << '\n';
      // Shuffle information around so we don't leave empty
      // spaces in arrays.
      if (&client == &ends[0]) {
        std::swap(ends[0], ends[1]);
        std::swap(fds[1], fds[2]);
      }
      ends[1].close();
      fds[2] = { -1, 0 };
      --nends;

      // TODO: Actually remove the port from the dataplane.
    }

    buf[n] = 0;
    std::cout << buf;

    // TODO: Create the context for the packet.

  };


  // Main lookp.
  running = true;
  while (running) {
    // Poll for 100 milliseconds. Note that this can fail with
    // EINTR, which really isn't an error.
    poll(fds, 1 + nends, 1000);

    // Process input events.
    if (fds[0].can_read())
      accept(server);
    if (nends > 0 && fds[1].can_read())
      input(ends[0]);
    if (nends > 1 && fds[2].can_read())
      input(ends[1]);
  }

  // Take the dataplane down.
  dp.down();
  dp.unload_application();

  return 0;
}
