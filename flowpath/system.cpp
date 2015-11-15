#include <dlfcn.h>
#include <exception>

#include "system.hpp"
#include "port_table.hpp"

namespace fp 
{
using Dataplane_table = std::unordered_map<std::string, Dataplane*>;


static Port_table port_table;
static Module_table module_table;
static Dataplane_table dataplane_table;


// Creates a new port, adds it to the master port table, and
// returns a pointer to the new port.
Port*
System::create_port(Port::Type port_type, std::string const& args)
{
  // Create the port of given type with args in the master port table.
  Port* p = port_table.alloc(port_type, args);
  
  // Return a pointer to it.
  return p;
}


// Deletes the given port from the system port table.
void
System::delete_port(Port::Id id)
{
  if (port_table.find(id))
    port_table.dealloc(id);
}


// Creates a new data plane and returns a pointer to it. If the 
// name already exists it throws an exception.
Dataplane*
System::create_dataplane(std::string const& name, std::string const& app)
{
  // Check if a dataplane with this name already exists.
  if (dataplane_table.find(name) != dataplane_table.end())
    throw std::string("Data plane name already exists");

  // Allocate the new data plane in the master data plane table.
  dataplane_table.insert({name, new Dataplane(name, app)});
 
  return dataplane_table.at(name);
}


// Deletes the given data plane from the system data plane table.
void
System::delete_dataplane(std::string const& name)
{
  auto dp = dataplane_table.find(name);
  if (dp != dataplane_table.end())
    dataplane_table.erase(dp);
  else
    throw std::string("Data plane name not in use");
}


// Loads the application at the given path. If it exists, throws a message.
// If the application does not exist, it creates the module and adds it to
// the module table.
void
System::load_application(std::string const& path)
{
  // Check if library from this path has already been loaded.
  if (module_table.find(path) != module_table.end())
    throw std::string("Application at '" + path + "' has already been loaded");

  // Get the application handle.
  void* app_hndl = get_app_handle(path);
  
  // Register the path with the Application_library object.
  module_table.insert({path, new Application_library(path, app_hndl)});
}


// Unloads the given application. If it does not exist, throws a message.
void
System::unload_application(std::string const& path)
{
  auto app = module_table.find(path);
  // Check if library from this path has already been loaded.
  if (app != module_table.end())
    throw std::string("Application at '" + path + "' is not loaded.");
  
  // Close the application handle.
  dlclose(app->second->handles_["app"]);

  // Remove the application from the module table.
  module_table.erase(app);
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


} // end namespace fp