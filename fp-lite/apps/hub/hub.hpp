#ifndef FP_HUB_HPP
#define FP_HUB_HPP

#include "application.hpp"
#include "packet.hpp"
#include "port.hpp"

// TODO: this will need a factory
struct Hub : fp::Application 
{
  Hub(fp::Dataplane&);
  ~Hub();

  void configure();

  void start();
  void stop();

  void add_port(fp::Port*);
  void remove_port(fp::Port*);

  void ingress(fp::Context&);
  void route(fp::Context&);
  void egress(fp::Context&);
};

// Thread work functions.
static void* pipeline(void*);
static void* port(void*);
static void* process(void*);

// This class serves as the factory for creating application objects
struct Factory : fp::Application_factory
{
  fp::Application* create(fp::Dataplane&);
  void destroy(fp::Application&);
};

#endif
