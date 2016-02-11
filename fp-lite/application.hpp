#ifndef FP_APPLICATION_HPP
#define FP_APPLICATION_HPP

#include <string>
#include <vector>
#include <unordered_map>


namespace fp
{

class Port;
struct Context;


// The Library class represents a dynamically loaded application.
struct Library
{
  using Pipeline_fn = void (*)(Context*);
  using Config_fn =   void (*)(void);
  using Port_fn =     void (*)(void*);

  Library(char const*);
  ~Library();

  char const* path;
  void*       handle;
  Pipeline_fn pipeline;
  Config_fn   config;
  Port_fn     ports;
};


// An application is a user-defined program that executes
// on a dataplane.
struct Application
{
  // State of the application
  enum State { NEW, READY, RUNNING, WAITING, STOPPED };

  // Constructors.
  Application(char const*);

  void start();
  void stop();

  // Returns the underlying library.
  Library const& library() const { return lib_; }
  Library&       library()       { return lib_; }

  // Returns the current application state.
  State state() const { return state_; }

  Library     lib_;
  State       state_;
};


// FIXME: Do I need these?
using Module_table = std::unordered_map<std::string, Application*>;

extern Module_table module_table;


} // end namespace fp

#endif
