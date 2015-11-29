#include <dlfcn.h>

#include "application.hpp"

namespace fp
{


//
Application::Application(std::string const& name)
  : name_(name), state_(NEW), lib_(get_app_handle(name))
{ 
  lib_.exec("ports")(&num_ports_);
}


//
Application::~Application()
{ 
  dlclose(lib_.handles_["app"]);
}


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


//
auto
Application::lib() const -> Library
{
  return lib_;
}


//
Library::Library(Handle app)
{ 
  handles_["app"] = app;
  for (auto& pair : handles_) {
    if (!pair.second)
      pair.second = get_sym_handle(app, pair.first);
  }
}


//
Library::~Library()
{ }


// Returns the function matching the given string name. Throws if not defined.
auto
Library::exec(std::string const& cmd) -> Func
{
  try 
  {
    return (Func)handles_.at(cmd);
  }
  catch (std::out_of_range)
  {
    throw std::string("Undefined reference to '" + cmd + "'");
  }
}


// Returns a handle to the given symbol from the given application handle.
void*
get_sym_handle(void* app_hndl, std::string const& sym)
{
  dlerror();
  void* sym_hndl = dlsym(app_hndl, sym.c_str());
  const char* err = dlerror();
  if (err)
    throw std::runtime_error(err);
  else
    return sym_hndl;
}


// Returns a handle to the application at the given path, if it exists.
void*
get_app_handle(std::string const& path)
{
  void* hndl = dlopen(path.c_str(), RTLD_LOCAL | RTLD_LAZY);
  if (hndl)
    return hndl;
  else
    throw std::runtime_error(dlerror());
}


} // end namespace fp

