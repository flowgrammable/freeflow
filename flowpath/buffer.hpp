#ifndef FP_BUFFER_HPP
#define FP_BUFFER_HPP

#include <cstdint>

namespace fp
{

namespace Buffer
{

struct Base
{
  Base(uint8_t*);
  virtual ~Base();

  uint8_t* data_;
};


// The default Flowpath buffer type.
//
// TODO: Implement a ring buffer so memory is pre-allocated
// and the contents of each buffer node gets recycled.
struct Flowpath : public Base
{
  using Base::Base;
  // TODO: Implement me?
  // Do we need more than just the base data pointer?
};


// The NADK buffer type.
struct Nadk : public Base
{
  using Base::Base;
  // TODO: Implement me.
};


// The netmap buffer type.
struct Netmap : public Base
{
  using Base::Base;
  // TODO: Implement me.
};


// The dpdk buffer type.
struct Dpdk : public Base
{
  using Base::Base;
  // TODO: Implement me.
};

} // end namespace buffer

} // end namespace fp

#endif
