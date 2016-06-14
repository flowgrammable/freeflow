
#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"
#include "context.hpp"
#include "application.hpp"

#include <freeflow/socket.hpp>
#include <freeflow/select.hpp>
#include <freeflow/time.hpp>

#include <future>
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


  // The port thread. This simply ingresses the port until it is
  // disconnected.
  //
  // NOTE: GCC is apparently adjusting away the reference type on a port
  // argument, so wasn't being called in a natural way -- although that might
  // be a warning that passing references into a thread is a bad idea. Maybe.
  auto proc1 = [ingress, &port1]() -> int {
    while (ingress(port1)) {
      assert(port1.fd() != -1);
    }
    return 1;
  };

  auto proc2 = [ingress, &port2]() -> int {
    while (ingress(port2)) {
      assert(port2.fd() != -1);
    }
    return 2;
  };

  
  int nports = 0; // Current number of devices.
  Time start;     // Records when both ends have connected
  Time stop;      // Records when both ends have disconnected

  // Main loop.
  running = true;
  while (running) {
    // Accept a receiver and a sender. Just quit if we got an error.
    while (nports < 2) {
      if (accept(server)) {
        ++nports;
      }
      else
        break;
    }

    // Spawn one thread per port.
    auto f1 = std::async(std::launch::async, proc1);
    auto f2 = std::async(std::launch::async, proc2);
    bool done1 = false;
    bool done2 = false;

    // Now that we've connected, start timing.
    start = now();

    f2.get();
    f1.get();

    // // TODO: C++11 does not have the ability to wait on multiple futures.
    // // Howver, C++14 has when_all(...), which should take the place of this
    // // crappy loop.
    // while (nports != 0) {
    //   std::cout << "waiting... " << nports << '\n';
    //   if (!done1 && f1.wait_for(10ms) == std::future_status::ready) {
    //     std::cout << "done 1\n";
    //     --nports;
    //     done1 = true;
    //   }
    //   if (!done2 && f2.wait_for(10ms) == std::future_status::ready) {
    //     std::cout << "done 2\n";
    //     --nports;
    //     done2 = true;
    //   }
    // }

    // Time the experiment. Note that port1 statsare uniniteresting.  
    stop = now();
    print_stats(port2, stop - start);

    // Uh... this is broke as fuck. Just stop.
    break;
  }


  // Take the dataplane down.
  dp.down();
  dp.unload_application();
  
  // Close the server socket.
  server.close();
  
  return 0;
}
