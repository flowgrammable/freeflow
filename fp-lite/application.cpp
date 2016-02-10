#include <dlfcn.h>
#include <algorithm>
#include <iostream>
#include "application.hpp"

namespace fp
{


// Creates a new application with the given name (path).
Application::Application(std::string const& name)
  : name_(name), state_(NEW), lib_(get_app_handle(name)), num_ports_(0)
{
  // Get the number of ports required for the application.
  lib_.ports(&num_ports_);
}


// Dtor. Close the application handle.
Application::~Application()
{
  dlclose(lib_.app_);
}


// Opens the applications ports and sets the state to 'running'.
void
Application::start()
{
  for (auto port : ports_)
    if (port->open() == -1)
      throw std::string("Error: open port " + std::to_string(port->id()));
  state_ = RUNNING;
}


// Closes the applications ports and sets the state to 'stopped'.
void
Application::stop()
{
  for (auto port : ports_)
    port->close();
  state_ = STOPPED;
}


// Adds a port to the application.
void
Application::add_port(Port* p)
{
  // Check if all allocated.
  if (ports_.size() == num_ports_)
    throw std::string("All ports have been added");
  else if (std::find(ports_.begin(), ports_.end(), p) == ports_.end()) {
    ports_.push_back(p);
  }
  else
    throw std::string("Port already exists");
}


// Removes a port from the application. This does not (should not) 
// delete the port from the system.
void
Application::remove_port(Port* p)
{
  auto idx = std::find(ports_.begin(), ports_.end(), p);
  if (idx != ports_.end())
    ports_.erase(idx);
  else
    throw std::string("Port not found");
}


// Returns the name (path) of the application.
std::string
Application::name() const
{
	return name_;
}


// Returns the state of the application.
Application::State
Application::state() const
{
	return state_;
}


// Returns a copy of the applications ports vector.
std::vector<Port*>
Application::ports() const
{
  return ports_;
}


// Returns the number of ports required by the application.
int
Application::num_ports() const
{
	return num_ports_;
}


// Returns a copy of the applications library.
Library
Application::lib() const
{
  return lib_;
}


// Library ctor. Links the definitions for library functions.
Library::Library(App_handle app)
{
  app_ = app;
  pipeline_ = (void (*)(Context*))get_sym_handle(app, "pipeline");
  config_   = (void (*)(void))    get_sym_handle(app, "config");
  ports_    = (void (*)(void*))   get_sym_handle(app, "ports");
}


//
Library::~Library()
{ }


/*
// Calls the library's pipeline function.
inline void 
Library::pipeline(Context* cxt) 
{ 
  pipeline_(cxt); 
}


// Calls the library's config function.
inline void 
Library::config() { 
  config_(); 
}


// Calls the library's ports function.
inline void 
Library::ports(void* arg) 
{ 
  ports_(arg); 
}
*/

// Returns a handle to the given symbol from the given application handle.
void*
get_sym_handle(void* app_hndl, std::string const& sym)
{
  dlerror();
  void* sym_hndl = dlsym(app_hndl, sym.c_str());
  const char* err = dlerror();
  if (err)
    throw std::runtime_error(err);
  else
    return sym_hndl;
}


// Returns a handle to the application at the given path, if it exists.
void*
get_app_handle(std::string const& path)
{
  void* hndl = dlopen(path.c_str(), RTLD_LOCAL | RTLD_LAZY);
  if (hndl)
    return hndl;
  else
    throw std::runtime_error(dlerror());
}


} // end namespace fp
