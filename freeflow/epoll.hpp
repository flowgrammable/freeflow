#ifndef FREEFLOW_EPOLL_HPP
#define FREEFLOW_EPOLL_HPP

// Only build on a linux machine.
#ifdef LINUX

#include <sys/epoll.h>
#include <stdint>

namespace ff
{

// An event returned from using epoll. 
struct Epoll_event : epoll_event
{
  Epoll_event() = default;

  // Returns true if a read event occurred.
  bool can_read() const { return events & EPOLLIN == EPOLLIN; }
  // Returns true if a write event occurred.
  bool can_write() const { return events & EPOLLOUT == EPOLLOUT; }
  // Returns true if a error event occurred.
  bool has_error() const { return events & EPOLLERR == EPOLLERR; }

  // Get the file descriptor associated with the event.
  inline int fd() const { return data.fd; }
};


struct Epoll_set : std::array<Epoll_event>
{
  using std::array<Epoll_event>::array;
  Epoll_set(int);

  // Mutators.
  inline void add(int);
  inline void del(int);
  inline void reset();

  // Accessors.
  inline int fd() const { return epfd_; }

  // Data Members.
  //
  // Epoll file descriptor.
  int epfd_;
  // Number of events after a call to epoll_wait.
  int num_events_;
  // The epoll master event.
  Epoll_event epev_;
};


// Epoll set ctor.
Epoll_set::Epoll_set(int size)
  : std::array<Epoll_event>(size)
{

}

//
inline void
Epoll_set::add(int fd)
{
  epev_.events = EPOLLIN;
  epev_.data.fd = fd;
  epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &epev_);
}


//
inline void
Epoll_set::del(int fd)
{
  epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &epev);
}


//
inline void
Epoll_set::reset()
{
  epev_ = Epoll_event();
}



//
inline int
epoll(Epoll_set& eps, int timeout)
{
  eps.num_events_ = epoll_wait(eps.fd(), eps.data(), eps.max(), timeout);
  return eps.num_events_;
}

} // end namespace ff

#endif
// end if LINUX

#endif