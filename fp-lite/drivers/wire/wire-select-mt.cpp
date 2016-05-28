
#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"
#include "context.hpp"
#include "application.hpp"
#include "thread.hpp"
#include "queue.hpp"
#include "buffer.hpp"

#include <freeflow/socket.hpp>
#include <freeflow/select.hpp>
#include <freeflow/time.hpp>

#include <string>
#include <array>
#include <iostream>
#include <signal.h>
#include <unistd.h>

#include <boost/lockfree/queue.hpp>

using namespace ff;
using namespace fp;


// Emulate a 2 port wire running over TCP ports.

// Global Members.
//

// NOTE: Clang will optimize assume that the running loop never terminates 
// if this is not declared volatile. Go figure.
static bool running;

// If true, the dataplane will terminate after forwarding packets from
// a source to a destination.
static bool once = false;

// Emulate a 2 port wire running over TCP ports with a thread per port.


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

// Local send/recv buffer size.
constexpr int local_buf_size = 2048;

// Port send queues.
boost::lockfree::queue<std::array<int, local_buf_size>, boost::lockfree::capacity<2048>> send_queue[2];


// The packet buffer pool.
static Pool& buffer_pool = Buffer_pool::get_pool(&dp);

// Set up the initial polling state.
Select_set ss;

// Timing information.
Time start;               // Records when both ends have connected
Time stop;                // Records when both ends have disconnected
Fp_seconds duration(0.0); // Cumulative time spent forwarding

// System stats.
uint64_t npackets = 0;  // Total number of packets processed
uint64_t nbytes = 0;    // Total number of bytes processed

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
port_work(void* arg)
{
  // Thread ID.
  int id = *((int*)arg);
  // Port FD.
  int fd = ports[id].fd();
  // Local recv/send buffers.
  std::array<int, local_buf_size> recv_buf;
  std::array<int, local_buf_size> send_buf;
  // Iterator to fill local recv buffer.
  auto recv_iter = recv_buf.begin();
  // TODO: Figure out a better conditional.
  while (ports[id].is_up()) {
    // Check if the fd is able to read/recv.
    if (ss.can_read(fd)) {
      
      // Get the next free buffer from the pool.
      Buffer& buf = buffer_pool.alloc();

      // Ingress the packet.
      if (ports[id].recv(buf.context())) {
        ++npackets;
        nbytes += buf.context().packet().size();
        // TODO: This really just runs one step of the pipeline. This needs
        // to be a loop that continues processing until there are no further
        // table redirections.
        Application* app = dp.get_application();

        // NOTE: Have process return the number of redirections to indicate
        // if further processing should happen? Something like...
        //
        // while (app->process(buf.cxt_)) { }
        app->process(buf.context());

        // Apply actions.
        buf.context().apply_actions();

        // Assuming there's an output send to it.
        if (buf.context().output_port()) {
          // If the buffer is full, push it into the send queue.
          if (++recv_iter == recv_buf.end()) {
            send_queue[buf.context().output_port()->id() - 1].push(recv_buf);
            recv_iter = recv_buf.begin() + 1;
          }
          // Add the packet buffer index to the local buffer.
          *recv_iter = buf.id();
        }
      }
      else {
        buffer_pool.dealloc(buf.id());
        ports[id].down();
      }
    } // end if-can-read
  
    // Check if the fd is able to write/send.
    if (ss.can_write(fd)) {
      // Drain the send queue.
      if (send_queue[id].pop(send_buf)) {
        for (int const& idx : send_buf) {
          if (ports[id].send(buffer_pool[idx].context()))
            buffer_pool.dealloc(idx);
        }
      }
      else if (nports == 1 && send_queue[id].empty())
        ports[id].down();
    } // end if-can-write
  } // end while-running

  // Cleanup.
  //
  // Detach the socket.
  Ipv4_stream_socket client = ports[id].detach();

  // Notify the application of the port change.
  Application* app = dp.get_application();
  app->port_changed(ports[id]);
  
  if (--nports == 0 && once) {
    stop = now();
    duration += stop - start;
    running = false;
  }
  
  return 0;
}


int
main(int argc, char* argv[])
{
  // Parse command line arguments.
  if (argc == 2) {
    if (std::string(argv[1]) == "once")
      once = true;
  }

  // TODO: Use sigaction.
  signal(SIGINT, on_signal);
  signal(SIGKILL, on_signal);
  signal(SIGHUP, on_signal);

  // Build a server socket that will accept network connections.
  Ipv4_socket_address addr(Ipv4_address::any(), 5000);
  Ipv4_stream_socket server(addr);

  set_option(server.fd(), reuse_address(true));

  // TODO: Handle exceptions.

  // Configure the dataplane. Ports must be added before
  // applications are loaded.
  for (int i = 0; i < 2; i++) {
    dp.add_port(&ports[i]);
    port_thread[i].assign(i, port_work);
  }

  dp.add_virtual_ports();
  dp.load_application("apps/wire.app");
  dp.up();

  ss.add_read(server.fd());


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

    // Bind the socket to a port.
    // TODO: Emit a port status change to the application. Does
    // that happen implicitly, or do we have to cause the dataplane
    // to do it.
    Port_tcp* port = nullptr;
    if (nports == 0)
      port = &ports[0];
    if (nports == 1)
      port = &ports[1];
    port->attach(std::move(client));
    port_thread[nports].run();
    ++nports;

    // Once we have two connections, start the timer.
    if (nports == 2)
      start = now();

    // Notify the application of the port change.
    Application* app = dp.get_application();
    app->port_changed(*port);
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
  }

  // Take the dataplane down.
  port_thread[0].halt();
  port_thread[1].halt();
  dp.down();
  dp.unload_application();

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
