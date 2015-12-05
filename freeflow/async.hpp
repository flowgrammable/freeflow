
#ifndef FREEFLOW_ASYNC_HPP
#define FREEFLOW_ASYNC_HPP

// This module provides internal facilities for building
// reactive loops. It does not dfeine specific methods
// for invoking the actions.

#include <unordered_map>
#include <unordered_set>


namespace ff
{

// An async I/O handler defines a polymorphic interface for responding 
// to I/O events.
//
// Every I/O handler is attached to a file descriptor. Note that 
// this object is not responsible for acquiring the descriptor, nor 
// releasing it.
struct Io_handler
{
  Io_handler(int fd)
    : fd_(fd)
  { }

  virtual ~Io_handler() { }

  int fd() const { return fd_; }

  virtual bool on_input() { return 1; }
  virtual bool on_output() { return 1; }
  virtual bool on_error() { return 1; }

  int fd_;
};


// A hash function for I/O handlers.
struct Io_hash
{
  std::size_t operator()(Io_handler const* io) const 
  {
    std::hash<int> h;
    return h(io->fd());
  }
};


// An I/O map is a mapping from file descriptors to I/O handlers. 
// This is required for any application that needs to demultiplex 
// events.
struct Io_map : std::unordered_map<int, Io_handler*>
{
  using std::unordered_map<int, Io_handler*>::unordered_map;
};



// An I/O set is an unordered set of I/O handler objects. This is
// typically used to associate a set of handlers with a particular
// event (e.g., remember I/O handlers that need to be closed).
struct Io_set : std::unordered_set<Io_handler*, Io_hash>
{
  using std::unordered_set<Io_handler*, Io_hash>::unordered_set;
};


} // namespace ff


#endif
