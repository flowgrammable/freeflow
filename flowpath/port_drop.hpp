#ifndef FP_PORT_DROP_HPP
#define FP_PORT_DROP_HPP

#include "port.hpp"

#include <freeflow/ip.hpp>

namespace fp
{

class Context;


// Represents the virtual "drop" port. Packets sent to the
// the drop port are dropped from the pipeline.
//
// The id for this port is within the range of reserved
// ports, but is an extension of what OpenFlow traditionally
// considers to be valid.
class Port_drop : public Port
{
public:
  static constexpr Id id = 0xfffffff0;

  Port_drop()
    : Port(id)
  { }

  // Packet related funtions.
  int open();
  int close();
  int send(Context*);
  int recv(Context*);
};


inline int
Port_drop::open()
{
  return true;
}


inline int
Port_drop::close()
{
  return true;
}


inline int
Port_drop::send(Context*)
{
  return true;
}


inline int
Port_drop::recv(Context*)
{
  return true;
}


} // end namespace fp

#endif
