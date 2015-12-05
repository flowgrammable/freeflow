
#ifndef FREEFLOW_CAPTURE_HPP
#define FREEFLOW_CAPTURE_HPP

// The PCAP module provides an interface to working with packet
// captures from tcpdump.

#include <cstdint>
#include <stdexcept>

#include <pcap/pcap.h>


namespace ff
{

namespace cap
{


// -------------------------------------------------------------------------- //
//                            Packet headers

// A packet provides a view into captured data from the device.
// In particular, the packet structure provides access to header
// information about the packet and its raw data.
//
// TODO: This is not well developed.
struct Packet
{
  Packet()
    : header(nullptr), data(nullptr)
  { }

  int captured() const { return header->caplen; }
  int size() const   { return header->len; }

  pcap_pkthdr*   header;
  uint8_t const* data;
};


// -------------------------------------------------------------------------- //
//                            Capture device


// A type used to support the opening of a capture.
struct Offline_path
{
  char const* path;
};


// A type used to support the initialize of a capture.
struct Offline_file
{
  FILE* file;
};


// Construct an offline initializer for `p`.
inline Offline_path
offline(char const* p) 
{ 
  return {p}; 
}


// Construct an offline initializer for the file `f`.
inline Offline_file
offline(FILE* f) 
{ 
  return {f}; 
}


// The capture class provides an interface to an offline or
// active catpure.
//
// Note that the capture device models a non-caching stream.
// This means that it is not possible to peek at the current
// packet without consuming it.
//
// TODO: Support the creation of new captures.
//
// TODO: Make the error buffer global or thread local?
//
// TODO: Allow capture streams to be used with the async module?
//
// FIXME: Make this move-only?
class Stream
{
public:
  Stream(Offline_path);
  Stream(Offline_file);
  ~Stream();

  // Observers
  bool ok() const { return status_ > 0; }

  // Streaming
  Stream& get(Packet&);

  // Contextual conversion to bool.
  explicit operator bool() const { return ok(); }

private:
  char    error_[PCAP_ERRBUF_SIZE]; // An error message (256 bytes)
  pcap_t* handle_;                  // The underlying device
  int     status_;                  // Result of the last get.
};


// Open the offline capture indicated by the path `p`. Throws
// an exception if the capture cannot be opened.
inline
Stream::Stream(Offline_path p)
  : handle_(::pcap_open_offline(p.path, error_))
{
  if (!handle_)
    throw std::runtime_error(error_);
}


// Open the offline capture indicated by the path `p`. Throws
// an exception if the capture cannot be opened.
inline
Stream::Stream(Offline_file f)
  : handle_(::pcap_fopen_offline(f.file, error_))
{
  if (!handle_)
    throw std::runtime_error(error_);
}


inline
Stream::~Stream()
{
  ::pcap_close(handle_);
}


// Attempt to get the next packet from the stream. Returns
// this object. If, after calling this function, the stream
// is not in a good state, the packet `p` is partially
// formed.
//
// Note that this function blocks until a packet is read.
inline Stream&
Stream::get(Packet& p)
{
  do { 
    status_ = ::pcap_next_ex(handle_, &p.header, &p.data);
  } while (status_ == 0);
  return *this;
}


} // namespace cap


} // namespace ff

#endif
