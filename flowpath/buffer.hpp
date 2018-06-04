#ifndef FP_BUFFER_HPP
#define FP_BUFFER_HPP

#include "types.hpp"

namespace fp
{

namespace Buffer
{

// Buffer type enumeration.
enum BUF_TYPE {
  FP_BUF_NULL,
  FP_BUF_SIMPLE,
  FP_BUF_DPDK,
  FP_BUF_NETMAP,
  FP_BUF_NADK,
  FP_BUF_ODP,
  FP_BUF_PCAP
};


struct Base
{
  Base();
  virtual ~Base();

  virtual inline const BUF_TYPE type() const { return FP_BUF_NULL; }

  int bytes_;
  uint8_t* data_;
};


// The default Flowpath buffer type.
//
// TODO: Implement a ring buffer so memory is pre-allocated
// and the contents of each buffer node gets recycled.
struct Simple : public Base
{
  using Base::Base;

  Simple(const uint8_t*, int);
  virtual ~Simple();

  virtual inline const BUF_TYPE type() const { return FP_BUF_SIMPLE; }

  // TODO: follow up to ensure bytes_ is being used properly...
};


// The NADK buffer type.
struct Nadk : public Simple
{
  using Simple::Simple;
  // TODO: Implement me.
};


// The netmap buffer type.
struct Netmap : public Simple
{
  using Simple::Simple;
  // TODO: Implement me.
};


// The dpdk buffer type.
struct Dpdk : public Simple
{
  using Simple::Simple;
  // TODO: Implement me.
};

// The odp buffer type.
// - this buffer type is simply a pointer to a pre-allocated buffer.
struct Odp : public Base
{
  using Base::Base;

  Odp(uint8_t*, int);
  Odp(const uint8_t*, int);
  ~Odp();

  inline const BUF_TYPE type() const { return FP_BUF_ODP; }
};


// The pcap buffer type.
struct Pcap : public Simple
{
  using Simple::Simple;

  Pcap(uint8_t*, int);
  ~Pcap();

  inline const BUF_TYPE type() const { return FP_BUF_PCAP; }

  int wire_bytes_;  // original bytes observed on wire...
  // NOTE: this is initialized to captured size and overwritten later as needed
};


} // end namespace buffer
} // end namespace fp

#endif
