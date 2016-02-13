
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

  // Pre-create all standard ports.
  Port_tcp port1(0);
  Port_tcp port2(1);

  // Configure the dataplane. Ports must be added before
  // applications are loaded.
  fp::Dataplane dp = "dp1";
  dp.add_port(&port1);
  dp.add_port(&port2);
  dp.add_drop_port();
  dp.load_application("apps/wire.app");
  dp.up();

  // Set up the initial polling state.
  Poll_file fds[] {
    { server.fd(), POLLIN },
    { -1, 0 },
    { -1, 0 }
  };
  int nports = 0; // Current number of ports

  // FIXME: Factor the accept/ingress code into something
  // a little more reusable.

  // Accept connections from the server socket.
  auto accept = [&](Ipv4_stream_socket& server)
  {
    // Accept the connection.
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

    // Bind the socket to a port.
    // TODO: Emit a port status change to the application. Does
    // that happen implicitly, or do we have to cause the dataplane
    // to do it.
    if (nports == 0)
      port1.bind(std::move(client));
    if (nports == 1)
      port2.bind(std::move(client));
    ++nports;
  };

  // Handle input from the client socket.
  auto ingress = [&](Port_tcp& port)
  {
    // Pre-buld the packet and context. And allow the port
    // to initialize them. We proess the packet below.
    Byte buf[2048];
    Context cxt(buf);
    port.recv(cxt);

    // If receiving causes the port to go down, then
    // terminate the connection.
    if (port.is_down()) {
      if (nports == 2) {
        if (port.fd() == fds[1].fd)
          fds[1] = fds[2];
        fds[2] = { -1, 0 };
      } else {
        fds[1] = { -1, 0 };
      }
      --nports;
      return;
    }

    // Otherwise, process the application.
    Application* app = dp.get_application();
    app->process(cxt);
  };

  // Select a port to handle input.
  auto input = [&](int fd)
  {
    if (fd == port1.fd())
      ingress(port1);
    else
      ingress(port2);
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
    if (fds[1].can_read())
      input(fds[1].fd);
    if (fds[2].can_read())
      input(fds[2].fd);
  }

  // Take the dataplane down.
  dp.down();
  dp.unload_application();

  return 0;
}
