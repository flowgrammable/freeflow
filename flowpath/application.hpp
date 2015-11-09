#ifndef FP_APPLICATION_HPP
#define FP_APPLICATION_HPP

#include <dlfcn.h>
#include <iostream>
#include <stdexcept>
#include <typeinfo>

#include "packet.hpp"
#include "context.hpp"
#include "port.hpp"
#include "queue.hpp"

namespace fp
{

struct Dataplane;


struct Application 
{
  using Queue = Locking_queue<Context*>;
// State of the application
  enum App_state { NEW, READY, RUNNING, WAITING, STOPPED };

  Dataplane*  dataplane; // Dataplane that owns this application
  std::string name;
  App_state   state;

  // Constructors
  Application() { }
  Application(Dataplane& dp, std::string const& n) 
    : dataplane(&dp), name(n), state(Application::NEW)
  { }

  virtual ~Application() { }

  virtual void configure() { state = Application::READY; }
  
  // Application state.
  virtual void start() = 0;
  virtual void stop() = 0;

  // Pipeline functions.
  virtual void ingress(Context&) = 0;
  virtual void egress(Context&) = 0;

  // Port functions.
  virtual void add_port(Port*)  = 0;
  virtual void remove_port(Port*) = 0;

  // We leave the rest of the stages to be defined by the application.
  // Any application need only implement these necessary functions to
  // be valid.
};


// The Application_factory is an abstract base class whose derived 
// classes are responsible for creating applications. Objects of this 
// class are returned from the loading of application libraries.
struct Application_factory 
{
  virtual Application* create(Dataplane&) = 0;
  virtual void destroy(Application&) = 0;
};


// The type of the factory function pointer
using Application_factory_fn = Application_factory*(*)();


// The Application_library class represents a dynamically loaded
// library that implements (one or more?) flowpath applications.
// The factory is used to instantiate those applications whenever
// they are launched.
struct Application_library
{  
  void* handle;                 // handle for the dll
  Application_factory* factory; // The factory object

  Application_library() { };
  Application_library(std::string const&);
  
  // Application management
  Application* create(Dataplane&) const;
  void destroy(Application&) const;
};


} // end namespace fp

#endif