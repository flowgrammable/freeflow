#ifndef FP_PORT_HPP
#define FP_PORT_HPP

#include <cstdlib>
#include <string>


// The Flowpath namespace.
namespace fp
{

struct Context;

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

  // Port configuration.
  struct Configuration
  {
    bool down      : 1; // Port is "administratively" down.
    bool no_recv   : 1; // Drop all packets received by this port.
    bool no_fwd    : 1; // Drop all packets sent from this port.
    bool no_pkt_in : 1; // Do not send packet-in messages for port.
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
  virtual bool send(Context const&) = 0;
  virtual bool recv(Context&) = 0;

  // Set the ports state to 'up' or 'down'.
  void up();
  void down();

  // Returns the state of the port.
  bool is_up() const   { return !config_.down; }
  bool is_down() const { return config_.down; }

  // Accessors.
  Id          id() const    { return id_; }
  Label       name() const  { return name_; }
  Statistics  stats() const { return stats_; }
  Descriptor  fd() const    { return fd_; }

  // Data members.
  Id              id_;        // The internal port ID.
  Address         addr_;      // The hardware address for the port.
  Label           name_;      // The name of the port.
  Statistics      stats_;     // Statistical information about the port.
  Configuration   config_;    // The current port configuration.

  // TODO: It's likely that DPDK descriptors are not
  // the same as POSIX file descriptors. This will need
  // to be factored out.
  Descriptor      fd_;        // The devices file descriptor.
};


// Port constructor that sets ID.
inline
Port::Port(Port::Id id, std::string const& name)
  : id_(id), name_(name), stats_(), config_()
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

#endif
