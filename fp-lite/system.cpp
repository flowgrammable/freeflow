
#include "system.hpp"
#include "application.hpp"
#include "endian.hpp"
#include "context.hpp"
#include "dataplane.hpp"

#include <cassert>


namespace fp
{

// Module_table     module_table;          // Flowpath module table.
// Dataplane_table  dataplane_table;       // Flowpath data plane table.
// Port_table       port_table;            // Flowpath port table.
// Thread_pool      thread_pool(0, true);  // Flowpath thread pool.


// // Creates a new port, adds it to the master port table, and
// // returns a pointer to the new port.
// Port*
// create_port(Port::Type port_type, std::string const& args)
// {
//   // Create the port of given type with args in the master port table.
//   Port* p = port_table.alloc(port_type, args);
//
//   // Return a pointer to it.
//   return p;
// }
//
//
// // Deletes the given port from the system port table.
// void
// delete_port(Port::Id id)
// {
//   if (port_table.find(id))
//     port_table.dealloc(id);
// }


// // Creates a new data plane and returns a pointer to it. If the
// // name already exists it throws an exception.
// Dataplane*
// create_dataplane(std::string const& name, std::string const& app)
// {
//   // Check if a dataplane with this name already exists.
//   if (dataplane_table.find(name) != dataplane_table.end())
//     throw std::string("Data plane name already exists");
//
//   // Allocate the new data plane in the master data plane table.
//   dataplane_table.insert({name, new Dataplane(name, app)});
//
//   return dataplane_table.at(name);
// }
//
//
// // Deletes the given data plane from the system data plane table.
// void
// delete_dataplane(std::string const& name)
// {
//   auto __system_dp = dataplane_table.find(name);
//   if (__system_dp != dataplane_table.end())
//     dataplane_table.erase(__system_dp);
//   else
//     throw std::string("Data plane name not in use");
// }
//
//
// // Loads the application at the given path. If it exists, throws a message.
// // If the application does not exist, it creates the module and adds it to
// // the module table.
// void
// load_application(std::string const& path)
// {
//   // Check if library from this path has already been loaded.
//   if (module_table.find(path) != module_table.end())
//     throw std::string("Application at '" + path + "' has already been loaded");
//
//   // Register the path with the Application_library object.
//   module_table.insert({path, new Application(path)});
// }
//
//
// // Unloads the given application. If it does not exist, throws a message.
// void
// unload_application(std::string const& path)
// {
//   auto app = module_table.find(path);
//   // Check if library from this path has already been loaded.
//   if (app != module_table.end())
//     throw std::string("Application at '" + path + "' is not loaded.");
//
//   // Remove the application from the module table.
//   module_table.erase(app);
// }


} // end namespace fp


//////////////////////////////////////////////////////////////////////////
//                    External Runtime System Calls                     //
//////////////////////////////////////////////////////////////////////////
//
// These are the set of system calls that an application can expect
// to be able to call at runtime.

// TODO: Add some proper error handling.

extern "C"
{


// Send the packet through the drop port.
void
fp_drop(fp::Context* cxt)
{
  if (!cxt)
    throw std::string("Null context pointer");
  if (fp::Port* drop = cxt->dataplane()->get_drop_port())
    cxt->set_output_port(drop->id());
  else
    throw std::string("No drop port allocated");
}


void
fp_flood(fp::Context* cxt)
{
  // Output to all ports other than in_port
  // fp::Port* flood = fp::port_table.flood_port();
  // flood->send(cxt);
}

// Outputs a copy of the packet on the port with the matching id.
// FIXME: Implement differently once we figure out how egress queues work.
void
fp_output_port(fp::Context* cxt, fp::Port::Id id)
{
  // fp::Port* p = cxt->dataplane()->get_port(id);
  // assert(p);
  fp_context_set_output_port(cxt, id);
}


// Apply the given action to the context.
void
fp_apply(fp::Context* cxt, fp::Action a)
{
  cxt->apply_action(a);
}


// Write the given action to the context's action list.
void
fp_write(fp::Context* cxt, fp::Action a)
{
  cxt->write_action(a);
}


// Clear the context's action list.
void
fp_clear(fp::Context* cxt)
{
  cxt->clear_actions();
}


// Dispatches the given context to the given table, if it exists.
// Accepts a variadic list of fields needed to construct a key to
// match against the table.
void
fp_goto_table(fp::Context* cxt, fp::Table* tbl, int n, ...)
{
  va_list args;
  va_start(args, n);
  fp::Key key = fp_gather(cxt, tbl->key_size(), n, args);
  va_end(args);

  fp::Flow flow = tbl->search(key);
  // execute the flow function
  flow.instr_(&flow, tbl, cxt);
}


// -------------------------------------------------------------------------- //
// Port and table operations


// Returns the port matching the given id or error otherwise.
//
// FIXME: This currently just verifies that id is in fact a valid
// port. Is that really what we want to do?
fp::Port::Id
fp_get_port_by_id(fp::Dataplane* dp, unsigned int id)
{
  assert(dp);
  if (dp->get_port(id))
    return id;
  else
    return 0;
}

// Returns whether or not the port is up or down
bool
fp_port_id_is_up(fp::Dataplane* dp, fp::Port::Id id)
{
  assert(dp);
  assert(dp->get_port(id));
  return dp->get_port(id)->is_up();
}

// Returns whether or not the given id exists.
bool
fp_port_id_is_down(fp::Dataplane* dp, fp::Port::Id id)
{
  assert(dp);
  assert(dp->get_port(id));
  return dp->get_port(id)->is_down();
}


int
fp_port_get_id(fp::Port* p)
{
  assert(p);
  return p->id();
}


int
fp_port_is_up(fp::Port* p)
{
  assert(p);
  return p->is_up();
}


int
fp_port_is_down(fp::Port* p)
{
  assert(p);
  return p->is_down();
}


// Copies the values within 'n' fields into a byte buffer
// and constructs a key from it.
//
// TODO: I suspect that this is fundamentally broken. Why doesn't
// the language assemble the key -- it knows where stuff is stored.
// we shouldn't rely on the data plane to figure that out.
fp::Key
fp_gather(fp::Context* cxt, int key_width, int n, va_list args)
{
  assert(cxt);
  
  // FIXME: We're using a fixed size key of 128 bytes right now
  // apparently. This should probably be dynamic.
  fp::Byte buf[fp::key_size];
  
  // Iterate through the fields given in args and copy their
  // values into a byte buffer.
  int i = 0;
  int j = 0;
  int in_port;
  int in_phy_port;
  while (i < n) {
    int f = va_arg(args, int);
    fp::Binding b;
    fp::Byte* p = nullptr;

    // Check for "Special fields"
    switch (f) {
      // Looking for "in_port"
      case 255:
        in_port = cxt->input_port_id();
        p = reinterpret_cast<fp::Byte*>(&in_port);
        // Copy the field into the buffer.
        std::copy(p, p + sizeof(in_port), &buf[j]);
        j += sizeof(in_port);
        break;

      // Looking for "in_phys_port"
      case 256:
        in_phy_port = cxt->input_physical_port_id();
        p = reinterpret_cast<fp::Byte*>(&in_phy_port);

        // Copy the field into the buffer.
        std::copy(p, p + sizeof(in_phy_port), &buf[j]);
        j += sizeof(in_phy_port);
        break;

      // Regular fields
      default:
        // Lookup the field in the context.
        b = cxt->get_field_binding(f);
        p = cxt->get_field(b.offset);
        
        // Copy the field into the buffer.
        std::copy(p, p + b.length, &buf[j]);
        
        // Then reverse the field in place.
        fp::network_to_native_order(&buf[j], b.length);
        j += b.length;
        break;
    }
    ++i;
  }

  // Copy the buffer into a key.
  //
  // FIXME: This deals only with fixed-size keys.
  fp::Key k;
  std::memcpy(&k, buf, sizeof(k));
  return k;
}


// Creates a new table in the given data plane with the given size,
// key width, and table type.
fp::Table*
fp_create_table(fp::Dataplane* dp, int id, int key_width, int size, fp::Table::Type type)
{
  assert(dp);

  fp::Table* tbl = nullptr;

  switch (type)
  {
    case fp::Table::Type::EXACT:
      // Make a new hash table.
      tbl = new fp::Hash_table(id, size, key_width);
      dp->tables_.insert({id, tbl});
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


// Creates a new flow rule from the given key and function pointer
// and adds it to the given table.
//
// FIXME: Currently ignoring timeout.
void
fp_add_init_flow(fp::Table* tbl, void* fn, void* key, unsigned int timeout, unsigned int egress)
{
  assert(tbl);
  assert(fn);
  assert(key);

  // FIXME: This assumes a fixed-length key size. We should probably be
  // passing the length in -- or better yet, simply do all of this
  // from within the language.
  fp::Key k;
  std::memcpy(&k, key, sizeof(k));

  // cast the flow into a flow instruction
  fp::Flow_instructions instr = reinterpret_cast<fp::Flow_instructions>(fn);
  fp::Flow flow(0, fp::Flow_counters(), instr, fp::Flow_timeouts(), 0, 0, egress);

  tbl->insert(k, flow);
}


// FIXME: Ignoring timeouts.
void
fp_add_new_flow(fp::Table* tbl, void* fn, void* key, unsigned int timeout, unsigned int egress)
{
  assert(tbl);
  assert(fn);
  assert(key);


  // FIXME: This assumes a fixed-length key size. We should probably be
  // passing the length in -- or better yet, simply do all of this
  // from within the language.
  fp::Key k;
  std::memcpy(&k, key, sizeof(k));
  
  // cast the flow into a flow instruction
  fp::Flow_instructions instr = reinterpret_cast<fp::Flow_instructions>(fn);
  fp::Flow flow(0, fp::Flow_counters(), instr, fp::Flow_timeouts(), 0, 0, egress);

  tbl->insert(k, flow);
}


fp::Port::Id
fp_get_flow_egress(fp::Flow* f)
{
  assert(f && f->egress_ > 0);
  return f->egress_;
}


// Adds the miss case for the table.
//
// FIXME: Ignoring timeout value.
void
fp_add_miss(fp::Table* tbl, void* fn, unsigned int timeout, unsigned int egress)
{
  assert(tbl);
  assert(fn);

  // cast the flow into a flow instruction
  fp::Flow_instructions instr = reinterpret_cast<fp::Flow_instructions>(fn);
  fp::Flow flow(0, fp::Flow_counters(), instr, fp::Flow_timeouts(), 0, 0, egress);
  tbl->insert_miss(flow);
}


// Removes the given key from the given table, if it exists.
void
fp_del_flow(fp::Table* tbl, void* key)
{
  assert(tbl);
  assert(key);

  fp::Key k;
  std::memcpy(&k, key, sizeof(k));
  
  // delete the key
  tbl->erase(k);
}

// Removes the miss case from the given table and replaces
// it with the default.
void
fp_del_miss(fp::Table* tbl)
{
  assert(tbl);
  tbl->erase_miss();
}


// Raise an event.
//
// TODO: Make this asynchronous on another thread. 
void
fp_raise_event(fp::Context* cxt, void* handler)
{
  assert(cxt);
  assert(handler);

  // Cast the handler back to its appropriate function type
  // of void (*)(Context*)
  void (*event)(fp::Context*) = (void (*)(fp::Context*))(handler);
  
  // Invoke the event.
  // FIXME: This should produce a copy of the context and process it
  // seperately.
  //
  // FIXME: Pass it to a thread instead.
  event(cxt);
}


} // extern "C"
