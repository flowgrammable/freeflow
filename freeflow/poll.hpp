
#ifndef FREEFLOW_POLL_HPP
#define FREEFLOW_POLL_HPP

#include <algorithm>
#include <vector>

#include <poll.h>

namespace ff
{

// -------------------------------------------------------------------------- //
//                            Polling structures

// A poll descriptor associates a file descriptor with a
// set of events for which it should be polled.
struct Poll_file : pollfd
{
  Poll_file() = default;

  // Initilize the polling file with the descriptor and a
  // set of events. The events read is zero-initialized.
  Poll_file(int f, short e)
    : pollfd { f, e, 0 }
  { }

  // Returns true if reading will not block.
  bool can_read() const { return revents & POLLIN; }

  // Returns true if writing will not block.
  bool can_write() const { return revents & POLLOUT; }

  // Returns true if there is an error condition.
  bool has_error() const { return revents & POLLERR; }
};


// A poll set is a sequence of polled files. A file
// descriptor may (but probably should not) appear
// multiple times in this sequence.
struct Poll_set : std::vector<Poll_file>
{
  using std::vector<Poll_file>::vector;

  void reset();

  iterator       file(int);
  const_iterator file(int) const;
};


// Reset the result events from polling.
inline void
Poll_set::reset()
{
  for (Poll_file& f : *this)
    f.revents = 0;
}


// Find the poll file having the given file descriptor.
inline Poll_set::iterator
Poll_set::file(int fd)
{
  return std::find_if(begin(), end(), [fd](Poll_file& f) {
    return f.fd == fd;
  });
}


inline Poll_set::const_iterator
Poll_set::file(int fd) const
{
  return std::find_if(begin(), end(), [fd](Poll_file const& f) {
    return f.fd == fd;
  });
}


// -------------------------------------------------------------------------- //
//                            Polling operations

// Poll a sequence of n files until an event has occurred.
inline int
poll(Poll_file* pfds, int n)
{
  return ::poll(pfds, n, -1);
}


// Poll a sequence of n files for timeout milliseconds.
inline int
poll(Poll_file* pfds, int n, int timeout)
{
  return ::poll(pfds, n, timeout);
}


// Poll a set of n files for timeout milliseconds.
inline int
poll(Poll_set& ps, int timeout)
{
  pollfd* pfds = &*ps.begin();
  return poll(pfds, ps.size(), timeout);
}


} // namespace ff

#endif
