
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


// Implements a general purpose discard server on port 5000.
//
// TODO: This actually seems like a useful service. After all, there's an
// RFC for a discard protocol.


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


int
main(int argc, char* argv[])
{
  // Parse command line arguments.
  if (argc >= 2) {
    if (std::string(argv[1]) == "once")
      once = true;
  }

  // Construct a path to the wire application.
  std::string path = "apps/";
  if (argc == 3) {
    path = std::string(argv[2]);
  }
  path += "nop.app";

  // TODO: Use sigaction.
  signal(SIGINT, on_signal);
  signal(SIGKILL, on_signal);
  signal(SIGHUP, on_signal);

  // Build a server socket that will accept network connections.
  Ipv4_socket_address addr(Ipv4_address::any(), 5000);
  Ipv4_stream_socket server(addr);
  set_option(server.fd(), reuse_address(true));
  //set_option(server.fd(), nonblocking(true));
  // TODO: Handle exceptions.

  // Pre-create all standard ports.
  Port_eth_tcp port1(1);

  // Configure the dataplane. Ports must be added before
  // applications are loaded.
  fp::Dataplane dp = "dp1";
  dp.add_port(&port1);
  dp.add_virtual_ports();
  dp.load_application(path.c_str());
  dp.up();

  // Set up the initial polling state.
  Select_set ss;
  ss.add_read(server.fd());
  
  // Current number of devices.
  int nports = 0;
  
  // Timing information.
  Time start;               // Records when both ends have connected
  Time stop;                // Records when both ends have disconnected
  Fp_seconds duration(0.0); // Cumulative time spent forwarding


  // FIXME: Factor the accept/ingress code into something a little more 
  // reusable.

  // Accept connections from the server socket.
  auto accept = [&](Ipv4_stream_socket& server)
  {
    // Accept the connection.
    Ipv4_socket_address addr;
    Ipv4_stream_socket client = server.accept(addr);
    if (!client)
      return; // TODO: Log the error.

    // If we already have two endpoints, just return, which will cause 
    // the socket to be closed.
    if (nports == 1) {
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
    port1.attach(std::move(client));
    ++nports;

    // Once we have two connections, start the timer.
    if (nports == 1)
      start = now();

    // Notify the application of the port change.
    Application* app = dp.get_application();
    app->port_changed(port1);
  };

  // Handle input from the client socket.
  //
  // TODO: This defines the basic ingress pipeline. How do we refactor this 
  // to make it reasonably composable.
  auto ingress = [&](Port_tcp& port)
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
      --nports;

      // Once both ports have disconnected, accumulate statistics.
      if (nports == 0) {
        stop = now();
        duration += stop - start;

        // Dump session stats
        uint64_t npackets = port1.stats().packets_rx;
        uint64_t nbytes = port1.stats().bytes_rx;
        double Mb = double(nbytes * 8) / (1 << 20);
        double s = duration.count();
        double Mbps = Mb / s;
        long Pps = npackets / s;

        // FIXME: Make this prettier.
        std::cout.precision(6);
        std::cout << "processed " << npackets << " packets in " 
                  << s << " seconds (" << Pps << " Pps)\n";
        std::cout << "processed " << nbytes << " bytes in " 
                  << s << " seconds (" << Mbps << " Mbps)\n";

        // If running in one-shot mode, stop now.
        if (once)
          running = false;
      }
      return;
    }

    // Otherwise, process the application.
    //
    // TODO: This really just runs one step of the pipeline. This needs
    // to be a loop that continues processing until there are no further
    // table redirections.
    Application* app = dp.get_application();
    app->process(cxt);

    // Apply actions after pipeline processing.
    // cxt.apply_actions();

    // // Assuming there's an output send to it.
    // if (Port* out = cxt.output_port())
    //   out->send(cxt);
  };


  // Main loop.
  running = true;
  while (running) {
    // Wait for 100 milliseconds. Note that this can fail with
    // EINTR, which really isn't an error.
    //
    // NOTE: It seems the common practice is to re-poll when EINTR
    // occurs since like you said, it's not really an error. Most
    // impls just stick it in a do-while(errno != EINTR);
    int n = select(ss, 10ms);
    if (n <= 0)
      continue;

    // Is a connection available?
    if (ss.can_read(server.fd()))
      accept(server);
    
    // Process input.
    if (port1.fd() > 0 && ss.can_read(port1.fd()))
      ingress(port1);
  }


  // Take the dataplane down.
  dp.down();
  dp.unload_application();
  
  // Close the server socket.
  server.close();
  
  return 0;
}
