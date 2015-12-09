#ifndef FP_PORT_HPP
#define FP_PORT_HPP

#include <string>
#include <netinet/in.h>

#include "packet.hpp"
#include "queue.hpp"
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
  enum Type { udp };

  // Port specific types.
  using Id = unsigned int;
  using Address = unsigned char const*;
  using Label = std::string;
  using Queue = Locking_queue<Context*>;
  using Function = void*(*)(void*);

  // Port configuration.
  struct __attribute__((__packed__)) Configuration
  {
    uint8_t down      : 1; // Port is "administratively" down.
    uint8_t no_recv   : 1; // Drop all packets received by this port.
    uint8_t no_fwd    : 1; // Drop all packets sent from this port.
    uint8_t no_pkt_in : 1; // Do not send packet-in messages for port.
    uint8_t unused    : 4;
  };

  // Ctor/Dtor.
  Port(Id);
  virtual ~Port();

  // Equality semantics.
  bool operator==(Port&);
  bool operator==(Port*);
  bool operator!=(Port&);
  bool operator!=(Port*);

  // The set of necessary port related functions that any port-type
  // must define.
  virtual int       open() = 0;
  virtual Context*  recv() = 0;
  virtual int       send() = 0;
  virtual void      send(Context*);
  virtual void      drop(Context*);
  virtual void      close() = 0;
  virtual Function  work_fn() = 0;

  // Set the ports state to 'up' or 'down'.
  void up();
  void down();

  // Accessors.
  Id id() const;

  // Data members.
  Id              id_;        // The internal port ID.
  Address         addr_;      // The hardware address for the port.
  Label           name_;      // The name of the port.
  Configuration   config_;    // The current port configuration.
  Queue           tx_queue_;  // The ports transmit (send) queue.
};


bool operator==(Port*, std::string const&);


} // end namespace FP

#endif
