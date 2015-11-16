#ifndef FP_SYSTEM_HPP
#define FP_SYSTEM_HPP

#include "dataplane.hpp"
#include "port.hpp"
#include "port_table.hpp"
#include "thread.hpp"


namespace fp 
{

struct System 
{
  Port* create_port(Port::Type, std::string const&);
  void 	delete_port(Port::Id);

  Dataplane* create_dataplane(std::string const&, std::string const&);
  void			 delete_dataplane(std::string const&);

  void load_application(std::string const&);
  void unload_application(std::string const&);

  void* get_app_handle(std::string const&);
};

} // end namespace fp

#endif