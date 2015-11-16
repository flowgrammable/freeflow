#include "system.hpp"
#include "dataplane.hpp"
#include "port.hpp"

#include <string>
#include <iostream>
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
  Port* p1 = sys.create_port(Port::Type::udp, ":5000");
  std::cerr << "Created port 'p1' with id '" << p1->id() << "'\n";
  Port* p2 = sys.create_port(Port::Type::udp, ":5001");
  std::cerr << "Created port 'p2' with id '" << p2->id() << "'\n";

  // Load the application library
  sys.load_application("apps/wire.app");
  std::cerr << "Loaded application 'apps/wire.app'\n";

  // Create the dataplane with the loaded application library.
  Dataplane* dp = sys.create_dataplane("dp1", "apps/wire.app");
  std::cerr << "Created data plane '" << dp->name() << "'\n";

  // Configure the data plane based on the applications needs.
  dp->configure();
  std::cerr << "Data plane configured\n";

  // Add all ports
  dp->add_port(p1);
  std::cerr << "Added port 'p1' to data plane 'dp1'\n";
  dp->add_port(p2);
  std::cerr << "Added port 'p2' to data plane 'dp1'\n";

  // Start the wire.
  dp->up();
  std::cerr << "Data plane 'dp1' up\n";

  // Loop forever.
  // TODO: Implement graceful termination.
  while (running) { };

  // Stop the wire.
  dp->down();
  std::cerr << "Data plane 'dp1' down\n";
  
  // TODO: Report some statistics?
  // dp->app()->statistics(); ?

  // Cleanup
  sys.delete_port(p1->id());
  std::cerr << "Deleted port 'p1' with id '" << p1->id() << "'\n";
  sys.delete_port(p2->id());
  std::cerr << "Deleted port 'p2' with id '" << p1->id() << "'\n";
  sys.delete_dataplane("dp1");

  sys.unload_application("apps/wire.app");
  std::cerr << "Unloaded application 'apps/wire.app'\n";
  
  return 0;
}
