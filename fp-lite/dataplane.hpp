#ifndef FP_DATAPLANE_HPP
#define FP_DATAPLANE_HPP

#include <list>
#include <string>
#include <vector>
#include <unordered_map>

#include "port.hpp"
#include "application.hpp"

namespace fp
{

extern Module_table module_table;
extern Thread_pool thread_pool;

struct Table;


// The flowpath data plane module. Contains an application, a name,
// and the tables the application will use during decode/lookup.
struct Dataplane
{
  // Ctor/dtor.
  Dataplane(std::string const&, std::string const&);
  ~Dataplane();

  // Wrappers to the application's add/remove port functions.
  void add_port(Port*);
  void remove_port(Port*);

  // Mutators.
  void up();
  void down();
  void configure();

  // Accessors.
  Application*        app() const;
  std::string         name() const;
  std::vector<Table*> tables() const;
  Table*              table(int);

  // Data members.
  //
  // Data plane name.
  std::string name_;

  // The set of tables applications can use.
  std::vector<Table*> tables_;

  // Application.
  Application* app_;
};

using Dataplane_table = std::unordered_map<std::string, Dataplane*>;

} // namespace

#endif
