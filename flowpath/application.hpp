#ifndef FP_APPLICATION_HPP
#define FP_APPLICATION_HPP

#include <map>
#include <string>

#include "context.hpp"
#include "port.hpp"

namespace fp
{

struct Application 
{
  // State of the application
  enum App_state { NEW, READY, RUNNING, WAITING, STOPPED };

  // Application name.
  std::string name_;
  // Application state.
  App_state   state_;

  // Application port resources.
  Port**  ports_;
  int     num_ports_;

  // Constructors.
  Application(std::string const&, int);
  ~Application();

  // Application state.
  inline void start();
  inline void stop();

  // Configuration function.
  void configure();

  // Port functions.
  void add_port(Port*);
  void remove_port(Port*);

  // Accessors.
  std::string name() const;
  App_state state() const;
  Port** ports() const;
  int num_ports() const;
};


} // end namespace fp

#endif