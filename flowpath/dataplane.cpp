#include "dataplane.hpp"
#include "system.hpp"
#include <algorithm>

namespace fp {

Dataplane::Dataplane()
  : Dataplane(nullptr, "", "") 
{ }

Dataplane::~Dataplane() 
{ 

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
Dataplane::add_application(const std::string& path)
{
  // Check if library from this path hasn't been loaded.
  if (system->module_table.find(path) == system->module_table.end())
    throw("module does not exist");

  app_ = system->module_table[path].create(*this);
}


// TODO: Document me.
void 
Dataplane::remove_application(Application* app)
{
  delete app_;
  app_ = nullptr;
}


// Runs the configure function for all applications that
// have not been configured yet. This allows the applications
// to perform any necessary tasks before starting
void 
Dataplane::configure()
{
  if (app_->state == Application::NEW)
    app_->configure();
}


// TODO: Document me.
void 
Dataplane::up()
{
  if (app_->state == Application::READY)
    app_->start();
  else
    throw("Err(Dataplane::up): Application not configured, unable to start.");  
}


// Stops the application.
void 
Dataplane::down()
{
  if (app_->state == Application::RUNNING)
    app_->stop();
}


} // end namespace fp
