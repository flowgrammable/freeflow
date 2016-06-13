
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

  // Set up the event structure. Note that this is captured and modified
  // by the accept and ingress handlers.
  Select_set ss;
  ss.add_read(server.fd());
  

  // FIXME: Factor the accept/ingress code into something a little more 
  // reusable.

  // Accept connections from the server socket. Returns true if we
  // accepted a connection.
  auto accept = [&](Ipv4_stream_socket& server) -> bool
  {
    // Accept the connection.
    Ipv4_socket_address addr;
    Ipv4_stream_socket client = server.accept(addr);
    if (!client)
      return false; // TODO: Log the error.

    // If we already have two endpoints, just return, which will cause 
    // the socket to be closed.
    if (port1.fd() > 0)
      return false;

    // Update the poll set.
    ss.add_read(client.fd());
    
    // Bind the socket to a port.
    // TODO: Emit a port status change to the application. Does
    // that happen implicitly, or do we have to cause the dataplane
    // to do it.
    port1.attach(std::move(client));

    // Notify the application of the port change.
    Application* app = dp.get_application();
    app->port_changed(port1);

    return true;
  };

  // Handle input from the client socket. Returns true if ingress and 
  // processing succeeded and false if ingress failed (indicating a 
  // port error).
  auto ingress = [&](Port_tcp& port) -> bool
  {
    // Ingress the packet.
    Byte buf[4096];
    Context cxt(&dp, buf);
    bool ok = port.recv(cxt);

    // Handle error or closure.
    if (!ok) {
      // Detach the socket.
      Ipv4_stream_socket client = port.detach();

      // Notify the application of the port change.
      // Application* app = dp.get_application();
      // app->port_changed(port);

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


  // Current number of connected ports.
  int nports = 0;

  // Timing information.
  Time start;               // Records when both ends have connected
  Time stop;                // Records when both ends have disconnected

  // Main loop.
  running = true;
  while (running) {

    // #if 0
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(server.fd(), &fds);
    if (nports > 0)
      FD_SET(port1.fd(), &fds);
    int maxfd = std::max(server.fd(), port1.fd()) + 1;

    int n = ::select(maxfd, &fds, nullptr, nullptr, nullptr);
    if (n < 0) {
      std::cerr << "select error: " << std::strerror(errno) << '\n';
      return -1;
    }

    if (FD_ISSET(server.fd(), &fds))
      if (accept(server)) {
        ++nports;
        start = now();
      }
    
    if (nports > 0 && FD_ISSET(port1.fd(), &fds)) {
      std::uint32_t hdr;
      int k1 = ::recv(port1.fd(), (Byte*)&hdr, 4, MSG_PEEK);  
      if (k1 <= 0 || k1 != 4) {
        // Detach the socket so that it closes.
        Ipv4_stream_socket sock = port1.detach();

        // Update ports and print stats.
        --nports;
        stop = now();
        print_stats(port1, stop - start);

        if (once)
          return 0;
        else
          continue;
      }
      hdr = ntohl(hdr);

      // Read the rest of the message.
      char buf[4096];
      int k2 = recv(port1.fd(), buf, hdr + 4);
      if (k2 <= 0) {
        std::cerr << "receive error: " << std::strerror(errno) << '\n';
        throw std::runtime_error("fuck");
      } 
      else {
        port1.stats_.packets_rx++;
        port1.stats_.bytes_rx += k2;
      }
    }
    // #endif

    #if 0
    // Wait until we've received an event.
    //
    // TODO: Handle eintr gracefully?
    int n = select(ss);
    if (n <= 0)
      continue;

    // Is a connection available?
    if (ss.can_read(server.fd()))
      if (accept(server)) {
        ++nports;
        start = now();
      }

    // Process input.
    if (port1.fd() > 0 && ss.can_read(port1.fd()))
      if (!ingress(port1)) {
        --nports;
        stop = now();
        print_stats(port1, stop - start);

        // If running once, stop now.
        if (once)
          return 0;
      }
    #endif
  }


  // Take the dataplane down.
  dp.down();
  dp.unload_application();
  
  // Close the server socket.
  server.close();
  
  return 0;
}
