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
Dataplane::add_application(Application* app)
{
  app_ = app;
}


// TODO: Document me.
void 
Dataplane::remove_application(Application* app)
{
  delete app_;
  app_ = nullptr;
}


// TODO: Document me.
void 
Dataplane::up()
{
  if (app_->state == Application::READY)
    app_->start();
  else
    throw std::string("Configuration failed, unable to start data plane");
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
}


} // end namespace fp
