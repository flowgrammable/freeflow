#include "application_library.hpp"

namespace fp
{


//
Application_library::Application_library(std::string const& name, Handle app)
  : name_(name), app_(app)
{ 
	for (auto& pair : handles_) {
		pair.second = get_sym_handle(app_, pair.first);
	}
	ports_ = *((int*)get_sym_handle(app_, "ports"));
}


//
Application_library::~Application_library()
{ }


// Returns the function matching the given string name. Throws if not defined.
auto
Application_library::exec(std::string const& cmd) -> Handle
{
	try 
	{
		return handles_.at(cmd)
	}
	catch (std::out_of_range)
	{
		throw std::string("Undefined reference to '" + cmd + "' in '" + app_ + "'");
	}
}


// Returns a handle to the given symbol from the given application handle.
void*
Application_library::get_sym_handle(void* app_hndl, std::string const& sym)
{
  dlerror();
  void* sym_hndl = dlsym(app_hndl, sym.c_str());
  const char* err = dlerror();
  if (err)
    throw std::runtime_error(err);
  else
    return sym_hndl;
}


} // end namespace fp