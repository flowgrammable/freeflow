
#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"
#include "context.hpp"
#include "application.hpp"

#include <freeflow/socket.hpp>
#include <freeflow/poll.hpp>

#include <string>
#include <iostream>
#include <signal.h>


using namespace ff;
using namespace fp;


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

  // TODO: Handle exceptions.

  // Configure the dataplane. Ports must be added
  // before applications are loaded.
  fp::Dataplane dp = "dp1";
  dp.add_drop_port();
  dp.load_application("apps/wire.app");
  dp.up();


  // The ports representing clients. These are allocated when
  // a client is connected.
  Port_tcp* ports[] { nullptr, nullptr };
  int nports = 0; // Current number of ports
  int id = 0;    // The port id counter

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
    if (nports == 2) {
      std::cout << "[flowpath] reject connection " << addr.port() << '\n';
      return;
    }
    std::cout << "[flowpath] accept connection " << addr.port() << '\n';

    // Update the poll set.
    fds[nports + 1] = { client.fd(), POLLIN };

    // Create and register a new port.
    Port_tcp* port = new Port_tcp(id, std::move(client));
    dp.add_port(port);
    ports[nports] = port;

    // Update counts.
    id = (id + 1) % 2;
    ++nports;
  };


  // Handle input from the client socket.
  auto input = [&](Port_tcp& port) {
    Byte buf[2048];
    Context cxt(Ingress_info{}, buf);
    port.recv(cxt);

    // If receiving causes the port to go down, then
    // terminate the connection.
    if (port.is_down()) {
      // Close the port and remove it from the system.
      port.close();
      dp.remove_port(&port);

      // Shuffle information around so we don't leave empty
      // spaces in arrays.
      if (&port == ports[0]) {
        ports[0] = ports[1];
        fds[1] = fds[2];
      }
      ports[1] = nullptr;
      fds[2] = { -1, 0 };
      --nports;
      return;
    }

    // Otherwise, process the application.
    Application* app = dp.get_application();
    app->process(cxt);
  };


  // Main lookp.
  running = true;
  while (running) {
    // Poll for 100 milliseconds. Note that this can fail with
    // EINTR, which really isn't an error.
    poll(fds, 1 + nports, 1000);

    // Process input events.
    if (fds[0].can_read())
      accept(server);
    if (nports > 0 && fds[1].can_read())
      input(*ports[0]);
    if (nports > 1 && fds[2].can_read())
      input(*ports[1]);
  }

  // Take the dataplane down.
  dp.down();
  dp.unload_application();

  return 0;
}
