#include "system.hpp"
#include "dataplane.hpp"
#include "port.hpp"

#include <string>
#include <signal.h>

// Emulate a 2 port wire running over UDP ports.
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

  // Load the application library
  sys.load_application("apps/wire.app");

  // Create the dataplane
  Dataplane& dp = sys.make_dataplane("dp1", "wire");

  // Create application, associating it with the dataplane
  dp.add_application("apps/wire.app");

  // Add all ports
  dp.add_port(p1);
  dp.add_port(p2);

  dp.configure();

  // Start the wire.
  dp.up();

  // Loop forever.
  // TODO: Implement graceful termination.
  while (running) { };

  dp.down();

  return 0;
}
