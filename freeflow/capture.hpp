
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
// Packet headers

// A packet provides a view into captured data from the device.
// In particular, the packet structure provides access to header
// information about the packet and its raw data.
//
// TODO: This is not well developed.
struct Packet
{
  Packet()
    : hdr(nullptr), buf(nullptr)
  { }

  // Returns the total number of bytes in the packet. Note that
  // this may be larger than the number of bytes captured.
  int total_size() const { return hdr->len; }

  // Returns the number of bytes actually captured. The captured
  // size is less than or equal to total size.
  int captured_size() const { return hdr->caplen; }

  // Returns true when the packet is fully captured.
  bool is_complete() const { return captured_size() == total_size(); }

  /* Returns the time stamp of the packet. This represents the time
     elapsed since the beginning of the capture. */
  timeval timestamp() const { return hdr->ts; }

  // Returns the underlying packet data.
  uint8_t const* data() const { return buf; }

  pcap_pkthdr*   hdr;
  uint8_t const* buf;
};


// -------------------------------------------------------------------------- //
// Link types


// Describes the link layer protocol captured by a stream. The
// capture can occur at any layer, so we need to provide a method
// for an anlayzer to perform an initial decode.
//
// TODO: Where do the LINKTYPE_ names live?
enum Link_type
{
  null_link     = DLT_NULL,
  ethernet_link = DLT_EN10MB,
  cooked_link   = DLT_LINUX_SLL,
  wifi_link     = DLT_IEEE802_11_RADIO,
  ip_link       = DLT_RAW,
};


// -------------------------------------------------------------------------- //
// Capture device

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

  Link_type link_type() const;

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


// Returns the link layer type of the stream.
inline Link_type
Stream::link_type() const
{
  return (Link_type)pcap_datalink(handle_);
}



// Attempt to get the next packet from the stream. Returnsthis object.
// If, after calling this function, the stream is not in a good state,
// the packet `p` is partially formed.
//
// Note that this function blocks until a packet is read.
inline Stream&
Stream::get(Packet& p)
{
  do {
    status_ = ::pcap_next_ex(handle_, &p.hdr, &p.buf);
  } while (status_ == 0);
  return *this;
}


} // namespace cap


} // namespace ff

#endif
