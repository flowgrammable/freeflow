#include <dlfcn.h>
#include <exception>

#include "system.hpp"
#include "port.hpp"
#include "port_table.hpp"

namespace fp 
{


extern Port_table port_table;


// Creates a new UDP port, adds it to the master port table, and
// returns a reference to the new port.
Port*
System::make_port(Port::Type port_type, std::string const& args)
{
  // Create the port of given type with args in the master port table.
  Port* p = port_table.alloc(port_type, args);
  
  // Return a reference to it.
  return p;
}


// Creates a new data plane and returns a reference to it. If the 
// name already exists it throws an exception.
Dataplane& 
System::make_dataplane(std::string const& name, Application_library* app_lib)
{
  // Check if a dataplane with this name already exists.
  if (dataplane_table.find(name) != dataplane_table.end())
  {
    throw("data plane name exists");
  }

  // Allocate the new data plane.
  Dataplane* dp = new Dataplane(name, app_lib);

  // Update the master port table.
  dataplane_table.insert({name, dp});

  // Return a reference to it.
  return *dp;
}


// Loads the application at the given path. If it exists, throws a message.
// If the application does not exist, it creates the module and adds it to
// the module table.
Application_library*
System::load_application(std::string const& path)
{
  // Check if library from this path has already been loaded.
  if (module_table.find(path) != module_table.end())
    throw("module exists");

  void* app_hndl = get_app_handle(path);
  void* pipe_hndl = get_sym_handle(app_hndl, "process");
  void* cfg_hndl = get_sym_handle(app_hndl, "config");
  void* ports = get_sym_handle(app_hndl, "ports");
  int num_ports = *((int*)ports);
  Application_library* app_lib = new Application_library(app_hndl, pipe_hndl, cfg_hndl, ports);

  // Register the path with the Application_library object.
  module_table.insert({path, &app_lib});
  return app_lib;
}


// Returns a handle to the application at the given path, if it exists.
void*
System::get_app_handle(std::string const& path)
{
  void* hndl = dlopen(path.c_str(), RTLD_LOCAL | RTLD_LAZY);
  if (hndl)
    return hndl;
  else
    throw std::runtime_error(dlerror());
}


// Returns a handle to the given symbol from the given application handle.
void*
System::get_sym_handle(void* app_hndl, std::string const& sym)
{
  dlerror();
  void* sym_hndl = dlsym(app_hndl, sym.c_str());
  const char* err = dlerror();
  if (err)
    throw std::runtime_error(err);
  else
    return sym_hndl;
}


} // end namespace fp