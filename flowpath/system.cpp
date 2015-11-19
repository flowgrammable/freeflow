#include <exception>
#include <unordered_map>

#include "system.hpp"
#include "port_table.hpp"
#include "application.hpp"


namespace fp 
{


extern Module_table     module_table;         // Flowpath module table.
Dataplane_table         dataplane_table;      // Flowpath data plane table.
extern Port_table       port_table;           // Flowpath port table.
extern Thread_pool      thread_pool; // Flowpath thread pool.


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
  
  // Register the path with the Application_library object.
  module_table.insert({path, new Application(path)});
}


// Unloads the given application. If it does not exist, throws a message.
void
System::unload_application(std::string const& path)
{
  auto app = module_table.find(path);
  // Check if library from this path has already been loaded.
  if (app != module_table.end())
    throw std::string("Application at '" + path + "' is not loaded.");

  // Remove the application from the module table.
  module_table.erase(app);
}


} // end namespace fp