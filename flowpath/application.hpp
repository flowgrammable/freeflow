#ifndef FP_APPLICATION_HPP
#define FP_APPLICATION_HPP

#include <unordered_map>
#include <string>

#include "context.hpp"
#include "port.hpp"

namespace fp
{

struct Port;

// The Application_library class represents a dynamically loaded
// library that implements flowpath applications.
// The factory is used to instantiate those applications whenever
// they are launched.
struct Library
{  
  using Handle = void*;
  using Func = void (*)(void*);
  using Map = std::unordered_map<std::string, Handle>;
  
  // The defined application handles.
  Map handles_ = 
  {
    {"app", nullptr},
    {"pipeline", nullptr},
    {"config", nullptr},
    {"ports", nullptr}
  };
  
  Library(Handle);
  ~Library();

  // Returns the function matching the given string name.
  auto exec(std::string const&) -> Func;

};


struct Application 
{
  // State of the application
  enum State { NEW, READY, RUNNING, WAITING, STOPPED };

  // Application name.
  std::string name_;
  // Application state.
  State   state_;
  // Application library.
  Library lib_;

  // Application port resources.
  Port**  ports_;
  int     num_ports_;

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
  std::string name() const;
  auto state() const -> State;
  Port** ports() const;
  int num_ports() const;
  auto lib() const -> Library;
};

void* get_sym_handle(void*, std::string const&);
void* get_app_handle(std::string const&);

using Module_table = std::unordered_map<std::string, Application*>;

extern Module_table module_table;

} // end namespace fp

#endif