#ifndef FP_APPLICATION_LIBRARY_HPP
#define FP_APPLICATION_LIBRARY_HPP

namespace fp
{


// The Application_library class represents a dynamically loaded
// library that implements flowpath applications.
// The factory is used to instantiate those applications whenever
// they are launched.
struct Application_library
{  
  using Handle = void*;
  using Map = std::unordered_map<std::string, Handle>;

  // The library name.
  const std::string name_;
  
  // The defined application handles.
  Map handles_ = 
  {
    {"app", nullptr},
    {"pipeline", nullptr},
    {"config", nullptr}
  };

  // The number of ports required by the application.
  int    num_ports_;
  
  Application_library(std::string const&, Handle);
  ~Application_library();

  // Returns the function matching the given string name.
  auto exec(std::string const&) -> Handle;
  void* get_sym_handle(void*, std::string const&);
};


}

#endif