
#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"
#include "context.hpp"
#include "application.hpp"

#include <freeflow/time.hpp>
#include <freeflow/socket.hpp>
#include <freeflow/poll.hpp>

#include <string>
#include <iostream>
#include <signal.h>


using namespace ff;
using namespace fp;


// The slurp device simply receives packets from a single
// connected port. Note that this does not drive an application.
// This used only to measure performance of ingress ports.
//
// TODO: Allow this to have multiple ports?


// NOTE: Clang will optimize assume that the running loop
// never terminates if this is not declared volatile.
// Go figure.
static bool volatile running;


// When getting a particular kind of signal. Indicate that we should
// stop running.
void
on_signal(int sig)
{
  running = false;
}


// Records statistics about a live connection.
struct Connection
{
  Time start;
  Time stop;
  std::uint64_t packets;
  std::uint64_t bytes;
};


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
  Port_eth_tcp port(0);

  // Configure the dataplane. Ports must be added before
  // applications are loaded.
  fp::Dataplane dp = "dp1";
  dp.add_port(&port);
  dp.load_application("apps/nop.app");
  dp.up();

  // Set up the initial polling state.
  Poll_file fds[] {
    { server.fd(), POLLIN },
    { -1, 0 },
  };

  // FIXME: I don't need this. There's only 0 or 1 port.
  int nports = 0; // Current number of ports

  // Save stats for the connection.
  Connection con;

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
    if (nports == 1) {
      std::cout << "[slurp] reject connection " << addr.port() << '\n';
      return;
    }
    std::cout << "[slurp] accept connection " << addr.port() << '\n';

    // Update the poll set.
    fds[1] = { client.fd(), POLLIN };

    // Bind the socket to a port.
    // TODO: Emit a port status change to the application. Does
    // that happen implicitly, or do we have to cause the dataplane
    // to do it.
    port.attach(std::move(client));
    nports = 1;

    // Reset connetion stats.
    con = Connection();
    con.start = now();
  };

  // Handle input from the client socket.
  //
  // TODO: This defines the basic ingress pipeline. How
  // do we refactor this to make it reasonably composable.
  auto ingress = [&]()
  {
    // Ingress the packet.
    Byte buf[2048];
    Context cxt(buf, &dp);
    bool ok = port.recv(cxt);

    // Handle error or closure.
    if (!ok) {
      std::cout << "[slurp] close connection\n";

      // Finish and print stats. prior to detaching the socket.
      con.stop = now();
      Fp_seconds dur = con.stop - con.start;
      double s = dur.count();
      double Pps = con.packets / s;
      double Mb = double(con.bytes * 8) / (1 << 20);
      double Mbps = Mb / s;

      std::cout << "[slurp] received " << con.packets << " packets in "
                << s << " seconds (" << Pps << " Pps)\n";
      std::cout << "[slurp] received " << con.bytes << " bytes in "
                << s << " seconds (" << Mbps << " Mbps)\n";

      // Detache the socket.
      Ipv4_stream_socket client = port.detach();

      // Update the poll set.
      fds[1] = { -1, 0 };
      nports = 0;
      return;
    }

    // FIXME: Update local counts, etc, but otherwise, don't
    // actually do anything
    con.packets += 1;
    con.bytes += cxt.packet().size();
  };

  // Print fancy numbers.
  std::cout.imbue(std::locale(""));

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
      ingress();
  }

  // Take the dataplane down.
  dp.down();
  dp.unload_application();

  return 0;
}
