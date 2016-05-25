#ifndef FREEFLOW_EPOLL_HPP
#define FREEFLOW_EPOLL_HPP

// Only build on a linux machine.
#ifdef __linux__

#include <sys/epoll.h>
#include <sys/socket.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <string>

namespace ff
{

// An event returned from using epoll.
struct Epoll_event : epoll_event
{
  Epoll_event() = default;

  // Returns true if a read event occurred.
  bool can_read() const { return (events & EPOLLIN); }
  
  // Returns true if a write event occurred.
  bool can_write() const { return (events & EPOLLOUT); }
  
  // Returns true if a error event occurred.
  bool has_error() const { return (events & EPOLLERR); }

  // Get the file descriptor associated with the event.
  inline int fd() const { return data.fd; }
};



struct Epoll_set : std::vector<Epoll_event>
{
  using std::vector<Epoll_event>::vector;
  Epoll_set(int);

  // Mutators.
  inline void add(int);
  inline void del(int);
  inline void reset();

  // Accessors.
  //
  // Return the epoll file descriptor.
  inline int fd() const { return epfd_; }
  
  // Return the maximum number of events to listen for.
  inline int max() const { return max_; }
  
  // Returns true if the given file descriptor can read.
  bool can_read(int);

  // Returns true if the given file descriptor can write.
  bool can_write(int);

  // Returns true if the given file descriptor has an error.
  bool has_error(int);

  // Reports the error for the given file descriptor.
  std::string get_error(int);

  // Data Members.
  //
  // Epoll file descriptor.
  int epfd_;
  
  // Number of events after a call to epoll_wait.
  int num_events_;
  
  // Maximum number of events to listen for.
  int max_;
  
  // The epoll master event.
  Epoll_event epev_;
};


// Epoll set ctor.
Epoll_set::Epoll_set(int size)
  : std::vector<Epoll_event>(size), max_(size)
{
  epfd_ = epoll_create(size);
}

//
inline void
Epoll_set::add(int fd)
{
  epev_.events = EPOLLIN | EPOLLOUT;
  epev_.data.fd = fd;
  epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &epev_);
}


//
inline void
Epoll_set::del(int fd)
{
  epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &epev_);
}


//
inline void
Epoll_set::reset()
{
  epev_ = Epoll_event();
}


//
inline bool
Epoll_set::can_read(int fd)
{
  return std::find_if(begin(), end(), [fd](Epoll_event& epev) {
    return (epev.fd() == fd && epev.can_read());
  }) != end();
}


//
inline bool
Epoll_set::can_write(int fd)
{
  return std::find_if(begin(), end(), [fd](Epoll_event& epev) {
    return (epev.fd() == fd && epev.can_write());
  }) != end();
}


//
inline bool
Epoll_set::has_error(int fd)
{
  return std::find_if(begin(), end(), [fd](Epoll_event& epev) {
    return (epev.fd() == fd && epev.has_error());
  }) != end();
}


//
inline std::string
Epoll_set::get_error(int fd)
{
  int err = 0;
  socklen_t errlen = sizeof(err);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)&err, &errlen);
  return strerror(err);
}


// Epoll operations.

// Call epoll on an epoll set with a given timeout. Returns the number of
// events, or -1 if an error occurred.
//
// FIXME: Take a duration, not an int.
inline int
epoll(Epoll_set& eps, int timeout)
{
  eps.num_events_ = epoll_wait(eps.fd(), eps.data(), eps.max(), timeout);
  return eps.num_events_;
}

} // namespace ff

#endif


#endif
