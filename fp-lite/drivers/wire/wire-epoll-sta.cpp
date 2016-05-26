// Only build this example on a linux machine, as epoll
// is not portable to Mac.


#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"
#include "context.hpp"
#include "application.hpp"
#include "queue.hpp"

#include <freeflow/socket.hpp>
#include <freeflow/epoll.hpp>
#include <freeflow/time.hpp>

#include <string>
#include <iostream>
#include <signal.h>
#include <unistd.h>



using namespace ff;
using namespace fp;


// Emulate a 2 port wire running over TCP ports in an STA.

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
  set_option(server.fd(), reuse_address(true));
  set_option(server.fd(), nonblocking(true));

  // TODO: Handle exceptions.

  // Pre-create all standard ports.
  Port_eth_tcp port1(1);
  Port_eth_tcp port2(2);

  // Reporting stastics init.
  Port::Statistics p1_stats = {0,0,0,0};
  Port::Statistics p2_stats = {0,0,0,0};

  // Egress queue.
  Queue<Context> egress_queue;

  // Configure the dataplane. Ports must be added before
  // applications are loaded.
  fp::Dataplane dp = "dp1";
  dp.add_port(&port1);
  dp.add_port(&port2);
  dp.add_drop_port();
  dp.load_application("apps/wire.app");
  dp.up();

  // Set up the initial polling state.
  Epoll_set eps(3);
  // Current number of ports.
  int nports = 0; 
  // Add the server socket to the select set.
  eps.add(server.fd());

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
    eps.add(client.fd());
    // Set non-blocking.
    set_option(client.fd(), nonblocking(true));

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
  auto ingress = [&](Port_eth_tcp& port)
  {
    //std::cout << "[wire] ingress on: " << port.id() << '\n';
    // Ingress the packet.
    Byte buf[2048];
    Context cxt(buf);
    bool ok = port.recv(cxt);

    // Handle error or closure.
    if (!ok) {
      // Detach the socket.
      Ipv4_stream_socket client = port.detach();

      // Notify the application of the port change.
      Application* app = dp.get_application();
      app->port_changed(port);

      // Update the poll set.
      eps.del(client.fd());
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
    if (cxt.output_port())
      egress_queue.enqueue(cxt);
  };

  // Select a port to handle input.
  auto input = [&](int fd)
  {
    if (fd == port1.fd())
      ingress(port1);
    else
      ingress(port2);
  };


  // Apply egress processing on a port.
  auto egress = [&](Port_eth_tcp& port)
  {
    while (egress_queue.size()) {
      Context cxt = egress_queue.dequeue();
      port.send(cxt);
    }
  };

  // Select a port to handle output.
  auto output = [&](int fd)
  {
    if (fd == port1.fd())
      egress(port1);
    else
      egress(port2);
  };

  // Report statistics.
  auto report = [&]()
  {
    auto p1_curr = port1.stats();
    auto p2_curr = port2.stats();
    // Clears the screen.
    system("clear");
    std::cout << "Receive Rate  (Pkt/s): " << (p2_curr.packets_rx -
      p2_stats.packets_rx) << '\n';
    std::cout << "Receive Rate   (Gb/s): " <<  ((p2_curr.bytes_rx -
      p2_stats.bytes_rx) * 8.0 / (1 << 30)) << '\n';
    std::cout << "Transmit Rate (Pkt/s): " << (p1_curr.packets_tx -
      p1_stats.packets_tx) << "\n";
    std::cout << "Transmit Rate  (Gb/s): " <<  ((p1_curr.bytes_tx -
      p1_stats.bytes_tx) * 8.0 / (1 << 30)) << "\n\n";
    p1_stats = p1_curr;
    p2_stats = p2_curr;
  };

  // Main lookup.
  running = true;

  // Init timer for reporting.
  Time last = now();
  Time curr;
  while (running) {
    // Wait for 100 milliseconds. Note that this can fail with
    // EINTR, which really isn't an error.
    //
    // NOTE: It seems the common practice is to re-poll when EINTR
    // occurs since like you said, it's not really an error. Most
    // impls just stick it in a do-while(errno != EINTR);
    epoll(eps, 100);

    if (eps.can_read(server.fd()))
      accept(server);
    if (eps.can_read(port2.fd()))
      input(port2.fd());
    if (eps.can_write(port1.fd()))
      output(port1.fd());

    curr = now();
    Fp_seconds dur = curr - last;
    double duration = dur.count();
    if (duration >= 1.0) {
      report();
      last = curr;
    }
  }

  eps.clear();

  // Take the dataplane down.
  dp.down();
  dp.unload_application();

  return 0;
}
