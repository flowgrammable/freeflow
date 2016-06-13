
#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"
#include "context.hpp"
#include "application.hpp"

#include <freeflow/socket.hpp>
#include <freeflow/select.hpp>
#include <freeflow/time.hpp>

#include <string>
#include <iostream>

#include <signal.h>


using namespace ff;
using namespace fp;


// Emulate a 2 port wire running over TCP ports.

// Tracks whether the wire is running.
static bool running;

// If true, the dataplane will terminate after forwarding packets from
// a source to a destination.
static bool once = false;


void
on_signal(int sig)
{
  running = false;
}


// Print cumulative stats for the the given port.
void 
print_stats(Port& p, Fp_seconds sec)
{
  uint64_t npackets = p.stats().packets_rx;
  uint64_t nbytes = p.stats().bytes_rx;
  double Mb = double(nbytes * 8) / (1 << 20);
  double s = sec.count();
  double Mbps = Mb / s;
  long Pps = npackets / s;

  std::cout.precision(6);
  std::cout << "processed " << npackets << " packets in " 
            << s << " seconds (" << Pps << " Pps)\n";
  std::cout << "processed " << nbytes << " bytes in " 
            << s << " seconds (" << Mbps << " Mbps)\n";
}


int
main(int argc, char* argv[])
{
  // Parse command line arguments.
  if (argc >= 2) {
    if (std::string(argv[1]) == "once")
      once = true;
  }

  // Hardcode the app for testing.
  std::string path = "wire.app";

  // Install signal handlers for common signals. Note that this will
  // ensure that system calls fail with EINTR when one of these
  // signals is received.
  struct sigaction act;
  act.sa_handler = on_signal;
  act.sa_flags = 0;
  sigaction(SIGINT, &act, nullptr);
  sigaction(SIGTERM, &act, nullptr);
  sigaction(SIGHUP, &act, nullptr);

  // Build a server socket that will accept network connections.
  Ipv4_socket_address addr(Ipv4_address::any(), 5000);
  Ipv4_stream_socket server(addr);
  set_option(server.fd(), reuse_address(true));

  // Pre-create all standard ports.
  Port_eth_tcp port1(80);
  Port_eth_tcp port2(443);

  // Configure the dataplane. Ports must be added before
  // applications are loaded.
  fp::Dataplane dp = "dp1";
  dp.add_port(&port1);
  dp.add_port(&port2);
  dp.add_virtual_ports();
  dp.load_application(path.c_str());
  dp.up();

  // Set up the initial polling state.
  Select_set ss;
  ss.add_read(server.fd());
  

  // FIXME: Factor the accept/ingress code into something a little more 
  // reusable.


  // Accept connections from the server socket. Returns true if a
  // connection was accepted and false otherwise.
  auto accept = [&](Ipv4_stream_socket& server) -> bool
  {
    // Accept the connection.
    Ipv4_socket_address addr;
    Ipv4_stream_socket client = server.accept(addr);
    if (!client) {
      std::cerr << "accept error: " << std::strerror(errno) << '\n';
      return false;
    }

    // Figure out which port we're attaching the connection to (if any),
    // and save the total number of conneted ports for later.
    Port_tcp* port;
    if (port1.fd() == -1)
      port = &port1;
    else if (port2.fd() == -1)
      port = &port2;
    else {
      std::cerr << "accept error: device is fully connected\n";
      return false;
    }

    // Update the poll set.
    ss.add_read(client.fd());

    // And atach the socket to the port.
    port->attach(std::move(client));

    // Notify the application of the port change.
    Application* app = dp.get_application();
    app->port_changed(*port);

    return true;
  };


  // Handle input from the client socket. Returns false if the port
  // was disconnected and true if the packet was processed.
  auto ingress = [&](Port_tcp& port) -> bool
  {
    // Ingress the packet.
    Byte buf[2048];
    Context cxt(&dp, buf);
    bool ok = port.recv(cxt);

    // Handle error or closure.
    if (!ok) {
      // Detach the socket.
      Ipv4_stream_socket client = port.detach();

      // Notify the application of the port change.
      Application* app = dp.get_application();
      app->port_changed(port);

      // Update the poll set.
      ss.del_read(client.fd());
      return false;
    }

    // Otherwise, process the application.
    //
    // TODO: This really just runs one step of the pipeline. This needs
    // to be a loop that continues processing until there are no further
    // table redirections.
    Application* app = dp.get_application();
    app->process(cxt);

    // Apply actions after pipeline processing.
    cxt.apply_actions();

    // Assuming there's an output send to it.
    if (Port* out = cxt.output_port())
      out->send(cxt);
    return true;
  };


  
  int nports = 0; // Current number of devices.
  Time start;     // Records when both ends have connected
  Time stop;      // Records when both ends have disconnected

  // Main loop.
  running = true;
  while (running) {
    // Wait until we've received an event.
    int n = select(ss);
    if (n <= 0) {
      continue;
    }

    // Is a connection available?
    if (ss.can_read(server.fd())) {
      if (accept(server)) {
        ++nports;
        if (nports == 2)
          start = now();
      }
      else {
        // FIXME: What do we really want to do if the server drops?
        // Probably continue.
        break;
      }
    }

    if (nports > 0) {
      // Process events on port1.
      if (port1.fd() > 0 && ss.can_read(port1.fd())) {
        if (!ingress(port1)) {
          --nports;
        }
      }

      // Process events on port2.
      if (port2.fd() > 0 && ss.can_read(port2.fd())) {
        if (!ingress(port2)) {
          --nports;
        }
      }
  
      // If, after processing events, we have no ports, dump the stats
      // and start listening for connections again.
      //
      // NOTE: If the 2nd endpoint does not disconnect, then we probably
      // never see stats. It might be more appropriate to report the
      // precisely when each port is disconnected, although that might
      // make reporting a little difficult.
      if (nports == 0) {
        stop = now();
        Fp_seconds secs = stop - start;
        // print_stats(port1, secs); // The sender isn't interesting.
        print_stats(port2, secs);

        // If running once, stop now.
        if (once)
          return 0;
      }
    }
  }


  // Take the dataplane down.
  dp.down();
  dp.unload_application();
  
  // Close the server socket.
  server.close();
  
  return 0;
}
