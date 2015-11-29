#include <exception>
#include <unordered_map>

#include "system.hpp"
#include "port_table.hpp"
#include "application.hpp"


namespace fp
{


Module_table     module_table;     // Flowpath module table.
Dataplane_table  dataplane_table;  // Flowpath data plane table.
Port_table       port_table;       // Flowpath port table.
Thread_pool      thread_pool(4, true);      // Flowpath thread pool.


// Creates a new port, adds it to the master port table, and
// returns a pointer to the new port.
Port*
create_port(Port::Type port_type, std::string const& args)
{
  // Create the port of given type with args in the master port table.
  Port* p = port_table.alloc(port_type, args);

  // Return a pointer to it.
  return p;
}


// Deletes the given port from the system port table.
void
delete_port(Port::Id id)
{
  if (port_table.find(id))
    port_table.dealloc(id);
}


// Creates a new data plane and returns a pointer to it. If the
// name already exists it throws an exception.
Dataplane*
create_dataplane(std::string const& name, std::string const& app)
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
delete_dataplane(std::string const& name)
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
load_application(std::string const& path)
{
  // Check if library from this path has already been loaded.
  if (module_table.find(path) != module_table.end())
    throw std::string("Application at '" + path + "' has already been loaded");

  // Register the path with the Application_library object.
  module_table.insert({path, new Application(path)});
}


// Unloads the given application. If it does not exist, throws a message.
void
unload_application(std::string const& path)
{
  auto app = module_table.find(path);
  // Check if library from this path has already been loaded.
  if (app != module_table.end())
    throw std::string("Application at '" + path + "' is not loaded.");

  // Remove the application from the module table.
  module_table.erase(app);
}


} // end namespace fp


//////////////////////////////////////////////////////
//              External runtime system calls       //
//////////////////////////////////////////////////////
//


// Returns the port matching the given name.
extern "C" fp::Port*
fp_get_port(std::string const& name)
{
  return fp::port_table.find(name);
}


// Outputs the contexts packet on the port with the matching name.
extern "C" void
fp_output_port(fp::Context* cxt, std::string const& name)
{
  fp::port_table.find(name)->send(cxt);
}


// Creates a new table in the given data plane with the given size,
// key width, and table type.
extern "C" fp::Table*
fp_create_table(fp::Dataplane* dp, int size, int key_width, fp::Table::Type type)
{
  fp::Table* tbl = nullptr;
  switch (type)
  {
    case fp::Table::Type::EXACT:
    // Make a new hash table.
    tbl = new fp::Hash_table(size);
    break;
    case fp::Table::Type::PREFIX:
    // Make a new prefix match table.
    break;
    case fp::Table::Type::WILDCARD:
    // Make a new wildcard match table.
    break;
    default:
    throw std::string("Unknown table type given");
  }
  return tbl;
}


// Dispatches the given context to the given table, if it exists.
extern "C" void
fp_goto_table(fp::Context* cxt, fp::Table* tbl)
{

}


// Creates a new flow rule from the given key and function pointer
// and adds it to the given table.
extern "C" void
fp_add_flow(fp::Table* tbl, void* key, void* fn)
{

}


// Removes the given key from the given table, if it exists.
extern "C" void
fp_del_flow(fp::Table* tbl, void* key)
{

}


// Binds a given field index to a section in the packet contexts raw
// packet data. Using the given header offset, field offset, and field
// length we can grab exactly what we need.
extern "C" void
fp_bind(fp::Context* cxt, int hdr_off, int field_off, int field_len, int field)
{

}


// Binds a header to the context at the given offset with the given length.
extern "C" void
fp_bind_hdr(fp::Context* cxt, int hdr_off, int len)
{

}


// Loads the field from the context.
extern "C" void
fp_load(fp::Context* cxt, int field)
{

}


// Creates a subset of the fields in the packet context.
extern "C" void
fp_gather(fp::Context* cxt, ...)
{

}


// Advances the current context byte pointer 'n' bytes.
extern "C" void
fp_advance(fp::Context* cxt, int n)
{

}
