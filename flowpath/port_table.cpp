#include "port_table.hpp"
#include "port.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <iterator>
#include <iostream>

// FIXME: OSX uses kqueue rather than epoll.
#if ! __APPLE__
  #include <sys/epoll.h>
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

  // Get the next packet to send.
  Context* cxt = nullptr;
  int bytes = 0;
  while (tx_queue_.size()) {
    cxt = tx_queue_.front();
    // Iterator over ports
    auto iter = port_table.list().begin();
    while (++iter != port_table.list().end()) {
      Port_udp* p = (Port_udp*)*iter;

      // Skip the source port.
      if (p->id_ == cxt->in_port)
        continue;

      // Send the packet to the next port.
      int l_bytes = sendto(fd_, cxt->packet_->data(), cxt->packet_->size(), 0,
        (struct sockaddr*)&p->src_addr_, sizeof(struct sockaddr_in));

      // Destroy the packet data.
      delete cxt->packet();

      // Destroy the packet context.
      delete cxt;
      tx_queue_.pop();

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
  : data_(size, nullptr), handles_(), thread_(0, port_table_work)
{
  flood_port_ = new Port_flood("127.0.0.1:8675;flood");
  //flood_port_->thread_->assign(flood_port_->id_, flood);
  //broad_port_ = new Port_broad(":8674");
  //drop_port_ = new Port_drop(":8673");
  //thread_.run();
}


// Port table destructor.
Port_table::~Port_table()
{
  thread_.halt();
  data_.clear();
  handles_.clear();
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
#ifdef FP_ENABLE_ODP
  case Port::Type::odp_burst:
    data_[idx] = (value_type)new Port_odp(idx+1, args);
    break;
#endif // FP_ENABLE_ODP
  default:
    std::cerr << "Warning - unknown Port::Type (" << port_type
              << ") on Port_table::alloc" << std::endl;
  }
  handles_.insert({data_[idx]->fd(), data_[idx]});
  // Return a pointer to the newly allocated port object.
  return data_[idx];
}


// Deallocates the internal port ID given.
void
Port_table::dealloc(Port::Id id)
{
  if (data_[id-1]) {
    handles_.erase(data_[id-1]->fd());
    delete data_[id-1];
  }
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


auto
Port_table::handles() -> handler_type
{
  return handles_;
}


void
Port_table::handle(int fd)
{
  handles_[fd]->recv();
  handles_[fd]->send();
}


void*
port_table_work(void* arg)
{
  // Setup epoll.
  struct epoll_event event, event_list[16];
  int epoll_fd;

  // Create the epoll file descriptor.
  if ((epoll_fd = epoll_create1(0)) == -1) {
    perror("port_table_work epoll_create");
    return nullptr;
  }

  // Start polling...
  //
  // FIXME: Create a condition for running...
  while (true) {
    // Wait for MAX_EVENTS (16) or timeout at 1000ms (1s).
    int num_events;

    // NOTE: We loop on waiting to ensure we start over if we get
    // interrupted by a system call and need to call wait again.
    do {
      num_events = epoll_wait(epoll_fd, event_list, 16, 1000);
    } while (num_events == -1 && errno == EINTR);

    // Check for errors.
    if (num_events == -1) {
      perror("port_table_work epoll_wait");
      continue;
    }

    // Process messages.
    //
    // TODO: Check for erronious file descriptors and remove
    // them from the poll set.
    for (int i = 0; i < num_events; i++) {
      // Handle the event.
      port_table.handle(event_list[i].data.fd);
    }

    // Add new file descriptors to the poll set from the handler map.
    for (auto const & pair : port_table.handles()) {
      // Set the fd and event type(s).
      event.data.fd = pair.second->fd();
      event.events = EPOLLIN | EPOLLOUT;
      // Attempt to add it to the poll set. Ignore if already exists.
      if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pair.second->fd(), &event) == -1)
        if (errno != EEXIST)
          perror("port_table_work epoll_ctl_add");
    }
  }
}


} // end namespace fp
