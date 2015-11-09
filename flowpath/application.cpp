#include "application.hpp"

namespace fp
{



// Helper function to load a dll
void* 
load_library(std::string const& path)
{
  void* handle = dlopen(path.c_str(), RTLD_LOCAL | RTLD_LAZY);
  // Error loading the library
  if (not handle)
    throw std::runtime_error(dlerror());
  return handle;
}


// Helper function to load a symbol from a dll
void*
load_library_symbol(void* handle, std::string const& symbol) 
{
  // clear any previous errors if present
  dlerror(); 
  void* loaded_symbol = dlsym(handle, symbol.c_str());
  // get error status after dlsym call
  const char* dlsym_error = dlerror(); 
  // error loading the symbol
  if (dlsym_error) 
    throw std::runtime_error(dlsym_error);
  return loaded_symbol;
}


Application_library::Application_library(std::string const& path)
  : handle(load_library(path)), 
    factory(((Application_factory_fn)load_library_symbol(handle, "factory"))())
{
  // // Attempt to load the application's library file
  // void* handle = load_library(path);

  // // Attempt to load the factory symbol from the library
  // void* factory_fn = load_library_symbol(handle, "factory");
  
  // // Create a factory object for the application
  // Application_factory* factory = (Application_factory_fn)factory_fn();

}


// Create a new application through the library interface
Application*
Application_library::create(Dataplane& dp) const {
  return factory->create(dp); 
}


// Destroy an application
void
Application_library::destroy(Application& app) const { 
  return factory->destroy(app); 
}


} // end namespace fp

