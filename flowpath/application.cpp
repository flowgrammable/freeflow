#include <dlfcn.h>

#include "application.hpp"
#include "thread.hpp"

namespace fp
{

extern Module_table module_table;

//
Application::Application(std::string const& name, int num_ports)
  : name_(n), state_(NEW), num_ports_(num_ports)
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
void
Application::configure()
{	
  module_table[name_]->exec("config")();
}


//
std::string
Application::name() const
{
	return name_;
}


//
App_state
Application::state() const
{
	return state_;
}


//
Port**
Application::ports() const
{
	return ports_;
}


//
int
Application::num_ports() const
{
	return num_ports_;
}




} // end namespace fp

