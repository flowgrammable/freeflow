#ifndef FP_WIRE_HPP
#define FP_WIRE_HPP

#include "application.hpp"
#include "packet.hpp"
#include "port.hpp"

struct Wire : fp::Application 
{

  Wire(fp::Dataplane&);
  ~Wire();

  // Startup events.
  void start();
  void stop();

  // Pipeline functions.
  void ingress(fp::Context&);
  void route(fp::Context&);
  void egress(fp::Context&);

  // Port functions.
  void add_port(fp::Port*);
  void remove_port(fp::Port*);
};


// Thread work function.
static void* pipeline(void*);
static void* port(void*);
static void* process(void*);

// This class serves as the factory for creating application objects.
struct Factory : fp::Application_factory
{
  fp::Application* create(fp::Dataplane&);
  void destroy(fp::Application&);
};

#endif
