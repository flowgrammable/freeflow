#include "dataplane.hpp"
#include "port.hpp"
#include "application.hpp"

#include <cassert>


namespace fp
{

// Add a port to the dataplane.
void
Dataplane::add_port(Port* p)
{
  ports_.emplace(p->id(), p);
}


// Remove a port from the data plane.
void
Dataplane::remove_port(Port* p)
{
  ports_.erase(p->id());
}


// Returns the port with the given identifier.
Port*
Dataplane::get_port(uint32_t id)
{
  auto iter = ports_.find(id);
  if (iter != ports_.end())
    return iter->second;
  return nullptr;
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
  for (auto const& kv : ports_) {
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
  for (auto const& kv : ports_) {
    Port* p = kv.second;
    if (!p->close())
      throw std::runtime_error("open port");
  }

  // Then stop the application.
  app_->stop(*this);
}


} // end namespace fp
