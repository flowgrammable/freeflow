#include <algorithm>

#include "dataplane.hpp"
#include "application_library.hpp"

namespace fp {

extern Module_table module_table;

// Data plane ctor.
Dataplane::Dataplane(std::string const& name, std::string const& app)
  : name_(name)
{ 
  auto app_lib = module_table.find(app);
  if (app_lib != module_table.end())
    app_ = new Application(app_lib->name_, app_lib->num_ports_);
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
  if (app_->state == Application::READY)
    app_->start();
  else if (app_->state == Application::NEW)
    throw std::string("Data plane has not been configured, unable to start");
}


// Stops the application.
void 
Dataplane::down()
{
  if (app_->state == Application::RUNNING)
    app_->stop();
}


// Configures the data plane based on the application.
void
Dataplane::configure()
{
  if (app_->state == Application::NEW)
    app_->configure();
  else
    throw std::string("Data plane has already been configured")
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
