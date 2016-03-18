#ifndef FP_PORT_TABLE_HPP
#define FP_PORT_TABLE_HPP

#include "port.hpp"
#include "port_udp.hpp"
#include "port_tcp.hpp"
#include "port_odp.hpp"
#include "thread.hpp"

#include <string>
#include <vector>
#include <climits>
#include <unordered_map>

namespace fp
{

// This is a special port used to flood a packet to all ports
// except for the one that received it.
class Port_flood : public Port_udp
{
public:
  using Port_udp::Port_udp;
  // Constructor/Destructor.
  Port_flood(std::string const& addr)
    : Port_udp(0xffff, addr)
  { }

  ~Port_flood() { }

  int   send();
};

// Global port table type.
class Port_table
{
  using store_type = std::vector<Port*>;
  using value_type = Port*;
  using iter_type  = std::vector<Port*>::iterator;
  using handler_type = std::unordered_map<int, Port*>;
public:
  // Constructors.
  //
  // Default.
  Port_table();
  // Initial size.
  Port_table(int);

  // Destructor.
  ~Port_table();

  // Allocator.
  Port* alloc(Port::Type, std::string const&);

  // Deallocator.
  void  dealloc(Port::Id);

  // Accessors.
  value_type    find(Port::Id);
  value_type    find(std::string const&);
  store_type    list();
  handler_type  handles();

  value_type flood_port() const { return flood_port_; }
  value_type broad_port() const { return broad_port_; }
  value_type drop_port()  const { return drop_port_; }

  // Event handler.
  void handle(int);

private:
  store_type    data_;
  handler_type  handles_;
  Thread        thread_;

  // Reserved ports.
  value_type flood_port_;
  value_type broad_port_;
  value_type drop_port_;
};

extern Port_table port_table;
extern void* port_table_work(void*);

} // end namespace fp

#endif
