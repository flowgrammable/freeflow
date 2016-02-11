
#include "application.hpp"

#include <algorithm>
#include <iostream>

#include <dlfcn.h>


namespace fp
{

// -------------------------------------------------------------------------- //
// Library objects

// Returns a handle to the application at the given path, if
// it exists. Throws an exception otherwise.
//
// Note that dlopen will search LD_LIBRARY_PATH (on GNU systems)
// or DYLD_LIBRARY_PATH (on Mac OS X) for a library that matches
// the given path.
//
// Multiple calls to this function will simply increase the
// reference count. We can let the system manage that.
static inline void*
lib_open(char const* path)
{
  if (void* lib = ::dlopen(path, RTLD_LOCAL | RTLD_LAZY))
    return lib;
  throw std::runtime_error(dlerror());
}


// Close the given library. This just decrements the reference count.
// Code is unloaded when the reference count reaches 0.
static inline void
lib_close(void* lib)
{
  ::dlclose(lib);
}


// Returns a handle to the given symbol from the given application handle.
static inline void*
lib_resolve(void* lib, char const* name)
{
  if (void* sym = ::dlsym(lib, name))
    return sym;
  throw std::runtime_error(dlerror());
}


Library::Library(char const* p)
  : path(p), handle(lib_open(p))
{
  pipeline = (Pipeline_fn)lib_resolve(handle, "pipeline");
  config = (Config_fn)lib_resolve(handle, "config");
  ports = (Port_fn)lib_resolve(handle, "ports");
}


Library::~Library()
{
  lib_close(handle);
}


// -------------------------------------------------------------------------- //
// Application objects objects

// Creates a new application with the given path.
Application::Application(char const* name)
  : lib_(name), state_(NEW)
{ }


// Opens the applications ports and sets the state to 'running'.
void
Application::start()
{
  // for (auto port : ports_)
  //   if (port->open() == -1)
  //     throw std::string("Error: open port " + std::to_string(port->id()));
  // state_ = RUNNING;
}


// Closes the applications ports and sets the state to 'stopped'.
void
Application::stop()
{
  // for (auto port : ports_)
  //   port->close();
  // state_ = STOPPED;
}




} // end namespace fp
