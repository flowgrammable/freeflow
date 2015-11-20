#include "system.hpp"
#include "dataplane.hpp"
#include "port.hpp"

#include <signal.h>
#include <string>

// Emulate a 4 port hub running over UDP ports.
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

  // Instantiate ports.
  // TODO: Supply a bind address?
  fp::Port* p1 = fp::create_port(fp::Port::Type::udp, ":5000");
  fp::Port* p2 = fp::create_port(fp::Port::Type::udp, ":5001");
  fp::Port* p3 = fp::create_port(fp::Port::Type::udp, ":5002");
  fp::Port* p4 = fp::create_port(fp::Port::Type::udp, ":5003");

  // Load the application library
  fp::load_application("apps/hub.app");

  // Create the dataplane with the application
  fp::Dataplane* dp = fp::create_dataplane("dp1", "apps/hub.app");

  // Add all ports
  dp->add_port(p1);
  dp->add_port(p2);
  dp->add_port(p3);
  dp->add_port(p4);

  
  // After adding the ports and applications, set up the tables
  // for the dataplane.
  dp->configure();

  // Start the hub.
  dp->up();

  // Loop forever.
  // TODO: Implement graceful termination.
  while (running) { };

  dp->down();

  return 0;
}
