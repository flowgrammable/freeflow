
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
//#include <queue>

using namespace ff;
using namespace fp;


// Emulate a 2 port wire running over TCP ports.

// NOTE: Clang will optimize assume that the running loop never terminates 
// if this is not declared volatile. Go figure.
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
  path += "wire.app";

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
  // Current number of devices.
  int nports = 0;
  // Output queues.
  //std::queue<Context> send_queue[2];
  // Timing information.
  Time start;               // Records when both ends have connected
  Time stop;                // Records when both ends have disconnected
  Fp_seconds duration(0.0); // Cumulative time spent forwarding

  // System stats.
  uint64_t npackets = 0;  // Total number of packets processed
  uint64_t nbytes = 0;    // Total number of bytes processed


  // FIXME: Factor the accept/ingress code into something a little more 
  // reusable.

  // Accept connections from the server socket.
  auto accept = [&](Ipv4_stream_socket& server)
  {
    std::cout << "[flowpath] accept\n";
    
    // Accept the connection.
    Ipv4_socket_address addr;
    Ipv4_stream_socket client = server.accept(addr);
    if (!client)
      return; // TODO: Log the error.

    // If we already have two endpoints, just return, which will cause 
    // the socket to be closed.
    if (nports == 2) {
      std::cout << "[flowpath] reject connection " << addr.port() << '\n';
      return;
    }
    std::cout << "[flowpath] accept connection " << addr.port() << '\n';

    // Update the poll set.
    ss.add_read(client.fd());
    //set_option(client.fd(), nonblocking(true));
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

    // Once we have two connections, start the timer.
    if (nports == 2)
      start = now();

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
    Context cxt(buf, &dp);
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

        // If running in one-shot mode, stop now.
        if (once)
          running = false;
      }
      return;
    }
    else {
      ++npackets;
      nbytes += cxt.packet().size();
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
      //send_queue[out->id()].push(cxt);
      out->send(cxt);
  };

  /*
  auto egress = [&](Port_tcp& port)
  {
    if (!send_queue[port.id()].empty()) {
      Context cxt = send_queue[port.id()].front();
      port.send(cxt);
      send_queue[port.id()].pop();
    }      
  };
  */
  // Select a port to handle input.
  auto input = [&](int fd)
  {
    if (fd == port1.fd())
      ingress(port1);
    else
      ingress(port2);
  };

  /*
  // Select a port to handle output.
  auto output = [&](int fd)
  {
    if (fd == port1.fd())
      egress(port1);
    else
      egress(port2);
  };
  */
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
      input(port1.fd());
    if (port2.fd() > 0 && ss.can_read(port2.fd()))
      input(port2.fd());
    // Process output.
    /*
    if (port1.fd() > 0 && ss.can_write(port1.fd()))
      output(port1.fd());
    if (port2.fd() > 0 && ss.can_write(port2.fd()))
      output(port2.fd());
    */
  }

  // Take the dataplane down.
  dp.down();
  dp.unload_application();
  
  // Close the server socket.
  server.close();
  
  // Write out stats.
  double s = duration.count();
  double Mb = double(nbytes * 8) / (1 << 20);
  double Mbps = Mb / s;
  long Pps = npackets / s;

  // FIXME: Make this pretty.
  // std::cout.imbue(std::locale(""));
  std::cout.precision(6);
  std::cout << "processed " << npackets << " packets in " 
            << s << " seconds (" << Pps << " Pps)\n";
  std::cout << "processed " << nbytes << " bytes in " 
            << s << " seconds (" << Mbps << " Mbps)\n";

  return 0;
}
