#include "system.hpp"
#include "dataplane.hpp"
#include "port.hpp"
#include "port_udp.hpp"
#include "thread.hpp"

#include <string>
#include <iostream>
#include <signal.h>

// Emulate a 2 port wire running over UDP ports.

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
  //
  // P1 : Connected to an echo client.
  fp::Port* p1 = fp::create_port(fp::Port::Type::udp, ":5000");
  std::cerr << "Created port 'p1' with id '" << p1->id() << "'\n";
  // Create a thread to service the port.
  fp::Thread* t1 = new fp::Thread(p1->id(), p1->work_fn());

  // P2 : Bound to a netcat UDP port; Acts as the entry point.
  fp::Port* p2 = fp::create_port(fp::Port::Type::udp, ":5001");
  std::cerr << "Created port 'p2' with id '" << p2->id() << "'\n";
  // Create a thread to service the port.
  fp::Thread* t2 = new fp::Thread(p2->id(), p2->work_fn());

  // Load the application library
  fp::load_application("apps/wire.app");
  std::cerr << "Loaded application 'apps/wire.app'\n";

  // Create the dataplane with the loaded application library.
  fp::Dataplane* dp = fp::create_dataplane("dp1", "apps/wire.app");
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

  // Start ports.
  t1->run();
  t2->run();

  // Loop forever.
  while (running) { };

  // Stop the wire.
  dp->down();

  // Stop the port threads.
  t1->halt();
  t2->halt();

  std::cerr << "Data plane 'dp1' down\n";

  // TODO: Report some statistics?
  // dp->app()->statistics(); ?

  // Cleanup
  fp::delete_port(p1->id());
  std::cerr << "Deleted port 'p1' with id '" << p1->id() << "'\n";
  fp::delete_port(p2->id());
  std::cerr << "Deleted port 'p2' with id '" << p1->id() << "'\n";
  fp::delete_dataplane("dp1");

  fp::unload_application("apps/wire.app");
  std::cerr << "Unloaded application 'apps/wire.app'\n";

  // Delete port threads.
  delete t1;
  delete t2;

  return 0;
}
