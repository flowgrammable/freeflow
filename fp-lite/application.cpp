
#include "application.hpp"

#include <cassert>
#include <stdexcept>

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


// Returns a handle to the given symbol from the given application
// handle. The result can be nullptr.
static inline void*
lib_resolve(void* lib, char const* name)
{
  void* sym = ::dlsym(lib, name);
  if (!sym)
    dlerror();
  return sym;
}


// Returns a handle to the given symbol from the given application
// handle. Throws an exception if there is no such symbol.
static inline void*
lib_require(void* lib, char const* name)
{
  if (void* sym = ::dlsym(lib, name))
    return sym;
  throw std::runtime_error(dlerror());
}


Library::Library(char const* p)
  : path(p), handle(lib_open(p))
{
  load = (Init_fn)lib_resolve(handle, "load");
  unload = (Init_fn)lib_resolve(handle, "unload");
  start = (Init_fn)lib_resolve(handle, "start");
  stop = (Init_fn)lib_resolve(handle, "stop");

  port_added = (Port_fn)lib_resolve(handle, "port_added");
  port_removed = (Port_fn)lib_resolve(handle, "port_removed");
  port_changed = (Port_fn)lib_resolve(handle, "port_changed");

  proc = (Proc_fn)lib_require(handle, "process");
}


Library::~Library()
{
  lib_close(handle);
}

// -------------------------------------------------------------------------- //
// Application objects objects


// Load the application. If the application has a load function
// then that is executed to provide one-time initialization for
// the application.
//
// An application can only be loaded when it is in its initial
// state. Once initialized, the application is in the ready state.
int
Application::load(Dataplane& dp)
{
  assert(state_ == INIT);
  int ret = 0;
  if (lib_.load)
    ret = lib_.load(&dp);
  //if (!ret)
    state_ = READY;
  return ret;
}


// Unload the application. If the application has an unload
// function then that is executed to provide one-time finalization
// for the application.
//
// An application can only be unloaded when it is ready or stopped.
// The application returns to its initial state after being
// unloaded.
int
Application::unload(Dataplane& dp)
{
  assert(state_ == READY || state_ == STOPPED);
  int ret = 0;
  if (lib_.unload)
    ret = lib_.unload(&dp);
  //if (!ret)
    state_ = INIT;
  return ret;
}


// Opens the applications ports and sets the state to 'running'.
int
Application::start(Dataplane& dp)
{
  assert(state_ == READY);
  int ret = 0;
  if (lib_.start)
    ret = lib_.start(&dp);
  //if (!ret)
    state_ = RUNNING;
  return 0;
}


// Closes the applications ports and sets the state to 'stopped'.
int
Application::stop(Dataplane& dp)
{
  assert(state_ == RUNNING);
  int ret = 0;
  if (lib_.stop)
    ret = lib_.stop(&dp);
  //if (!ret)
    state_ = STOPPED;
  return 0;
}


int
Application::port_added(Port& p)
{
  if (Library::Port_fn f = lib_.port_added)
    return f(p.id());
  return 0;
}


int
Application::port_removed(Port& p)
{
  if (Library::Port_fn f = lib_.port_removed)
    return f(p.id());
  return 0;
}


int
Application::port_changed(Port& p)
{
  if (Library::Port_fn f = lib_.port_changed)
    return f(p.id());
  return 0;
}


int
Application::process(Context& cxt)
{
  assert(state_ == RUNNING);
  return lib_.proc(&cxt);
}


} // end namespace fp
