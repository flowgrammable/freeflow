#ifndef FP_APPLICATION_HPP
#define FP_APPLICATION_HPP


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
  using Pipe_fn = void (*)(Context*);
  using Config_fn = void (*)(void);

  // State of the application
  enum App_state { NEW, READY, RUNNING, WAITING, STOPPED };

  // Application name.
  std::string name_;
  // Application state.
  App_state   state_;

  // Hook for pipeline function.
  Pipe_fn*    pipeline_;
  // Hook for configuration function.
  Config_fn*  config_;

  // Application port resources.
  Port**  ports_;
  int     num_ports_;

  // Constructors.
  Application(std::string const&, Pipe_fn, Config_fn, int);
  ~Application();

  // Application state.
  inline void start();
  inline void stop();

  // Pipeline function.
  auto pipeline() -> Pipe_fn;

  // Configuration function.
  void configure();

  // Port functions.
  void add_port(Port*);
  void remove_port(Port*);

  // We leave the rest of the stages to be defined by the application.
  // Any application need only implement these necessary functions to
  // be valid.
};


// The Application_library class represents a dynamically loaded
// library that implements flowpath applications.
// The factory is used to instantiate those applications whenever
// they are launched.
struct Application_library
{  
  using Handle = void*;

  // The library name.
  const std::string name_;
  // The application handle.
  Handle app_;
  // The pipeline symbol handle.
  Handle pipeline_;
  // The configuration symbol handle.
  Handle config_;
  // The number of ports required by the application.
  int    num_ports_;
  Application_library(std::string const&, Handle, Handle, Handle, int);
  ~Application_library();
};


} // end namespace fp

#endif