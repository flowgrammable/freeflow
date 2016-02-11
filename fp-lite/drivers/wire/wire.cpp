
#include "port.hpp"
#include "port_tcp.hpp"
#include "dataplane.hpp"

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
  running = false;
}


int
main()
{
  // TODO: Use sigaction.
  signal(SIGINT, on_signal);
  signal(SIGKILL, on_signal);
  signal(SIGHUP, on_signal);

  // TODO: Handle exceptions.

  // Create some ports. This is the "system" port table.
  fp::Port_tcp p1(0, ff::Ipv4_address::any(), 5000);
  fp::Port_tcp p2(1, ff::Ipv4_address::any(), 5001);

  // Configure the dataplane. Ports must be added
  // before applications are loaded.
  fp::Dataplane dp = "dp1";
  dp.add_drop_port();
  dp.add_port(&p1);
  dp.add_port(&p2);
  dp.load_application("apps/wire.app");
  dp.up();

  // Loop forever.
  running = true;
  while (running) { }

  // Take the dataplane down.
  dp.down();
  dp.unload_application();

  return 0;
}
