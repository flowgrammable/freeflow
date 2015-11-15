#include <dlfcn.h>

#include "application.hpp"

namespace fp
{

//
Application::Application(std::string const& name, int num_ports)
  : name_(name), state_(NEW), num_ports_(num_ports)
{ }


//
Application::~Application()
{ }


//
void
Application::start()
{
  state_ = RUNNING;
}


//
void
Application::stop()
{
  state_ = STOPPED;
}


//
void
Application::add_port(Port* p)
{
	bool res = false;
  for (int i = 0; i < num_ports_; i++)
  	if (ports_[i] == nullptr) {
  		ports_[i] = p;
  		res = true;
  	}
	if (!res)
		throw std::string("All ports have been added");
}


//
void
Application::remove_port(Port* p)
{
  for (int i = 0; i < num_ports_; i++)
  	if (ports_[i] == p)
  		ports_[i] = nullptr;
}


//
std::string
Application::name() const
{
	return name_;
}


//
auto
Application::state() const -> State
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

