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
System::make_dataplane(std::string const& name, std::string const& type)
{
  // Check if a dataplane with this name already exists.
  if (dataplane_table.find(name) != dataplane_table.end())
  {
    throw("data plane name exists");
  }

  // Allocate the new data plane.
  Dataplane* dp = new Dataplane(this, name, type);

  // Update the master port table.
  dataplane_table.insert({name, dp});

  // Return a reference to it.
  return *dp;
}


void
System::load_application(std::string const& path)
{
  // Check if library from this path has already been loaded.
  if (module_table.find(path) != module_table.end())
    throw("module exists");

  // Construct a new application library object.
  Application_library app_lib = Application_library(path);

  // Register the path with the Application_library object.
  module_table.insert({path, app_lib});
}


} // end namespace fp