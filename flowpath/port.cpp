#include "port.hpp"

#include <climits>
#include <netinet/in.h>
#include <cassert>

namespace fp
{

// Port definitions.
//


// Port constructor that sets ID.
Port::Port(Port::Id id, std::string const& name)
  : id_(id), name_(name)
{ }


// Port dtor.
Port::~Port()
{
	if (addr_)
		delete addr_;
}


// Comparators.
//
bool
Port::operator==(Port& other)
{
  return this->id_ == other.id_;
}


bool
Port::operator==(Port* other)
{
  return this->id_ == other->id_;
}


bool
operator==(Port* p, std::string const& name)
{
  return p->name_ == name;
}


bool
Port::operator!=(Port& other)
{
  return !(*this == other);
}


bool
Port::operator!=(Port* other)
{
  return !(this == other);
}


// Changes the port configuration to 'up'; that is, there are
// no flags set that would indicate the port is not able to
// function.
void
Port::up()
{
  // Clear the bitfield (uint8_t).
  *(uint8_t*)(&config_) = 0;
  // Clear statistics.
  stats_.byt_tx = stats_.byt_rx = stats_.pkt_tx = stats_.pkt_rx = 0;
}


// Change the port to a 'down' configuration.
void
Port::down()
{
  config_.down = 1;
}


// Enqueues context in the ports transmit queue.
void
Port::send(Context* cxt)
{
  tx_queue_.enqueue(cxt);
}


// Sends the packet to the drop port.
void
Port::drop(Context* cxt)
{
  // port_table.drop()->send(cxt);
}


// Gets the port id.
Port::Id
Port::id() const
{
  return id_;
}


// Gets the port name.
Port::Label
Port::name() const
{
  return name_;
}

// Gets the port statistics.
Port::Statistics
Port::stats() const
{
  return stats_;
}

} // end namespace FP
