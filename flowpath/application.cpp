#include "application.hpp"

namespace fp
{


//
Application::Application(std::string const& name, Fn pipeline, Fn config, int num_ports)
  : name_(n), state_(NEW), pipeline_(pipeline), config_(config), num_ports_(num_ports)
{ }


//
Application::~Application()
{ }


//
inline void
Application::start()
{
  state_ = RUNNING;
}


//
inline void
Application::stop()
{
  state_ = STOPPED;
}


//
auto
Application::pipeline() -> Pipe_fn
{
  return *pipeline_;
}


//
void
Application::configure()
{
  (*(*config_))();
  ports_ = new Port*[num_ports_];
  state_ = READY;
}


//
Application_library::Application_library(std::string const& name, Handle app, Handle pipeline, Handle config)
  : name_(name), app_(app), pipeline_(pipeline), config_(config)
{ }


//
Application_library::~Application_library()
{ }


} // end namespace fp

