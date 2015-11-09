#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>

#include "hub.hpp"
#include "thread.hpp"
#include "port_udp.hpp"
#include "port_table.hpp"


// Global factory object for creating/destroying Hub objects
static Factory factory_;

// The hub ports.
static fp::Port* ports[4];

// Thread pool for pipeline work.
static fp::Thread_pool pipeline_pool(5, true, pipeline);

// The hub object.
static Hub* hub;

// The global port table.
extern fp::Port_table port_table;

extern "C" void*
factory() 
{ 
  return &factory_; 
}


fp::Application* 
Factory::create(fp::Dataplane& dp) 
{ 
  return new Hub(dp); 
}


void 
Factory::destroy(fp::Application& a) 
{ 
  delete& a; 
}


Hub::Hub(fp::Dataplane& dp) 
  : fp::Application(dp, "hub")
{
  // Initialize ports.
  ports[0] = ports[1] = ports[2] = ports[3] = nullptr;
  hub = this;
  std::cout << "application 'hub' created\n";
}


Hub::~Hub() 
{ 
  std::cout << "application 'hub' destroyed\n";
}


#if 0
void 
egress_after_miss(fp::Dataplane& dp, fp::Context& ctx) 
{
  hub->egress(&ctx);
}
#endif

// The main hub processing loop. This routine will be executed by a
// separate thread.
static void*
pipeline(void* args)
{
  int id = *((int*)args);
  fp::Task* tsk = nullptr;
  std::cerr << "[thread:" << id << "] starting\n";

  while(hub->state != fp::Application::STOPPED) {
    // Get the next task to be executed.
    if (pipeline_pool.request(*tsk)) {
      tsk->execute();
      delete tsk;
    }
  }

  std::cerr << "[thread:" << id << "] stopped\n";
  return 0;
}


//
static void*
port(void* args)
{
// Figure out who I am.
  int id = *((int*)args);
  fp::Port_udp* self = (fp::Port_udp*)ports[id];
  fp::Context* cxt;

  // Setup (e)polling
  int pfd = epoll_create(1);
  static struct epoll_event handler, *events;
  handler.events = EPOLLIN | EPOLLET;
  handler.data.fd = self->sock_fd_;
  epoll_ctl(pfd, EPOLL_CTL_ADD, self->sock_fd_, &handler);

  // Start rx/tx.
  std::cerr << "[port_thread:" << id << "] starting\n";
  while (hub->state != fp::Application::STOPPED) {
    // Check for rx (poll). 
    int n = epoll_wait(pfd, events, 10, -1);
    if (n < 0)
      throw("epoll_wait error");
    else {
      while (n--) {
        // Receive data.
        cxt = self->recv();
        
        // Create a new set of instructions to execute and assign
        // it to the pipeline work queue.
        pipeline_pool.assign(fp::Task(process, cxt));
      }
    }

    // Check if there is anything to send.
    if (self->tx_queue_.size())
      self->send();
  }
  
  // Cleanup.
  delete[] events;
  close(pfd);

  std::cerr << "[thread:" << id << "] stopped\n";
  return 0;
}


// Set up the flow table.
//
// TODO: Actually set up the flow table.
void 
Hub::configure() 
{ 
  // fp::Hash_table* tbl = new fp::Hash_table();

  // // set up flow for table misses
  // fp::Flow miss_flow;
  // miss_flow.instr_ = egress_after_miss;
  // tbl->miss_ = miss_flow; 

  // dataplane->tables.insert({ "hub", tbl }); 
}


//
void
Hub::start()
{
  // Set ports up.
  for (int i = 0; i < 4; ++i)
    if (ports[i])
      ports[i]->up();    

  // Set the run flag.
  state = fp::Application::RUNNING;

  std::cerr << "[hub] started\n";
}


//
void
Hub::stop()
{
  // Toggle the run flag.
  state = fp::Application::STOPPED;

  // Set the ports down.
  for (int i = 0; i < 4; i++)
    ports[i]->down();

  std::cerr << "[hub] stopped\n";
}



// The 'main' function of the hub application.
static void*
process(void* arg)
{
  fp::Context* cxt = (fp::Context*)arg;
  hub->ingress(*cxt);
  hub->route(*cxt);
  hub->egress(*cxt);
  return 0;
}


//
void
Hub::ingress(fp::Context& cxt)
{ }


// 
void
Hub::egress(fp::Context& cxt)
{
  if (cxt.out_port == port_table.flood_port()->id_)
    port_table.flood_port()->send(&cxt);
}


//
void
Hub::route(fp::Context& cxt)
{
  if (cxt.in_port != port_table.flood_port()->id_)
    cxt.out_port = port_table.flood_port()->id_;
}


// Adds the port to the application.
void
Hub::add_port(fp::Port* p)
{
  if (!ports[0])
    ports[0] = p;
  else if (!ports[1])
    ports[1] = p;
  else if (!ports[2])
    ports[2] = p;
  else if (!ports[3])
    ports[3] = p;
  else
    throw("All ports allocated");
}


// Removes the port from the application.
void
Hub::remove_port(fp::Port* p)
{
  if (ports[0] == p)
    ports[0] = nullptr;
  else if (ports[1] == p)
    ports[1] = nullptr;
  else if (ports[2] == p)
    ports[2] = nullptr;
  else if (ports[3] == p)
    ports[3] = nullptr;
}