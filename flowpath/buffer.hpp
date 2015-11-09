#ifndef FP_BUFFER_HPP
#define FP_BUFFER_HPP

namespace fp
{

namespace Buffer
{

// Default buffer size.
const int FP_INIT_BUF = 4096;

// The base buffer object for packets.
// 
// TODO: Move this to its own file with a collection of
// more robust buffer alloc/dealloc methods?
struct Base
{
  unsigned char data_[FP_INIT_BUF];
  Base(unsigned char*);
};


// The default Flowpath buffer type.
struct Flowpath : public Base
{
  // TODO: Implement me? 
  // Do we need more than just the base data pointer?
};


// The NADK buffer type.
struct Nadk : public Base
{
  // TODO: Implement me.
};


// The netmap buffer type.
struct Netmap : public Base
{
  // TODO: Implement me.
};


// The dpdk buffer type.
struct Dpdk : public Base
{
  // TODO: Implement me.
};

} // end namespace buffer

} // end namespace fp

#endif