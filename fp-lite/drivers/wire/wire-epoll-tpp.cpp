// Only build this example on a linux machine, as epoll
// is not portable to Mac.


#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"
#include "context.hpp"
#include "application.hpp"
#include "thread.hpp"
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


// Emulate a 2 port wire running over TCP ports with a thread per port.

// Global Members.
//

// Running flag.
// NOTE: Clang will optimize assume that the running loop
// never terminates if this is not declared volatile.
// Go figure.
static bool volatile running;

// A server socket that will accept network connections.
Ipv4_socket_address addr(Ipv4_address::any(), 5000);
Ipv4_stream_socket server(addr);

// Pre-create all standard ports.
Port_eth_tcp ports[2] =
{
  {1},
  {2}
};

// Port threads.
Thread port_thread[2];

// Current number of ports.
int nports = 0;

// The data plane object.
Dataplane dp = "dp1";

// Egress processing queue.
Locked_queue<Context> egress_queue;


// Signal handling.
//
// TODO: Use sigaction
void
on_signal(int sig)
{
  running = false;
}


// Apply ingress and pipeline processing on a new packet (context).
// After processing, the context will be copied to the egress queue.
//
// FIXME: Currently we assume ingress processing happens on port2.
// Maybe wrap ingress and egress calls into a different function
// and use epoll to determine if you should send/recv?
void*
ingress(void* arg)
{
  int id = *((int*)arg);
  //Port_eth_tcp* self = &ports[id];

  // Ingress the packet.
  //
  // TODO: Figure out a better conditional.
  while (running) {
    Byte buf[4096];
    Context cxt(buf, &dp);
    ports[id].recv(cxt);
    // TODO: This really just runs one step of the pipeline. This needs
    // to be a loop that continues processing until there are no further
    // table redirections.
    Application* app = dp.get_application();
    app->process(cxt);

    // Assuming there's an output send to it.
    if (cxt.output_port())
      egress_queue.enqueue(cxt);
  }

  // Detach the socket.
  Ipv4_stream_socket client = ports[id].detach();

  // Notify the application of the port change.
  Application* app = dp.get_application();
  app->port_changed(ports[id]);

  --nports;
  return 0;
}


// Apply egress processing on a context. Fetches a context from
// the egress queue and sends it.
//
// FIXME: Currently we assume egress processing happens on port1.
// Maybe wrap ingress and egress calls into a different function
// and use epoll to determine if you should send/recv?
void*
egress(void* arg)
{
  // Figure out who I am.
  int id = *((int*)arg);

  // Egress a packet.
  //
  // TODO: Figure out a better conditional.
  while (running) {
    // Get the current number of items in the egress queue.
    int num = egress_queue.size();
    // Send num packets.
    while (num-- > 0) {
      Context cxt = egress_queue.dequeue();
      ports[id].send(cxt);
    }
  }

  // Detach the socket.
  Ipv4_stream_socket client = ports[id].detach();

  // Notify the application of the port change.
  Application* app = dp.get_application();
  app->port_changed(ports[id]);

  --nports;
  return 0;
}


// The main driver for the flowpath wire server.
int
main()
{
  // TODO: Use sigaction.
  signal(SIGINT, on_signal);
  signal(SIGKILL, on_signal);
  signal(SIGHUP, on_signal);


  set_option(server.fd(), reuse_address(true));
  set_option(server.fd(), nonblocking(true));

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
    set_option(client.fd(), nonblocking(true));
    // Bind the socket to a port.
    // TODO: Emit a port status change to the application. Does
    // that happen implicitly, or do we have to cause the dataplane
    // to do it.
    Port_tcp* port;
    if (nports == 0)
      port = &ports[0];
    else if (nports == 1)
      port = &ports[1];
    port->attach(std::move(client));
    port_thread[nports].run();
    ++nports;

    // Notify the application of the port change.
    Application* app = dp.get_application();
    app->port_changed(*port);
  };

  // Reporting stastics init.
  Port::Statistics p1_stats;
  Port::Statistics p2_stats;

  // Configure the dataplane. Ports must be added before
  // applications are loaded.

  dp.add_port(&ports[0]);
  dp.add_port(&ports[1]);
  port_thread[0].assign(0, egress);
  port_thread[1].assign(1, ingress);
  dp.add_drop_port();
  dp.load_application("apps/wire.app");
  dp.up();

  // Set up the initial polling state.
  Epoll_set eps(1);

  // Add the server socket to the select set.
  eps.add(server.fd());

  // Report statistics.
  auto report = [&]()
  {
    auto p1_curr = ports[0].stats();
    auto p2_curr = ports[1].stats();

    int pkt_tx = p1_curr.packets_tx - p1_stats.packets_tx;
    int pkt_rx = p2_curr.packets_rx - p2_stats.packets_rx;
    double bit_tx = (p1_curr.bytes_tx - p1_stats.bytes_tx) * 8.0 / (1 << 20);
    double bit_rx = (p2_curr.bytes_rx - p2_stats.bytes_rx) * 8.0 / (1 << 20);
    // Clears the screen.
    system("clear");
    std::cout << "Receive Rate  (Pkt/s): " << pkt_rx << '\n';
    std::cout << "Receive Rate   (Mb/s): " <<  bit_rx << '\n';
    std::cout << "Transmit Rate (Pkt/s): " << pkt_tx << '\n';
    std::cout << "Transmit Rate  (Mb/s): " <<  bit_tx << '\n';
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
    epoll(eps, 1000);

    if (eps.can_read(server.fd()))
      accept(server);

    curr = now();
    Fp_seconds dur = curr - last;
    double duration = dur.count();
    if (duration >= 1.0) {
      report();
      last = curr;
    }
  }

  eps.clear();
  port_thread[0].halt();
  port_thread[1].halt();
  // Take the dataplane down.
  dp.down();
  dp.unload_application();

  return 0;
}
