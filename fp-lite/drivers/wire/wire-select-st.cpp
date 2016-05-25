
#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"
#include "context.hpp"
#include "application.hpp"

#include <freeflow/socket.hpp>
#include <freeflow/select.hpp>

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
  Port_eth_tcp port1(0);
  Port_eth_tcp port2(1);

  // Configure the dataplane. Ports must be added before
  // applications are loaded.
  fp::Dataplane dp = "dp1";
  dp.add_port(&port1);
  dp.add_port(&port2);
  dp.add_drop_port();
  dp.load_application("apps/wire.app");
  dp.up();

  // Set up the initial polling state.
  Select_set ss;
  int nports = 0; // Current number of ports
  // Add the server socket to the select set.
  ss.add_read(server.fd());

  // FIXME: Factor the accept/ingress code into something
  // a little more reusable.

  // Accept connections from the server socket.
  auto accept = [&](Ipv4_stream_socket& server)
  {
    std::cout << "[flowpath] accept\n";
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
    ss.add_read(client.fd());

    // Bind the socket to a port.
    // TODO: Emit a port status change to the application. Does
    // that happen implicitly, or do we have to cause the dataplane
    // to do it.
    Port_tcp* port = nullptr;
    if (nports == 0)
      port = &port1;
    if (nports == 1)
      port = &port2;
    port->attach(std::move(client));
    ++nports;

    // Notify the application of the port change.
    Application* app = dp.get_application();
    app->port_changed(*port);
  };

  // Handle input from the client socket.
  //
  // TODO: This defines the basic ingress pipeline. How
  // do we refactor this to make it reasonably composable.
  auto ingress = [&](Port_tcp& port)
  {
    // Ingress the packet.
    Byte buf[2048];
    Context cxt(buf);
    bool ok = port.recv(cxt);

    // Handle error or closure.
    if (!ok) {
      // Detache the socket.
      Ipv4_stream_socket client = port.detach();

      // Notify the application of the port change.
      Application* app = dp.get_application();
      app->port_changed(port);

      // Update the poll set.
      ss.del_read(client.fd());
      --nports;
      return;
    }

    // Otherwise, process the application.
    //
    // TODO: This really just runs one step of the pipeline. This needs
    // to be a loop that continues processing until there are no further
    // table redirections.
    Application* app = dp.get_application();
    app->process(cxt);

    // Assuming there's an output send to it.
    if (Port* out = cxt.output_port())
      out->send(cxt);
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
    // Wait for 100 milliseconds. Note that this can fail with
    // EINTR, which really isn't an error.
    //
    // NOTE: It seems the common practice is to re-poll when EINTR
    // occurs since like you said, it's not really an error. Most
    // impls just stick it in a do-while(errno != EINTR);
    select(ss, 1000);

    if (ss.can_read(server.fd()))
      accept(server);
    if (ss.can_read(port1.fd()))
      input(port1.fd());
    if (ss.can_read(port2.fd()))
      input(port2.fd());
  }

  // Take the dataplane down.
  dp.down();
  dp.unload_application();

  return 0;
}
