#ifndef FP_APPLICATION_HPP
#define FP_APPLICATION_HPP

#include <unordered_map>
#include <string>
#include <vector>

#include "port.hpp"

namespace fp
{


class Port;
struct Context;
struct Thread;

// The Library class represents a dynamically loaded application
// library that contains user provided definitions for application
// functions such as the pipeline and configuration.
struct Library
{
  using App_handle =  void*;
  using Pipeline_fn = void (*)(Context*);
  using Config_fn =   void (*)(void);
  using Port_fn =     void (*)(void*);

  // The user defined application functions.
  const std::string handles_[3] =
  {
    "pipeline",
    "config",
    "ports"
  };

  Library(App_handle);
  ~Library();

  // Executes the function matching the given string name.
  void exec(std::string const&, void* arg = nullptr);

  App_handle  app_;
  Pipeline_fn pipeline_;
  Config_fn   config_;
  Port_fn     port_;
};


struct Application
{
  // State of the application
  enum State { NEW, READY, RUNNING, WAITING, STOPPED };

  // Constructors.
  Application(std::string const&);
  ~Application();

  // Application state.
  void start();
  void stop();

  // Port functions.
  void add_port(Port*);
  void remove_port(Port*);

  // Accessors.
  std::string         name() const;
  State               state() const;
  std::vector<Port*>  ports() const;
  int                 num_ports() const;
  Library             lib() const;

  // Data members.
  //
  // Application name.
  std::string name_;

  // Application state.
  State       state_;

  // Application library.
  Library     lib_;

  // Application port resources.
  std::vector<Port*>    ports_;
  int     num_ports_;
};

void* get_sym_handle(void*, std::string const&);
void* get_app_handle(std::string const&);

using Module_table = std::unordered_map<std::string, Application*>;

extern Module_table module_table;

} // end namespace fp

#endif
