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
  Application_library* app_lib = sys.load_application("apps/wire.app");

  // Create the dataplane with the loaded application library.
  Dataplane* dp = sys.make_dataplane("dp1", app_lib);

  // Configure the data plane based on the applications needs.
  dp->configure();

  // Add all ports
  dp->add_port(p1);
  dp->add_port(p2);

  // Start the wire.
  dp->up();

  // Loop forever.
  // TODO: Implement graceful termination.
  while (running) { };

  dp->down();

  return 0;
}
