#ifndef FP_DATAPLANE_HPP
#define FP_DATAPLANE_HPP

#include <list>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

#include "port.hpp"
#include "application.hpp"

namespace fp
{


struct Dataplane
{
  // Data plane name.
  std::string name_; 

  std::map<std::string, Table*> tables;

  // Application.
  Application* app_;

  Dataplane(std::string const&, std::string const&);
  ~Dataplane();

  void add_port(Port*);
  void remove_port(Port*);

  void up();
  void down();

  void configure();

  Application* app();
  std::string name() const;
};

} // namespace

#endif
