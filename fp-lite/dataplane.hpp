#ifndef FP_DATAPLANE_HPP
#define FP_DATAPLANE_HPP

#include <string>
#include <list>
#include <unordered_map>

namespace fp
{

struct Table;
class Application;
class Port;


// The flowpath data plane module. Contains an application, a name,
// and the tables the application will use during decode/lookup.
//
// TODO: Rethink how the port table works. Who assigns ids?
//
// TODO: Support multuiple applications.
class Dataplane
{
public:
  using Port_list = std::list<Port*>;
  using Port_map  = std::unordered_map<uint32_t, Port*>;
  using Table_map = std::unordered_map<uint32_t, Table*>;

  Dataplane(char const* n)
    : name_(n), drop_(nullptr), app_(nullptr)
  { }

  ~Dataplane();

  // Port management.
  //
  // FIXME: Ports cannot be added while the dataplane is running.
  // Adding a port means rebuilding the port tables so that an
  // application never has null ports.
  void add_port(Port*);
  void add_virtual_ports();
  void remove_port(Port*);

  // Returns the list of system ports.
  Port_list const& ports() const { return ports_; }
  Port_list&       ports()       { return ports_; }

  Port* get_port(uint32_t) const;
  Port* get_drop_port() const { return drop_; }
  Port* get_flood_port() const { return flood_; }

  // Application management.
  void load_application(char const*);
  void unload_application();

  Application* get_application() const { return app_; }

  // Table management.

  // State management.
  void up();
  void down();

  // Returns the name of the data plane.
  std::string const& name() const { return name_; }

  std::string name_;

  // TODO: Factor the reserved ports into a struct?
  // TODO: Support more reseverved ports.
  Port_list ports_;
  Port_map  portmap_;
  Port*     drop_;
  Port*     flood_;

  Table_map tables_;
  Application* app_;
};


// Returns the port with the given identifier.
inline Port*
Dataplane::get_port(uint32_t id) const
{
  auto iter = portmap_.find(id);
  if (iter != portmap_.end())
    return iter->second;
  return nullptr;
}


// -------------------------------------------------------------------------- //
// Application interface

extern "C"
{

Port* fp_get_drop_port(Dataplane*);
Port* fp_get_flood_port(Dataplane*);

} // extern "C"


} // namespace

#endif
