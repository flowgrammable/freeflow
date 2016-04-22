// Only build this example on a linux machine, as epoll
// is not portable to Mac.

#define BOOST 1
#define FP 0

#define ARRAY 0
#define VECTOR 0

#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"
#include "context.hpp"
#include "application.hpp"
#include "thread.hpp"
#include "queue.hpp"
#include "buffer.hpp"

#include <freeflow/socket.hpp>
#include <freeflow/epoll.hpp>
#include <freeflow/time.hpp>

#include <string>
#include <queue>
#include <array>
#include <vector>
#include <iostream>
#include <signal.h>
#include <unistd.h>

#include <boost/lockfree/queue.hpp>


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

// Port send queues.
#if BOOST
  //boost::lockfree::queue<int, boost::lockfree::fixed_sized<false>> send_queue[2];
  boost::lockfree::queue<std::array<int, 2048>, boost::lockfree::capacity<65534>> send_queue[2];
#else
  #if ARRAY
    Locked_queue<std::array<int, 1024>> send_queue[2];
  #else
    Locked_queue<std::vector<int>> send_queue[2];
  #endif
#endif

// The packet buffer pool.
static Pool& buffer_pool = Buffer_pool::get_pool();

// Set up the initial polling state.
Epoll_set eps(3);

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
port_work(void* arg)
{
  int id = *((int*)arg);
  int fd = ports[id].fd();
  #if FP
    #if ARRAY
      std::array<int, 1024> recv_bin;
      std::array<int, 1024> send_bin;
    #else
      std::vector<int> recv_bin(1024);
      std::vector<int> send_bin(1024);
    #endif
  #else
    std::array<int, 2048> recv_bin;
    std::array<int, 2048> send_bin;
  #endif
  int num_recv = 0;


  // TODO: Figure out a better conditional.
  while (running) {
    // Check if the fd is able to read/recv.
    if (eps.can_read(fd)) {
      
      // Get the next free buffer from the pool.
      Buffer& buf = buffer_pool.alloc();

      // Ingress the packet.
      if (ports[id].recv(buf.context())) {
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
          #if BOOST
            //while (!send_queue[buf.context().output_port()->id() - 1].bounded_push(buf.id())){ }
            //send_queue[buf.context().output_port()->id() - 1].push(buf.id());
            recv_bin[num_recv++] = buf.id();
            if (num_recv == 2048) {
              send_queue[buf.context().output_port()->id() - 1].push(recv_bin);
              num_recv = 0;
            }
          #else
            // Cache in local container.
            #if ARRAY
              recv_bin[num_recv] = buf.id();
            #else
              recv_bin.push_back(buf.id());
            #endif
            // If local container is 'full', push it to the global queue for sending.
            if (++num_recv == 1024) {
              send_queue[buf.context().output_port()->id() - 1].enqueue(recv_bin);
              
              #if VECTOR
                recv_bin.clear();
              #endif

              num_recv = 0;
            }
          #endif
        }
      }
      else
        buffer_pool.dealloc(buf.id());

    } // end if-can-read
  
    // Check if the fd is able to write/send.
    if (eps.can_write(fd)) {
      //#if FP
        while (send_queue[id].pop(send_bin)) {
          for (int& idx : send_bin) {
            ports[id].send(buffer_pool[idx].context());
            buffer_pool.dealloc(idx);
          }
        }
      #if 0
        int idx = -1;
        while (send_queue[id].pop(idx)) {
          ports[id].send(buffer_pool[idx].context());
          buffer_pool.dealloc(idx);
        }
      #endif
    } // end if-can-write

  } // end while-running
  
  // Detach the socket.
  Ipv4_stream_socket client = ports[id].detach();

  // Notify the application of the port change.
  Application* app = dp.get_application();
  app->port_changed(ports[id]);
  --nports;
  std::string stats = "port[" + std::to_string(id) + "] recv: " + 
    std::to_string(ports[id].stats().packets_rx) + " sent: " + 
    std::to_string(ports[id].stats().packets_tx) + "\n";
  std::cout << stats;
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
    //set_option(client.fd(), nodelay(true));
    eps.add(client.fd());
    // Bind the socket to a port.
    // TODO: Emit a port status change to the application. Does
    // that happen implicitly, or do we have to cause the dataplane
    // to do it.
    Port_tcp* port = nullptr;
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
  Port::Statistics p1_stats = {0,0,0,0};
  Port::Statistics p2_stats = {0,0,0,0};

  // Configure the dataplane. Ports must be added before
  // applications are loaded.

  for (int i = 0; i < 2; i++) {
    dp.add_port(&ports[i]);
    port_thread[i].assign(i, port_work);
  }

  dp.add_drop_port();

  dp.load_application("apps/wire.app");
  dp.up();

  // Add the server socket to the select set.
  eps.add(server.fd());

  // Report statistics.
  auto report = [&]()
  {
    auto p1_curr = ports[0].stats();
    auto p2_curr = ports[1].stats();

    uint64_t pkt_tx = p1_curr.packets_tx - p1_stats.packets_tx;
    uint64_t pkt_rx = p2_curr.packets_rx - p2_stats.packets_rx;
    long double bit_tx = (p1_curr.bytes_tx - p1_stats.bytes_tx) * 8.0 / (1 << 20);
    long double bit_rx = (p2_curr.bytes_rx - p2_stats.bytes_rx) * 8.0 / (1 << 20);
    // Clears the screen.
    (void)system("clear");
    std::cout << "Receive Rate  (Pkt/s): " << pkt_rx << '\n';
    std::cout << "Receive Rate   (Mb/s): " << bit_rx << '\n';
    std::cout << "Transmit Rate (Pkt/s): " << pkt_tx << '\n';
    std::cout << "Transmit Rate  (Mb/s): " << bit_tx << "\n\n";
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

    curr = now();
    Fp_seconds dur = curr - last;
    double duration = dur.count();
    if (duration >= 1.0) {
      report();
      last = curr;
    }
  }

  port_thread[0].halt();
  port_thread[1].halt();
  eps.clear();
  // Take the dataplane down.
  dp.down();
  dp.unload_application();

  return 0;
}
