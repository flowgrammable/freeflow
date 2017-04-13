#include "system.hpp"
#include "dataplane.hpp"
#include "port.hpp"
#include "port_odp.hpp"
///#include "packet.hpp" // temporary for sizeof packet
///#include "context.hpp" // temporary for sizeof context

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
main(int argc, const char* argv[])
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
    // - ODP_THREAD_CONTROL: allows scheduling by OS
    // - ODP_THREAD_WORKER:  pins thread and tries to prevent scheduling by OS
    // WARN: segfaults if any child threads do not call odp_init_local(...)
    ///if (odp_init_local(ODP_THREAD_CONTROL))
    if (odp_init_local(ODP_THREAD_WORKER))
      throw std::string("Error: ODP local init failed.");

    /* Create packet pool */
    // ODP Library: Create ODP packet pool.
    // - buffer size
    // - pool size
    // - provide space for Context and Packet in user area of pkt buffer
    odp_pool_param_t params;
    odp_pool_param_init(&params);
    params.pkt.seg_len = fp::SHM_PKT_POOL_BUF_SIZE;
    params.pkt.len     = fp::SHM_PKT_POOL_BUF_SIZE;
    params.pkt.num     = fp::SHM_PKT_POOL_SIZE/fp::SHM_PKT_POOL_BUF_SIZE;
    ///params.pkt.uarea_size  = sizeof(fp::Context) + sizeof(fp::Packet);
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
    //fp::Port* p1 = fp::create_port(fp::Port::Type::odp_burst, "veth1;p1");
    fp::Port* p1 = fp::create_port(fp::Port::Type::odp_burst, "0;p1");
    std::cerr << "Created port " << p1->name() << " with id '" << p1->id() << std::endl;


    // P2 : Bound to a netcat TCP port; Acts as the entry point.
    //fp::Port* p2 = fp::create_port(fp::Port::Type::odp_burst, "veth3;p2");
    fp::Port* p2 = fp::create_port(fp::Port::Type::odp_burst, "1;p2");
    std::cerr << "Created port " << p2->name() << " with id '" << p2->id() << std::endl;


    // Load the application library
    const char* appPath = "apps/wire.app";
    if (argc == 2)
      appPath = argv[1];

    fp::load_application(appPath);
    std::cerr << "Loaded application " << appPath << std::endl;

    // Create the dataplane with the loaded application library.
    fp::Dataplane* dp = fp::create_dataplane("dp1", appPath);
    std::cerr << "Created data plane " << dp->name() << std::endl;

    // Configure the data plane based on the applications needs.
    dp->configure();
    std::cerr << "Data plane configured." << std::endl;

    // Add all ports
    dp->add_port(p1);
    std::cerr << "Added port 'p1' to data plane 'dp1'" << std::endl;
    dp->add_port(p2);
    std::cerr << "Added port 'p2' to data plane 'dp1'" << std::endl;

    // Start the wire.
    dp->up();
    std::cerr << "Data plane 'dp1' up" << std::endl;

    // Loop forever.
    // - currently recv() automatically runs packet through pipeline.
    while (running) {
      // FIXME: Hack to recieve on both ports until new port_table interface is defined...
      int pkts = 0;
      pkts = p1->recv();
      pkts = p1->send();

      pkts = p2->recv();
      pkts = p2->send();
    };

    // Stop the wire.
    dp->down();
    std::cerr << "Data plane 'dp1' down" << std::endl;

    // TODO: Report some statistics?
    // dp->app()->statistics(); ?

    // Cleanup
    fp::delete_port(p1->id());
    std::cerr << "Deleted port 'p1' with id '" << p1->id() << std::endl;
    fp::delete_port(p2->id());
    std::cerr << "Deleted port 'p2' with id '" << p1->id() << std::endl;
    fp::delete_dataplane("dp1");

    fp::unload_application("apps/wire.app");
    std::cerr << "Unloaded application 'apps/wire.app'" << std::endl;
  }
  catch (std::string msg)
  {
    std::cerr << msg << std::endl;
    odp_term_global();
    return -1;
  }

  odp_term_global();
  return 0;
}
