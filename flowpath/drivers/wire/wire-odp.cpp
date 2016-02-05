#include "system.hpp"
#include "dataplane.hpp"
#include "port.hpp"
#include "port_odp.hpp"

#include <string>
#include <iostream>
//#include <csignal>
#include <cstdlib>
//#include <unistd.h>

// odp includes:
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include <example_debug.h>

#include <odp.h>
#include <odp/helper/linux.h>
#include <odp/helper/eth.h>
#include <odp/helper/ip.h>

// Emulate a 2 port wire running using OpenDataplane (ODP) ports.

static bool running;

fp::Port::Statistics stats1;
fp::Port::Statistics stats2;

void
sig_handle(int sig)
{
  running = false;
}

void
report(fp::Port* p1, fp::Port* p2)
{
  auto curr1 = p1->stats();
  auto curr2 = p2->stats();
  double through_rx1 = curr1.pkt_rx - stats1.pkt_rx;
  double through_rx2 = curr2.pkt_rx - stats2.pkt_rx;
  double through_tx1 = curr1.pkt_tx - stats1.pkt_tx;
  double through_tx2 = curr2.pkt_tx - stats2.pkt_tx;
  stats1 = curr1;
  stats2 = curr2;
  system("clear");
  std::cout << "p1:\npkt_rx: " << stats1.pkt_rx << "\npkt_tx: " << stats1.pkt_tx << '\n';
  std::cout << "byt_rx: " << stats1.byt_rx << "\nbyt_tx: " << stats1.byt_tx << '\n';
  std::cout << "throughput_rx: " << through_rx1 << "\nthroughput_tx: " << through_tx1 << "\n\n";

  std::cout << "p2:\npkt_rx: " << stats2.pkt_rx << "\npkt_tx: " << stats2.pkt_tx << '\n';
  std::cout << "byt_rx: " << stats2.byt_rx << "\nbyt_tx: " << stats2.byt_tx << '\n';
  std::cout << "throughput_rx: " << through_rx2 << "\nthroughput_tx: " << through_tx2 << "\n\n";
}

int
main()
{
  signal(SIGINT, sig_handle);
  signal(SIGKILL, sig_handle);
  signal(SIGHUP, sig_handle);
  running = true;
  try
  {
    // Initialize ODP:
    /* Init ODP before calling anything else */
    if (odp_init_global(NULL, NULL)) {
      EXAMPLE_ERR("Error: ODP global init failed.\n");
      exit(EXIT_FAILURE);
    }

    /* Init this thread */
    if (odp_init_local(ODP_THREAD_CONTROL)) {
      EXAMPLE_ERR("Error: ODP local init failed.\n");
      exit(EXIT_FAILURE);
    }

    /* Default to system CPU count unless user specified */
    int num_workers = 1;  // just 1 thread for now

    /* Get default worker cpumask */
    odp_cpumask_t cpumask;
    char cpumaskstr[ODP_CPUMASK_STR_SIZE];
    num_workers = odp_cpumask_default_worker(&cpumask, num_workers);
    (void)odp_cpumask_to_str(&cpumask, cpumaskstr, sizeof(cpumaskstr));

    printf("num worker threads: %i\n", num_workers);
    printf("first CPU:          %i\n", odp_cpumask_first(&cpumask));
    printf("cpu mask:           %s\n", cpumaskstr);

    /* Create packet pool */
    odp_pool_param_t params;
    odp_pool_param_init(&params);
    params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
    params.pkt.len     = SHM_PKT_POOL_BUF_SIZE;
    params.pkt.num     = SHM_PKT_POOL_SIZE/SHM_PKT_POOL_BUF_SIZE;
    params.type        = ODP_POOL_PACKET;

    odp_pool_t pool = odp_pool_create("packet_pool", &params);

    if (pool == ODP_POOL_INVALID) {
      EXAMPLE_ERR("Error: packet pool create failed.\n");
      exit(EXIT_FAILURE);
    }
    odp_pool_print(pool);

    /* Create a pktio instance for each interface */
    create_pktio("veth1", pool, ODP_PKTIN_MODE_SCHED);
    create_pktio("veth3", pool, ODP_PKTIN_MODE_SCHED);



    // Initialize noproto:
    // Instantiate ports.
    //
    // P1 : Connected to an echo client.
    fp::Port* p1 = fp::create_port(fp::Port::Type::odp, "veth1;p1");
    std::cerr << "Created port " << p1->name() << " with id '" << p1->id() << "'\n";


    // P2 : Bound to a netcat UDP port; Acts as the entry point.
    fp::Port* p2 = fp::create_port(fp::Port::Type::odp, "veth3;p2");
    std::cerr << "Created port " << p2->name() << " with id '" << p2->id() << "'\n";


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

    // Loop forever.
    while (running) {
      sleep(1);
      report(p1, p2);
    }

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
  }
  catch (std::string msg)
  {
    std::cerr << msg << std::endl;
  }
  return 0;
}
