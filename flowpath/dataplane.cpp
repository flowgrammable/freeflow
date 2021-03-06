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


// Starts the data plane packet processors. If the application has configured
// the data plane, it will install the application in the thread pool and start
// it. Otherwise it reports errors.
void
Dataplane::up()
{
  if (!app_)
    throw std::string("No applicaiton is installed.");
  else if (app_->state() == Application::State::READY) {
    thread_pool.install(app());
    thread_pool.start();
  }
  else if (app_->state() == Application::State::NEW)
    throw std::string("Data plane has not been configured, unable to start");
}


// Stops the data plane packet processors, if they are running.
void
Dataplane::down()
{
  if (app_->state() == Application::State::RUNNING) {
    thread_pool.stop();
    thread_pool.uninstall();
  }
  else
    throw std::string("Data plane is not running.");
}


// Configures the data plane based on the application.
void
Dataplane::configure()
{
  if (app_->state() == Application::State::NEW) {
    app_->lib().config();
    app_->state_ = Application::State::READY;
  }
  else
    throw std::string("Data plane has already been configured.");
}


// Gets the data plane name.
std::string
Dataplane::name() const
{
  return name_;
}


// Gets the data planes application.
Application*
Dataplane::app() const
{
  return app_;
}


// Gets the data planes tables.
std::vector<Table*>
Dataplane::tables() const
{
  return tables_;
}


// Gets the table at the given index.
Table*
Dataplane::table(int idx)
{
  return tables_.at(idx);
}


} // end namespace fp
