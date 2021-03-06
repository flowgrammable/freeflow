#include "dataplane.hpp"
#include "port.hpp"
#include "port_drop.hpp"
#include "port_flood.hpp"
#include "application.hpp"

#include <cassert>
#include <algorithm>
#include <iostream>


namespace fp
{

Dataplane::~Dataplane()
{
  delete drop_;
  delete flood_;
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
Dataplane::add_virtual_ports()
{
  drop_ = new Port_drop;
  flood_ = new Port_flood;
  portmap_.emplace(drop_->id(), drop_);
  portmap_.emplace(flood_->id(), flood_);
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
  // FIXME: Bandaid for compiled steve app -> fp usage.
  app_->load(*this);
  /*
  if (app_->load(*this)) {
    delete app_;
    throw std::runtime_error("loading application");
  }
  */
  // Notify the application of all system ports.
  for (Port* p : ports_)
    app_->port_added(*p);
}


// Unload the application.
//
// FIXME: It should probably be the case that the data plane
// is down when the application is removed.
void
Dataplane::unload_application()
{
  assert(app_);
  // FIXME: Bandaid for compiled steve app -> fp usage.
  app_->unload(*this);
  /*
  if (app_->unload(*this))
    throw std::runtime_error("unloading application");
  */
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
  // Start the application.
  if (app_)
    app_->start(*this);
}


// Stops the data plane packet processors, if they are running.
//
// TODO: We shouldn't close ports until we know that all
// in-flight packets have been processed.
void
Dataplane::down()
{
  // Then stop the application.
  if (app_)
    app_->stop(*this);
}


// -------------------------------------------------------------------------- //
// Application interface

Port*
fp_get_drop_port(Dataplane* dp)
{
  return dp->get_drop_port();
}

Port*
fp_get_flood_port(Dataplane* dp)
{
  return dp->get_flood_port();
}

} // end namespace fp
