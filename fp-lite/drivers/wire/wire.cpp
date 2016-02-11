
#include "port.hpp"
#include "port_tcp.hpp"
#include "application.hpp"

#include <string>
#include <iostream>
#include <signal.h>

// Emulate a 2 port wire running over TCP ports.

// NOTE: Clang will optimize assume that the running loop
// never terminates if this is not declared volatile.
// Go figure.
static bool volatile running;

void
on_signal(int sig)
{
  std::cout << "HERE\n";
  running = false;
}


int
main()
{
  // TODO: Use sigaction.
  signal(SIGINT, on_signal);
  signal(SIGKILL, on_signal);
  signal(SIGHUP, on_signal);

  try
  {
    // Create some ports.
    fp::Port_tcp p1(0, ff::Ipv4_address::any(), 5000);
    fp::Port_tcp p2(1, ff::Ipv4_address::any(), 5001);

    fp::Application app("apps/wire.app");
    std::cout << "LOADED? " << app.library().path << '\n';
    std::cout << "LOADED? " << app.library().handle << '\n';
    app.library().config();

#if 0

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
#endif

    // Loop forever.
    running = true;
    while (running) { }

#if 0
    // Stop the wire.
    dp->down();
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
#endif
  }
  catch (std::exception& err)
  {
    std::cout << err.what() << '\n';
    return 1;
  }
  return 0;
}
