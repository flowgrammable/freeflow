#include "dataplane.hpp"
#include "port.hpp"
#include "port_drop.hpp"
#include "application.hpp"

#include <cassert>


namespace fp
{

Dataplane::~Dataplane()
{
  delete drop_;
}


// Add a logical or phyisical port to the dataplane. This must
// not be used for reserved ports.
void
Dataplane::add_port(Port* p)
{
  ports_.push_back(p);
  portmap_.emplace(p->id(), p);
}


// Add an explicit drop port to the dataplane.
void
Dataplane::add_drop_port()
{
  drop_ = new Port_drop;
  portmap_.emplace(drop_->id(), drop_);
}


// Remove a port from the data plane.
//
// FIXME: Implement me.
void
Dataplane::remove_port(Port* p)
{
  auto iter = std::find(ports_.begin(), ports_.end(), p);
  portmap_.erase(p->id());
  ports_.erase(iter);
  delete p;
}


// Load an application from the given path.
void
Dataplane::load_application(char const* path)
{
  assert(!app_);
  app_ = new Application(path);
  if (app_->load(*this)) {
    delete app_;
    throw std::runtime_error("loading application");
  }

  // Notify the application of all system ports.
  for (Port* p : ports_)
    app_->port_added(*p);
}


// Unload the application.
void
Dataplane::unload_application()
{
  assert(app_);
  if (app_->unload(*this))
    throw std::runtime_error("unloading application");
  delete app_;
  app_ = nullptr;
}


// Starts executing an application on a dataplane.
//
// FIXME: Dataplanes also have state. We don't want to re-up
// if we've already upped.
void
Dataplane::up()
{
  assert(app_);

  // Start the application.
  app_->start(*this);

  // And then open all ports.
  for (auto const& kv : portmap_) {
    Port* p = kv.second;
    if (!p->open())
      throw std::runtime_error("open port");
  }
}


// Stops the data plane packet processors, if they are running.
void
Dataplane::down()
{
  // Down all ports.
  //
  // TODO: We shouldn't close ports until we know that all
  // in-flight packets have been processed.
  for (auto const& kv : portmap_) {
    Port* p = kv.second;
    if (!p->close())
      throw std::runtime_error("open port");
  }

  // Then stop the application.
  app_->stop(*this);
}


// -------------------------------------------------------------------------- //
// Application interface

Port*
fp_get_drop_port(Dataplane* dp)
{
  return dp->get_drop_port();
}


} // end namespace fp
