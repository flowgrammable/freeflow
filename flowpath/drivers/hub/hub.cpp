#include "system.hpp"
#include "application.hpp"
#include "dataplane.hpp"
#include "port.hpp"

#include <signal.h>
#include <string>

// Emulate a 4 port hub running over UDP ports.
using namespace fp;

static bool running;

void
sig_handle(int sig)
{
  running = false;
}

int 
main()
{
  signal(SIGINT, sig_handle);
  running = true;
  System sys;

  // Instantiate ports.
  // TODO: Supply a bind address?
  Port* p1 = sys.make_port(Port::Type::udp, ":5000");
  Port* p2 = sys.make_port(Port::Type::udp, ":5001");
  Port* p3 = sys.make_port(Port::Type::udp, ":5002");
  Port* p4 = sys.make_port(Port::Type::udp, ":5003");

  // Load the application library
  sys.load_application("apps/hub.app");

  // Create the dataplane
  Dataplane& dp = sys.make_dataplane("dp1", "hub");

  // Create application, associating it with the dataplane
  dp.add_application("apps/hub.app");

  // Add all ports
  dp.add_port(p1);
  dp.add_port(p2);
  dp.add_port(p3);
  dp.add_port(p4);

  
  // After adding the ports and applications, set up the tables
  // for the dataplane.
  dp.configure();

  // Start the hub.
  dp.up();

  // Loop forever.
  // TODO: Implement graceful termination.
  while (running) { };

  dp.down();

  return 0;
}
