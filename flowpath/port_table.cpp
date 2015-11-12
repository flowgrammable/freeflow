#include "port_table.hpp"
#include "port.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

namespace fp
{

// The global port table.
Port_table port_table;


// Send the packet to all ports but the receiver.
int
Port_flood::send()
{
  // Check that this port is usable.
  if (config_.down)
    throw("port down");

  // Get the next packet to send.
  Context* cxt = nullptr;
  int bytes = 0;
  while ((cxt = tx_queue_.dequeue())) {
    // Iterator over ports
    auto iter = port_table.list().begin();
    while (++iter != port_table.list().end()) {
      Port_udp* p = (Port_udp*)*iter;

      // Skip the source port.
      if (p->id_ == cxt->in_port)
        continue;
      
      // Send the packet to the next port.
      int l_bytes = sendto(sock_fd_, cxt->packet_->buf_.data_, cxt->packet_->size_, 0, 
        (struct sockaddr*)&p->src_addr_, sizeof(struct sockaddr_in));
    
      // Destroy the packet data.
      packet_destroy(cxt->packet_);
      
      // Destroy the packet context.
      delete(cxt);

      // Update number of bytes sent.
      bytes += l_bytes;
    }
  }
  // Return number of bytes sent.
  return bytes;
}


// Port table definitions.
//
// Default port table constructor.
Port_table::Port_table()
  : Port_table(100)
{ }


// Port table constructor with initial size.
Port_table::Port_table(int size)
  : data_(size, nullptr)
{ 
  flood_port_ = new Port_flood(":8675");
  //flood_port_->thread_->assign(flood_port_->id_, flood);
  //broad_port_ = new Port_broad(":8674");
  //drop_port_ = new Port_drop(":8673");
}


// Port table destructor.
Port_table::~Port_table()
{
  data_.clear();

}


// Allocates a new internal ID for a port to use.
auto
Port_table::alloc(Port::Type port_type, std::string const& args) -> value_type
{
  // Find an open slot and calculate its index.
  auto iter = std::find(data_.begin(), data_.end(), nullptr);
  Port::Id idx = std::distance(data_.begin(), iter);

  // Allocate the proper port type.
  switch (port_type)
  {
    case Port::Type::udp:
      data_[idx] = (value_type)new Port_udp(idx+1, args);
    break;
  }

  // Return a pointer to the newly allocated port object.
  return data_[idx];
}


// Deallocates the internal port ID given.
void     
Port_table::dealloc(Port::Id id)
{  
  if (data_[id-1])
    delete data_[id-1];
  data_[id-1] = nullptr;
}


// Accessors.
auto
Port_table::find(Port::Id id) -> value_type
{
  return (data_[id-1] ? data_[id-1] : nullptr);
}


auto
Port_table::list() -> store_type
{
  return data_;
}


} // end namespace fp