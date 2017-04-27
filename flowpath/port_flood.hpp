#ifndef FP_PORT_FLOOD_HPP
#define FP_PORT_FLOOD_HPP

#include "port.hpp"

#include <freeflow/ip.hpp>

namespace fp
{

class Context;


// Represents the virtual "flood" port. Packets sent to the
// the flood port are sent from all ports except the input port.
//
// The id for this port is within the range of reserved
// ports, but is an extension of what OpenFlow traditionally
// considers to be valid.
class Port_flood : public Port
{
public:
  static constexpr Id id = 0xffffffef;

  Port_flood()
    : Port(id)
  { }

  // Packet related funtions.
  int open();
  int close();
  int send(Context*);
  int recv(Context*);
};


inline int
Port_flood::open()
{
  return true;
}


inline int
Port_flood::close()
{
  return true;
}


inline int
Port_flood::send(Context* cxt)
{
//  Port* input = cxt.input_port();
//  for (auto p : cxt.dataplane()->ports()) {
//    if (p != input)
//      p->send(cxt);
//  }
  assert(false);
  return true;
}


inline int
Port_flood::recv(Context* cxt)
{
  assert(false);
  return true;
}


} // end namespace fp

#endif
