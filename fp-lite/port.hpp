#ifndef FP_PORT_HPP
#define FP_PORT_HPP

#include "context.hpp"

#include <string>


// The Flowpath namespace.
namespace fp
{

class Context;

// The base class for any port object. Contains methods for receiving,
// sending, and dropping packets, as well as the ability to close the
// port and modify its configuration.
class Port
{
public:
  enum Type { udp, tcp };

  // Port specific types.
  using Id = unsigned int;
  using Address = unsigned char const*;
  using Label = std::string;
  using Descriptor = int;

  // A port's configuration. These states can be set to determine
  // the behavior of the port.
  struct Configuration
  {
    bool down      : 1; // Port is administratively down.
    bool no_recv   : 1; // Drop all packets received by this port.
    bool no_fwd    : 1; // Drop all packets sent from this port.
    bool no_pkt_in : 1; // Do not send packet-in messages for port.
  };

  // A port's current state. These describe the observable state
  // of the device and cannot be modified.
  struct State
  {
    bool link_down : 1;
    bool blocked   : 1;
    bool live      : 1;
  };

  // Port statistics.
  struct Statistics
  {
    uint64_t packets_rx;
    uint64_t packets_tx;
    uint64_t bytes_rx;
    uint64_t bytes_tx;
  };

  // Ctor/Dtor.
  Port(Id, std::string const& = "");
  virtual ~Port();

  // The set of necessary port related functions that any port-type
  // must define.
  virtual bool open() = 0;
  virtual bool close() = 0;
  virtual bool send(Context) = 0;
  virtual bool recv(Context&) = 0;

  // Set the ports state to 'up' or 'down'.
  void up();
  void down();

  bool is_link_down() const  { return state_.link_down; }
  bool is_admin_down() const { return config_.down; }

  // Returns true if the port is either administratively or
  // physically down.
  bool is_down() const { return is_admin_down() || is_link_down(); }

  // Returns true if the port is, in some wy, active. This
  // means that it can potentially send or receive packets.
  bool is_up() const   { return !is_down(); }


  // Accessors.
  Id          id() const    { return id_; }
  Label       name() const  { return name_; }
  Statistics  stats() const { return stats_; }

protected:
  Id              id_;        // The internal port ID.
  Address         addr_;      // The hardware address for the port.
  Label           name_;      // The name of the port.
  Statistics      stats_;     // Statistical information about the port.
  Configuration   config_;    // The current port configuration.
  State           state_;     // The runtime state of the port.
};


// Port constructor that sets ID.
inline
Port::Port(Port::Id id, std::string const& name)
  : id_(id), name_(name), stats_(), config_(), state_()
{ }


// Port dtor.
inline
Port::~Port()
{ }


// Changes the port configuration to 'up'.
inline void
Port::up()
{
  config_.down = false;

  // FIXME: Actually reset stats?
  stats_ = Statistics();
}


// Change the port to a 'down' configuration.
inline void
Port::down()
{
  config_.down = true;
}


} // namespace fp


// -------------------------------------------------------------------------- //
// Application interface

extern "C"
{

int fp_port_get_id(fp::Port*);
int fp_port_is_up(fp::Port*);
int fp_port_is_down(fp::Port*);


} // extern "C"

#endif
