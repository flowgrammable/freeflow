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

struct System;


struct Dataplane
{
  int         id;    /* Unique data plane id. */
  int         state; /* The current state of the dataplane. */
  std::string name;  /* Data plane name. */
  std::string type;  /* Data plane type. */

  std::map<std::string, Table*> tables;

  // Application.
  Application* app_;

  /* Configuration parameters for Modular stages. */
  size_t key_size;   /* Number of bytes of user-defined Key */
  
  System* system;    /* The system that owns the dataplane */
  
  Dataplane();
  Dataplane(System* sys, std::string const& dp_name, std::string const& dp_type)
    : name(dp_name), type(dp_type), system(sys)
  { }
  
  ~Dataplane();

  void add_port(Port*);
  void remove_port(Port*);

  void add_application(const std::string&);
  void remove_application(Application*);

  void configure();

  void up();
  void down();
};


using Dataplane_table = std::unordered_map<std::string, Dataplane*>;

} // namespace

#endif
