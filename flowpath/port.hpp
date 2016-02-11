#ifndef FP_PORT_HPP
#define FP_PORT_HPP

#include <string>
#include <queue>
#include <netinet/in.h>

#include "packet.hpp"
#include "thread.hpp"


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
  enum Type { udp, tcp, odp_burst, odp_queue };

  // Port specific types.
  using Id = unsigned int;
  using Address = unsigned char const*;
  using Label = std::string;
  using Queue = std::queue<Context*>;
  using Descriptor = int;

  // Port configuration.
  struct __attribute__((__packed__)) Configuration
  {
    uint8_t down      : 1; // Port is "administratively" down.
    uint8_t no_recv   : 1; // Drop all packets received by this port.
    uint8_t no_fwd    : 1; // Drop all packets sent from this port.
    uint8_t no_pkt_in : 1; // Do not send packet-in messages for port.
    uint8_t unused    : 4;
  };

  // Port statistics.
  struct Statistics
  {
    Statistics()
      : pkt_rx(0), pkt_tx(0), byt_rx(0), byt_tx(0)
    { }

    // Packet statistics.
    uint64_t pkt_rx;
    uint64_t pkt_tx;
    uint64_t byt_rx;
    uint64_t byt_tx;
  };

  // Ctor/Dtor.
  Port(Id, std::string const& = "");
  virtual ~Port();

  // Equality semantics.
  bool operator==(Port&);
  bool operator==(Port*);
  bool operator!=(Port&);
  bool operator!=(Port*);

  // The set of necessary port related functions that any port-type
  // must define.
  virtual int  open() = 0;
  virtual int  recv() = 0;
  virtual int  send() = 0;
  virtual void close() = 0;

  // Functions derived ports aren't responsible for defining.
  void send(Context*);
  void drop(Context*);

  // Set the ports state to 'up' or 'down'.
  void up();
  void down();

  // Accessors.
  Id          id() const;
  Label       name() const;
  Statistics  stats() const;
  Descriptor  fd() const;

  // Data members.
  Id              id_;        // The internal port ID.
  Address         addr_;      // The hardware address for the port.
  Label           name_;      // The name of the port.
  Statistics      stats_;     // Statistical information about the port.
  Configuration   config_;    // The current port configuration.
  Descriptor      fd_;        // The devices file descriptor.
  Queue           tx_queue_;  // The ports transmit (send) queue.
};


bool operator==(Port*, std::string const&);


} // end namespace FP

#endif
