#include <algorithm>

#include "dataplane.hpp"
#include "application_library.hpp"
#include "thread.hpp"

namespace fp {

extern Module_table module_table;
static Thread_pool thread_pool(4, true);

// Data plane ctor.
Dataplane::Dataplane(std::string const& name, std::string const& app)
  : name_(name)
{ 
  auto app_lib = module_table.find(app);
  if (app_lib != module_table.end()) 
    app_ = new Application(app_lib->second->name(), app_lib->second->num_ports());
  else
    throw std::string("Unknown application name '" + app + "'");
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
  if (app_->state() == Application::State::NEW) {
    ((void (*)())module_table[app_->name()]->exec("config"))();
  }
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


} // end namespace fp
