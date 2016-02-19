#ifndef FREEFLOW_SELECT_HPP
#define FREEFLOW_SELECT_HPP

#include <sys/select.h>
#include <cstring>

namespace ff
{


// A file set contains file descriptors being monitored for 
// activity in a select set.
struct Select_file : fd_set
{
  Select_file();
  Select_file(Select_file const&);
  Select_file& operator=(Select_file const&);

  inline void add(int);
  inline void del(int);
  inline void reset();
  inline int  contains(int) const;
};


// File set ctor. Zero initializes the set.
Select_file::Select_file()
{
  FD_ZERO(this);
}


// Select file copy ctor.
Select_file::Select_file(Select_file const& other)
{
  FD_ZERO(this);
  std::memcpy(this, &other, sizeof(fd_set));
}

Select_file&
Select_file::operator=(Select_file const& other)
{
  FD_ZERO(this);
  std::memcpy(this, &other, sizeof(fd_set));
  return *this;
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


//
struct Select_set
{
  // Select set ctor.
  Select_set();

  // Mutators.
  //
  // Add a file descriptor to a select file.
  inline void add_read(int fd);
  inline void add_write(int fd);
  inline void add_error(int fd);

  // Remove a file descriptor from a select file.
  inline void del_read(int fd);
  inline void del_write(int fd);
  inline void del_error(int fd);
  
  // Clear a select file.
  inline void reset_read();
  inline void reset_write();
  inline void reset_error();

  // Query Functions.
  //
  inline bool can_read(int fd)  const;
  inline bool can_write(int fd) const;
  inline bool has_error(int fd) const;

  // Accessors.
  //
  inline int  max() const;

  // Current max FD (last added).
  int max_;

  // The master select files, needed for copying.
  Select_file master_[3];

  // The auxilliary select files for read, write, error.
  Select_file aux_[3];
};


// Select set ctor. Intialize master files and auxilliary files.
Select_set::Select_set()
  : max_(0)
{ 
  master_[0] = Select_file();
  master_[1] = Select_file();
  master_[2] = Select_file();
}


// Add a file descriptor to the read select file.
inline void 
Select_set::add_read(int fd)  
{ 
  master_[0].add(fd); 
  max_ = fd > max_ ? fd : max_; 
}


// Add a file descriptor to the write select file.
inline void 
Select_set::add_write(int fd) 
{ 
  master_[1].add(fd); 
  max_ = fd > max_ ? fd : max_; 
}


// Add a file descriptor to the error select file.
inline void 
Select_set::add_error(int fd) 
{ 
  master_[2].add(fd); 
  max_ = fd > max_ ? fd : max_; 
}


// Remove a file descriptor from a the read select file.
inline void 
Select_set::del_read(int fd)  
{ 
  master_[0].del(fd); 
}


// Remove a file descriptor from a the write select file.
inline void 
Select_set::del_write(int fd) 
{ 
  master_[1].del(fd); 
}


// Remove a file descriptor from a the error select file.
inline void 
Select_set::del_error(int fd) 
{ 
  master_[2].del(fd); 
}


// Reset the read select file.
inline void 
Select_set::reset_read()  
{ 
  master_[0].reset(); 
}

// Reset the write select file.
inline void 
Select_set::reset_write() 
{ 
  master_[1].reset(); 
}


// Reset the error select file.
inline void 
Select_set::reset_error() 
{ 
  master_[2].reset(); 
}


// Check if the file descriptor is able to read without blocking.
inline bool 
Select_set::can_read(int fd) const 
{ 
  return aux_[0].contains(fd); 
}


// Check if the file descriptor is able to write (may block).
inline bool 
Select_set::can_write(int fd) const 
{ 
  return aux_[1].contains(fd); 
}


// Check if the file descriptor has reported an error.
inline bool 
Select_set::has_error(int fd) const 
{ 
  return aux_[2].contains(fd); 
}


// Returns the highest file descriptor added to any select file.
inline int  
Select_set::max() const
{
  return max_;
}


// -------------------------------------------------------------------------- //
//                            Select operations

inline int
select(Select_set& ss, int timeout)
{
  // Copy the master list into the auxilliary list. This is necessary due to the
  // fact that select alters the fd_sets in-place.
  for (int i = 0; i < 3; i++)
    ss.aux_[i] = ss.master_[i];

  // Create a timeout in milliseconds.
  struct timeval to = {0, timeout * 1000};

  // Call select on the select sets files.
  return select(ss.max() + 1, &ss.aux_[0], &ss.aux_[1], &ss.aux_[2], &to);
}


}

#endif