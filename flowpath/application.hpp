#ifndef FP_APPLICATION_HPP
#define FP_APPLICATION_HPP

#include <map>
#include <string>

#include "context.hpp"
#include "port.hpp"

namespace fp
{

struct Port;

struct Application 
{
  // State of the application
  enum State { NEW, READY, RUNNING, WAITING, STOPPED };

  // Application name.
  std::string name_;
  // Application state.
  State   state_;

  // Application port resources.
  Port**  ports_;
  int     num_ports_;

  // Constructors.
  Application(std::string const&, int);
  ~Application();

  // Application state.
  void start();
  void stop();

  // Port functions.
  void add_port(Port*);
  void remove_port(Port*);

  // Accessors.
  std::string name() const;
  auto state() const -> State;
  Port** ports() const;
  int num_ports() const;
};


} // end namespace fp

#endif