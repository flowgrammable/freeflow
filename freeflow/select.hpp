#ifndef FREEFLOW_SELECT_HPP
#define FREEFLOW_SELECT_HPP

#include <freeflow/time.hpp>

#include <cstring>
#include <cassert>
#include <algorithm>

#include <sys/select.h>

#include <iostream>

namespace ff
{


// A file set contains file descriptors being monitored for 
// activity in a select set.
struct Select_file : fd_set
{
  Select_file();

  inline void add(int);
  inline void del(int);
  inline void reset();
  inline int contains(int) const;
};


// File set constructor. Initialize all file descriptors to 0.
Select_file::Select_file()
{
  FD_ZERO(this);
}


// Adds a file descriptor to the file set.
inline void 
Select_file::add(int fd)
{
  FD_SET(fd, this);
}


// Removes a file descriptor from the file set.
inline void 
Select_file::del(int fd)
{
  FD_CLR(fd, this);
}


// Clears the file set.
inline void 
Select_file::reset()
{
  FD_ZERO(this);
}


// Checks if the given file descriptor exists in
// the file set. Returns the file descriptor if true,
// otherwise returns 0.
inline int  
Select_file::contains(int fd) const
{
  return FD_ISSET(fd, this);
}


// A select-set contains fd-sets that can be used to determine
// whether an event is detected or not.
struct Select_set
{
  Select_set();

  // Add a file descriptor to a select file.
  void add_read(int fd);
  void add_write(int fd);
  void add_error(int fd);

  // Remove a file descriptor from a select file.
  void del_read(int fd);
  void del_write(int fd);
  void del_error(int fd);
  
  // Clear a select file.
  void reset_read();
  void reset_write();
  void reset_error();
  void reset();

  // Query Functions.
  bool can_read(int fd)  const;
  bool can_write(int fd) const;
  bool has_error(int fd) const;

  // Accessors.
  int  max() const;

  Select_file orig_[3]; // Specified files to select on.
  Select_file test_[3]; // Used to store results of test.
  int max_;             // Current max FD (last added).
};


// Select set ctor. Initialize master files and auxilliary files.
Select_set::Select_set()
  : orig_(), test_(), max_(0)
{ 
}


// Add a file descriptor to the read select file.
inline void 
Select_set::add_read(int fd)  
{ 
  assert(fd >= 0);
  orig_[0].add(fd); 
  max_ = std::max(max_, fd);
}


// Add a file descriptor to the write select file.
inline void 
Select_set::add_write(int fd) 
{ 
  assert(fd >= 0);
  orig_[1].add(fd); 
  max_ = std::max(max_, fd);
}


// Add a file descriptor to the error select file.
inline void 
Select_set::add_error(int fd) 
{ 
  assert(fd >= 0);
  orig_[2].add(fd); 
  max_ = std::max(max_, fd);
}


// Remove a file descriptor from a the read select file.
inline void 
Select_set::del_read(int fd)  
{ 
  assert(fd >= 0);
  orig_[0].del(fd); 
}


// Remove a file descriptor from a the write select file.
inline void 
Select_set::del_write(int fd) 
{ 
  assert(fd >= 0);
  orig_[1].del(fd); 
}


// Remove a file descriptor from a the error select file.
inline void 
Select_set::del_error(int fd) 
{ 
  assert(fd >= 0);
  orig_[2].del(fd); 
}


// Reset the read select file.
inline void 
Select_set::reset_read()  
{ 
  orig_[0].reset(); 
}

// Reset the write select file.
inline void 
Select_set::reset_write() 
{ 
  orig_[1].reset(); 
}


// Reset the error select file.
inline void 
Select_set::reset_error() 
{ 
  orig_[2].reset(); 
}


// Reset all file sets.
inline void
Select_set::reset()
{
  reset_read();
  reset_write();
  reset_error();
}


// Check if the file descriptor is able to read without blocking.
inline bool 
Select_set::can_read(int fd) const 
{ 
  assert(fd >= 0);
  return test_[0].contains(fd); 
}


// Check if the file descriptor is able to write (may block).
inline bool 
Select_set::can_write(int fd) const 
{ 
  assert(fd >= 0);
  return test_[1].contains(fd); 
}


// Check if the file descriptor has reported an error.
inline bool 
Select_set::has_error(int fd) const 
{ 
  assert(fd >= 0);
  return test_[2].contains(fd); 
}


// Returns the highest file descriptor added to any select file.
inline int  
Select_set::max() const
{
  return max_;
}


// -------------------------------------------------------------------------- //
// Select function

// FIXME: Take a duration for the time out instead of an int.
inline int
select(Select_set& ss, Microseconds timeout)
{
  assert(timeout.count() < 1000000);

  // Copy the master list into the test list. 
  ss.test_[0] = ss.orig_[0];
  ss.test_[1] = ss.orig_[1];
  ss.test_[2] = ss.orig_[2];

  // Create a timeout in milliseconds.
  struct timeval to = {0, (suseconds_t)timeout.count()};

  // Call select on the select sets files.
  int ret = select(ss.max() + 1, &ss.test_[0], &ss.test_[1], &ss.test_[2], &to);

  // On error, reset the test fds, so that we don't get false positives.
  if (ret < 0) {
    ss.test_[0].reset();
    ss.test_[1].reset();
    ss.test_[2].reset();
  }

  return ret;
}


} // namespace freeflow

#endif
