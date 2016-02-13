#include "system.hpp"
#include "dataplane.hpp"
#include "port.hpp"
#include "port_odp.hpp"

#include <string>
#include <iostream>
#include <signal.h>

// Emulate a 2 port wire running over TCP ports.

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
  signal(SIGKILL, sig_handle);
  signal(SIGHUP, sig_handle);
  running = true;
  try
  {
//// Open Data Plane Example ////

    /* Init ODP before calling anything else */
    // ODP Library: initialize global ODP framework.  (once / process).
    if (odp_init_global(NULL, NULL))
      throw std::string("Error: ODP global init failed.");

    /* Init this thread */
    // ODP Library: initialize thread-specific ODP framework (once / thread).
    if (odp_init_local(ODP_THREAD_CONTROL))
      throw std::string("Error: ODP local init failed.");

    /* Create packet pool */
    // ODP Library: Create ODP packet pool.
    // - buffer size
    // - pool size
    odp_pool_param_t params;
    odp_pool_param_init(&params);
    params.pkt.seg_len = fp::SHM_PKT_POOL_BUF_SIZE;
    params.pkt.len     = fp::SHM_PKT_POOL_BUF_SIZE;
    params.pkt.num     = fp::SHM_PKT_POOL_SIZE/fp::SHM_PKT_POOL_BUF_SIZE;
    params.type        = ODP_POOL_PACKET;

    odp_pool_t pool = odp_pool_create(fp::PKT_POOL_NAME, &params);
    if (pool == ODP_POOL_INVALID)
      throw std::string("Error: packet pool create failed.");
    odp_pool_print(pool);

    /* Create a pktio instance for each interface */
    // Local Helper Fn: For each interface name specified, assign pktio handle to pool.
    // - In ODP, a pktio handle is an interface to a port.  pktio include a queue,
    //   which is why it is closely tied with (and must be assigned to) a packet pool.
    // - Specifify mode:
    // -- Burst: raw access to I/O (direct calls to recv / send)
    // -- Queue: create an ODP event queue (event notification on packet receipt)
    // -- Scheduled: extends queue to provies scheduling mechansim for defining priorities, etc.
    // Steps include:
    // - pktio = odp_pktio_open(dev, pool, &pktio_param);
    // - [ inq_def = odp_queue_create(inq_name,ODP_QUEUE_TYPE_PKTIN, &qparam); ]
    // - ret = odp_pktio_start(pktio);
/*
    for (i = 0; i < args->appl.if_count; ++i)
      create_pktio(args->appl.if_names[i], pool, args->appl.mode);
*/


    // Create and init worker threads
    // ODP Library: Create processing threads.
    // Critical: this does not appear to be platform agnostic...
/*
    memset(thread_tbl, 0, sizeof(thread_tbl));

    cpu = odp_cpumask_first(&cpumask);
    for (i = 0; i < num_workers; ++i) {
      odp_cpumask_t thd_mask;
      void *(*thr_run_func) (void *);
      int if_idx;

      if_idx = i % args->appl.if_count;

      args->thread[i].pktio_dev = args->appl.if_names[if_idx];
      args->thread[i].mode = args->appl.mode;

      if (args->appl.mode == APPL_MODE_PKT_BURST)
        thr_run_func = pktio_ifburst_thread;
      else // APPL_MODE_PKT_QUEUE
        thr_run_func = pktio_queue_thread;

      //  Create threads one-by-one instead of all-at-once,
      //  because each thread might get different arguments.
      //  Calls odp_thread_create(cpu) for each thread

      odp_cpumask_zero(&thd_mask);
      odp_cpumask_set(&thd_mask, cpu);
      odph_linux_pthread_create(&thread_tbl[i], &thd_mask,
              thr_run_func,
              &args->thread[i],
              ODP_THREAD_WORKER);
      cpu = odp_cpumask_next(&cpumask, cpu);
    }
*/

    /* Master thread waits for other threads to exit */
    // ODP Library: Wait for worker threads to stop.
    //odph_linux_pthread_join(thread_tbl, num_workers);

    //free(args->appl.if_names);
    //free(args->appl.if_str);
    //free(args);
    //printf("Exit\n\n");



//// Flowpath Wire Example ////
    // Instantiate ports.
    //
    // P1 : Connected to an echo client.
    fp::Port* p1 = fp::create_port(fp::Port::Type::odp_burst, "veth1;p1");
    std::cerr << "Created port " << p1->name() << " with id '" << p1->id() << "'\n";


    // P2 : Bound to a netcat TCP port; Acts as the entry point.
    fp::Port* p2 = fp::create_port(fp::Port::Type::odp_burst, "veth3;p2");
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
    while (running) { };

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
    return -1;
  }
  return 0;
}
