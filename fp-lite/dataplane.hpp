#ifndef FP_DATAPLANE_HPP
#define FP_DATAPLANE_HPP

#include <string>
#include <vector>
#include <unordered_map>


namespace fp
{

struct Table;
class Port;
class Application;


// The flowpath data plane module. Contains an application, a name,
// and the tables the application will use during decode/lookup.
//
// TODO: Support multuiple applications.
class Dataplane
{
public:
  using Port_map = std::unordered_map<uint32_t, Port*>;
  using Table_map = std::unordered_map<uint32_t, Table*>;

  Dataplane(char const* n)
    : name_(n)
  { }

  // Port management.
  void add_port(Port*);
  void remove_port(Port*);
  Port* get_port(uint32_t);

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
  Port_map  ports_;
  Table_map tables_;
  Application* app_;
};


} // namespace

#endif
