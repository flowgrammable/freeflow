#include "port_table.hpp"
#include "port.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

// FIXME: Apple uses kqueue instead of epoll.
#if ! __APPLE__
#  include <sys/epoll.h>
#endif
#include <fcntl.h>

namespace fp
{


// Send the packet to all ports but the receiver.
int
Port_flood::send()
{
  // Check that this port is usable.
  if (config_.down)
    throw("port down");
  #if 0
  // Setup locals.
  Context* cxt = nullptr;
  int bytes = 0;
  int len = tx_queue_.size();
  for (int i = 0; i < len; i++) {
    // Get the next packet to send.
    cxt = tx_queue_.dequeue();

    // Iterator over ports
    auto iter = port_table.list().begin();
    while (++iter != port_table.list().end()) {
      // Skip the source port.
      if (iter->id() == cxt->in_port)
        continue;

      // Send the packet to the next port.
      // FIXME: Implement this.
      
      // Update number of bytes sent.
      bytes += l_bytes;
    }
    // Destroy the packet data.
    packet_destroy(cxt->packet_);

    // Destroy the packet context.
    delete(cxt);
  }
  // Return number of bytes sent.
  return bytes;
  #endif
  return 0;
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
    case Port::Type::tcp:
      data_[idx] = (value_type)new Port_tcp(idx+1, args);
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
//

// Returns the port that matches the given 'id'. If not found,
// returns a nullptr.
auto
Port_table::find(Port::Id id) -> value_type
{
  if (data_[id-1])
    return data_[id-1];
  else if (id == flood_port_->id())
    return flood_port();
  else if (id == broad_port_->id())
    return broad_port();
  else if (id == drop_port_->id())
    return drop_port();
  else
    return nullptr;
}


// Returns the port with the matching 'name'. If not found,
// returns a nullptr.
auto
Port_table::find(std::string const& name) -> value_type
{
  auto res = std::find(data_.begin(), data_.end(), name);
  if (res != data_.end())
    return *res;
  else if (name == "flood")
    return flood_port();
  else if (name == "broadcast")
    return broad_port();
  else if (name == "drop")
    return drop_port();
  else
    return nullptr;
}


auto
Port_table::list() -> store_type
{
  return data_;
}


void
port_mgr_work()
{
  // Setup epoll to watch multiple ports.
  struct epoll_event event, event_list[16];
  int conn_fd, epoll_fd, res;

  if ((epoll_fd = epoll_create1(0)) == -1)
    perror("port_mgr epoll_create");

  while(true) {
    // Poll for new events.
    res = epoll_wait(epoll_fd, event_list, 16, 1000);

    // Check for errors.
    if (res == -1) {
      perror("port_mgr epoll_wait");
      continue;
    }

    // Handle events.
    for (int i = 0; i < res; i++) {
      for (auto port : port_table.list()) {
        if (port->fd() == event_list[i].data.fd) {
          if (event_list[i].events & EPOLLIN)
            port->recv();
          else if (event_list[i].events & EPOLLOUT)
            port->send();
        }
      }
    }

    // Update the poll set.
    for (auto port : port_table.list()) {
      // Remove any downed ports from the poll set.
      if (port->config_.down) {       
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, port->fd(), &event) == -1)
          if (errno != ENOENT)
            perror("port_mgr epoll_ctl_del");
        continue;
      }
      
      // Attempt to add the usable port to the poll set.
      //
      // Set the socket to be non-blocking.
      int flags = fcntl(port->fd(), F_GETFL, 0);
      fcntl(port->fd(), F_SETFL, flags | O_NONBLOCK);
      
      // Set the events to listen for.
      event.events = EPOLLIN | EPOLLOUT | EPOLLET;
      event.data.fd = port->fd();

      // Add the file descriptor to the poll set.
      if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, port->fd(), &event) == -1)
        if (errno != EEXIST)
          perror("port_mgr epoll_ctl_add");
    }
  }
}


} // end namespace fp
