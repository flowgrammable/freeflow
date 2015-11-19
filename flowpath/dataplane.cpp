#include <algorithm>

#include "dataplane.hpp"

namespace fp {

extern Module_table module_table;
extern Thread_pool thread_pool;

// Data plane ctor.
Dataplane::Dataplane(std::string const& name, std::string const& app_name)
  : name_(name)
{ 
  auto app = module_table.find(app_name);
  if (app != module_table.end()) 
    app_ = app->second;
  else
    throw std::string("Unknown application name '" + app_name + "'");
}

Dataplane::~Dataplane() 
{ 
  delete app_;
}


// Adds the port to the local list.
void 
Dataplane::add_port(Port* p)
{
	app_->add_port(p);
}


// Removes the port from the local list if it exists.
void 
Dataplane::remove_port(Port* p)
{
	app_->remove_port(p);
}


// TODO: Document me.
void 
Dataplane::up()
{
  if (app_->state() == Application::State::READY) {
    thread_pool.install(app());
    thread_pool.start();
  }
  else if (app_->state() == Application::State::NEW)
    throw std::string("Data plane has not been configured, unable to start");
}


// Stops the application.
void 
Dataplane::down()
{
  if (app_->state() == Application::State::RUNNING) {
    thread_pool.stop();
    thread_pool.uninstall();
  }
  else
    throw std::string("Data plane is not 'up'");
}


// Configures the data plane based on the application.
void
Dataplane::configure()
{
  if (app_->state() == Application::State::NEW)
    app_->lib().exec("config")(nullptr);
  else
    throw std::string("Data plane has already been configured");
}


// Gets the data plane name.
std::string
Dataplane::name() const
{
  return name_;
}


// Gets the data planes application.
Application*
Dataplane::app()
{
  return app_;
}


// Creates a new table in the given data plane with the given size,
// key width, and table type.
extern "C" Table* 
create_table(Dataplane* dp, int size, int key_width, Table::Type type)
{
  return nullptr;
}


// Creates a new flow rule from the given key and function pointer
// and adds it to the given table.
extern "C" void   
add_flow(Table* tbl, void* key, void* fn)
{

}


// Removes the given key from the given table, if it exists.
extern "C" void   
del_flow(Table* tbl, void* key)
{

}


// Returns the port matching the given name.
extern "C" Port*  
get_port(std::string const& name)
{
  return nullptr;
}


// Outputs the contexts packet on the port with the matching name.
extern "C" void   
output_port(Context* cxt, std::string const& name)
{

}


// Dispatches the given context to the given table, if it exists.
extern "C" void   
goto_table(Context* cxt, Table* tbl)
{

}


// Binds a given field index to a section in the packet contexts raw
// packet data. Using the given header offset, field offset, and field
// length we can grab exactly what we need.
extern "C" void   
bind(Context* cxt, int hdr_off, int field_off, int field_len, int field)
{

}


// Binds a header to the context at the given offset with the given length.
extern "C" void   
bind_hdr(Context* cxt, int hdr_off, int len)
{

}


// Loads the field from the context.
extern "C" void   
load(Context* cxt, int field)
{

}


// Creates a subset of the fields in the packet context.
extern "C" void   
gather(Context* cxt, ...)
{

}


// Advances the current context byte pointer 'n' bytes.
extern "C" void   
advance(Context* cxt, int n)
{

}


} // end namespace fp
