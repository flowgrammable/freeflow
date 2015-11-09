#ifndef FP_SYSTEM_HPP
#define FP_SYSTEM_HPP

#include <map>

#include "dataplane.hpp"
#include "port.hpp"
#include "application.hpp"


namespace fp 
{

using Module_table = std::map<std::string, Application_library>;

struct System 
{
  Dataplane_table dataplane_table;  // Master dataplane table.
  Module_table 		module_table; 		// DL modules table.

  Port* make_port(Port::Type, std::string const&);

  Dataplane& make_dataplane(std::string const&, std::string const&);

  void load_application(std::string const&);

};

} // end namespace fp

#endif